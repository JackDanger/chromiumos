// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_REAL_X_CONNECTION_H_
#define WINDOW_MANAGER_REAL_X_CONNECTION_H_

#include "window_manager/x_connection.h"

namespace window_manager {

// This wraps an actual connection to an X server.
class RealXConnection : public XConnection {
 public:
  explicit RealXConnection(Display* display);

  bool GetWindowGeometry(
      XWindow xid, int* x, int* y, int* width, int* height);
  bool MapWindow(XWindow xid);
  bool UnmapWindow(XWindow xid);
  bool MoveWindow(XWindow xid, int x, int y);
  bool ResizeWindow(XWindow xid, int width, int height);
  bool ConfigureWindow(XWindow xid, int x, int y, int width, int height);
  bool RaiseWindow(XWindow xid);
  bool FocusWindow(XWindow xid, Time event_time);
  bool StackWindow(XWindow xid, XWindow other, bool above);
  bool ReparentWindow(XWindow xid, XWindow parent, int x, int y);
  bool SetWindowBorderWidth(XWindow xid, int width);
  bool SelectInputOnWindow(XWindow xid, int event_mask, bool preserve_existing);
  bool DeselectInputOnWindow(XWindow xid, int event_mask);
  bool AddPassiveButtonGrabOnWindow(XWindow xid, int button, int event_mask);
  bool RemovePassiveButtonGrabOnWindow(XWindow xid, int button);
  bool AddActivePointerGrabForWindow(
      XWindow xid, int event_mask, Time timestamp);
  bool RemoveActivePointerGrab(bool replay_events);
  bool RemoveInputRegionFromWindow(XWindow xid);
  bool GetSizeHintsForWindow(
      XWindow xid, XSizeHints* hints, long* supplied_hints);
  bool GetTransientHintForWindow(XWindow xid, XWindow* owner_out);
  bool GetWindowAttributes(XWindow xid, XWindowAttributes* attr_out);
  bool RedirectWindowForCompositing(XWindow xid);
  bool UnredirectWindowForCompositing(XWindow xid);
  XWindow GetCompositingOverlayWindow(XWindow root);
  XWindow GetRootWindow() { return root_; }
  XWindow CreateWindow(XWindow parent, int x, int y, int width, int height,
                       bool override_redirect, bool input_only, int event_mask);
  bool DestroyWindow(XWindow xid);
  bool IsWindowShaped(XWindow xid);
  bool SelectShapeEventsOnWindow(XWindow xid);
  bool GetWindowBoundingRegion(XWindow xid, ByteMap* bytemap);
  bool SelectXRandREventsOnWindow(XWindow xid);
  bool GetAtom(const std::string& name, XAtom* atom_out);
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
  bool WaitForEvent(XWindow xid, int event_mask, XEvent* event_out);
  XWindow GetSelectionOwner(XAtom atom);
  bool SetSelectionOwner(XAtom atom, XWindow xid, Time timestamp);
  bool SetWindowCursor(XWindow xid, uint32 shape);
  bool GetChildWindows(XWindow xid, std::vector<XWindow>* children_out);
  bool GetParentWindow(XWindow xid, XWindow* parent);
  KeySym GetKeySymFromKeyCode(uint32 keycode);
  uint32 GetKeyCodeFromKeySym(KeySym keysym);
  std::string GetStringFromKeySym(KeySym keysym);
  bool GrabKey(KeyCode keycode, uint32 modifiers);
  bool UngrabKey(KeyCode keycode, uint32 modifiers);
  bool SetDetectableKeyboardAutoRepeat(bool detectable);

 private:
  bool GrabServerImpl();
  bool UngrabServerImpl();

  // Install a custom error handler so we don't crash if we receive an
  // error from the X server.  Calls to TrapErrors() cannot be nested.
  void TrapErrors();

  // Remove the custom error handler, restoring the previously-installed
  // handler.  Returns the last-received error code or 0 if no errors were
  // received.
  int UntrapErrors();

  // Get the code of the last error since TrapErrors() was called.
  int GetLastErrorCode();

  // Get a string describing an error code.
  std::string GetErrorText(int error_code);

  // The actual connection to the X server.
  Display* display_;

  // The root window.
  XWindow root_;

  // ID for the UTF8_STRING atom (we look this up ourselves so as to avoid
  // a circular dependency with AtomCache).
  XAtom utf8_string_atom_;

  DISALLOW_COPY_AND_ASSIGN(RealXConnection);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_REAL_X_CONNECTION_H_
