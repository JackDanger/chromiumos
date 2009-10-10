// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "offline_credential_store.h"

#include <ctype.h>
#include <stdio.h>
#include <glog/logging.h>
#include <openssl/evp.h>

#include "pam_google/pam_prompt_wrapper.h"  // for kUserEnvVariable.

namespace {

using chromeos_pam::Blob;

char DecodeChar(char in) {
  in = tolower(in);
  if ((in <= '9') && (in >= '0')) {
    return in - '0';
  } else {
    CHECK_GE(in, 'a');
    CHECK_LE(in, 'f');
    return in - 'a' + 10;
  }
}

class ScopedFilePointer {
 public:
  explicit ScopedFilePointer(FILE* const fp) : fp_(fp) {}
  ~ScopedFilePointer() {
    if (!fp_)
      return;
    CHECK_EQ(0, fclose(fp_));
  }
  FILE* Get() { return fp_; }
 private:
  FILE* const fp_;
  DISALLOW_COPY_AND_ASSIGN(ScopedFilePointer);
};

// separates email and hashed password
const char kFieldDelimiter = ' ';

};  // namespace {}

namespace chromeos_pam {

const int kWeakHashBits = 128;

string OfflineCredentialStore::AsciiEncode(const Blob& blob) {
  char table[] = "0123456789abcdef";
  string out;
  for (Blob::const_iterator it = blob.begin(); blob.end() != it; ++it) {
    out += table[((*it) >> 4) & 0xf];
    out += table[*it & 0xf];
  }
  CHECK_EQ(blob.size() * 2, out.size());
  return out;
}

Blob OfflineCredentialStore::AsciiDecode(const string& str) {
  Blob out;
  if (str.size() % 2)
    return out;
  for (string::const_iterator it = str.begin(); it != str.end(); ++it) {
    char append = DecodeChar(*it);
    append <<= 4;

    ++it;

    append |= DecodeChar(*it);
    out.push_back(append);
  }
  CHECK_EQ(out.size() * 2, str.size());
  return out;
}

void OfflineCredentialStore::ExportCredentials(const string& name,
                                               const Blob& hash) {
  // Export name for use in other pam modules.
  setenv(kUserEnvVariable, name.c_str(), 1);
  // Export |name| as an environment variable for the screen locker too.
  string environment_var(kUserEnvVariable);
  environment_var.append("=");
  environment_var.append(name);
  wrapper_->PamPutenv(environment_var.c_str());

  // export ascii-encoded hash as PAM_AUTHTOK.
  string ascii_hash = AsciiEncode(hash);
  wrapper_->PamSetItem(PAM_AUTHTOK,
                       reinterpret_cast<const void*>(ascii_hash.c_str()));
}

void OfflineCredentialStore::Store(const string& name, const Blob& hash) {
  ExportCredentials(name, hash);

  if (!credentials_loaded_)
    LoadCredentials();
  credentials_[name] = hash;

  // store new credentials to disk
  ScopedFilePointer fp(fopen(path_.c_str(), "w+"));
  if (!fp.Get()) {
    LOG(WARNING) << "Could not open offline credential store.";
    return;
  }
  for (map<string, Blob>::iterator it = credentials_.begin();
       it != credentials_.end(); ++it) {
    int count_written = fwrite(it->first.data(), 1, it->first.size(), fp.Get());
    if (static_cast<int>(it->first.size()) != count_written) {
      LOG(WARNING) << "Failed writing name to offline credential store";
      return;
    }
    count_written = fwrite(&kFieldDelimiter, 1, 1, fp.Get());
    if (1 != count_written) {
      LOG(WARNING) << "Failed writing delimiter to offline credential store";
      return;
    }
    string encoded_hash = AsciiEncode(it->second);
    count_written = fwrite(encoded_hash.data(), 1, encoded_hash.size(),
                           fp.Get());
    if (static_cast<int>(encoded_hash.size()) != count_written) {
      LOG(WARNING) << "Failed writing hash to offline credential store";
      return;
    }
    count_written = fwrite("\n", 1, 1, fp.Get());
    if (1 != count_written) {
      LOG(WARNING) << "Failed writing newline to offline credential store";
      return;
    }
  }
}

bool OfflineCredentialStore::Contains(const string& name, const Blob& hash) {
  if (!credentials_loaded_)
    LoadCredentials();
  map<string, Blob>::iterator it = credentials_.find(name);
  if (credentials_.end() != it && it->second == hash) {
    ExportCredentials(name, hash);
    return true;
  }
  return false;
}

bool OfflineCredentialStore::LoadCredentials() {
  CHECK(!credentials_loaded_);
  credentials_loaded_ = true;
  ScopedFilePointer fp(fopen(path_.c_str(), "r"));
  if (!fp.Get())
    return false;
  char line[1024];
  while (fgets(line, sizeof(line), fp.Get())) {
    char *delimiter = strchr(line, kFieldDelimiter);
    if ('\0' == *delimiter) {
      // delimiter not found. abort
      return false;
    }
    string name(line, delimiter);
    string pass(delimiter + 1);
    if ('\n' != pass[pass.size() - 1]) {
      // didn't get the entire hash. abort
      // we probably didn't read the entire line in, which means
      // the line is very long, which is quite suspicious.
      return false;
    }
    pass.resize(pass.size() - 1);
    if (pass.size() % 2) {
      // bad length
      return false;
    }
    Blob binary_pass = AsciiDecode(pass);
    if (binary_pass.size() > 0)
      credentials_[name] = binary_pass;
  }
  return true;
}

// returns a weak hash of |password|.
Blob OfflineCredentialStore::WeakHash(const char* const password) {
  EVP_MD_CTX mdctx;
  const EVP_MD *md;
  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;

  OpenSSL_add_all_digests();

  md = EVP_get_digestbyname("sha256");
  EVP_MD_CTX_init(&mdctx);
  EVP_DigestInit_ex(&mdctx, md, NULL);
  EVP_DigestUpdate(&mdctx, password, strlen(password));
  EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
  EVP_MD_CTX_cleanup(&mdctx);

  // grab the top |kWeakHashBits| bits, zero the rest.
  memset(md_value + kWeakHashBits/8, 0, kWeakHashBits/8);
  return Blob(md_value, md_value + kWeakHashBits/8);
}

const string kDefaultOfflineCredentialStorePath(
    "/var/cache/google_offline_login_cache.txt");

};  // namespace chromeos_pam
