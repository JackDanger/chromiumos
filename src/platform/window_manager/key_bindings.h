// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_KEY_BINDINGS_H_
#define WINDOW_MANAGER_KEY_BINDINGS_H_

// KeyBindings
//
// The KeyBindings class supports installing named actions and keyboard combos
// that trigger an installed action.
//
// A named action can have begin, repeat, and end callbacks associated with it
// which correspond to key down, key repeat, and key release respectively.
// Any of these callbacks may be NULL. Any number of KeyCombo's can be bound
// to a given action. A KeyCombo is a keysym and modifier combination such as
// (XK_Tab, kAltMask). For example, to install a "switch-window" action with
// the alt-tab key combo and have SwitchWindowCallback called on combo press:
//
//   KeyBindings bindings;
//   bindings.AddAction("switch-window",
//                      NewPermanentCallback(SwitchWindowCallback),
//                      NULL,    // No repeat callback
//                      NULL);   // No end callback
//   bindings.AddBinding(
//       KeyBindings::KeyCombo(XK_Tab, kAltMask), "switch-window");

#include <map>
#include <set>
#include <string>

extern "C" {
#include <X11/keysym.h>
#include <X11/X.h>
#include <X11/Xlib.h>
}

#include "base/basictypes.h"
#include "chromeos/callback.h"

typedef ::Window XWindow;

namespace window_manager {

struct Action;
class XConnection;

class KeyBindings {
 public:
  // Set of possible modifer mask bits. OR these together to create a KeyCombo
  // modifiers value.
  static const uint32 kShiftMask = ShiftMask;
  static const uint32 kControlMask = ControlMask;
  static const uint32 kAltMask = Mod1Mask;
  static const uint32 kMetaMask = Mod2Mask;     // TODO: Verify
  static const uint32 kNumLockMask = Mod3Mask;  // TODO: Verify
  static const uint32 kSuperMask = Mod4Mask;
  static const uint32 kHyperMask = Mod5Mask;    // TODO: Verify

  // A key and modifier combination, such as (XK_Tab, kAltMask) for alt-tab.
  struct KeyCombo {
    // We lowercase keysyms (the uppercase distinction when Shift is down
    // or Caps Lock is on isn't useful for us) and mask LockMask out of the
    // modifier (so that bindings will still be recognized if Caps Lock is
    // enabled).
    explicit KeyCombo(KeySym key_param, uint32 modifiers_param = 0);

    KeySym key;
    uint32 modifiers;
  };

  struct KeyComboComparator {
    bool operator()(const KeyCombo& a,
                    const KeyCombo& b) const;
  };

  KeyBindings(XConnection* xconn);
  ~KeyBindings();

  // Add a new action. This will fail if the action already exists.
  // NOTE: The KeyBindings class will take ownership of passed-in
  // callbacks, any of which may be NULL.
  bool AddAction(const std::string& action_name,
                 chromeos::Closure* begin_closure,   // On combo press
                 chromeos::Closure* repeat_closure,  // On combo auto-repeat
                 chromeos::Closure* end_closure);    // On combo release

  // Removes an action. Any key bindings to this action will also be removed.
  bool RemoveAction(const std::string& action_name);

  // Add a binding from the given KeyCombo to the action. KeyCombo's must be
  // unique, but it is fine to have more than one combo map to a given action.
  bool AddBinding(const KeyCombo& combo,
                  const std::string& action_name);

  // Remove the KeyCombo. This may fail if the action to which the combo was
  // bound has been removed, in which case the combo was already cleaned up.
  bool RemoveBinding(const KeyCombo& combo);

  // These should be called by the window manager in order to process bindings.
  bool HandleKeyPress(KeySym keysym, uint32 modifiers);
  bool HandleKeyRelease(KeySym keysym, uint32 modifiers);

 private:
  // Returns the modifier mask value that is equivalent to the given keysym
  // if the keysym is a modifier type; else 0.
  uint32 KeySymToModifier(uint32 keysym);

  XConnection* xconn_;  // Weak reference

  typedef std::map<std::string, Action*> ActionMap;
  ActionMap actions_;

  typedef std::map<KeyCombo, std::string, KeyComboComparator> BindingsMap;
  BindingsMap bindings_;

  // Map from a keysym to the names of all of the actions that use it as
  // their non-modifier key.
  typedef std::map<KeySym, std::set<std::string> > KeySymMap;
  KeySymMap action_names_by_keysym_;

  DISALLOW_COPY_AND_ASSIGN(KeyBindings);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_KEY_BINDINGS_H_
