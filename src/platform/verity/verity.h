// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __CHROMEOS_VERITY_VERITY_H
#define __CHROMEOS_VERITY_VERITY_H


#include <gflags/gflags.h>
#include <glog/logging.h>
#include <openssl/evp.h>
#include <search.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "base/scoped_ptr.h"
#include "digester.h"
#include "openssl_digester.h"

DECLARE_bool(verity_learning_mode);

namespace chromeos {

using verity::Digester;
using verity::OpenSSLDigester;
// Verity
// Encapsulates the verifying implementation used by
// the fuse_bridge to create an integrity verifying
// filesystem.
// Manifest file format:
//  number_of_files\n
//  /path/to/file file_size digest1 digest2 ...
//  ...
class Verity {
 public:
  static const char *kManifestFile;
  static const char *kManifestDigest;
  static const char *kDigestAlgorithm;
  static const int kHexDigestLength;
  static const int kBlockSize;
  static const unsigned int kMaxTableSize;

  Verity() : initialized_(false),
             learning_(FLAGS_verity_learning_mode),
             manifest_map_(NULL),
             manifest_length_(0),
             manifest_start_(NULL),
             manifest_end_(NULL) {
    digester_.reset(new OpenSSLDigester(kDigestAlgorithm));
    memset(&table_, '\0', sizeof(table_));
  }

  ~Verity() {
    if (manifest_map_) {
      munmap(manifest_map_, manifest_length_ + 1);
    }
  }


  bool Initialize();
  // TODO: abstract this away from the read semantics and provide a
  // iterator-like interface for verifying.
  int Read(const char *path, char *buf, size_t size, off_t offset, int fd);

  // TODO: Open() that enforces file ownership and permissions possibly just
  //       by including a digest of a slightly modified stat struct.

  // Takes ownership of the digester pointer.
  void set_digester(Digester *d) { digester_.reset(d); }

 private:
  inline off_t block_size() { return 4096; }  // matches kBlockSize
  inline off_t block_mask() { return ~(block_size()-1); }
  inline off_t block_base(off_t offset) { return offset & block_mask(); }
  inline off_t block_align(off_t size) {
    return size + ((block_size()-1) & block_mask());
  }

  bool MapManifest(const char *manifest);
  bool ExtractEntries();
  bool PopulateHash();

  bool initialized_;
  bool learning_;
  void *manifest_map_;
  off_t manifest_length_;
  char *manifest_start_;
  char *manifest_end_;

  struct hsearch_data table_;
  size_t table_size_;
  scoped_ptr<Digester> digester_;
};

}  // namespace chromeos

#endif  // __CHROMEOS_VERITY_VERITY_H
