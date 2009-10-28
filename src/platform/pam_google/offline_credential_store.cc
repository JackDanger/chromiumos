// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pam_google/offline_credential_store.h"

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
  explicit ScopedFilePointer(FILE* fp) : fp_(fp) {}
  ~ScopedFilePointer() {
    if (!fp_)
      return;
    CHECK_EQ(0, fclose(fp_));
  }
  FILE* Get() { return fp_; }
  FILE* Release() {
    FILE* fp_copy = fp_;
    fp_ = NULL;
    return fp_copy;
  }
 private:
  FILE* fp_;
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

void OfflineCredentialStore::Store(const string& name,
                                   const string& salt,
                                   const Blob& hash) {
  if (!credentials_loaded_)
    LoadCredentials();
  credentials_[name] = pair<Blob, string>(hash, salt);

  // store new credentials to disk
  ScopedFilePointer fp(fopen(path_.c_str(), "w+"));
  if (!fp.Get()) {
    LOG(WARNING) << "Could not open offline credential store.";
    return;
  }
  for (map<string, pair<Blob, string> >::iterator it = credentials_.begin();
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
    string encoded_hash = AsciiEncode((it->second).first);
    count_written = fwrite(encoded_hash.data(), 1, encoded_hash.size(),
                           fp.Get());
    if (static_cast<int>(encoded_hash.size()) != count_written) {
      LOG(WARNING) << "Failed writing hash to offline credential store";
      return;
    }
    count_written = fwrite(&kFieldDelimiter, 1, 1, fp.Get());
    if (1 != count_written) {
      LOG(WARNING) << "Failed writing delimiter to offline credential store";
      return;
    }
    count_written = fwrite((it->second).second.data(),
                           1,
                           (it->second).second.size(),
                           fp.Get());
    if (static_cast<int>((it->second).second.size()) != count_written) {
      LOG(WARNING) << "Failed writing salt to offline credential store";
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
  map<string, pair<Blob, string> >::iterator it = credentials_.find(name);
  if(credentials_.end() != it && it->second.first == hash)
    return true;
  return false;
}

string OfflineCredentialStore::GetSalt(const string& name) {
  static const int kSaltLength = 16;
  if (!credentials_loaded_)
    LoadCredentials();
  map<string, pair<Blob, string> >::iterator it = credentials_.find(name);
  if (credentials_.end() != it) {
    return string(it->second.second);
  }
  // Create a new salt if the user isn't known.
  return GenerateSalt(kSaltLength);
}

bool OfflineCredentialStore::LoadCredentials() {
  CHECK(!credentials_loaded_);
  credentials_loaded_ = true;
  ScopedFilePointer fp(fopen(path_.c_str(), "r"));
  if (!fp.Get())
    return false;
  char line[1024];
  while (fgets(line, sizeof(line), fp.Get())) {
    char *user_delimiter = strchr(line, kFieldDelimiter);
    if (!user_delimiter || '\0' == *user_delimiter) {
      // delimiter not found. abort
      return false;
    }
    string name(line, user_delimiter);
    char *salt_delimiter = strchr(user_delimiter + 1, kFieldDelimiter);

    if (!salt_delimiter || '\0' == *salt_delimiter) {
      // Delimiter not found. Either the offline credential file is
      // corrupted, or it's still using the old style credentials with no salt.
      // In either case, truncate the file.
      CHECK_EQ(0, fclose(fp.Release()));
      LOG(WARNING) << "Malformed credential file found";
      ScopedFilePointer truncate_fp(fopen(path_.c_str(), "w"));
      if (!truncate_fp.Get()) {
        LOG(WARNING) << "Couldn't truncate malformed credential file";
        return false;
      }
      return true;
    }

    string pass(user_delimiter + 1, salt_delimiter);
    if (pass.size() % 2) {
      // bad length
      LOG(WARNING) << "Bad password hash length.";
      return false;
    }

    string salt(salt_delimiter + 1);
    if ('\n' != salt[salt.size() - 1]) {
      // didn't get the entire salt. abort
      // we probably didn't read the entire line in, which means
      // the line is very long, which is quite suspicious.
      return false;
    }
    salt.resize(salt.size() - 1);

    Blob binary_pass = AsciiDecode(pass);
    if (binary_pass.size() > 0)
      credentials_[name] = pair<Blob, string>(binary_pass, salt);
  }
  return true;
}

bool OfflineCredentialStore::GetRandom(char *buf, size_t size) {
  const char *kDevUrandom = "/dev/urandom";
  ScopedFilePointer fp(fopen(kDevUrandom, "r"));
  if (!fp.Get()) {
    LOG(WARNING) << "Could not open /dev/urandom";
    return false;
  }
  if (fread(buf, size, 1, fp.Get()) != 1) {
    LOG(ERROR) << "Could not read bytes from urandom";
    return false;
  }
  return true;
}

string OfflineCredentialStore::GenerateSalt(unsigned int length) {
  char* salt = new char[length+1];
  if (!GetRandom(salt, length)) {
    return string("");
  }
  return AsciiEncode(Blob(salt, salt+length));
}

string OfflineCredentialStore::GetSystemSalt() {
  const char *kSaltFile = "/home/.shadow/salt";
  static const int kMaxSystemSalt = 256;
  char buf[kMaxSystemSalt];
  ScopedFilePointer fp(fopen(kSaltFile, "r"));
  if (!fp.Get()) {
    LOG(WARNING) << "Could not open " << kSaltFile;
    return "nosyssalt";
  }
  size_t salt_len = fread(buf, 1, sizeof(buf), fp.Get());
  if (salt_len == 0) {
    LOG(ERROR) << "Could not read system salt file";
    return "nosyssalt";
  }
  return AsciiEncode(Blob(buf, buf+salt_len));
}

// returns a weak hash of |salt||password|.
Blob OfflineCredentialStore::WeakHash(const char* const salt,
                                      const char* const password) {
  EVP_MD_CTX mdctx;
  const EVP_MD *md;
  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;


  OpenSSL_add_all_digests();

  md = EVP_get_digestbyname("sha256");
  EVP_MD_CTX_init(&mdctx);
  EVP_DigestInit_ex(&mdctx, md, NULL);
  EVP_DigestUpdate(&mdctx, salt, strlen(salt));
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
