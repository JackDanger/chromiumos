// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __PLATFORM_WINDOW_MANAGER_KEY_BINDINGS_H__
#define __PLATFORM_WINDOW_MANAGER_KEY_BINDINGS_H__

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
//
extern "C" {
#include <X11/keysym.h>
#include <X11/X.h>
#include <X11/Xlib.h>
}
#include <map>

#include "base/basictypes.h"
#include "base/callback.h"

typedef ::Window XWindow;

namespace chromeos {

struct Action;
class XConnection;

class KeyBindings {
 public:
  // Set of possible modifer mask bits. OR these together to create a KeyCombo
  // modifiers value.
  static const uint kShiftMask = ShiftMask;
  static const uint kLockMask = LockMask;
  static const uint kControlMask = ControlMask;
  static const uint kAltMask = Mod1Mask;
  static const uint kMetaMask = Mod2Mask;     // TODO: Verify
  static const uint kNumLockMask = Mod3Mask;  // TODO: Verify
  static const uint kSuperMask = Mod4Mask;
  static const uint kHyperMask = Mod5Mask;    // TODO: Verify

  // A key and modifier combination, such as (XK_Tab, kAltMask) for alt-tab.
  struct KeyCombo {
    explicit KeyCombo(KeySym key_param)
        : key(key_param), modifiers(0) { }
    KeyCombo(KeySym key_param, uint modifiers_param)
        : key(key_param), modifiers(modifiers_param) { }

    KeySym key;
    uint modifiers;
  };
  struct KeyComboComparator {
    bool operator()(const KeyCombo& a,
                    const KeyCombo& b) const;
  };

  KeyBindings(XConnection* xconn);
  ~KeyBindings();

  // Add a new action. This will fail if the action already exists.
  // NOTE: The KeyBindings class will take ownership of passed in callbacks.
  bool AddAction(const string& action_name,
                 Closure* begin_closure,   // [optional] On combo press
                 Closure* repeat_closure,  // [optional] On combo auto-repeat
                 Closure* end_closure);    // [optional] On combo release

  // Removes an action. Any key bindings to this action will also be removed.
  bool RemoveAction(const string& action_name);

  // Add a binding from the given KeyCombo to the action. KeyCombo's must be
  // unique, but it is fine to have more than one combo map to a given action.
  bool AddBinding(const KeyCombo& combo,
                  const string& action_name);

  // Remove the KeyCombo. This may fail if the action to which the combo was
  // bound has been removed, in which case the combo was already cleaned up.
  bool RemoveBinding(const KeyCombo& combo);

  // These should be called by the window manager in order to process bindings.
  bool HandleKeyPress(KeySym keysym, uint32 modifiers);
  bool HandleKeyRelease(KeySym keysym, uint32 modifiers);

 private:
  // Returns the modifier mask value that is equivalent to the given keysym
  // if the keysym is a modifier type; else 0.
  uint KeySymToModifier(uint keysym);

  XConnection* xconn_;  // Weak reference

  typedef map<string, Action*> ActionMap;
  ActionMap actions_;

  typedef map<KeyCombo, string, KeyComboComparator> BindingsMap;
  BindingsMap bindings_;

  DISALLOW_COPY_AND_ASSIGN(KeyBindings);
};

}  // namespace chromeos

#endif  // __PLATFORM_WINDOW_MANAGER_KEY_BINDINGS_H__
