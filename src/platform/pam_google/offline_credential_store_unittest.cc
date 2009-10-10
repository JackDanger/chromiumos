// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for OfflineCredientialStore.

#include <errno.h>
#include "pam_google/offline_credential_store.h"
#include <gtest/gtest.h>

namespace chromeos_pam {

class OfflineCredentialStoreTest : public ::testing::Test { };

class ExportWrapperMock : public ExportWrapper {
 public:
  ExportWrapperMock() : ExportWrapper(reinterpret_cast<pam_handle_t*>(7)) {}
  virtual ~ExportWrapperMock() {}

  void PamPutenv(const char *name_value) {
  }
  void PamSetItem(int item_type, const void *item) {
  }
};

TEST(OfflineCredentialStoreTest, SimpleStoreRecallTest) {
  const string kPath = "/tmp/cred_store.txt";
  if (-1 == remove(kPath.c_str()))
    EXPECT_EQ(ENOENT, errno);
  OfflineCredentialStore store(kPath, new ExportWrapperMock);
  const char* ba5e_str = "ba5e";
  const Blob blob_hash(ba5e_str, ba5e_str + sizeof(ba5e_str));
  EXPECT_FALSE(store.Contains("foo", blob_hash));
  store.Store("foo", blob_hash);
  EXPECT_TRUE(store.Contains("foo", blob_hash));
}

TEST(OfflineCredentialStoreTest, FileRecallTest) {
  const string kPath = "/tmp/cred_store.txt";
  if (-1 == remove(kPath.c_str()))
    EXPECT_EQ(ENOENT, errno);
  const char* ba5e_str = "ba5e";
  const Blob blob_hash(ba5e_str, ba5e_str + sizeof(ba5e_str));
  Blob blob_hash2(blob_hash);
  blob_hash2[0] = 'c';
  {
    OfflineCredentialStore store(kPath, new ExportWrapperMock);
    store.Store("foo", blob_hash);
    store.Store("bar", blob_hash);
    store.Store("foo", blob_hash2);
    EXPECT_TRUE(store.Contains("foo", blob_hash2));
    EXPECT_TRUE(store.Contains("bar", blob_hash));
    EXPECT_FALSE(store.Contains("foo", blob_hash));
    EXPECT_FALSE(store.Contains("bar", blob_hash2));
  }
  {
    OfflineCredentialStore store(kPath, new ExportWrapperMock);
    EXPECT_TRUE(store.Contains("foo", blob_hash2));
    EXPECT_TRUE(store.Contains("bar", blob_hash));
    EXPECT_FALSE(store.Contains("foo", blob_hash));
    EXPECT_FALSE(store.Contains("bar", blob_hash2));
  }
}

TEST(OfflineCredentialStoreTest, AsciiEncodeTest) {
  char val = 127;  // 0x7f
  Blob blob(&val, &val + 1);
  EXPECT_EQ(OfflineCredentialStore::AsciiEncode(blob), "7f");
}

TEST(OfflineCredentialStoreTest, WeakHashTest) {
  EXPECT_TRUE(
      OfflineCredentialStore::AsciiDecode("f3ba5baccac0510cb9e078253900d36b") ==
      OfflineCredentialStore::WeakHash("ba5e"));
  EXPECT_TRUE(
      OfflineCredentialStore::AsciiDecode("37e683e0e54d1476a3db5053d3800826") ==
      OfflineCredentialStore::WeakHash("adlr"));
  EXPECT_FALSE(
      OfflineCredentialStore::AsciiDecode("36170fad19721070609f5f1b8cfd37a7") ==
      OfflineCredentialStore::WeakHash("adlx"));
}

}  // namespace chromeos_pam
