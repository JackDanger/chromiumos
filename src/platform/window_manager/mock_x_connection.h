// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_MOCK_X_CONNECTION_H_
#define WINDOW_MANAGER_MOCK_X_CONNECTION_H_

#include <map>
#include <set>
#include <string>
#include <tr1/memory>
#include <utility>

#include "base/logging.h"

#include "window_manager/x_connection.h"

namespace window_manager {

// This is a fake implementation of a connection to an X server.
class MockXConnection : public XConnection {
 public:
  MockXConnection();
  ~MockXConnection();

  void Free(void* item) {}
  XVisualInfo* GetVisualInfo(long mask,
                             XVisualInfo* visual_template,
                             int* item_count);
  bool GetWindowGeometry(XWindow xid, WindowGeometry* geom_out);
  bool MapWindow(XWindow xid);
  bool UnmapWindow(XWindow xid);
  bool MoveWindow(XWindow xid, int x, int y);
  bool ResizeWindow(XWindow xid, int width, int height);
  bool ConfigureWindow(XWindow xid, int x, int y, int width, int height) {
    return (MoveWindow(xid, x, y) && ResizeWindow(xid, width, height));
  }
  bool RaiseWindow(XWindow xid);
  bool FocusWindow(XWindow xid, Time event_time);
  bool StackWindow(XWindow xid, XWindow other, bool above);
  bool ReparentWindow(XWindow xid, XWindow parent, int x, int y) {
    return true;
  }
  bool SetWindowBorderWidth(XWindow xid, int width) { return true; }
  bool SelectInputOnWindow(XWindow xid, int event_mask, bool preserve_existing);
  bool DeselectInputOnWindow(XWindow xid, int event_mask);
  bool AddButtonGrabOnWindow(
      XWindow xid, int button, int event_mask, bool synchronous);
  bool RemoveButtonGrabOnWindow(XWindow xid, int button);
  bool AddPointerGrabForWindow(XWindow xid, int event_mask, Time timestamp);
  bool RemovePointerGrab(bool replay_events, Time timestamp);
  bool RemoveInputRegionFromWindow(XWindow xid) { return true; }
  bool GetSizeHintsForWindow(XWindow xid, SizeHints* hints_out);
  bool GetTransientHintForWindow(XWindow xid, XWindow* owner_out);
  bool GetWindowAttributes(XWindow xid, WindowAttributes* attr_out);
  bool RedirectWindowForCompositing(XWindow xid);
  bool UnredirectWindowForCompositing(XWindow xid);
  XWindow GetCompositingOverlayWindow(XWindow root) { return overlay_; }
  XPixmap GetCompositingPixmapForWindow(XWindow window) { return None; }
  bool FreePixmap(XPixmap pixmap) { return true; }
  XWindow GetRootWindow() { return root_; }
  XWindow CreateWindow(XWindow parent, int x, int y, int width, int height,
                       bool override_redirect, bool input_only, int event_mask);
  bool DestroyWindow(XWindow xid);
  bool IsWindowShaped(XWindow xid);
  bool SelectShapeEventsOnWindow(XWindow xid);
  bool GetWindowBoundingRegion(XWindow xid, ByteMap* bytemap);
  bool SelectRandREventsOnWindow(XWindow xid);
  bool GetAtoms(const std::vector<std::string>& names,
                std::vector<XAtom>* atoms_out);
  bool GetAtomName(XAtom atom, std::string* name);
  bool GetIntArrayProperty(XWindow xid, XAtom xatom, std::vector<int>* values);
  bool SetIntArrayProperty(
      XWindow xid, XAtom xatom, XAtom type, const std::vector<int>& values);
  bool GetStringProperty(XWindow xid, XAtom xatom, std::string* out);
  bool SetStringProperty(XWindow xid, XAtom xatom, const std::string& value);
  bool DeletePropertyIfExists(XWindow xid, XAtom xatom);
  bool SendEvent(XWindow xid, XEvent* event, int event_mask);
  bool WaitForEvent(XWindow xid, int event_mask, XEvent* event_out) {
    return true;
  }
  XWindow GetSelectionOwner(XAtom atom);
  bool SetSelectionOwner(XAtom atom, XWindow xid, Time timestamp);
  bool SetWindowCursor(XWindow xid, uint32 shape);
  bool GetChildWindows(XWindow xid, std::vector<XWindow>* children_out);
  // Treat keycodes and keysyms as equivalent for key_bindings_test.
  KeySym GetKeySymFromKeyCode(uint32 keycode) { return keycode; }
  uint32 GetKeyCodeFromKeySym(KeySym keysym) { return keysym; }
  std::string GetStringFromKeySym(KeySym keysym) { return ""; }
  bool GrabKey(KeyCode keycode, uint32 modifiers);
  bool UngrabKey(KeyCode keycode, uint32 modifiers);
  XDamage CreateDamage(XDrawable drawable, int level) { return None; }
  void DestroyDamage(XDamage damage) {}
  void SubtractRegionFromDamage(XDamage damage, XserverRegion repair,
                                XserverRegion parts) {}
  bool SetDetectableKeyboardAutoRepeat(bool detectable) { return true; }
  bool QueryKeyboardState(std::vector<uint8_t>* keycodes_out) { return true; }
  bool QueryPointerPosition(int* x_root, int* y_root);

  // Testing-specific code.
  struct WindowInfo {
    WindowInfo(XWindow xid, XWindow parent);
    ~WindowInfo();

    // Information about a button grab installed on this window.
    struct ButtonGrabInfo {
      ButtonGrabInfo() : event_mask(0), synchronous(false) {}
      ButtonGrabInfo(int event_mask, bool synchronous)
          : event_mask(event_mask),
            synchronous(synchronous) {
      }
      int event_mask;
      bool synchronous;
    };

    // Convenience method to check whether a particular button is grabbed.
    bool button_is_grabbed(int button) {
      return button_grabs.find(button) != button_grabs.end();
    }

    XWindow xid;
    XWindow parent;
    int x, y;
    int width, height;
    int border_width;
    int depth;
    bool mapped;
    bool override_redirect;
    bool input_only;
    bool redirected;
    int event_mask;
    std::map<XAtom, std::vector<int> > int_properties;
    std::map<XAtom, std::string> string_properties;
    XWindow transient_for;
    uint32 cursor;
    XConnection::SizeHints size_hints;

    // Window's shape, if it's been shaped using the shape extension.
    // NULL otherwise.
    scoped_ptr<ByteMap> shape;

    // Have various extension events been selected using
    // Select*EventsOnWindow()?
    bool shape_events_selected;
    bool randr_events_selected;

    // Client messages sent to the window.
    std::vector<XClientMessageEvent> client_messages;

    // Has the window has been mapped, unmapped, or configured via XConnection
    // methods?  Used to check that changes aren't made to override-redirect
    // windows.
    bool changed;

    // Information about button grabs installed on this window, keyed by
    // button.
    std::map<int, ButtonGrabInfo> button_grabs;

   private:
    DISALLOW_COPY_AND_ASSIGN(WindowInfo);
  };

  WindowInfo* GetWindowInfo(XWindow xid);

  WindowInfo* GetWindowInfoOrDie(XWindow xid) {
    WindowInfo* info = GetWindowInfo(xid);
    CHECK(info);
    return info;
  }

  XWindow focused_xid() const { return focused_xid_; }
  XWindow pointer_grab_xid() const { return pointer_grab_xid_; }

  bool KeyIsGrabbed(KeyCode keycode, uint32 modifiers) {
    return grabbed_keys_.count(std::make_pair(keycode, modifiers)) > 0;
  }

  const Stacker<XWindow>& stacked_xids() const {
    return *(stacked_xids_.get());
  }

  // Set the pointer position for QueryPointerPosition().
  void SetPointerPosition(int x, int y) {
    pointer_x_ = x;
    pointer_y_ = y;
  }

  // Set a window as having an active pointer grab.  This is handy when
  // simulating a passive button grab being upgraded due to a button press.
  void set_pointer_grab_xid(XWindow xid) {
    pointer_grab_xid_ = xid;
  }

  // Helper methods tests can use to initialize events.
  static void InitButtonPressEvent(XEvent* event, XWindow xid,
                                   int x, int y, int button);
  // This just creates a message with 32-bit values.
  static void InitClientMessageEvent(
      XEvent* event, XWindow xid, XAtom type,
      long arg1, long arg2, long arg3, long arg4);
  static void InitConfigureNotifyEvent(XEvent* event, const WindowInfo& info);
  static void InitConfigureRequestEvent(
      XEvent* event, XWindow xid, int x, int y, int width, int height);
  static void InitCreateWindowEvent(XEvent* event, const WindowInfo& info);
  static void InitDestroyWindowEvent(XEvent* event, XWindow xid);
  static void InitEnterWindowEvent(XEvent* event, XWindow window);
  // The 'mode' parameter is e.g. NotifyNormal, NotifyGrab, etc., and
  // 'detail' is e.g. NotifyAncestor, NotifyVirtual, etc.  See
  // http://tronche.com/gui/x/xlib/events/input-focus/normal-and-grabbed.html
  // for more information about this.
  static void InitFocusInEvent(
      XEvent* event, XWindow xid, int mode, int detail);
  static void InitFocusOutEvent(
      XEvent* event, XWindow xid, int mode, int detail);
  static void InitLeaveWindowEvent(XEvent* event, XWindow window);
  static void InitMapEvent(XEvent* event, XWindow xid);
  static void InitMapRequestEvent(XEvent* event, const WindowInfo& info);
  static void InitUnmapEvent(XEvent* event, XWindow xid);

 private:
  bool GrabServerImpl() { return true; }
  bool UngrabServerImpl() { return true; }

  std::map<XWindow, std::tr1::shared_ptr<WindowInfo> > windows_;

  // All windows other than the overlay and root, in top-to-bottom stacking
  // order.
  scoped_ptr<Stacker<XWindow> > stacked_xids_;

  XWindow next_window_;

  XWindow root_;
  XWindow overlay_;
  XAtom next_atom_;
  std::map<std::string, XAtom> name_to_atom_;
  std::map<XAtom, std::string> atom_to_name_;
  std::map<XAtom, XWindow> selection_owners_;
  XWindow focused_xid_;

  // Window that has currently grabbed the pointer, or None.
  XWindow pointer_grab_xid_;

  // Keys that have been grabbed (pairs are key codes and modifiers).
  std::set<std::pair<KeyCode, uint32> > grabbed_keys_;

  // Current position of the mouse pointer for QueryPointerPosition().
  int pointer_x_;
  int pointer_y_;

  DISALLOW_COPY_AND_ASSIGN(MockXConnection);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_MOCK_X_CONNECTION_H_
