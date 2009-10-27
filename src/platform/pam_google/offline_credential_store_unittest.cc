// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for OfflineCredientialStore.

#include <errno.h>
#include <gtest/gtest.h>
#include "pam_google/offline_credential_store.h"

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
  store.Store("foo", "salt", blob_hash);
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
    store.Store("foo", "salt1", blob_hash);
    store.Store("bar", "salt2", blob_hash);
    store.Store("foo", "salt1", blob_hash2);
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

TEST(OfflineCredentialStoreTest, AsciiDecodeTest) {
  char val = 127;  // 0x7f
  Blob blob(&val, &val + 1);
  EXPECT_TRUE(OfflineCredentialStore::AsciiDecode("7f") == blob);
}

TEST(OfflineCredentialStoreTest, GetRandomTest) {
  const size_t kBufLen = 16;
  char *random_buf1 = new char[kBufLen+1];
  char *random_buf2 = new char[kBufLen+1];
  EXPECT_TRUE(OfflineCredentialStore::GetRandom(random_buf1, kBufLen));
  EXPECT_TRUE(OfflineCredentialStore::GetRandom(random_buf2, kBufLen));
  EXPECT_FALSE(
      OfflineCredentialStore::AsciiEncode(Blob(random_buf1,
                                               random_buf1+kBufLen)) ==
      OfflineCredentialStore::AsciiEncode(Blob(random_buf2,
                                               random_buf2+kBufLen)));
}

TEST(OfflineCredentialStoreTest, GenerateSaltTest) {
  int length = 16;
  string salt1 = OfflineCredentialStore::GenerateSalt(length);
  string salt2 = OfflineCredentialStore::GenerateSalt(length);
  EXPECT_EQ(salt1.size(), 2*length);
  EXPECT_EQ(salt2.size(), 2*length);
  EXPECT_FALSE(salt1 == salt2);
}

TEST(OfflineCredentialStoreTest, WeakHashTest) {
  EXPECT_TRUE(
      OfflineCredentialStore::AsciiDecode("f3ba5baccac0510cb9e078253900d36b") ==
      OfflineCredentialStore::WeakHash("", "ba5e"));
  EXPECT_TRUE(
      OfflineCredentialStore::AsciiDecode("37e683e0e54d1476a3db5053d3800826") ==
      OfflineCredentialStore::WeakHash("", "adlr"));
  EXPECT_FALSE(
      OfflineCredentialStore::AsciiDecode("36170fad19721070609f5f1b8cfd37a7") ==
      OfflineCredentialStore::WeakHash("", "adlx"));
  EXPECT_TRUE(
      OfflineCredentialStore::AsciiDecode("cf0bb2e6b85bc8b29012a7faa6c28b86") ==
      OfflineCredentialStore::WeakHash("fakesalt", "ba5e"));
  EXPECT_TRUE(
      OfflineCredentialStore::AsciiDecode("4748eb68f4f859df6c8de7d929f90b33") ==
      OfflineCredentialStore::WeakHash("fakesalt", "adlr"));
  EXPECT_FALSE(
      OfflineCredentialStore::AsciiDecode("6170fad19721070609f5f1b8cfd317a7") ==
      OfflineCredentialStore::WeakHash("fakesalt", "adlx"));
}

TEST(OfflineCredentialStoreTest, GetSaltTest) {
  const string kPath = "/tmp/cred_store.txt";
  const char *name1 = "fakeuser1";
  const char *name2 = "fakeuser2";
  string salt1 = "fakesalt1";
  string salt2 = "";

  OfflineCredentialStore store(kPath, new ExportWrapperMock);
  store.Store(name1, salt1, OfflineCredentialStore::WeakHash(salt1.c_str(),
                                                             name1));
  salt2 = store.GetSalt(name2);
  store.Store(name2, salt2, OfflineCredentialStore::WeakHash(salt2.c_str(),
                                                             name2));
  EXPECT_TRUE(salt1 == store.GetSalt(name1));
  EXPECT_TRUE(salt2 == store.GetSalt(name2));
}

}  // namespace chromeos_pam
