// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __PLATFORM_WINDOW_MANAGER_ATOM_CACHE_H__
#define __PLATFORM_WINDOW_MANAGER_ATOM_CACHE_H__

#include <map>
#include <string>

extern "C" {
#include <X11/Xlib.h>
}

#include "base/basictypes.h"

using namespace std;

namespace chromeos {

class XConnection;  // from x_connection.h

typedef ::Atom XAtom;

// Atom names with "_" prefixes (if any) stripped.
//
// When adding a new value, also insert a mapping to its actual name in
// kAtomInfos in atom_cache.cc.
enum Atom {
  ATOM_CHROME_GET_SERVER_TIME = 0,
  ATOM_CHROME_WINDOW_TYPE,
  ATOM_CHROME_WM_MESSAGE,
  ATOM_MANAGER,
  ATOM_NET_ACTIVE_WINDOW,
  ATOM_NET_CLIENT_LIST,
  ATOM_NET_CLIENT_LIST_STACKING,
  ATOM_NET_CURRENT_DESKTOP,
  ATOM_NET_DESKTOP_GEOMETRY,
  ATOM_NET_DESKTOP_VIEWPORT,
  ATOM_NET_NUMBER_OF_DESKTOPS,
  ATOM_NET_SUPPORTED,
  ATOM_NET_SUPPORTING_WM_CHECK,
  ATOM_NET_WM_CM_S0,
  ATOM_NET_WM_NAME,
  ATOM_NET_WM_STATE,
  ATOM_NET_WM_STATE_FULLSCREEN,
  ATOM_NET_WM_STATE_MODAL,
  ATOM_NET_WM_WINDOW_OPACITY,
  ATOM_NET_WORKAREA,
  ATOM_PRIMARY,
  ATOM_WM_DELETE_WINDOW,
  ATOM_WM_NORMAL_HINTS,
  ATOM_WM_PROTOCOLS,
  ATOM_WM_S0,
  ATOM_WM_STATE,
  ATOM_WM_SYSTEM_METRICS,
  ATOM_WM_TAKE_FOCUS,
  ATOM_WM_TRANSIENT_FOR,
  kNumAtoms,
};

// A simple class for looking up X atoms.  Using XInternAtom() to find the
// X atom for a given string requires a round trip to the X server; we
// avoid that by keeping a static map here.  To add some compile-time
// safety against typos in atom strings, values from the above Atom enum
// (rather than strings) are used to look up the X server's IDs for atoms.
// All atoms are fetched from the server just once, in the constructor.
class AtomCache {
 public:
  explicit AtomCache(XConnection* xconn);

  // Get the X server's ID for a value in our Atom enum.
  XAtom GetXAtom(Atom atom) const;

  // Debugging method to get the string value of an atom ID returned from
  // the X server.  Looks up the atom using XGetAtomName() if it's not
  // already present in the cache.  Only pass atoms that were received from
  // the X server (empty strings will be returned for invalid atoms).
  const string& GetName(XAtom xatom);

 private:
  XConnection* xconn_;  // not owned

  // Maps from our Atom enum to the X server's atom IDs and from the
  // server's IDs to atoms' string names.  These maps aren't necessarily in
  // sync; 'atom_to_xatom_' is constant after the constructor finishes but
  // GetName() caches additional string mappings in 'xatom_to_string_'.
  map<Atom, XAtom> atom_to_xatom_;
  map<XAtom, string> xatom_to_string_;

  DISALLOW_COPY_AND_ASSIGN(AtomCache);
};

}  // namespace chromeos

#endif
