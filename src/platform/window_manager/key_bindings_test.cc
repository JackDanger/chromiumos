// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chromeos/callback.h"
#include "chromeos/obsolete_logging.h"
#include "window_manager/key_bindings.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/test_lib.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace window_manager {

using chromeos::NewPermanentCallback;

struct TestAction {
  explicit TestAction(const std::string& name_param)
      : name(name_param),
        begin_call_count(0),
        repeat_call_count(0),
        end_call_count(0) {
  }
  ~TestAction() {
  }

  void Reset() {
    begin_call_count = 0;
    repeat_call_count = 0;
    end_call_count = 0;
  }

  void BeginCallback() {
    VLOG(1) << "Begin: " << name;
    ++begin_call_count;
  }
  void RepeatCallback() {
    VLOG(1) << "Repeat: " << name;
    ++repeat_call_count;
  }
  void EndCallback() {
    VLOG(1) << "End: " << name;
    ++end_call_count;
  }

  std::string name;
  int begin_call_count;
  int repeat_call_count;
  int end_call_count;
};

class KeyBindingsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    xconn_.reset(new MockXConnection());
    bindings_.reset(new KeyBindings(xconn_.get()));
    for (int i = 0; i < kNumActions; ++i) {
      actions_.push_back(new TestAction(StringPrintf("action_%d", i)));
    }
  }
  virtual void TearDown() {
    STLDeleteElements(&actions_);
  }

  void AddAction(int number,
                 bool use_begin_closure,
                 bool use_repeat_closure,
                 bool use_end_closure) {
    CHECK_LT(number, kNumActions);
    TestAction* const action = actions_[number];
    bindings_->AddAction(
        action->name,
        use_begin_closure ?
        NewPermanentCallback(action, &TestAction::BeginCallback) : NULL,
        use_repeat_closure ?
        NewPermanentCallback(action, &TestAction::RepeatCallback) : NULL,
        use_end_closure ?
        NewPermanentCallback(action, &TestAction::EndCallback) : NULL);
  }

  void AddAllActions() {
    for (int i = 0; i < kNumActions; ++i) {
      AddAction(i, true, true, true);
    }
  }

  scoped_ptr<window_manager::MockXConnection> xconn_;
  scoped_ptr<window_manager::KeyBindings> bindings_;
  std::vector<TestAction*> actions_;

  static const int kNumActions = 10;
};

TEST_F(KeyBindingsTest, Basic) {
  // Action 0: Requests begin, end, and repeat callbacks.
  AddAction(0, true, true, true);
  bindings_->AddBinding(KeyBindings::KeyCombo(XK_e, KeyBindings::kControlMask),
                       actions_[0]->name);

  // -- Combo press for action 0
  EXPECT_TRUE(bindings_->HandleKeyPress(XK_e, KeyBindings::kControlMask));
  EXPECT_EQ(1, actions_[0]->begin_call_count);
  EXPECT_EQ(0, actions_[0]->repeat_call_count);
  EXPECT_EQ(0, actions_[0]->end_call_count);

  // -- Combo repeats for action 0
  EXPECT_TRUE(bindings_->HandleKeyPress(XK_e, KeyBindings::kControlMask));
  EXPECT_EQ(1, actions_[0]->begin_call_count);
  EXPECT_EQ(1, actions_[0]->repeat_call_count);
  EXPECT_EQ(0, actions_[0]->end_call_count);
  EXPECT_TRUE(bindings_->HandleKeyPress(XK_e, KeyBindings::kControlMask));
  EXPECT_EQ(1, actions_[0]->begin_call_count);
  EXPECT_EQ(2, actions_[0]->repeat_call_count);
  EXPECT_EQ(0, actions_[0]->end_call_count);

  // -- Combo release for action 0
  bindings_->HandleKeyRelease(XK_e, KeyBindings::kControlMask);
  EXPECT_EQ(1, actions_[0]->begin_call_count);
  EXPECT_EQ(2, actions_[0]->repeat_call_count);
  EXPECT_EQ(1, actions_[0]->end_call_count);

  // -- Unregistered combo presses.
  EXPECT_FALSE(bindings_->HandleKeyPress(XK_t, KeyBindings::kControlMask));
  EXPECT_FALSE(bindings_->HandleKeyRelease(XK_t, KeyBindings::kControlMask));
  EXPECT_FALSE(bindings_->HandleKeyPress(XK_e, KeyBindings::kShiftMask));
  EXPECT_FALSE(bindings_->HandleKeyRelease(XK_e, KeyBindings::kShiftMask));
  EXPECT_FALSE(bindings_->HandleKeyPress(XK_e, 0));
  EXPECT_FALSE(bindings_->HandleKeyRelease(XK_e, 0));
  EXPECT_EQ(1, actions_[0]->begin_call_count);
  EXPECT_EQ(2, actions_[0]->repeat_call_count);
  EXPECT_EQ(1, actions_[0]->end_call_count);
}

TEST_F(KeyBindingsTest, ModifierKey) {
  // Action 0: Requests begin and end callbacks.
  AddAction(0, true, true, true);

  // Bind a modifier as the main key. Upon release, the modifiers mask will
  // also contain the modifier, so make sure that doesn't mess things up.
  KeyBindings::KeyCombo combo(XK_Super_L, KeyBindings::kControlMask);
  bindings_->AddBinding(combo, actions_[0]->name);

  // -- Combo press for action 0
  EXPECT_TRUE(bindings_->HandleKeyPress(XK_Super_L, KeyBindings::kControlMask));
  EXPECT_EQ(1, actions_[0]->begin_call_count);
  EXPECT_EQ(0, actions_[0]->end_call_count);

  // -- Combo release for action 0
  // NOTE: We add in the modifier mask for the key itself.
  bindings_->HandleKeyRelease(
      XK_Super_L, KeyBindings::kControlMask | KeyBindings::kSuperMask);
  EXPECT_EQ(1, actions_[0]->begin_call_count);
  EXPECT_EQ(1, actions_[0]->end_call_count);
}

TEST_F(KeyBindingsTest, NullCallbacks) {
  // Action 0: Requests end callback only.
  AddAction(0, false, false, true);
  bindings_->AddBinding(KeyBindings::KeyCombo(XK_e, KeyBindings::kControlMask),
                       actions_[0]->name);

  // Action 1: Requests begin callback only.
  AddAction(1, true, false, false);
  bindings_->AddBinding(KeyBindings::KeyCombo(XK_b, KeyBindings::kControlMask),
                       actions_[1]->name);

  // Action 2: Requests repeat callback only.
  AddAction(2, false, true, false);
  bindings_->AddBinding(KeyBindings::KeyCombo(XK_r, KeyBindings::kControlMask),
                       actions_[2]->name);

  // -- Combo press for action 0
  EXPECT_FALSE(bindings_->HandleKeyPress(XK_e, KeyBindings::kControlMask));
  EXPECT_EQ(0, actions_[0]->begin_call_count);
  EXPECT_EQ(0, actions_[0]->repeat_call_count);
  EXPECT_EQ(0, actions_[0]->end_call_count);

  // -- Combo repeat for action 0
  EXPECT_FALSE(bindings_->HandleKeyPress(XK_e, KeyBindings::kControlMask));
  EXPECT_EQ(0, actions_[0]->begin_call_count);
  EXPECT_EQ(0, actions_[0]->repeat_call_count);
  EXPECT_EQ(0, actions_[0]->end_call_count);

  // -- Combo release for action 0
  EXPECT_TRUE(bindings_->HandleKeyRelease(XK_e, KeyBindings::kControlMask));
  EXPECT_EQ(0, actions_[0]->begin_call_count);
  EXPECT_EQ(0, actions_[0]->repeat_call_count);
  EXPECT_EQ(1, actions_[0]->end_call_count);

  // -- Combo press for action 1
  EXPECT_TRUE(bindings_->HandleKeyPress(XK_b, KeyBindings::kControlMask));
  EXPECT_EQ(1, actions_[1]->begin_call_count);
  EXPECT_EQ(0, actions_[1]->repeat_call_count);
  EXPECT_EQ(0, actions_[1]->end_call_count);

  // -- Combo repeat for action 1
  EXPECT_FALSE(bindings_->HandleKeyPress(XK_b, KeyBindings::kControlMask));
  EXPECT_EQ(1, actions_[1]->begin_call_count);
  EXPECT_EQ(0, actions_[1]->repeat_call_count);
  EXPECT_EQ(0, actions_[1]->end_call_count);

  // -- Combo release for action 1
  EXPECT_FALSE(bindings_->HandleKeyRelease(XK_b, KeyBindings::kControlMask));
  EXPECT_EQ(1, actions_[1]->begin_call_count);
  EXPECT_EQ(0, actions_[1]->repeat_call_count);
  EXPECT_EQ(0, actions_[1]->end_call_count);

  // -- Combo press for action 2
  EXPECT_FALSE(bindings_->HandleKeyPress(XK_r, KeyBindings::kControlMask));
  EXPECT_EQ(0, actions_[2]->begin_call_count);
  EXPECT_EQ(0, actions_[2]->repeat_call_count);
  EXPECT_EQ(0, actions_[2]->end_call_count);

  // -- Combo repeat for action 2
  EXPECT_TRUE(bindings_->HandleKeyPress(XK_r, KeyBindings::kControlMask));
  EXPECT_EQ(0, actions_[2]->begin_call_count);
  EXPECT_EQ(1, actions_[2]->repeat_call_count);
  EXPECT_EQ(0, actions_[2]->end_call_count);

  // -- Combo release for action 2
  EXPECT_FALSE(bindings_->HandleKeyRelease(XK_r, KeyBindings::kControlMask));
  EXPECT_EQ(0, actions_[2]->begin_call_count);
  EXPECT_EQ(1, actions_[2]->repeat_call_count);
  EXPECT_EQ(0, actions_[2]->end_call_count);
}

TEST_F(KeyBindingsTest, InvalidOperations) {
  EXPECT_FALSE(bindings_->RemoveAction("nonexistant"));
  EXPECT_FALSE(bindings_->RemoveBinding(KeyBindings::KeyCombo(XK_e)));
  EXPECT_FALSE(bindings_->AddBinding(KeyBindings::KeyCombo(XK_e),
                                     "nonexistant"));

  EXPECT_TRUE(bindings_->AddAction("test", NULL, NULL, NULL));
  EXPECT_FALSE(bindings_->AddAction("test", NULL, NULL, NULL));  // Double add

  KeyBindings::KeyCombo combo(XK_e);
  EXPECT_TRUE(bindings_->AddBinding(combo, "test"));
  EXPECT_FALSE(bindings_->AddBinding(combo, "test"));  // Double add
}

TEST_F(KeyBindingsTest, ManyActionsAndBindings) {
  AddAllActions();

  // Add multiple key bindings for each action.
  const int kBindingsPerAction = 4;
  for (int i = 0; i < kBindingsPerAction; ++i) {
    for (int j = 0; j < kNumActions; ++j) {
      KeySym key = XK_a + (i * kNumActions) + j;
      EXPECT_TRUE(
          bindings_->AddBinding(KeyBindings::KeyCombo(key), actions_[j]->name));
    }
  }

  // Test key combos across all bindings.
  const int kNumActivates = 2;
  for (int i = 0; i < kBindingsPerAction; ++i) {
    for (int j = 0; j < kNumActions; ++j) {
      KeySym key = XK_a + (i * kNumActions) + j;
      for (int k = 0; k < kNumActivates; ++k) {
        const int count = i * kNumActivates + k;
        EXPECT_TRUE(bindings_->HandleKeyPress(key, 0));    // Press
        EXPECT_EQ(count + 1, actions_[j]->begin_call_count);
        EXPECT_EQ(count, actions_[j]->repeat_call_count);
        EXPECT_EQ(count, actions_[j]->end_call_count);
        EXPECT_TRUE(bindings_->HandleKeyPress(key, 0));    // Repeat
        EXPECT_EQ(count + 1, actions_[j]->begin_call_count);
        EXPECT_EQ(count + 1, actions_[j]->repeat_call_count);
        EXPECT_EQ(count, actions_[j]->end_call_count);
        EXPECT_TRUE(bindings_->HandleKeyRelease(key, 0));  // Release
        EXPECT_EQ(count + 1, actions_[j]->begin_call_count);
        EXPECT_EQ(count + 1, actions_[j]->repeat_call_count);
        EXPECT_EQ(count + 1, actions_[j]->end_call_count);
      }
    }
  }

  // Remove half of the key bindings
  for (int i = 0; i < (kBindingsPerAction / 2); ++i) {
    for (int j = 0; j < kNumActions; ++j) {
      KeySym key = XK_a + (i * kNumActions) + j;
      EXPECT_TRUE(bindings_->RemoveBinding(KeyBindings::KeyCombo(key)));
    }
  }

  // Test all key combos again, but half the bindings have been removed.
  for (int i = 0; i < kNumActions; ++i) {
    actions_[i]->Reset();
  }
  for (int i = 0; i < kBindingsPerAction; ++i) {
    for (int j = 0; j < kNumActions; ++j) {
      KeySym key = XK_a + (i * kNumActions) + j;
      const bool has_binding = (i >= (kBindingsPerAction / 2));
      EXPECT_EQ(has_binding, bindings_->HandleKeyPress(key, 0));
      EXPECT_EQ(has_binding, bindings_->HandleKeyRelease(key, 0));
      if (has_binding) {
        EXPECT_GT(actions_[j]->begin_call_count, 0);
        EXPECT_GT(actions_[j]->end_call_count, 0);
      } else {
        // These key bindings were removed.
        EXPECT_EQ(0, actions_[j]->begin_call_count);
        EXPECT_EQ(0, actions_[j]->end_call_count);
      }
    }
  }

  // Remove all of the actions; key combos should be cleaned up automatically.
  for (int i = 0; i < kNumActions; ++i) {
    actions_[i]->Reset();
    EXPECT_TRUE(bindings_->RemoveAction(actions_[i]->name));
  }
  for (int i = 0; i < kBindingsPerAction; ++i) {
    for (int j = 0; j < kNumActions; ++j) {
      KeySym key = XK_a + (i * kNumActions) + j;
      EXPECT_FALSE(bindings_->HandleKeyPress(key, 0));
      EXPECT_FALSE(bindings_->HandleKeyRelease(key, 0));
      EXPECT_EQ(0, actions_[j]->begin_call_count);
      EXPECT_EQ(0, actions_[j]->end_call_count);
    }
  }

  // Attempts to remove bindings should fail.
  for (int i = 0; i < kBindingsPerAction; ++i) {
    for (int j = 0; j < kNumActions; ++j) {
      KeySym key = XK_a + (i * kNumActions) + j;
      EXPECT_FALSE(bindings_->RemoveBinding(KeyBindings::KeyCombo(key)));
    }
  }

  // Attempts to remove actions should fail (already removed).
  for (int i = 0; i < kNumActions; ++i) {
    EXPECT_FALSE(bindings_->RemoveAction(actions_[i]->name));
  }
}

// Test that we use the lowercase versions of keysyms.
TEST_F(KeyBindingsTest, Lowercase) {
  // Add a Ctrl+E (uppercase 'e') binding and check that it's activated by
  // both uppercase and lowercase keysyms.
  AddAction(0, true, false, true);
  bindings_->AddBinding(KeyBindings::KeyCombo(XK_E, KeyBindings::kControlMask),
                       actions_[0]->name);
  EXPECT_TRUE(bindings_->HandleKeyPress(XK_e, KeyBindings::kControlMask));
  EXPECT_TRUE(bindings_->HandleKeyRelease(XK_e, KeyBindings::kControlMask));
  EXPECT_TRUE(bindings_->HandleKeyPress(XK_E, KeyBindings::kControlMask));
  EXPECT_TRUE(bindings_->HandleKeyRelease(XK_E, KeyBindings::kControlMask));
  EXPECT_EQ(2, actions_[0]->begin_call_count);
  EXPECT_EQ(2, actions_[0]->end_call_count);

  // Add a Ctrl+j (lowercase 'j') binding and check that it's activated
  // by both too.
  AddAction(1, true, false, true);
  bindings_->AddBinding(KeyBindings::KeyCombo(XK_j, KeyBindings::kControlMask),
                       actions_[1]->name);
  EXPECT_TRUE(bindings_->HandleKeyPress(XK_j, KeyBindings::kControlMask));
  EXPECT_TRUE(bindings_->HandleKeyRelease(XK_j, KeyBindings::kControlMask));
  EXPECT_TRUE(bindings_->HandleKeyPress(XK_J, KeyBindings::kControlMask));
  EXPECT_TRUE(bindings_->HandleKeyRelease(XK_J, KeyBindings::kControlMask));
  EXPECT_EQ(2, actions_[1]->begin_call_count);
  EXPECT_EQ(2, actions_[1]->end_call_count);

  // Add a Shift+r (lowercase 'r') binding and check that it's activated by
  // the corresponding uppercase keysym (the X server will give us
  // uppercase since shift is down).
  AddAction(2, true, false, true);
  bindings_->AddBinding(KeyBindings::KeyCombo(XK_r, KeyBindings::kShiftMask),
                       actions_[2]->name);
  EXPECT_TRUE(bindings_->HandleKeyPress(XK_R, KeyBindings::kShiftMask));
  EXPECT_TRUE(bindings_->HandleKeyRelease(XK_R, KeyBindings::kShiftMask));
  EXPECT_EQ(1, actions_[2]->begin_call_count);
  EXPECT_EQ(1, actions_[2]->end_call_count);
}

// Test that keysyms are still recognized when Caps Lock is on.
TEST_F(KeyBindingsTest, RemoveCapsLock) {
  AddAction(0, true, false, true);
  bindings_->AddBinding(KeyBindings::KeyCombo(XK_e, KeyBindings::kControlMask),
                       actions_[0]->name);

  // We need to grab both Ctrl+e and Ctrl+CapsLock+e; we wouldn't get
  // triggered when Caps Lock is on otherwise.
  KeyCode keycode = xconn_->GetKeyCodeFromKeySym(XK_e);
  EXPECT_TRUE(xconn_->KeyIsGrabbed(keycode, KeyBindings::kControlMask));
  EXPECT_TRUE(
      xconn_->KeyIsGrabbed(keycode, KeyBindings::kControlMask | LockMask));

  EXPECT_TRUE(bindings_->HandleKeyPress(
                  XK_E, KeyBindings::kControlMask | LockMask));
  EXPECT_TRUE(bindings_->HandleKeyRelease(
                  XK_E, KeyBindings::kControlMask | LockMask));
  EXPECT_EQ(1, actions_[0]->begin_call_count);
  EXPECT_EQ(0, actions_[0]->repeat_call_count);
  EXPECT_EQ(1, actions_[0]->end_call_count);
}

// Test that we terminate in-progress actions correctly when their modifier
// keys get released before the non-modifier key.
TEST_F(KeyBindingsTest, ModifierReleasedFirst) {
  AddAction(0, true, false, true);
  bindings_->AddBinding(KeyBindings::KeyCombo(XK_k, KeyBindings::kControlMask),
                       actions_[0]->name);

  EXPECT_FALSE(bindings_->HandleKeyPress(XK_Control_L, 0));
  EXPECT_TRUE(bindings_->HandleKeyPress(XK_k, KeyBindings::kControlMask));
  EXPECT_FALSE(bindings_->HandleKeyRelease(XK_Control_L,
                                           KeyBindings::kControlMask));
  EXPECT_TRUE(bindings_->HandleKeyRelease(XK_k, 0));

  EXPECT_EQ(1, actions_[0]->begin_call_count);
  EXPECT_EQ(1, actions_[0]->end_call_count);
}

}  // namespace window_manager

int main(int argc, char **argv) {
  return window_manager::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
