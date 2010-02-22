// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/atom_cache.h"

#include "base/logging.h"
#include "chromeos/obsolete_logging.h"

#include "window_manager/util.h"
#include "window_manager/x_connection.h"

namespace window_manager {

// A value from the Atom enum and the actual name that should be used to
// look up its ID on the X server.
struct AtomInfo {
  Atom atom;
  const char* name;
};

// Each value from the Atom enum must be present here.
static const AtomInfo kAtomInfos[] = {
  { ATOM_ATOM,                         "ATOM" },
  { ATOM_CHROME_GET_SERVER_TIME,       "_CHROME_GET_SERVER_TIME" },
  { ATOM_CHROME_STATE,                 "_CHROME_STATE" },
  { ATOM_CHROME_STATE_COLLAPSED_PANEL, "_CHROME_STATE_COLLAPSED_PANEL" },
  { ATOM_CHROME_WINDOW_TYPE,           "_CHROME_WINDOW_TYPE" },
  { ATOM_CHROME_WM_MESSAGE,            "_CHROME_WM_MESSAGE" },
  { ATOM_MANAGER,                      "MANAGER" },
  { ATOM_NET_ACTIVE_WINDOW,            "_NET_ACTIVE_WINDOW" },
  { ATOM_NET_CLIENT_LIST,              "_NET_CLIENT_LIST" },
  { ATOM_NET_CLIENT_LIST_STACKING,     "_NET_CLIENT_LIST_STACKING" },
  { ATOM_NET_CURRENT_DESKTOP,          "_NET_CURRENT_DESKTOP" },
  { ATOM_NET_DESKTOP_GEOMETRY,         "_NET_DESKTOP_GEOMETRY" },
  { ATOM_NET_DESKTOP_VIEWPORT,         "_NET_DESKTOP_VIEWPORT" },
  { ATOM_NET_NUMBER_OF_DESKTOPS,       "_NET_NUMBER_OF_DESKTOPS" },
  { ATOM_NET_SUPPORTED,                "_NET_SUPPORTED" },
  { ATOM_NET_SUPPORTING_WM_CHECK,      "_NET_SUPPORTING_WM_CHECK" },
  { ATOM_NET_WM_CM_S0,                 "_NET_WM_CM_S0" },
  { ATOM_NET_WM_NAME,                  "_NET_WM_NAME" },
  { ATOM_NET_WM_STATE,                 "_NET_WM_STATE" },
  { ATOM_NET_WM_STATE_FULLSCREEN,      "_NET_WM_STATE_FULLSCREEN" },
  { ATOM_NET_WM_STATE_MAXIMIZED_HORZ,  "_NET_WM_STATE_MAXIMIZED_HORZ" },
  { ATOM_NET_WM_STATE_MAXIMIZED_VERT,  "_NET_WM_STATE_MAXIMIZED_VERT" },
  { ATOM_NET_WM_STATE_MODAL,           "_NET_WM_STATE_MODAL" },
  { ATOM_NET_WM_WINDOW_OPACITY,        "_NET_WM_WINDOW_OPACITY" },
  { ATOM_NET_WORKAREA,                 "_NET_WORKAREA" },
  { ATOM_PRIMARY,                      "PRIMARY" },
  { ATOM_WM_DELETE_WINDOW,             "WM_DELETE_WINDOW" },
  { ATOM_WM_HINTS,                     "WM_HINTS" },
  { ATOM_WM_NORMAL_HINTS,              "WM_NORMAL_HINTS" },
  { ATOM_WM_PROTOCOLS,                 "WM_PROTOCOLS" },
  { ATOM_WM_S0,                        "WM_S0" },
  { ATOM_WM_STATE,                     "WM_STATE" },
  { ATOM_WM_SYSTEM_METRICS,            "WM_SYSTEM_METRICS" },
  { ATOM_WM_TAKE_FOCUS,                "WM_TAKE_FOCUS" },
  { ATOM_WM_TRANSIENT_FOR,             "WM_TRANSIENT_FOR" },
};

AtomCache::AtomCache(XConnection* xconn)
    : xconn_(xconn) {
  CHECK(xconn_);

  CHECK_EQ(sizeof(kAtomInfos) / sizeof(AtomInfo), kNumAtoms)
      << "Each value in the Atom enum in atom_cache.h must have "
      << "a mapping in kAtomInfos in atom_cache.cc";
  std::vector<std::string> names;
  std::vector<XAtom> xatoms;

  for (int i = 0; i < kNumAtoms; ++i) {
    names.push_back(kAtomInfos[i].name);
  }

  CHECK(xconn_->GetAtoms(names, &xatoms));
  CHECK_EQ(xatoms.size(), kNumAtoms);

  for (size_t i = 0; i < kNumAtoms; ++i) {
    VLOG(2) << "Registering atom " << XidStr(xatoms[i])
            << " (" << kAtomInfos[i].name << ")";
    atom_to_xatom_[kAtomInfos[i].atom] = xatoms[i];
    xatom_to_string_[xatoms[i]] = kAtomInfos[i].name;
  }
}

XAtom AtomCache::GetXAtom(Atom atom) const {
  std::map<Atom, XAtom>::const_iterator it = atom_to_xatom_.find(atom);
  CHECK(it != atom_to_xatom_.end())
      << "Couldn't find X atom for Atom " << XidStr(atom);
  return it->second;
}

const std::string& AtomCache::GetName(XAtom xatom) {
  std::map<XAtom, std::string>::const_iterator
      it = xatom_to_string_.find(xatom);
  if (it != xatom_to_string_.end()) {
    return it->second;
  }
  std::string name;
  if (!xconn_->GetAtomName(xatom, &name)) {
    LOG(ERROR) << "Unable to look up name for atom " << XidStr(xatom);
    static const std::string kEmptyName = "";
    return kEmptyName;
  }
  return xatom_to_string_.insert(make_pair(xatom, name)).first->second;
}

}  // namespace window_manager
