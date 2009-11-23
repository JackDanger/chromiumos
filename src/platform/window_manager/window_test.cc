// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/shadow.h"
#include "window_manager/test_lib.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace chromeos {

class WindowTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    xconn_.reset(new MockXConnection);
    clutter_.reset(new MockClutterInterface);
    wm_.reset(new WindowManager(xconn_.get(), clutter_.get()));
    CHECK(wm_->Init());
  }
  virtual void TearDown() {
  }

  scoped_ptr<MockXConnection> xconn_;
  scoped_ptr<MockClutterInterface> clutter_;
  scoped_ptr<WindowManager> wm_;
};

TEST_F(WindowTest, WindowType) {
  XWindow xid = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      10, 20,  // x, y
      30, 40,  // width, height
      false,   // override redirect
      false,   // input only
      0);      // event mask
  Window win(wm_.get(), xid);

  // Without a window type, we should have a shadow.
  EXPECT_EQ(WmIpc::WINDOW_TYPE_UNKNOWN, win.type());
  EXPECT_TRUE(win.shadow() != NULL);

  // Toplevel windows should have shadows too.
  ASSERT_TRUE(wm_->wm_ipc()->SetWindowType(
                  xid, WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL, NULL));
  EXPECT_TRUE(win.FetchAndApplyWindowType(true));  // update_shadow
  EXPECT_EQ(WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL, win.type());
  EXPECT_TRUE(win.shadow() != NULL);

  // Tab summary windows shouldn't have shadows.
  ASSERT_TRUE(wm_->wm_ipc()->SetWindowType(
                  xid, WmIpc::WINDOW_TYPE_CHROME_TAB_SUMMARY, NULL));
  EXPECT_TRUE(win.FetchAndApplyWindowType(true));  // update_shadow
  EXPECT_EQ(WmIpc::WINDOW_TYPE_CHROME_TAB_SUMMARY, win.type());
  EXPECT_TRUE(win.shadow() == NULL);

  // Nor should info bubbles.
  ASSERT_TRUE(wm_->wm_ipc()->SetWindowType(
                  xid, WmIpc::WINDOW_TYPE_CHROME_INFO_BUBBLE, NULL));
  EXPECT_TRUE(win.FetchAndApplyWindowType(true));  // update_shadow
  EXPECT_EQ(WmIpc::WINDOW_TYPE_CHROME_INFO_BUBBLE, win.type());
  EXPECT_TRUE(win.shadow() == NULL);
}

TEST_F(WindowTest, ChangeClient) {
  XWindow xid = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      10, 20,  // x, y
      30, 40,  // width, height
      false,   // override redirect
      false,   // input only
      0);      // event mask
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);

  Window window(wm_.get(), xid);

  // Make sure that the window's initial attributes are loaded correctly.
  EXPECT_EQ(xid, window.xid());
  EXPECT_EQ(10, window.client_x());
  EXPECT_EQ(20, window.client_y());
  EXPECT_EQ(30, window.client_width());
  EXPECT_EQ(40, window.client_height());
  EXPECT_EQ(false, window.mapped());

  EXPECT_TRUE(window.MapClient());
  EXPECT_TRUE(info->mapped);

  // Move the window.
  EXPECT_TRUE(window.MoveClient(100, 200));
  EXPECT_EQ(100, info->x);
  EXPECT_EQ(200, info->y);
  EXPECT_EQ(100, window.client_x());
  EXPECT_EQ(200, window.client_y());

  // Resize the window.
  EXPECT_TRUE(window.ResizeClient(300, 400, Window::GRAVITY_NORTHWEST));
  EXPECT_EQ(300, info->width);
  EXPECT_EQ(400, info->height);
  EXPECT_EQ(300, window.client_width());
  EXPECT_EQ(400, window.client_height());

  // We need to be able to update windows' local geometry variables in
  // response to ConfigureNotify events to be able to handle
  // override-redirect windows, so make sure that that works correctly.
  window.SaveClientPosition(50, 60);
  window.SaveClientAndCompositedSize(70, 80);
  EXPECT_EQ(50, window.client_x());
  EXPECT_EQ(60, window.client_y());
  EXPECT_EQ(70, window.client_width());
  EXPECT_EQ(80, window.client_height());
}

TEST_F(WindowTest, ChangeComposited) {
  XWindow xid = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      10, 20,  // x, y
      30, 40,  // width, height
      false,   // override redirect
      false,   // input only
      0);      // event mask
  Window window(wm_.get(), xid);

  const MockClutterInterface::Actor* actor =
      dynamic_cast<const MockClutterInterface::Actor*>(window.actor());
  ASSERT_TRUE(actor != NULL);

  // Initially, we should place the composited window at the same location
  // as the client window.
  EXPECT_EQ(10, actor->x());
  EXPECT_EQ(20, actor->y());
  EXPECT_EQ(10, window.composited_x());
  EXPECT_EQ(20, window.composited_y());
  EXPECT_EQ(30, window.actor()->GetWidth());
  EXPECT_EQ(40, window.actor()->GetHeight());
  EXPECT_DOUBLE_EQ(1.0, actor->scale_x());
  EXPECT_DOUBLE_EQ(1.0, actor->scale_y());
  EXPECT_DOUBLE_EQ(1.0, window.composited_scale_x());
  EXPECT_DOUBLE_EQ(1.0, window.composited_scale_y());

  // Move the composited window to a new spot.
  window.MoveComposited(40, 50, 0);
  EXPECT_EQ(40, actor->x());
  EXPECT_EQ(50, actor->y());
  EXPECT_EQ(40, window.composited_x());
  EXPECT_EQ(50, window.composited_y());

  window.ScaleComposited(0.75, 0.25, 0);
  EXPECT_EQ(0.75, actor->scale_x());
  EXPECT_EQ(0.25, actor->scale_y());
  EXPECT_EQ(0.75, window.composited_scale_x());
  EXPECT_EQ(0.25, window.composited_scale_y());
}

TEST_F(WindowTest, TransientFor) {
  XWindow xid = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      300, 300,  // x, y
      60, 40,    // width, height
      false,     // override redirect
      false,     // input only
      0);        // event mask
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);

  XWindow owner_xid = 1234;  // arbitrary ID
  info->transient_for = owner_xid;
  Window win(wm_.get(), xid);
  EXPECT_EQ(owner_xid, win.transient_for_xid());

  XWindow new_owner_xid = 5678;
  info->transient_for = new_owner_xid;
  EXPECT_TRUE(win.FetchAndApplyTransientHint());
  EXPECT_EQ(new_owner_xid, win.transient_for_xid());
}

TEST_F(WindowTest, GetMaxSize) {
  XWindow xid = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      10, 20,  // x, y
      30, 40,  // width, height
      false,   // override redirect
      false,   // input only
      0);      // event mask

  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  info->size_hints.min_width = 400;
  info->size_hints.min_height = 300;
  info->size_hints.max_width = 800;
  info->size_hints.max_height = 600;
  info->size_hints.width_inc = 10;
  info->size_hints.height_inc = 5;
  info->size_hints.base_width = 40;
  info->size_hints.base_width = 30;
  info->size_hints.flags = PMinSize | PMaxSize | PResizeInc | PBaseSize;

  Window win(wm_.get(), xid);
  ASSERT_TRUE(win.FetchAndApplySizeHints());
  int width = 0, height = 0;

  // We should get the minimum size if we request a size smaller than it.
  win.GetMaxSize(300, 200, &width, &height);
  EXPECT_EQ(400, width);
  EXPECT_EQ(300, height);

  // And the maximum size if we request one larger than it.
  win.GetMaxSize(1000, 800, &width, &height);
  EXPECT_EQ(800, width);
  EXPECT_EQ(600, height);

  // Test that the size increment hints are honored when we request a size
  // that's not equal to the base size plus some multiple of the size
  // increments.
  win.GetMaxSize(609, 409, &width, &height);
  EXPECT_EQ(600, width);
  EXPECT_EQ(405, height);
}

// Test WM_DELETE_WINDOW and WM_TAKE_FOCUS from ICCCM's WM_PROTOCOLS.
TEST_F(WindowTest, WmProtocols) {
  // Create a window.
  XWindow xid = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      10, 20,  // x, y
      30, 40,  // width, height
      false,   // override redirect
      false,   // input only
      0);      // event mask
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);

  // Set its WM_PROTOCOLS property to indicate that it supports both
  // message types.
  std::vector<int> values;
  values.push_back(static_cast<int>(wm_->GetXAtom(ATOM_WM_DELETE_WINDOW)));
  values.push_back(static_cast<int>(wm_->GetXAtom(ATOM_WM_TAKE_FOCUS)));
  xconn_->SetIntArrayProperty(xid,
                              wm_->GetXAtom(ATOM_WM_PROTOCOLS),  // atom
                              XA_ATOM,                           // type
                              values);

  Window win(wm_.get(), xid);

  // Send a WM_DELETE_WINDOW message to the window and check that its
  // contents are correct.
  Time timestamp = 43;  // arbitrary
  EXPECT_TRUE(win.SendDeleteRequest(timestamp));
  ASSERT_EQ(1, info->client_messages.size());
  const XClientMessageEvent& delete_msg = info->client_messages[0];
  EXPECT_EQ(wm_->GetXAtom(ATOM_WM_PROTOCOLS), delete_msg.message_type);
  EXPECT_EQ(XConnection::kLongFormat, delete_msg.format);
  EXPECT_EQ(wm_->GetXAtom(ATOM_WM_DELETE_WINDOW), delete_msg.data.l[0]);
  EXPECT_EQ(timestamp, delete_msg.data.l[1]);

  // Now do the same thing with WM_TAKE_FOCUS.
  timestamp = 98;  // arbitrary
  info->client_messages.clear();
  EXPECT_TRUE(win.TakeFocus(timestamp));
  ASSERT_EQ(1, info->client_messages.size());
  const XClientMessageEvent& focus_msg = info->client_messages[0];
  EXPECT_EQ(wm_->GetXAtom(ATOM_WM_PROTOCOLS), focus_msg.message_type);
  EXPECT_EQ(XConnection::kLongFormat, focus_msg.format);
  EXPECT_EQ(wm_->GetXAtom(ATOM_WM_TAKE_FOCUS), focus_msg.data.l[0]);
  EXPECT_EQ(timestamp, focus_msg.data.l[1]);

  // Get rid of the window's WM_PROTOCOLS support.
  xconn_->DeletePropertyIfExists(xid, wm_->GetXAtom(ATOM_WM_PROTOCOLS));
  win.FetchAndApplyWmProtocols();
  info->client_messages.clear();

  // SendDeleteRequest() should fail outright if the window doesn't support
  // WM_DELETE_WINDOW.
  EXPECT_FALSE(win.SendDeleteRequest(1));
  EXPECT_EQ(0, info->client_messages.size());

  // TakeFocus() should manually assign the focus with a SetInputFocus
  // request instead of sending a message.
  EXPECT_EQ(None, xconn_->focused_xid());
  EXPECT_TRUE(win.TakeFocus(timestamp));
  EXPECT_EQ(0, info->client_messages.size());
  EXPECT_EQ(xid, xconn_->focused_xid());
}

TEST_F(WindowTest, WmState) {
  const XAtom wm_state_atom = wm_->GetXAtom(ATOM_NET_WM_STATE);
  const XAtom fullscreen_atom = wm_->GetXAtom(ATOM_NET_WM_STATE_FULLSCREEN);
  const XAtom max_horz_atom = wm_->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_HORZ);
  const XAtom max_vert_atom = wm_->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_VERT);
  const XAtom modal_atom = wm_->GetXAtom(ATOM_NET_WM_STATE_MODAL);

  // Create a window with its _NET_WM_STATE property set to only
  // _NET_WM_STATE_MODAL and make sure that it's correctly loaded in the
  // constructor.
  XWindow xid = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      10, 20,  // x, y
      30, 40,  // width, height
      false,   // override redirect
      false,   // input only
      0);      // event mask
  xconn_->SetIntProperty(xid,
                         wm_state_atom,  // atom
                         XA_ATOM,        // type
                         modal_atom);
  Window win(wm_.get(), xid);
  EXPECT_FALSE(win.wm_state_fullscreen());
  EXPECT_TRUE(win.wm_state_modal());

  // Now make the Window object handle a message removing the modal
  // state...
  XEvent event;
  MockXConnection::InitClientMessageEvent(
      &event,
      xid,            // window
      wm_state_atom,  // type
      0,              // arg1: remove
      modal_atom,     // arg2
      None,           // arg3
      None);          // arg4
  EXPECT_TRUE(win.HandleWmStateMessage(event.xclient));
  EXPECT_FALSE(win.wm_state_fullscreen());
  EXPECT_FALSE(win.wm_state_modal());

  // ... and one adding the fullscreen state.
  MockXConnection::InitClientMessageEvent(
      &event,
      xid,              // window
      wm_state_atom,    // type
      1,                // arg1: add
      fullscreen_atom,  // arg2
      None,             // arg3
      None);            // arg4
  EXPECT_TRUE(win.HandleWmStateMessage(event.xclient));
  EXPECT_TRUE(win.wm_state_fullscreen());
  EXPECT_FALSE(win.wm_state_modal());

  // Check that the window's _NET_WM_STATE property was updated in response
  // to the messages.
  std::vector<int> values;
  ASSERT_TRUE(xconn_->GetIntArrayProperty(xid, wm_state_atom, &values));
  ASSERT_EQ(1, values.size());
  EXPECT_EQ(fullscreen_atom, values[0]);

  // Test that we can toggle states (and that we process messages listing
  // multiple states correctly).
  MockXConnection::InitClientMessageEvent(
      &event,
      xid,              // window
      wm_state_atom,    // type
      2,                // arg1: toggle
      fullscreen_atom,  // arg2
      modal_atom,       // arg2
      None);            // arg4
  EXPECT_TRUE(win.HandleWmStateMessage(event.xclient));
  EXPECT_FALSE(win.wm_state_fullscreen());
  EXPECT_TRUE(win.wm_state_modal());

  values.clear();
  ASSERT_TRUE(xconn_->GetIntArrayProperty(xid, wm_state_atom, &values));
  ASSERT_EQ(1, values.size());
  EXPECT_EQ(modal_atom, values[0]);

  // Test that ChangeWmState() works for clearing the modal state and
  // setting both maximized states.
  std::vector<std::pair<XAtom, bool> > changed_states;
  changed_states.push_back(std::make_pair(modal_atom, false));
  changed_states.push_back(std::make_pair(max_horz_atom, true));
  changed_states.push_back(std::make_pair(max_vert_atom, true));
  EXPECT_TRUE(win.ChangeWmState(changed_states));
  values.clear();
  ASSERT_TRUE(xconn_->GetIntArrayProperty(xid, wm_state_atom, &values));
  ASSERT_EQ(2, values.size());
  EXPECT_EQ(max_horz_atom, values[0]);
  EXPECT_EQ(max_vert_atom, values[1]);
}

TEST_F(WindowTest, Shape) {
  // Create a shaped window.
  int width = 10;
  int height = 5;
  XWindow xid = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      10, 20,  // x, y
      width, height,
      false,   // override redirect
      false,   // input only
      0);      // event mask
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  info->shape.reset(new ByteMap(width, height));
  info->shape->Clear(0xff);
  info->shape->SetRectangle(0, 0, 3, 3, 0x0);

  Window win(wm_.get(), xid);
  EXPECT_TRUE(info->shape_events_selected);
  EXPECT_TRUE(win.shaped());
  EXPECT_TRUE(win.shadow() == NULL);

  // Set the opacity for the window's shadow (even though it doesn't have a
  // shadow right now).
  double shadow_opacity = 0.5;
  win.SetShadowOpacity(shadow_opacity, 0);  // anim_ms

  // Check that the shape mask got applied to the Clutter actor.
  MockClutterInterface::TexturePixmapActor* mock_actor =
      dynamic_cast<MockClutterInterface::TexturePixmapActor*>(win.actor());
  CHECK(mock_actor);
  ASSERT_TRUE(mock_actor->alpha_mask_bytes() != NULL);
  EXPECT_PRED_FORMAT3(BytesAreEqual,
                      info->shape->bytes(),
                      mock_actor->alpha_mask_bytes(),
                      width * height);

  // Change the shape and check that the window updates its actor.
  info->shape->Clear(0xff);
  info->shape->SetRectangle(width - 3, height - 3, 3, 3, 0x0);
  win.FetchAndApplyShape(true);  // update_shadow
  EXPECT_TRUE(win.shaped());
  EXPECT_TRUE(win.shadow() == NULL);
  ASSERT_TRUE(mock_actor->alpha_mask_bytes() != NULL);
  EXPECT_PRED_FORMAT3(BytesAreEqual,
                      info->shape->bytes(),
                      mock_actor->alpha_mask_bytes(),
                      width * height);

  // Now clear the shape and make sure that the mask is removed from the
  // actor.
  info->shape.reset();
  win.FetchAndApplyShape(true);  // update_shadow
  EXPECT_FALSE(win.shaped());
  EXPECT_TRUE(mock_actor->alpha_mask_bytes() == NULL);

  // The newly-created shadow should have the opacity that we set earlier.
  ASSERT_TRUE(win.shadow() != NULL);
  EXPECT_EQ(shadow_opacity, win.shadow()->opacity());
}

}  // namespace chromeos

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CommandLine::Init(argc, argv);
  logging::InitLogging(NULL,
                       FLAGS_logtostderr ?
                         logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG :
                         logging::LOG_NONE,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
