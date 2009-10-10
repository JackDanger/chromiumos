// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __PLATFORM_WINDOW_MANAGER_UTIL_H__
#define __PLATFORM_WINDOW_MANAGER_UTIL_H__

#include <ctime>
#include <list>
#include <map>
#include <sys/time.h>

#include <glog/logging.h>

#include "base/basictypes.h"

using namespace std;

typedef unsigned int uint;

namespace chromeos {

// Stacker maintains an ordering of objects (e.g. windows) in which changes
// can be made in faster-than-linear time.
template<class T>
class Stacker {
 public:
  Stacker() {}

  // Get the (top-to-bottom) ordered list of items.
  const list<T>& items() const { return items_; }

  // Has a particular item been registered?
  bool Contains(T item) const {
    return (index_.find(item) != index_.end());
  }

  // Get an item's 0-based position in the stack, or -1 if it isn't
  // present.  Slow but useful for testing.
  int GetIndex(T item) const {
    int i = 0;
    for (typename list<T>::const_iterator it = items_.begin();
         it != items_.end(); ++it, ++i) {
      if (*it == item)
        return i;
    }
    return -1;
  }

  // Get the item under 'item' on the stack, or NULL if 'item' is on the
  // bottom of the stack.
  const T* GetUnder(T item) const {
    typename IteratorMap::const_iterator map_it = index_.find(item);
    if (map_it == index_.end()) {
      LOG(WARNING) << "Got request for item under not-present item " << item;
      return NULL;
    }
    typename list<T>::iterator list_it = map_it->second;
    list_it++;
    if (list_it == items_.end()) {
      return NULL;
    }
    return &(*list_it);
  }

  // Add an item on the top of the stack.
  void AddOnTop(T item) {
    if (Contains(item)) {
      LOG(WARNING) << "Ignoring request to add already-present item "
                   << item << " on top";
      return;
    }
    items_.push_front(item);
    index_.insert(make_pair(item, items_.begin()));
  }

  // Add an item on the bottom of the stack.
  void AddOnBottom(T item) {
    if (Contains(item)) {
      LOG(WARNING) << "Ignoring request to add already-present item "
                   << item << " on bottom";
      return;
    }
    items_.push_back(item);
    index_.insert(make_pair(item, --(items_.end())));
  }

  // Add 'item' above 'other_item'.  'other_item' must already exist on the
  // stack.
  void AddAbove(T other_item, T item) {
    if (Contains(item)) {
      LOG(WARNING) << "Ignoring request to add already-present item "
                   << item << " above item " << other_item;
      return;
    }
    typename IteratorMap::iterator other_it = index_.find(other_item);
    if (other_it == index_.end()) {
      LOG(WARNING) << "Ignoring request to add item " << item
                   << " above not-present item " << other_item;
      return;
    }
    typename list<T>::iterator new_it = items_.insert(other_it->second, item);
    index_.insert(make_pair(item, new_it));
  }

  // Add 'item' below 'other_item'.  'other_item' must already exist on the
  // stack.
  void AddBelow(T other_item, T item) {
    if (Contains(item)) {
      LOG(WARNING) << "Ignoring request to add already-present item "
                   << item << " below item " << other_item;
      return;
    }
    typename IteratorMap::iterator other_it = index_.find(other_item);
    if (other_it == index_.end()) {
      LOG(WARNING) << "Ignoring request to add item " << item
                   << " below not-present item " << other_item;
      return;
    }
    // Lists don't support operator+ or operator-, so we need to use ++.
    // Make a copy of the iterator before doing this so that we don't screw
    // up the previous value in the map.
    typename list<T>::iterator new_it = other_it->second;
    typename list<T>::iterator it = items_.insert(++new_it, item);
    index_.insert(make_pair(item, it));
  }

  // Remove an item from the stack.
  void Remove(T item) {
    typename IteratorMap::iterator it = index_.find(item);
    if (it == index_.end()) {
      LOG(WARNING) << "Ignoring request to remove not-present item " << item;
      return;
    }
    items_.erase(it->second);
    index_.erase(it);
  }

 private:
  // Items stacked from top to bottom.
  list<T> items_;

  typedef map<T, typename list<T>::iterator> IteratorMap;

  // Index into 'items_'.
  IteratorMap index_;

  DISALLOW_COPY_AND_ASSIGN(Stacker);
};


// ByteMap unions rectangles into a 2-D array of bytes.  That's it. :-P
class ByteMap {
 public:
  ByteMap(int width, int height);
  ~ByteMap();

  int width() const { return width_; }
  int height() const { return height_; }
  const unsigned char* bytes() const { return bytes_; }

  // Copy the bytes from 'other', which must have the same dimensions as
  // this map.
  void Copy(const ByteMap& other);

  // Set every byte to 'value'.
  void Clear(unsigned char value);

  // Set the bytes covered by the passed-in rectangle.
  void SetRectangle(int rect_x, int rect_y,
                    int rect_width, int rect_height,
                    unsigned char value);

 private:
  int width_;
  int height_;
  unsigned char* bytes_;

  DISALLOW_COPY_AND_ASSIGN(ByteMap);
};


template<class K, class V>
V FindWithDefault(const map<K, V>& the_map, const K& key, const V& def) {
  typename map<K, V>::const_iterator it = the_map.find(key);
  if (it == the_map.end()) {
    return def;
  }
  return it->second;
}


// Get the number of seconds since the epoch.
double GetCurrentTime();


// Fill 'tv' with the time from 'time'.
void FillTimeval(double time, struct timeval* tv);

}  // namespace chromeos

#endif  // __UTIL_H__
