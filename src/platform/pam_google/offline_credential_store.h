// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to store credentials on disk and retrieve them for offline login.

#ifndef CHROMEOS_OFFLINE_CREDENTIAL_STORE_H_
#define CHROMEOS_OFFLINE_CREDENTIAL_STORE_H_

#include <map>
#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include <security/pam_ext.h>

namespace chromeos_pam {

extern const string kDefaultOfflineCredentialStorePath;

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
  // Stores a mapping between |name| and |hash| in our offline credential store.
  void Store(const string& name, const Blob& hash);
  // True if |name|:|hash| is present in the offline store.  False otherwise.
  bool Contains(const string& name, const Blob& hash);

  // returns a weak hash of password.
  static Blob WeakHash(const char* const password);
  static string AsciiEncode(const Blob& blob);
  static Blob AsciiDecode(const string& str);

 protected:
  // For testing.
  void SetExportWrapper(ExportWrapper *wrapper) { wrapper_.reset(wrapper); }

 private:
  bool LoadCredentials();  // returns true on success
  // Exports |name| and |hash| to the environment so that they can be used by
  // other pam modules and components in the system.
  void ExportCredentials(const string& name, const Blob& hash);

  scoped_ptr<ExportWrapper> wrapper_;
  const string& path_;
  bool credentials_loaded_;
  map<string, Blob> credentials_;
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

#endif  // CHROMEOS_OFFLINE_CREDENTIAL_STORE_H_
