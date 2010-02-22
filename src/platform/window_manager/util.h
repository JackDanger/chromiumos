// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_UTIL_H_
#define WINDOW_MANAGER_UTIL_H_

#include <ctime>
#include <list>
#include <map>
#include <sys/time.h>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace window_manager {

// Stacker maintains an ordering of objects (e.g. windows) in which changes
// can be made in faster-than-linear time.
template<class T>
class Stacker {
 public:
  Stacker() {}

  // Get the (top-to-bottom) ordered list of items.
  const std::list<T>& items() const { return items_; }

  // Has a particular item been registered?
  bool Contains(T item) const {
    return (index_.find(item) != index_.end());
  }

  // Get an item's 0-based position in the stack, or -1 if it isn't
  // present.  Slow but useful for testing.
  int GetIndex(T item) const {
    int i = 0;
    for (typename std::list<T>::const_iterator it = items_.begin();
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
    typename std::list<T>::iterator list_it = map_it->second;
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
  void AddAbove(T item, T other_item) {
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
    typename std::list<T>::iterator new_it = items_.insert(other_it->second,
                                                           item);
    index_.insert(make_pair(item, new_it));
  }

  // Add 'item' below 'other_item'.  'other_item' must already exist on the
  // stack.
  void AddBelow(T item, T other_item) {
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
    typename std::list<T>::iterator new_it = other_it->second;
    typename std::list<T>::iterator it = items_.insert(++new_it, item);
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
  std::list<T> items_;

  typedef std::map<T, typename std::list<T>::iterator> IteratorMap;

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
V FindWithDefault(const std::map<K, V>& the_map, const K& key, const V& def) {
  typename std::map<K, V>::const_iterator it = the_map.find(key);
  if (it == the_map.end()) {
    return def;
  }
  return it->second;
}

template<class K, class V>
V FindWithDefault(const base::hash_map<K, V>& the_map,
                  const K& key,
                  const V& def) {
  typename base::hash_map<K, V>::const_iterator it = the_map.find(key);
  if (it == the_map.end()) {
    return def;
  }
  return it->second;
}


// Get the number of seconds since the epoch.
double GetCurrentTime();

// Fill 'tv' with the time from 'time'.
void FillTimeval(double time, struct timeval* tv);

// Helper method to convert an XID into a hex string.
std::string XidStr(unsigned long xid);

// Helper method to return the next highest power of two.
inline uint32 NextPowerOfTwo(uint32 x) {
  x--;
  x = x | (x >> 1);
  x = x | (x >> 2);
  x = x | (x >> 4);
  x = x | (x >> 8);
  x = x | (x >> 16);
  return x + 1;
}

}  // namespace window_manager

#endif  // WINDOW_MANAGER_UTIL_H_
