// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/key_bindings.h"

#include <set>

extern "C" {
#include <X11/Xutil.h>
}
#include <gflags/gflags.h>

#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "chromeos/obsolete_logging.h"
#include "window_manager/x_connection.h"

namespace window_manager {

using chromeos::Closure;
using std::set;
using std::string;

KeyBindings::KeyCombo::KeyCombo(KeySym key_param, uint32 modifiers_param) {
  KeySym upper_keysym = None, lower_keysym = None;
  XConvertCase(key_param, &lower_keysym, &upper_keysym);
  key = lower_keysym;
  modifiers = (modifiers_param & ~LockMask);
}

bool KeyBindings::KeyComboComparator::operator()(const KeyCombo& a,
                                                 const KeyCombo& b) const {
  return (a.key < b.key) ||
      ((a.key == b.key) && (a.modifiers < b.modifiers));
}

struct Action {
  Action(Closure* begin_closure_param,
         Closure* repeat_closure_param,
         Closure* end_closure_param)
      : running(false),
        begin_closure(begin_closure_param),
        repeat_closure(repeat_closure_param),
        end_closure(end_closure_param) { }
  ~Action() {
    CHECK(bindings.empty());
  }

  // Is this action currently "running"? For certain key combinations, the
  // X server will keep sending key presses while the key is held down. For
  // any such sequence, the action is "running" after the first combo press
  // until a combo release is seen.
  bool running;

  // Closure to run when the action begins (i.e. key combo press)
  scoped_ptr<Closure> begin_closure;

  // Closure to run on action repeat while running (i.e. key combo repeat)
  scoped_ptr<Closure> repeat_closure;

  // Closure to run when the action ends (i.e. key combo release)
  scoped_ptr<Closure> end_closure;

  // The set of key combinations currently bound to this action.
  set<KeyBindings::KeyCombo, KeyBindings::KeyComboComparator> bindings;

  DISALLOW_COPY_AND_ASSIGN(Action);
};

KeyBindings::KeyBindings(XConnection* xconn)
    : xconn_(xconn) {
  CHECK(xconn_);
  if (!xconn_->SetDetectableKeyboardAutoRepeat(true)) {
    LOG(WARNING) << "Unable to enable detectable keyboard autorepeat";
  }
}

KeyBindings::~KeyBindings() {
  while (!actions_.empty()) {
    RemoveAction(actions_.begin()->first);
  }

  // Removing all actions should have also removed all bindings.
  CHECK_EQ(bindings_.size(), 0);
}

bool KeyBindings::AddAction(const string& action_name,
                            Closure* begin_closure,
                            Closure* repeat_closure,
                            Closure* end_closure) {
  CHECK(!action_name.empty());
  if (actions_.find(action_name) != actions_.end()) {
    LOG(WARNING) << "Attempting to add action that already exists: "
                 << action_name;
    return false;
  }
  Action* const action = new Action(begin_closure, repeat_closure, end_closure);
  CHECK(actions_.insert(make_pair(action_name, action)).second);
  return true;
}

bool KeyBindings::RemoveAction(const string& action_name) {
  ActionMap::iterator iter = actions_.find(action_name);
  if (iter == actions_.end()) {
    LOG(WARNING) << "Attempting to remove non-existant action: " << action_name;
    return false;
  }
  Action* const action = iter->second;
  while (!action->bindings.empty()) {
    CHECK(RemoveBinding(*(action->bindings.begin())));
  }
  delete action;
  actions_.erase(iter);

  return true;
}

bool KeyBindings::AddBinding(const KeyCombo& combo, const string& action_name) {
  if (bindings_.find(combo) != bindings_.end()) {
    LOG(WARNING) << "Attempt to overwrite existing key binding for action: "
                 << action_name;
    return false;
  }
  ActionMap::iterator iter = actions_.find(action_name);
  if (iter == actions_.end()) {
    LOG(WARNING) << "Attempt to add key binding for missing action: "
                 << action_name;
    return false;
  }
  Action* const action = iter->second;
  CHECK(action->bindings.insert(combo).second);
  CHECK(bindings_.insert(make_pair(combo, action_name)).second);
  CHECK(action_names_by_keysym_[combo.key].insert(action_name).second);

  KeyCode keycode = xconn_->GetKeyCodeFromKeySym(combo.key);
  xconn_->GrabKey(keycode, combo.modifiers);
  // Also grab this key combination plus Caps Lock.
  xconn_->GrabKey(keycode, combo.modifiers | LockMask);
  return true;
}

bool KeyBindings::RemoveBinding(const KeyCombo& combo) {
  BindingsMap::iterator bindings_iter = bindings_.find(combo);
  if (bindings_iter == bindings_.end()) {
    return false;
  }
  ActionMap::iterator action_iter = actions_.find(bindings_iter->second);
  CHECK(action_iter != actions_.end());
  Action* action = action_iter->second;
  CHECK_EQ(action->bindings.erase(combo), 1);

  KeySymMap::iterator keysym_iter = action_names_by_keysym_.find(combo.key);
  CHECK(keysym_iter != action_names_by_keysym_.end());
  CHECK_EQ(keysym_iter->second.erase(bindings_iter->second), 1);
  if (keysym_iter->second.empty()) {
    action_names_by_keysym_.erase(keysym_iter);
  }
  bindings_.erase(bindings_iter);

  // If this action triggered its own binding's removal we won't know what
  // to do with the corresponding release, so go ahead and mark the action
  // as not running here.
  action->running = false;

  KeyCode keycode = xconn_->GetKeyCodeFromKeySym(combo.key);
  xconn_->UngrabKey(keycode, combo.modifiers);
  xconn_->UngrabKey(keycode, combo.modifiers | LockMask);
  return true;
}

bool KeyBindings::HandleKeyPress(KeySym keysym, uint32 modifiers) {
  KeyCombo combo(keysym, modifiers);
  BindingsMap::const_iterator bindings_iter = bindings_.find(combo);
  if (bindings_iter == bindings_.end()) {
    return false;
  }

  ActionMap::iterator action_iter = actions_.find(bindings_iter->second);
  CHECK(action_iter != actions_.end());
  Action* const action = action_iter->second;
  if (action->running) {
    if (action->repeat_closure.get()) {
      action->repeat_closure->Run();
      return true;
    }
  } else {
    action->running = true;
    if (action->begin_closure.get()) {
      action->begin_closure->Run();
      return true;
    }
  }
  return false;
}

bool KeyBindings::HandleKeyRelease(KeySym keysym, uint32 modifiers) {
  KeyCombo combo(keysym, modifiers);

  // It's possible that a combo's modifier key(s) will get released before
  // its non-modifier key: for an Alt+Tab combo, imagine seeing Alt press,
  // Tab press, Alt release, and then Tab release.  In this case, kAltMask
  // won't be present in the Tab release event's modifier bitmap.  We still
  // want to run the end closure for the in-progress action when we receive
  // the Tab release, so we check all of the non-modifier key's actions
  // here to see if any of them are active.
  KeySymMap::const_iterator keysym_iter =
      action_names_by_keysym_.find(combo.key);
  if (keysym_iter == action_names_by_keysym_.end()) {
    return false;
  }

  bool ran_end_closure = false;
  for (set<string>::const_iterator action_name_iter =
         keysym_iter->second.begin();
       action_name_iter != keysym_iter->second.end(); ++action_name_iter) {
    ActionMap::iterator action_iter = actions_.find(*action_name_iter);
    CHECK(action_iter != actions_.end());
    Action* const action = action_iter->second;
    if (action->running) {
      action->running = false;
      if (action->end_closure.get()) {
        action->end_closure->Run();
        ran_end_closure = true;
      }
    }
  }
  return ran_end_closure;
}

uint32 KeyBindings::KeySymToModifier(uint32 keysym) {
  switch (keysym) {
    case XK_Shift_L:
    case XK_Shift_R:
      return kShiftMask;
    case XK_Control_L:
    case XK_Control_R:
      return kControlMask;
    case XK_Alt_L:
    case XK_Alt_R:
      return kAltMask;
    case XK_Meta_L:
    case XK_Meta_R:
      return kMetaMask;
    case XK_Num_Lock:
      return kNumLockMask;
    case XK_Super_L:
    case XK_Super_R:
      return kSuperMask;
    case XK_Hyper_L:
    case XK_Hyper_R:
      return kHyperMask;
  }
  return 0;
}

}  // namespace window_manager
