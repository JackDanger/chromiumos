// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FUSE_USE_VERSION 26

#include <new>
#include <glog/logging.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "verity.h"

namespace chromeos {
namespace verity {
namespace fuse_bridge {

static void *verity_init(struct fuse_conn_info *conn) {
  Verity *v = new (std::nothrow) Verity();
  if (!v) {
    LOG(FATAL) << "failed to allocate a Verity instance";
    // TODO: do we need to worry about memory starvation at this point?
    exit(1);
  }

  if (!v->Initialize()) {
    LOG(FATAL) << "!!TODO!! failed to initialize verity subsystem.";
  }
  return reinterpret_cast<void *>(v);
}

static void verity_destroy(void *user_data) {
  Verity *v = reinterpret_cast<Verity *>(user_data);
  delete v;
}

#define wrap_fs_downcall(call) \
  int ret; \
  return ((ret = call) == -1 ? -errno : ret)
static int verity_access(const char *path, int mask) {
  wrap_fs_downcall(access(path, mask));
}

static int verity_chmod(const char *path, mode_t mode) {
  wrap_fs_downcall(chmod(path, mode));
}

static int verity_chown(const char *path, uid_t owner, gid_t group) {
  // We don't follow symlinks to ensure we stay where we think we are.
  // TODO: add lstat check to ensure a chown would be inside our mount dir
  wrap_fs_downcall(lchown(path, owner, group));
}

static int verity_create(const char *path,
                         mode_t mode,
                         struct fuse_file_info *fi) {
  int fd;
  if ((fd = open(path, fi->flags, mode)) == -1) {
    return -errno;
  }
  // Pack the fd into the file info for easy use elsewhere.
  fi->fh = fd;
  // I assume FUSE is doing the right thing so I just dump back 0.
  // TODO: test to see if FUSE expects the fd.
  return 0;
}

static int verity_fgetattr(const char *path,
                             struct stat *stbuf,
                             struct fuse_file_info *fi) {
  wrap_fs_downcall(fstat(fi->fh, stbuf));
}

static int verity_flush(const char *path, struct fuse_file_info *fi) {
  // As per fuse.h, this may be called multiple times so we must dup()
  // to avoid closing an in-use file descriptor.
  wrap_fs_downcall(close(dup(fi->fh)));
}

static int verity_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
  if (isdatasync) {
    wrap_fs_downcall(fdatasync(fi->fh));
  }
  wrap_fs_downcall(fsync(fi->fh));
}

static int verity_fsyncdir(const char *path, int isdatasync, struct fuse_file_info *fi) {
  // TODO: determine if this is called only after an opendir.
  DIR *dir = reinterpret_cast<DIR *>(fi->fh);
  if (!dir)
    return -EBADF;

  int fd = dirfd(dir);
  if (fd == -1)
    return -errno;

  if (isdatasync) {
    wrap_fs_downcall(fdatasync(fd));
  }
  wrap_fs_downcall(fsync(fd));
}

static int verity_ftruncate(const char *path, off_t length, struct fuse_file_info *fi) {
  wrap_fs_downcall(ftruncate(fi->fh, length));
}

static int verity_getattr(const char *path, struct stat *stbuf) {
  wrap_fs_downcall(lstat(path, stbuf));
}

static int verity_link(const char *oldpath, const char *newpath) {
  wrap_fs_downcall(link(oldpath, newpath));
}

static int verity_mkdir(const char *path, mode_t mode) {
  wrap_fs_downcall(mkdir(path, mode));
}

static int verity_mknod(const char *path, mode_t mode, dev_t dev) {
  // mkfifo and mknod share an entry point for fuse so we handle both.
  if (S_ISFIFO(mode)) {
    wrap_fs_downcall(mkfifo(path, mode));
  }
  wrap_fs_downcall(mknod(path, mode, dev));
}

static int verity_open(const char *path, struct fuse_file_info *fi) {
  int fd;
  if ((fd = open(path, fi->flags)) == -1) {
    return -errno;
  }

  // Pack the fd into the file info for easy use elsewhere.
  fi->fh = fd;
  return 0;
}

#define PAGE_SIZE 4096
#define PAGE_MASK (~(PAGE_SIZE-1))
#define PAGE_BASE(off) (off & PAGE_MASK)
#define PAGE_ALIGN(sz) ((sz + (PAGE_SIZE-1)) & PAGE_MASK)

static int verity_read(const char *path,
                       char *buf,
                       size_t size,
                       off_t offset,
                       struct fuse_file_info *fi) {
  struct fuse_context *ctx = fuse_get_context();
  Verity *verity = reinterpret_cast<Verity *>(ctx->private_data);
  return verity->Read(path, buf, size, offset, fi->fh);
}

static int verity_readlink(const char *path, char *buf, size_t len) {
  ssize_t ret;
  if (len < 1) {
    return -EINVAL;
  }
  if ((ret = readlink(path, buf, len - 1)) == -1) {
    return -errno;
  }
  // FUSE expects this to be NUL-terminated.
  buf[ret] = '\0';
  return ret;
}

static int verity_release(const char *path, struct fuse_file_info *fi) {
  wrap_fs_downcall(close(fi->fh));
}


static int verity_rename(const char *oldpath, const char *newpath) {
  wrap_fs_downcall(rename(oldpath, newpath));
}

static int verity_rmdir(const char *path) {
  wrap_fs_downcall(rmdir(path));
}

static int verity_statfs(const char *path, struct statvfs *stbuf) {
  wrap_fs_downcall(statvfs(path, stbuf));
}

static int verity_symlink(const char *oldpath, const char *newpath) {
  wrap_fs_downcall(symlink(oldpath, newpath));
}

static int verity_truncate(const char *path, off_t length) {
  wrap_fs_downcall(truncate(path, length));
}

static int verity_unlink(const char *path) {
  wrap_fs_downcall(unlink(path));
}

static int verity_utimens(const char *path, const struct timespec ts[2]) {
  // Too bad we don't have a valid utimens to call.
  struct timeval tv[2];
  tv[0].tv_sec = ts[0].tv_sec;
  tv[0].tv_usec = ts[0].tv_nsec / 1000;
  tv[1].tv_sec = ts[1].tv_sec;
  tv[1].tv_usec = ts[1].tv_nsec / 1000;
  wrap_fs_downcall(utimes(path, tv));
}

static int verity_write(const char *path,
                        const char *buf,
                        size_t size,
                        off_t offset,
                        struct fuse_file_info *fi) {
  wrap_fs_downcall(pwrite(fi->fh, buf, size, offset));
}

static int verity_opendir(const char *path, struct fuse_file_info *fi) {
  DIR *dir;
  if ((dir = opendir(path)) == NULL) {
    return -errno;
  }
  // Pack the DIR pointer into fh so that we can use it from
  // readdir and release it in releasedir.
  fi->fh = reinterpret_cast<unsigned long>(dir);
  return 0;
}

static int verity_readdir(const char *path,
                          void *buf,
                          fuse_fill_dir_t filler,
                          off_t offset,
                          struct fuse_file_info *fi) {
  DIR *dir = reinterpret_cast<DIR *>(fi->fh);
  if (!dir) {
    return -EBADF;
  }

  // Make sure we're at the right place.
  seekdir(dir, offset);

  // Populate the readdir response via the fuse filler helper.
  struct stat st;
  memset(&st, 0, sizeof(st));
  off_t noff;
  struct dirent *entry;
  do {
    if (!(entry = readdir(dir))) {
      break;
    }
    // Populate the inode and mode.
    st.st_ino = entry->d_ino;
    st.st_mode = entry->d_type << 12;
    // Make sure we have the correct next offset for the filler.
    noff = telldir(dir);
    // Tell filler what's up.
  } while (!filler(buf, entry->d_name, &st, noff));

  return 0;
}

static int verity_releasedir(const char *path, struct fuse_file_info *fi) {
  DIR *dir = reinterpret_cast<DIR *>(fi->fh);
  fi->fh = 0;
  wrap_fs_downcall(closedir(dir));
}

struct FuseOperations : fuse_operations {
  FuseOperations() {
    access     = verity_access;
    // bmap is only needed if blkdev is used.
    // bmap       = verity_bmap;
    chmod      = verity_chmod;
    chown      = verity_chown;
    create     = verity_create;
    flush      = verity_flush;
    fsync      = verity_fsync;
    fsyncdir   = verity_fsyncdir;
    fgetattr   = verity_fgetattr;
    ftruncate  = verity_ftruncate;
    getattr    = verity_getattr;
    link       = verity_link;
    // lock is needed if this will support a network-based fs.
    // Otherwise, the kernel can do its normal thing.
    //lock       = verity_lock;
    mkdir      = verity_mkdir;
    mknod      = verity_mknod;
    open       = verity_open;
    opendir    = verity_opendir;
    readlink   = verity_readlink;
    read       = verity_read;
    readdir    = verity_readdir;
    releasedir = verity_releasedir;
    statfs     = verity_statfs;
    symlink    = verity_symlink;
    release    = verity_release;
    rmdir      = verity_rmdir;
    rename     = verity_rename;
    truncate   = verity_truncate;
    unlink     = verity_unlink;
    utimens    = verity_utimens;
    write      = verity_write;

  #if 0  // TODO
    listxattr    = verity_listxattr;
    getxattr     = verity_getxattr;
    removexattr  = verity_removexattr;
    setxattr     = verity_setxattr;
  #endif

    init = verity_init;
    destroy  = verity_destroy;
  }
};

static const FuseOperations operations;

};  // namespace fuse_bridge
};  // namespace verity
};  // namesapce chromeos

// Export the expected fuse_operations struct
const struct fuse_operations *chromeos_verity_operations =
  reinterpret_cast<const fuse_operations *>(
    &chromeos::verity::fuse_bridge::operations);
