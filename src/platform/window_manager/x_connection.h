// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __PLATFORM_WINDOW_MANAGER_X_CONNECTION_H__
#define __PLATFORM_WINDOW_MANAGER_X_CONNECTION_H__

#include <map>
#include <string>
#include <vector>

extern "C" {
#include <X11/Xlib.h>
#include <X11/Xutil.h>
}

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

namespace chromeos {

typedef ::Atom XAtom;
typedef ::Window XWindow;

class ByteMap;  // from util.h
template<class T> class Stacker;  // from util.h

// This is an abstract base class representing a connection to the X
// server.
class XConnection {
 public:
  XConnection()
      : shape_event_base_(0),
        xrandr_event_base_(0),
        server_grabbed_(false) {
  }
  virtual ~XConnection() {}

  // Get the base event ID for extension events.
  int shape_event_base() const { return shape_event_base_; }
  int xrandr_event_base() const { return xrandr_event_base_; }

  // Get the position and dimensions of a window.
  // Unwanted parameters can be NULL.
  virtual bool GetWindowGeometry(
      XWindow xid, int* x, int* y, int* width, int* height) = 0;

  // Map or unmap a window.
  virtual bool MapWindow(XWindow xid) = 0;
  virtual bool UnmapWindow(XWindow xid) = 0;

  // Move or resize a window.  'width' and 'height' must be positive.
  virtual bool MoveWindow(XWindow xid, int x, int y) = 0;
  virtual bool ResizeWindow(XWindow xid, int width, int height) = 0;
  virtual bool ConfigureWindow(
      XWindow xid, int x, int y, int width, int height) = 0;

  // Configure a window to be 1x1 and offscreen.
  virtual bool ConfigureWindowOffscreen(XWindow xid) {
    return ConfigureWindow(xid, -1, -1, 1, 1);
  }

  // Raise a window on top of all other windows.
  virtual bool RaiseWindow(XWindow xid) = 0;

  // Stack a window directly above or below another window.
  virtual bool StackWindow(XWindow xid, XWindow other, bool above) = 0;

  // Give keyboard focus to a window.  'event_time' should be the
  // server-supplied time of the event that caused the window to be
  // focused.
  virtual bool FocusWindow(XWindow xid, Time event_time) = 0;

  // Reparent a window in another window.
  virtual bool ReparentWindow(XWindow xid, XWindow parent, int x, int y) = 0;

  // Set the width of a window's border.
  virtual bool SetWindowBorderWidth(XWindow xid, int width) = 0;

  // Select input events on a window.  If 'preserve_existing' is true, the
  // existing input selection for the window will be preserved.
  virtual bool SelectInputOnWindow(XWindow xid,
                                   int event_mask,
                                   bool preserve_existing) = 0;

  // Deselect certain input events on a window.
  virtual bool DeselectInputOnWindow(XWindow xid, int event_mask) = 0;

  // Grab the server, preventing other clients from communicating with it.
  // These methods invoke GrabServerImpl() and UngrabServerImpl().
  bool GrabServer();
  bool UngrabServer();

  // Install a passive button grab on a window.  When the specified button
  // is pressed, a synchronous active pointer grab will begin.
  virtual bool AddPassiveButtonGrabOnWindow(
      XWindow xid, int button, int event_mask) = 0;

  // Uninstall a passive button grab.
  virtual bool RemovePassiveButtonGrabOnWindow(XWindow xid, int button) = 0;

  // Actively grab the pointer.  Returns false if an error occurs or if the
  // grab fails (e.g. because it's already grabbed by another client).
  virtual bool AddActivePointerGrabForWindow(
      XWindow xid, int event_mask, Time timestamp) = 0;

  // Replay the pointer events that occurred during the current synchronous
  // active pointer grab (sending them to the original window instead of
  // just to the grabbing client) and remove the active grab.
  virtual bool RemoveActivePointerGrab(bool replay_events) = 0;

  // Remove the input region from a window, so that events fall through it.
  virtual bool RemoveInputRegionFromWindow(XWindow xid) = 0;

  // Get the size hints for a window.
  virtual bool GetSizeHintsForWindow(
      XWindow xid, XSizeHints* hints_out, long* supplied_hints_out) = 0;

  // Get the transient-for hint for a window.
  virtual bool GetTransientHintForWindow(XWindow xid, XWindow* owner_out) = 0;

  // Get a window's attributes.
  virtual bool GetWindowAttributes(
      XWindow xid, XWindowAttributes* attr_out) = 0;

  // Redirect the window to an offscreen pixmap so it can be composited.
  virtual bool RedirectWindowForCompositing(XWindow xid) = 0;

  // Undo a previous call to RedirectWindowForCompositing().  This is
  // useful when a plugin window gets reparented away from the root and we
  // realize that we won't need to composite it after all.
  virtual bool UnredirectWindowForCompositing(XWindow xid) = 0;

  // Get the overlay window.  (XComposite provides a window that is stacked
  // below the screensaver window but above all other windows).
  virtual XWindow GetCompositingOverlayWindow(XWindow root) = 0;

  virtual XWindow GetRootWindow() = 0;

  // Create a new override-redirect window.  'width' and 'height' must be
  // positive.  'event_mask' determines which events the window receives;
  // it takes values from the "Input Event Masks" section of X.h.  The
  // window is a child of 'parent'.
  virtual XWindow CreateWindow(
      XWindow parent,
      int x, int y,
      int width, int height,
      bool override_redirect,
      bool input_only,
      int event_mask) = 0;

  // Destroy a window.
  virtual bool DestroyWindow(XWindow xid) = 0;

  // Has a window's bounding region been shaped using the Shape extension?
  virtual bool IsWindowShaped(XWindow xid) = 0;

  // Select ShapeNotify events on a window.
  virtual bool SelectShapeEventsOnWindow(XWindow xid) = 0;

  // Get the rectangles defining a window's bounding region.
  virtual bool GetWindowBoundingRegion(XWindow xid, ByteMap* bytemap) = 0;

  // Select XRandR events on a window.
  virtual bool SelectXRandREventsOnWindow(XWindow xid) = 0;

  // Look up the X ID for a single atom, creating it if necessary.
  virtual bool GetAtom(const std::string& name, XAtom* atom_out) = 0;

  // Look up all of the atoms in 'names' in the X server, creating them if
  // necessary, and return the corresponding atom X IDs.
  virtual bool GetAtoms(const std::vector<std::string>& names,
                        std::vector<XAtom>* atoms_out) = 0;

  // Get the name of the passed-in atom, saving it to 'name'.  Returns
  // false if the atom isn't present in the server.
  virtual bool GetAtomName(XAtom atom, std::string* name) = 0;

  // Get or set a property consisting of a single 32-bit integer.
  // Calls the corresponding abstract {Get,Set}IntArrayProperty() method.
  bool GetIntProperty(XWindow xid, XAtom xatom, int* value);
  bool SetIntProperty(XWindow xid, XAtom xatom, XAtom type, int value);

  // Get or set a property consisting of one or more 32-bit integers.
  virtual bool GetIntArrayProperty(
      XWindow xid, XAtom xatom, std::vector<int>* values) = 0;
  virtual bool SetIntArrayProperty(
      XWindow xid, XAtom xatom, XAtom type, const std::vector<int>& values) = 0;

  // Get or set a string property (of type STRING or UTF8_STRING when
  // getting and UTF8_STRING when setting).
  virtual bool GetStringProperty(
      XWindow xid, XAtom xatom, std::string* out) = 0;
  virtual bool SetStringProperty(
      XWindow xid, XAtom xatom, const std::string& value) = 0;

  // Delete a property on a window if it exists.
  virtual bool DeletePropertyIfExists(XWindow xid, XAtom xatom) = 0;

  // Send an event to a window.  If 'event_mask' is 0, the event is sent to
  // the client that created the window; otherwise the event is sent to all
  // clients selecting any of the event types included in the mask.
  virtual bool SendEvent(XWindow xid, XEvent* event, int event_mask) = 0;

  // Search the event queue for a particular type of event for the
  // passed-in window, and then remove and return the event.  Blocks if a
  // matching event hasn't yet been received.
  virtual bool WaitForEvent(XWindow xid, int event_mask, XEvent* event_out) = 0;

  // Get the window owning the passed-in selection, or set the owner for a
  // selection.
  virtual XWindow GetSelectionOwner(XAtom atom) = 0;
  virtual bool SetSelectionOwner(XAtom atom, XWindow xid, Time timestamp) = 0;

  // Change the cursor for a window.  'shape' is a definition from
  // Xlib's cursorfont.h header.
  virtual bool SetWindowCursor(XWindow xid, uint32 shape) = 0;

  // Get all subwindows of a window in bottom-to-top stacking order.
  virtual bool GetChildWindows(
      XWindow xid, std::vector<XWindow>* children_out) = 0;

  // Get a window's parent.
  virtual bool GetParentWindow(XWindow xid, XWindow* parent) = 0;

  // Convert between keysyms and keycodes.
  // Keycodes fit inside of unsigned 8-bit values, but some of the testing
  // code relies on keycodes and keysyms being interchangeable, so we use
  // 32-bit values here instead.
  virtual KeySym GetKeySymFromKeyCode(uint32 keycode) = 0;
  virtual uint32 GetKeyCodeFromKeySym(KeySym keysym) = 0;

  // Get the string representation of a keysym.  Returns the empty string
  // for unknown keysyms.
  virtual std::string GetStringFromKeySym(KeySym keysym) = 0;

  // Grab or ungrab a key combination.
  virtual bool GrabKey(KeyCode keycode, uint32 modifiers) = 0;
  virtual bool UngrabKey(KeyCode keycode, uint32 modifiers) = 0;

  // When auto-repeating a key combo, the X Server may send:
  //   KeyPress   @ time_0    <-- Key pressed down
  //   KeyRelease @ time_1    <-- First auto-repeat
  //   KeyPress   @ time_1    <-- First auto-repeat, cont.
  //   KeyRelease @ time_2    <-- Key released
  //
  // Calling XkbSetDetectableAutorepeat() changes this behavior for this
  // client only to:
  //   KeyPress   @ time_0    <-- Key pressed down
  //   KeyPress   @ time_1    <-- First auto-repeat
  //   KeyRelease @ time_2    <-- Key released
  //
  // This clears up the problem with mis-reporting an auto-repeat key
  // release as an actual key release. Thanks!: http://wiki.tcl.tk/20299
  // TODO(tedbo): I lied. On the HP Mini 1000, for some reason it does
  // *not* clear up the issue. On my gHardy desktop it does work.
  // Why!!!!!!???
  virtual bool SetDetectableKeyboardAutoRepeat(bool detectable) = 0;

  // Value that should be used in event and property 'format' fields for
  // byte and long arguments.
  static const int kByteFormat;
  static const int kLongFormat;

 protected:
  // Base IDs for extension events.  Implementations should initialize
  // these in their constructors.
  int shape_event_base_;
  int xrandr_event_base_;

 private:
  virtual bool GrabServerImpl() = 0;
  virtual bool UngrabServerImpl() = 0;

  bool server_grabbed_;

  DISALLOW_COPY_AND_ASSIGN(XConnection);
};

};  // namespace chromeos

#endif
