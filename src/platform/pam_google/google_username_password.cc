// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GoogleUsernamePassword wraps a username/password pair that can be
// used to authenticate to Google.

#include "pam_google/google_username_password.h"

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "base/logging.h"
#include <curl/curl.h>

#include <string>

#include "pam_google/offline_credential_store.h"

namespace {

// Secure memset - volatile qualifier prevents a call to memset from being
// optimized away.
//
// Based on memset_s in:
// https://buildsecurityin.us-cert.gov/daisy/bsi-rules/home/g1/771-BSI.html
//
// TODO: Move to utility functions after optimizing further.
void* SecureMemset(void *v, int c, size_t n) {
  volatile unsigned char *p = static_cast<unsigned char *>(v);
  while (n--)
    *p++ = c;
  return v;
}

}  // namespace



namespace chromeos_pam {

GoogleUsernamePassword::GoogleUsernamePassword(const char *username,
                                               const int username_length,
                                               const char *password,
                                               const int password_length,
                                               OfflineCredentialStore *store)
    : dont_free_memory_(false), store_(store) {
  username_ = new char[username_length+1];
  password_ = new char[password_length+1];

  strncpy(username_, username, username_length);
  strncpy(password_, password, password_length);
  username_[username_length] = password_[password_length] = 0;
  curl_ = curl_easy_init();
  if (store_) {
    salt_ = store_->GetSalt(username_);
    system_salt_ = store_->GetSystemSalt();
  }
}

// ONLY FOR TESTING
GoogleUsernamePassword::GoogleUsernamePassword(const char *username,
                                               const int username_length,
                                               const char *password,
                                               const int password_length,
                                               OfflineCredentialStore *store,
                                               bool dont_free_memory)
    : dont_free_memory_(dont_free_memory), store_(store) {
  username_ = new char[username_length+1];
  password_ = new char[password_length+1];
  strncpy(username_, username, username_length);
  strncpy(password_, password, password_length);
  username_[username_length] = password_[password_length] = 0;
  curl_ = curl_easy_init();
  if (store_) {
    salt_ = store_->GetSalt(username_);
    system_salt_ = store_->GetSystemSalt();
  }
}

GoogleUsernamePassword::~GoogleUsernamePassword() {
  SecureMemset(username_, 0, strlen(username_));
  SecureMemset(password_, 0, strlen(password_));
  delete [] username_;
  if (!dont_free_memory_) {
    delete [] password_;
  }
  curl_easy_cleanup(curl_);
}

int GoogleUsernamePassword::Urlencode(const char *data,
                                      char *buffer,
                                      int length) {
  // urlencoded will be NULL if there's a problem, and a
  // null-terminated string upon success.
  char *urlencoded = curl_easy_escape(curl_, data, 0);
  if (NULL == urlencoded) {
    return -1;
  }

  int bytes_written = snprintf(buffer, length, "%s", urlencoded);
  SecureMemset(urlencoded, 0, strlen(urlencoded));
  curl_free(urlencoded);
  if (length <= bytes_written) {
    return -1;
  } else {
    buffer[bytes_written] = 0;
    return bytes_written;
  }
}

/**
 * See comment in google_credentials.h
 */
int GoogleUsernamePassword::Format(char *payload, int length) {
  static const char format[] = "Email=%s&"
                               "Passwd=%s&"
                               "PersistentCookie=%s&"
                               "accountType=%s&"
                               "source=%s&";

  // 3*strlen is the max you can get with character escaping
  char *encoded_username= new char[3*strlen(username_)];
  char *encoded_password= new char[3*strlen(password_)];

  Urlencode(username_, encoded_username, 3*strlen(username_));
  Urlencode(password_, encoded_password, 3*strlen(password_));

  int bytes_written = snprintf(payload, length,
                               format,
                               encoded_username,
                               encoded_password,
                               kCookiePersistence,
                               kAccountType,
                               kSource);
  SecureMemset(encoded_password, 0, 3*strlen(password_));
  if (length <= bytes_written) {
    return -1;
  } else {
    return bytes_written;
  }
}

/**
 * See comment in google_credentials.h
 */
void GoogleUsernamePassword::GetActiveUser(char *name_buffer, int length) {
  char *at_ptr = strrchr(username_, '@');
  *at_ptr = '\0';
  strncpy(name_buffer, username_, length);
  *at_ptr = '@';
}

/**
 * See comment in google_credentials.h
 */
void GoogleUsernamePassword::GetActiveUserFull(char *name_buffer, int length) {
  strncpy(name_buffer, username_, length);
}

#ifdef CHROMEOS_PAM_LOCALACCOUNT
bool GoogleUsernamePassword::IsLocalAccount() {
  return 0 == strncmp(username_, kLocalAccount, strlen(kLocalAccount));
}
#endif

bool GoogleUsernamePassword::IsAcceptable() {
  return strrchr(username_, '@') != NULL;
}

bool GoogleUsernamePassword::ValidForOfflineLogin() {
  if (store_ && store_->Contains(username_,
                                 store_->WeakHash(salt_.c_str(), password_))) {
    //  Also export credentials for other pam modules.
    store_->ExportCredentials(username_,
                              store_->WeakHash(system_salt_.c_str(),
                                               password_));
    return true;
  }
  return false;
}

void GoogleUsernamePassword::StoreCredentials() {
  if (store_) {
    // Export credentials for other pam modules. This uses the system salt.
    store_->ExportCredentials(username_,
                              store_->WeakHash(system_salt_.c_str(),
                                               password_));
    // Login credentials use a per-user salt.
    store_->Store(username_, salt_, store_->WeakHash(salt_.c_str(), password_));
  }
}

void GoogleUsernamePassword::GetWeakHash(char *hash_buffer, int length) {
  static const char kNul = '\0';
  static const char *kNoStore = "nostore";
  if (store_) {
    Blob hash = store_->WeakHash(salt_.c_str(), password_);
    // Perform a char-by-char copy.
    int copied = 0;
    Blob::const_iterator entry = hash.begin();
    char *buf_entry = hash_buffer;
    for ( ; entry != hash.end() && copied < length; ++copied) {
      *buf_entry++ = *entry++;
    }
    // Ensure NUL termination.
    if (copied < length) {
      *buf_entry = kNul;
    } else {
      hash_buffer[length - 1] = kNul;
    }
  } else {
    // We assume that the caller has at least given us a buffer of length 1.
    hash_buffer[0] = kNul;
    if (length > 1)
      strncpy(hash_buffer, kNoStore, length);
    LOG(WARNING) << "No offline store. Unable to export a weak password hash";
  }
}

}  // namespace chromeos_pam
