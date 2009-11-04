// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to store credentials on disk and retrieve them for offline login.

#ifndef CHROMEOS_PAM_OFFLINE_CREDENTIAL_STORE_H_
#define CHROMEOS_PAM_OFFLINE_CREDENTIAL_STORE_H_

#include <gtest/gtest.h>
#include <security/pam_ext.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

namespace chromeos_pam {

extern const std::string kDefaultOfflineCredentialStorePath;
extern const int kSaltLength;

using std::map;
using std::string;
using std::vector;

class ExportWrapper;
typedef vector<unsigned char> Blob;

class OfflineCredentialStore {
 public:
  explicit OfflineCredentialStore(ExportWrapper *wrapper)
      : wrapper_(wrapper),
        path_(kDefaultOfflineCredentialStorePath),
        credentials_loaded_(false) {}
  OfflineCredentialStore(const string& path, ExportWrapper *wrapper)
      : wrapper_(wrapper),
        path_(path),
        credentials_loaded_(false) {}
  virtual ~OfflineCredentialStore() {}
  // Exports |name| and |hash| to the environment so that they can be used by
  // other pam modules and components in the system.
  virtual void ExportCredentials(const string& name, const Blob& hash);
  // Stores a mapping between |name|, |salt| and |hash| in our offline
  // credential store.
  virtual void Store(const string& name, const string& salt, const Blob& hash);
  // True if |name|:|hash| is present in the offline store.  False otherwise.
  virtual bool Contains(const string& name, const Blob& hash);
  // Returns a given user's salt or a newly generated salt if none exists.
  virtual string GetSalt(const string& name);
  // Returns the salt from the system-wide shadow salt file.
  virtual string GetSystemSalt();

  // Returns an ASCII hexadecimal string with a newly generated salt of
  // specified length.
  static string GenerateSalt(unsigned int length);
  // Returns the ASCII hexadecimal representation of a binary blob.
  static string AsciiEncode(const Blob& blob);
  // Returns the binary blob represented by an ASCII hexadecimal string.
  static Blob AsciiDecode(const string& str);
  // Returns a weak hash of the salt combined with the password.
  static Blob WeakHash(const char* const salt, const char* const password);

 protected:
  // For testing.
  OfflineCredentialStore()
      :wrapper_(NULL), path_(""), credentials_loaded_(false) {}
  void SetExportWrapper(ExportWrapper *wrapper) { wrapper_.reset(wrapper); }

 private:
  bool LoadCredentials();  // returns true on success
  // Reads |size| random data into |buf| and returns true on success.
  static bool GetRandom(char *buf, size_t size);

  scoped_ptr<ExportWrapper> wrapper_;
  const std::string& path_;
  bool credentials_loaded_;
  // Stores username to hash, salt mappings.
  std::map<std::string, std::pair<Blob, std::string> > credentials_;

  FRIEND_TEST(OfflineCredentialStoreTest, LoadCredentialsMalformedTest);
  FRIEND_TEST(OfflineCredentialStoreTest, GetRandomTest);
  DISALLOW_COPY_AND_ASSIGN(OfflineCredentialStore);
};

class ExportWrapper {
 public:
  explicit ExportWrapper(pam_handle_t *pamh) : pamh_(pamh) {}
  virtual ~ExportWrapper() {}

  virtual void PamPutenv(const char *name_value) {
    pam_putenv(pamh_, name_value);
  }
  virtual void PamSetItem(int item_type, const void *item) {
    pam_set_item(pamh_, item_type, item);
  }
 private:
  pam_handle_t *pamh_;

  DISALLOW_COPY_AND_ASSIGN(ExportWrapper);
};

};  // namespace chromeos_pam

#endif  // CHROMEOS_PAM_OFFLINE_CREDENTIAL_STORE_H_
