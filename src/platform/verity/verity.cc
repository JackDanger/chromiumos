// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <openssl/evp.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "base/scoped_ptr.h"
#include "digester.h"
#include "openssl_digester.h"
#include "verity.h"

DEFINE_string(verity_manifest,
              "/verity.manifest",
              "Path to the verity manifest file");

DEFINE_string(verity_manifest_digest,
              "64a17769525546b19ea554ff27848544d621cdda",
              "SHA-1 digest of the manifest file");

DEFINE_bool(verity_learning_mode,
           false,
           "Emit digests instead of validating digests");

namespace chromeos {

bool Verity::Initialize() {
  digester_->Initialize();
  // If we're in learning mode, we just output:
  //   /path/to/file digest block_number
  // and don't bother parsing a manifest.
  if (learning_) {
    LOG(WARNING) << "!!! LEARNING MODE ENABLED !!!";
    table_size_ = kMaxTableSize;
    hcreate_r(table_size_, &table_);
    initialized_ = true;
    return true;
  } else {
    // Memory map the manifest and grab the size
    if (!MapManifest(FLAGS_verity_manifest.c_str())) {
      LOG(ERROR) << "Failed to calculate the manifest file size";
      return false;
    }
    // Compute a digest of the manifest now and check it.
    // TODO: interleave parsing with verification if this is too slow.
    if (!digester_->Check(static_cast<char *>(manifest_map_),
                          manifest_length_,
                          FLAGS_verity_manifest_digest.c_str())) {
      LOG(ERROR) << "Manifest digest does not match the hard-coded digest";
      return false;
    }
    // Now we can continue and hope that the integrity checked file is
    // pretty well-formed . . .
    if (!ExtractEntries()) {
      LOG(ERROR) << "Failed to get the entry count";
      return false;
    }

    if (!hcreate_r(table_size_, &table_)) {
      LOG(ERROR) << "Failed to allocate hash table";
      return false;
    }

    // Now we can crawl the list and populate the hash
    if (!PopulateHash()) {
      LOG(ERROR) << "Failed to populate the hash table";
      return false;
    }
  }
  initialized_ = true;
  return true;
}

// TODO: abstract this away from the read semantics and provide a
// iterator-like interface for verifying.
int Verity::Read(const char *path,
                 char *buf,
                 size_t size,
                 off_t offset,
                 int fd) {
  char localbuf[kBlockSize];
  int res;
  ssize_t read_data = 0;
  off_t real_offset = block_base(offset);
  ssize_t real_size = block_align(size);
  size_t found_data_len = 0;
  static char kEmpty[] = "";
  ENTRY e, *found;
  e.key = const_cast<char *>(path);

  if (!learning_) {
    // Determine if the file needs to be verified.
    hsearch_r(e, FIND, &found, &table_);
    if (!found) {
      if ((res = pread(fd, buf, size, offset)) == -1) {
        return -errno;
      }
      return res;
    }
    DLOG(INFO) << "file is in the hash table: " << path;
  } else {
    // Populate a dummy found entry for use during learning mode.
    found = &e;
    e.data = static_cast<void *>(kEmpty);
  }
  // TODO: add support for file size, permissions, and ownership.
  //       Perms and ownership checks should go into an open() wrapper.
  // Until we have a file size, we just determine how many blocks are
  // available by using strlen.
  found_data_len = strlen(static_cast<char *>(found->data));
  if (!found_data_len ||
      (found_data_len / (kHexDigestLength + 1) + 1 <
       block_base(size+offset) / block_size())) {
    if (!learning_) {
      DLOG(INFO) << path << ": unknown block range requested: ["
                 << block_base(offset)/block_size() << "-"
                 << block_base(size+offset)/block_size() << ")";
      // TODO: do something clever here for bad digests.
      return -EIO;
    }
  }
  // Now we read starting at a block_size aligned location in block_size
  // blocks until we've satisfied the request.
  for (read_data = 0; read_data < real_size; ) {
    size_t to_copy;
    off_t block = ((block_base(real_offset + read_data)) / block_size());
    char *expected_digest = static_cast<char *>(found->data) +
                            (block * (kHexDigestLength + 1));
    res = pread(fd, localbuf, block_size(), real_offset + read_data);
    if (res == -1)
      return -errno;

    // Check if the block has changed or if we should emit a learning-mode
    // digest.
    if (!learning_ && !digester_->Check(localbuf, res, expected_digest)) {
      LOG(ERROR) << path << " has been tampered with.";
      LOG(INFO) << "[" << path << ":" << block << "] != " << expected_digest;
      // TODO: do something clever here for bad digests.
      return -EIO;
    } else if (learning_) {
      char digest[kHexDigestLength + 1];
      if (!digester_->Compute(localbuf, res, digest, sizeof(digest))) {
        LOG(ERROR) << path << ": Failed to compute the digest for block "
                   << block;
        return -EIO;
      }
      // Learning-mode output.
      LOG(INFO) << "[learning] " << path << "|" << block << "|" << digest;
    }
    DLOG(INFO) << "[" << path << ":" << block << "] ok";

    // Update our size counter.
    read_data += res;

    // Adjust our "view" to what is aligned with the user request.
    to_copy = res - (offset - real_offset);
    to_copy = (size >= to_copy ? to_copy : size);

    // Copy the data to the user.
    // TODO: Avoid using a local buffer whenever the target buffer can hold
    //       the next block_size bytes to opportunistically avoid extra copies.
    memcpy(buf, localbuf + (offset - real_offset), to_copy);
    buf += to_copy;
    size -= to_copy;
    // If we've read less than block_size, we're either out of data on a non-blocking
    // call (which we don't handle well necessarily) or we've hit the end of the file.
    if (res < block_size())
      break;
  }
  return read_data;
}

bool Verity::MapManifest(const char *manifest) {
  int fd = open(manifest, O_RDONLY);
  struct stat st;

  if (!fd) {
    LOG(ERROR) << "Could not open() " << manifest;
    return false;
  }
  if (fstat(fd, &st)) {
    LOG(ERROR) << "Failed to fstat() " << manifest;
    return false;
  }
  manifest_length_ = st.st_size;
  // Memory map the entire file with read-ahead.  We will make
  // private modifications so that we can use it as part of the
  // in-memory data structure.
  // TODO: MAP_LOCKED?
  manifest_map_ = mmap(NULL,
                       manifest_length_ + 1,
                       PROT_READ|PROT_WRITE,
                       MAP_PRIVATE|MAP_POPULATE,
                       fd,
                       0);
  if (!manifest_map_) {
    PLOG(ERROR) << "Failed to mmap the manifest file";
    return false;
  }
  // Populate an end pointer for easy checking.
  manifest_end_ = static_cast<char *>(manifest_map_) + manifest_length_;
  // We could just map a zero page afterward.
  *manifest_end_ = '\0';
  return true;
}

bool Verity::ExtractEntries() {
  char *manifest_data = static_cast<char *>(manifest_map_);
  char *newline = strchr(manifest_data, '\n');
  if (!newline || newline == manifest_data) {
    LOG(ERROR) << "Manifest is malformed. No newline found";
    return false;
  }
  *newline = '\0';
  // Now grab the entry count.
  if (sscanf(manifest_data, "%zu", &table_size_) != 1) {
    LOG(ERROR) << "Unable to extract number of entries.";
    return false;
  }
  if (table_size_ > kMaxTableSize) {
    LOG(ERROR) << "Manifest file requested too many entries: " << table_size_;
    return false;
  }
  // Start the manifest entries at the next line.
  manifest_start_ = newline + 1;
  return true;
}

bool Verity::PopulateHash() {
  char *cursor = manifest_start_;
  size_t count = 0;
  for ( ; cursor < manifest_end_ && count < table_size_; ++count) {
    ENTRY e, *ep;
    char *newline = strchr(cursor, '\n');
    char *space = strchr(cursor, ' ');
    if (!newline) {
      break;
    }
    if (!space) {
      space = newline;
    } else {
      *space = 0;
    }
    e.key = cursor;  // filename\0
    e.data = space + 1;  // digest1 digest2 ...
    *newline = 0;
    cursor = newline + 1;
    // TODO: move to a better format
    //       include file size
    if (hsearch_r(e, ENTER, &ep, &table_) == 0) {
      LOG(ERROR) << "Failed to insert entry into hash";
      return false;
    }
  }
  return true;
}

};  // namespace chromeos
