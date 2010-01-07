// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_X_CONNECTION_H_
#define WINDOW_MANAGER_X_CONNECTION_H_

#include <map>
#include <string>
#include <vector>

extern "C" {
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>
}

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"

namespace window_manager {

typedef ::Atom XAtom;
typedef ::Damage XDamage;
typedef ::Display XDisplay;
typedef ::Drawable XDrawable;
typedef ::Pixmap XPixmap;
typedef ::Visual XVisual;
typedef ::VisualID XVisualID;
typedef ::Window XWindow;

class ByteMap;  // from util.h
template<class T> class Stacker;  // from util.h

// This is an abstract base class representing a connection to the X
// server.
class XConnection {
 public:
  XConnection()
      : shape_event_base_(0),
        randr_event_base_(0),
        server_grabbed_(false) {
  }
  virtual ~XConnection() {}

  virtual void Free(void* item) = 0;

  // Caller assumes ownership of the memory returned from this
  // function which must be freed by calling Free(), above.
  virtual XVisualInfo* GetVisualInfo(long mask,
                                     XVisualInfo* visual_template,
                                     int* item_count) = 0;

  // Get the base event ID for extension events.
  int damage_event_base() const { return damage_event_base_; }
  int shape_event_base() const { return shape_event_base_; }
  int randr_event_base() const { return randr_event_base_; }

  // Data returned by GetWindowGeometry().
  struct WindowGeometry {
    WindowGeometry()
        : x(0),
          y(0),
          width(1),
          height(1),
          border_width(0),
          depth(0) {
    }

    int x;
    int y;
    int width;
    int height;
    int border_width;
    int depth;
  };

  // Get a window's geometry.
  virtual bool GetWindowGeometry(XWindow xid, WindowGeometry* geom_out) = 0;

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
  // is pressed, an active pointer grab will be installed.  Only events
  // matched by 'event_mask' will be reported.  If 'synchronous' is false,
  // when all of the buttons are released, the pointer grab will be
  // automatically removed.  If 'synchronous' is true, no further pointer
  // events will be reported until the the pointer grab is manually removed
  // using RemovePointerGrab() -- this is useful in conjunction with
  // RemovePointerGrab()'s 'replay_events' parameter to send initial clicks
  // to client apps when implementing click-to-focus behavior.
  virtual bool AddButtonGrabOnWindow(
      XWindow xid, int button, int event_mask, bool synchronous) = 0;

  // Uninstall a passive button grab.
  virtual bool RemoveButtonGrabOnWindow(XWindow xid, int button) = 0;

  // Grab the pointer asynchronously, such that all subsequent events
  // matching 'event_mask' will be reported to the calling client.  Returns
  // false if an error occurs or if the grab fails (e.g. because it's
  // already grabbed by another client).
  virtual bool AddPointerGrabForWindow(
      XWindow xid, int event_mask, Time timestamp) = 0;

  // Remove a pointer grab, possibly also replaying the pointer events that
  // occurred during it if it was synchronous and 'replay_events' is true
  // (sending them to the original window instead of just to the grabbing
  // client).
  virtual bool RemovePointerGrab(bool replay_events, Time timestamp) = 0;

  // Remove the input region from a window, so that events fall through it.
  virtual bool RemoveInputRegionFromWindow(XWindow xid) = 0;

  // Data returned by GetSizeHintsForWindow().
  struct SizeHints {
    SizeHints() {
      Reset();
    }

    // Reset all of the hints to -1.
    void Reset() {
      width = -1;
      height = -1;
      min_width = -1;
      min_height = -1;
      max_width = -1;
      max_height = -1;
      width_increment = -1;
      height_increment = -1;
      min_aspect_x = -1;
      min_aspect_y = -1;
      max_aspect_x = -1;
      max_aspect_y = -1;
      base_width = -1;
      base_height = -1;
      win_gravity = -1;
    }

    // Hints are set to -1 if not defined.
    int width;
    int height;
    int min_width;
    int min_height;
    int max_width;
    int max_height;
    int width_increment;
    int height_increment;
    int min_aspect_x;
    int min_aspect_y;
    int max_aspect_x;
    int max_aspect_y;
    int base_width;
    int base_height;
    int win_gravity;
  };

  // Get the size hints for a window.
  virtual bool GetSizeHintsForWindow(XWindow xid, SizeHints* hints_out) = 0;

  // Get the transient-for hint for a window.
  virtual bool GetTransientHintForWindow(XWindow xid, XWindow* owner_out) = 0;

  // Data returned by GetWindowAttributes().
  struct WindowAttributes {
    WindowAttributes()
        : window_class(WINDOW_CLASS_INPUT_OUTPUT),
          map_state(MAP_STATE_UNMAPPED),
          override_redirect(false) {
    }

    enum WindowClass {
      WINDOW_CLASS_INPUT_OUTPUT = 0,
      WINDOW_CLASS_INPUT_ONLY,
    };
    WindowClass window_class;

    enum MapState {
      MAP_STATE_UNMAPPED = 0,
      MAP_STATE_UNVIEWABLE,
      MAP_STATE_VIEWABLE,
    };
    MapState map_state;

    bool override_redirect;

    XVisualID visual_id;
  };

  // Get a window's attributes.
  virtual bool GetWindowAttributes(XWindow xid, WindowAttributes* attr_out) = 0;

  // Redirect the window to an offscreen pixmap so it can be composited.
  virtual bool RedirectWindowForCompositing(XWindow xid) = 0;

  // Undo a previous call to RedirectWindowForCompositing().  This is
  // useful when a plugin window gets reparented away from the root and we
  // realize that we won't need to composite it after all.
  virtual bool UnredirectWindowForCompositing(XWindow xid) = 0;

  // Get the overlay window.  (XComposite provides a window that is stacked
  // below the screensaver window but above all other windows).
  virtual XWindow GetCompositingOverlayWindow(XWindow root) = 0;

  // Get a pixmap referring to a redirected window's offscreen storage.
  virtual XPixmap GetCompositingPixmapForWindow(XWindow window) = 0;

  // Free a pixmap.
  virtual bool FreePixmap(XPixmap pixmap) = 0;

  // Get the root window.
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

  // Create a new simple window.  'width' and 'height' must be
  // positive. The window is a child of 'parent'.  The border width is
  // zero.
  XWindow CreateSimpleWindow(XWindow parent,
                             int x, int y,
                             int width, int height);

  // Destroy a window.
  virtual bool DestroyWindow(XWindow xid) = 0;

  // Has a window's bounding region been shaped using the Shape extension?
  virtual bool IsWindowShaped(XWindow xid) = 0;

  // Select ShapeNotify events on a window.
  virtual bool SelectShapeEventsOnWindow(XWindow xid) = 0;

  // Get the rectangles defining a window's bounding region.
  virtual bool GetWindowBoundingRegion(XWindow xid, ByteMap* bytemap) = 0;

  // Select RandR events on a window.
  virtual bool SelectRandREventsOnWindow(XWindow xid) = 0;

  // Look up the X ID for a single atom, creating it if necessary.
  bool GetAtom(const std::string& name, XAtom* atom_out);

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

  // Manage damage regions.
  virtual XDamage CreateDamage(XDrawable drawable, int level) = 0;
  virtual void DestroyDamage(XDamage damage) = 0;
  virtual void SubtractRegionFromDamage(XDamage damage, XserverRegion repair,
                                        XserverRegion parts) = 0;

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

  // Get the pressed-vs.-not-pressed state of all keys.  'keycodes_out' is
  // a 256-bit vector representing the logical state of the keyboard (read:
  // keycodes, not keysyms), with bits set to 1 for depressed keys.
  virtual bool QueryKeyboardState(std::vector<uint8_t>* keycodes_out) = 0;

  // Helper method to check the state of a given key in
  // QueryKeyboardState()'s output.  Returns true if the key is depressed.
  inline static bool GetKeyCodeState(const std::vector<uint8_t>& states,
                                     KeyCode keycode) {
    return (states[keycode / 8] & (0x1 << (keycode % 8)));
  }

  // Value that should be used in event and property 'format' fields for
  // byte and long arguments.
  static const int kByteFormat;
  static const int kLongFormat;

 protected:
  // Base IDs for extension events.  Implementations should initialize
  // these in their constructors.
  int damage_event_base_;
  int damage_error_base_;
  int shape_event_base_;
  int randr_event_base_;

 private:
  virtual bool GrabServerImpl() = 0;
  virtual bool UngrabServerImpl() = 0;

  bool server_grabbed_;

  DISALLOW_COPY_AND_ASSIGN(XConnection);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_X_CONNECTION_H_
