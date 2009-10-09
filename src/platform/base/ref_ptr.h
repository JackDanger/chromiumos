// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __PLATFORM_BASE_REF_PTR_H__
#define __PLATFORM_BASE_REF_PTR_H__

#include <glog/logging.h>

// A reference-counted pointer class.
template<class T>
class ref_ptr {
 public:
  explicit ref_ptr(T* ptr=NULL)
      : ptr_(ptr),
        refs_(ptr ? new int(0) : NULL) {
    add_ref();
  }
  ref_ptr(const ref_ptr<T>& o)
      : ptr_(o.ptr_),
        refs_(o.refs_) {
    add_ref();
  }
  ~ref_ptr() {
    del_ref();
    ptr_ = NULL;
    refs_ = NULL;
  }
  ref_ptr<T>& operator=(const ref_ptr<T>& o) {
    if (ptr_ != o.ptr_) {
      del_ref();
      ptr_ = o.ptr_;
      refs_ = o.refs_;
      add_ref();
    }
    return *this;
  }

  T* get() const {
    return ptr_;
  }
  T &operator*() const {
    return *ptr_;
  }
  T *operator->() const {
    return ptr_;
  }
  void reset(T* ptr=NULL) {
    del_ref();
    ptr_ = ptr;
    refs_ = ptr ? new int(0) : NULL;
    add_ref();
  }
  void swap(ref_ptr<T>& other) {
    T* tmp_ptr = ptr_;
    ptr_ = other.ptr_;
    other.ptr_ = tmp_ptr;

    int* tmp_refs = refs_;
    refs_ = other.refs_;
    other.refs_ = tmp_refs;
  }

  // Release and return the pointer.
  // This must be the only reference to it.
  T* release() {
    if (refs_ != NULL) {
      CHECK_EQ(*refs_, 1);
      delete refs_;
      refs_ = NULL;
    }
    T* ptr = ptr_;
    ptr_ = NULL;
    return ptr;
  }

  bool operator==(const ref_ptr<T>& other) const {
    return ptr_ == other.ptr_;
  }
  bool operator==(const T* ptr) const {
    return ptr_ == ptr;
  }

 private:
  inline void add_ref() {
    if (refs_ != NULL) {
      (*refs_)++;
    }
  }
  inline void del_ref() {
    if (refs_ != NULL) {
      (*refs_)--;
      if (*refs_ == 0) {
        delete ptr_;
        delete refs_;
      }
    }
  }

  T* ptr_;
  int* refs_;
};

#endif  // __PLATFORM_BASE_REF_PTR_H__
