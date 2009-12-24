// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_REAL_X_CONNECTION_H_
#define WINDOW_MANAGER_REAL_X_CONNECTION_H_

extern "C" {
#include <xcb/xcb.h>
}

#include "window_manager/x_connection.h"

namespace window_manager {

// This wraps an actual connection to an X server.
class RealXConnection : public XConnection {
 public:
  explicit RealXConnection(Display* display);
  virtual ~RealXConnection();

  bool GetWindowGeometry(XWindow xid, WindowGeometry* geom_out);
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
  bool AddPassiveButtonGrabOnWindow(
      XWindow xid, int button, int event_mask, bool synchronous);
  bool RemovePassiveButtonGrabOnWindow(XWindow xid, int button);
  bool AddActivePointerGrabForWindow(
      XWindow xid, int event_mask, Time timestamp);
  bool RemoveActivePointerGrab(bool replay_events, Time timestamp);
  bool RemoveInputRegionFromWindow(XWindow xid);
  bool GetSizeHintsForWindow(XWindow xid, SizeHints* hints_out);
  bool GetTransientHintForWindow(XWindow xid, XWindow* owner_out);
  bool GetWindowAttributes(XWindow xid, WindowAttributes* attr_out);
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
  bool WaitForEvent(XWindow xid, int event_mask, XEvent* event_out);
  XWindow GetSelectionOwner(XAtom atom);
  bool SetSelectionOwner(XAtom atom, XWindow xid, Time timestamp);
  bool SetWindowCursor(XWindow xid, uint32 shape);
  bool GetChildWindows(XWindow xid, std::vector<XWindow>* children_out);
  KeySym GetKeySymFromKeyCode(uint32 keycode);
  uint32 GetKeyCodeFromKeySym(KeySym keysym);
  std::string GetStringFromKeySym(KeySym keysym);
  bool GrabKey(KeyCode keycode, uint32 modifiers);
  bool UngrabKey(KeyCode keycode, uint32 modifiers);
  bool SetDetectableKeyboardAutoRepeat(bool detectable);
  bool QueryKeyboardState(std::vector<uint8_t>* keycodes_out);

 private:
  bool GrabServerImpl();
  bool UngrabServerImpl();

  // Ask the server for information about an extension.  Out params may be
  // NULL.  Returns false if the extension isn't present.
  bool QueryExtensionInternal(const std::string& name,
                              int* first_event_out,
                              int* first_error_out);

  // Read a property set on a window.  Returns false on error or if the
  // property isn't set.  'format_out' and 'type_out' may be NULL.
  bool GetPropertyInternal(XWindow xid,
                           XAtom xatom,
                           std::string* value_out,
                           int* format_out,
                           XAtom* type_out);

  // Get the font cursor with the given ID, loading it if necessary.
  xcb_cursor_t GetCursorInternal(uint32 shape);

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

  // Check for an error caused by the XCB request using the passed-in
  // cookie.  If found, logs a warning of the form "Got XCB error while
  // [format]", with additional arguments printf-ed into 'format', and
  // returns false.
  bool CheckForXcbError(xcb_void_cookie_t cookie, const char* format, ...);

  // The actual connection to the X server.
  Display* display_;

  // XCB's representation of the connection to the X server.
  xcb_connection_t* xcb_conn_;

  // The root window.
  XWindow root_;

  // ID for the UTF8_STRING atom (we look this up ourselves so as to avoid
  // a circular dependency with AtomCache).
  XAtom utf8_string_atom_;

  // Map from cursor shapes to their XIDs.
  std::map<uint32, xcb_cursor_t> cursors_;

  DISALLOW_COPY_AND_ASSIGN(RealXConnection);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_REAL_X_CONNECTION_H_
