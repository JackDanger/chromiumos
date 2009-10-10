// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __PLATFORM_WINDOW_MANAGER_MOCK_X_CONNECTION_H__
#define __PLATFORM_WINDOW_MANAGER_MOCK_X_CONNECTION_H__

#include "window_manager/x_connection.h"

namespace chromeos {

// This is a fake implementation of a connection to an X server.
class MockXConnection : public XConnection {
 public:
  MockXConnection();
  ~MockXConnection();

  bool GetWindowGeometry(XWindow xid, int* x, int* y, int* width, int* height);
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
  bool AddPassiveButtonGrabOnWindow(XWindow xid, int button, int event_mask);
  bool RemovePassiveButtonGrabOnWindow(XWindow xid, int button);
  bool AddActivePointerGrabForWindow(
      XWindow xid, int event_mask, Time timestamp);
  bool RemoveActivePointerGrab(bool replay_events);
  bool RemoveInputRegionFromWindow(XWindow xid) { return true; }
  bool GetSizeHintsForWindow(
      XWindow xid, XSizeHints* hints, long* supplied_hints);
  bool GetTransientHintForWindow(XWindow xid, XWindow* owner_out);
  bool GetWindowAttributes(XWindow xid, XWindowAttributes* attr_out);
  bool RedirectWindowForCompositing(XWindow xid);
  bool UnredirectWindowForCompositing(XWindow xid);
  XWindow GetCompositingOverlayWindow(XWindow root) { return overlay_; }
  XWindow GetRootWindow() { return root_; }
  XWindow CreateWindow(XWindow parent, int x, int y, int width, int height,
                       bool override_redirect, bool input_only, int event_mask);
  bool DestroyWindow(XWindow xid);
  bool IsWindowShaped(XWindow xid);
  bool SelectShapeEventsOnWindow(XWindow xid);
  bool GetWindowBoundingRegion(XWindow xid, ByteMap* bytemap);
  bool GetAtom(const string& name, XAtom* atom_out);
  bool GetAtoms(const vector<string>& names, vector<XAtom>* atoms_out);
  bool GetAtomName(XAtom atom, string* name);
  bool GetIntArrayProperty(XWindow xid, XAtom xatom, vector<int>* values);
  bool SetIntArrayProperty(
      XWindow xid, XAtom xatom, XAtom type, const vector<int>& values);
  bool GetStringProperty(XWindow xid, XAtom xatom, string* out) { return true; }
  bool SetStringProperty(XWindow xid, XAtom xatom, const string& value) {
    return true;
  }
  bool DeletePropertyIfExists(XWindow xid, XAtom xatom);
  bool SendEvent(XWindow xid, XEvent* event, int event_mask);
  bool WaitForEvent(XWindow xid, int event_mask, XEvent* event_out) {
    return true;
  }
  XWindow GetSelectionOwner(XAtom atom);
  bool SetSelectionOwner(XAtom atom, XWindow xid, Time timestamp);
  bool SetWindowCursor(XWindow xid, uint32 shape);
  bool GetChildWindows(XWindow xid, vector<XWindow>* children_out);
  bool GetParentWindow(XWindow xid, XWindow* parent);
  // Treat keycodes and keysyms as equivalent for key_bindings_test.
  KeySym GetKeySymFromKeyCode(uint32 keycode) { return keycode; }
  uint32 GetKeyCodeFromKeySym(KeySym keysym) { return keysym; }
  string GetStringFromKeySym(KeySym keysym) { return ""; }
  bool GrabKey(KeyCode keycode, uint32 modifiers) { return true; }
  bool UngrabKey(KeyCode keycode, uint32 modifiers) { return true; }
  bool SetDetectableKeyboardAutoRepeat(bool detectable) { return true; }

  // Testing-specific code.
  struct WindowInfo {
    WindowInfo(XWindow xid, XWindow parent);
    ~WindowInfo();

    XWindow xid;
    XWindow parent;
    int x, y;
    int width, height;
    bool mapped;
    bool override_redirect;
    bool redirected;
    int event_mask;
    map<XAtom, vector<int> > int_properties;
    XWindow transient_for;
    uint32 cursor;
    XSizeHints size_hints;

    // Window's shape, if it's been shaped using the shape extension.
    // NULL otherwise.
    scoped_ptr<ByteMap> shape;

    // Have shape events been selected using SelectShapeEventsOnWindow()?
    bool shape_events_selected;

    // Client messages sent to the window.
    vector<XClientMessageEvent> client_messages;

    // Has the window has been mapped, unmapped, or configured via XConnection
    // methods?  Used to check that changes aren't made to override-redirect
    // windows.
    bool changed;

    // Have all of the mouse buttons been passively grabbed?
    bool all_buttons_grabbed;

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
  // The 'mode' parameter is e.g. NotifyNormal, NotifyGrab, etc.
  static void InitFocusInEvent(XEvent* event, XWindow xid, int mode);
  static void InitFocusOutEvent(XEvent* event, XWindow xid, int mode);
  static void InitMapEvent(XEvent* event, XWindow xid);
  static void InitUnmapEvent(XEvent* event, XWindow xid);

 private:
  bool GrabServerImpl() { return true; }
  bool UngrabServerImpl() { return true; }

  map<XWindow, ref_ptr<WindowInfo> > windows_;

  // All windows other than the overlay and root, in top-to-bottom stacking
  // order.
  scoped_ptr<Stacker<XWindow> > stacked_xids_;

  XWindow next_window_;

  XWindow root_;
  XWindow overlay_;
  XAtom next_atom_;
  map<string, XAtom> name_to_atom_;
  map<XAtom, string> atom_to_name_;
  map<XAtom, XWindow> selection_owners_;
  XWindow focused_xid_;

  // Window that has currently grabbed the pointer, or None.
  XWindow pointer_grab_xid_;

  DISALLOW_COPY_AND_ASSIGN(MockXConnection);
};

}  // namespace chromeos

#endif
