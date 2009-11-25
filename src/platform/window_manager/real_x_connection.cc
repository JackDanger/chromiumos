// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/real_x_connection.h"

extern "C" {
#include <X11/extensions/randr.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
}

#include "chromeos/obsolete_logging.h"

#include "window_manager/util.h"

namespace chromeos {

using std::string;
using std::vector;
using std::hex;

// Maximum number of integer values to allow in a property.
static const size_t kMaxIntPropertySize = 1024;

static int (*old_error_handler)(Display*, XErrorEvent*) = NULL;

static int last_trapped_error_code = 0;
static int last_trapped_request_code = 0;
static int last_trapped_minor_code = 0;

static int HandleXError(Display* display, XErrorEvent* event) {
  last_trapped_error_code = event->error_code;
  last_trapped_request_code = event->request_code;
  last_trapped_minor_code = event->minor_code;
  LOG(WARNING) << "Handled X error on display " << display << ":"
               << " error=" << last_trapped_error_code
               << " request=" << last_trapped_request_code
               << " minor=" << last_trapped_minor_code;
  return 0;
}


RealXConnection::RealXConnection(Display* display)
    : display_(display) {
  CHECK(display_);
  // TODO: Maybe handle multiple screens later, but we just use the default
  // one for now.
  root_ = DefaultRootWindow(display_);
  CHECK(GetAtom("UTF8_STRING", &utf8_string_atom_));

  int error_base = 0;  // unused; the Shape extension doesn't define any errors
  CHECK(XShapeQueryExtension(display_, &shape_event_base_, &error_base) == True)
      << "Shape extension unsupported by X server";
  CHECK(XRRQueryExtension(display_, &xrandr_event_base_, &error_base) == True)
      << "XRandR extension unsupported by X server";
}

bool RealXConnection::GetWindowGeometry(
    XWindow xid, int* x, int* y, int* width, int* height) {
  TrapErrors();
  XWindow root_ret;
  int x_ret, y_ret;
  unsigned int width_ret, height_ret, border_ret, depth_ret;
  XGetGeometry(display_, xid, &root_ret, &x_ret, &y_ret, &width_ret,
               &height_ret, &border_ret, &depth_ret);
  if (x)      *x = x_ret;
  if (y)      *y = y_ret;
  if (width)  *width = width_ret;
  if (height) *height = height_ret;
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while getting geometry for window "
                 << XidStr(xid) << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::MapWindow(XWindow xid) {
  TrapErrors();
  XMapWindow(display_, xid);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while mapping window " << XidStr(xid) << ": "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::UnmapWindow(XWindow xid) {
  TrapErrors();
  XUnmapWindow(display_, xid);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while unmapping window " << XidStr(xid) << ": "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::MoveWindow(XWindow xid, int x, int y) {
  TrapErrors();
  XMoveWindow(display_, xid, x, y);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while moving window " << XidStr(xid) << ": "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::ResizeWindow(XWindow xid, int width, int height) {
  TrapErrors();
  XResizeWindow(display_, xid, width, height);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while resizing window " << XidStr(xid) << ": "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::ConfigureWindow(
    XWindow xid, int x, int y, int width, int height) {
  TrapErrors();
  XWindowChanges changes;
  changes.x = x;
  changes.y = y;
  changes.width = width;
  changes.height = height;
  XConfigureWindow(display_, xid, CWX | CWY | CWWidth | CWHeight, &changes);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while configuring window " << XidStr(xid)
                 << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::RaiseWindow(XWindow xid) {
  TrapErrors();
  XRaiseWindow(display_, xid);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while raising window " << XidStr(xid) << ": "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::FocusWindow(XWindow xid, Time event_time) {
  VLOG(1) << "Focusing window " << XidStr(xid);
  TrapErrors();
  XSetInputFocus(display_, xid, RevertToParent, event_time);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while focusing window " << XidStr(xid) << ": "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::StackWindow(XWindow xid, XWindow other, bool above) {
  TrapErrors();
  XWindowChanges changes;
  changes.sibling = other;
  changes.stack_mode = above ? Above : Below;
  XConfigureWindow(display_, xid, CWSibling | CWStackMode, &changes);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while stacking window " << XidStr(xid) << ": "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::ReparentWindow(
    XWindow xid, XWindow parent, int x, int y) {
  TrapErrors();
  XReparentWindow(display_, xid, parent, x, y);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while reparenting window " << XidStr(xid)
                 << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::SetWindowBorderWidth(XWindow xid, int width) {
  DCHECK_GE(width, 0);
  TrapErrors();
  XWindowChanges changes;
  changes.border_width = width;
  XConfigureWindow(display_, xid, CWBorderWidth, &changes);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while setting " << XidStr(xid)
                 << "'s border width to " << width << ": "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::SelectInputOnWindow(
    XWindow xid, int event_mask, bool preserve_existing) {
  TrapErrors();
  if (preserve_existing) {
    XWindowAttributes attr;
    XGetWindowAttributes(display_, xid, &attr);
    event_mask |= attr.your_event_mask;
  }
  if (!GetLastErrorCode()) {
    // Only select the new mask if we were successful in fetching the
    // previous one to avoid blowing away the previous mask on failure.
    XSelectInput(display_, xid, event_mask);
  }
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while selecting input on window "
                 << XidStr(xid) << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::DeselectInputOnWindow(XWindow xid, int event_mask) {
  TrapErrors();
  XWindowAttributes attr;
  XGetWindowAttributes(display_, xid, &attr);
  attr.your_event_mask &= ~event_mask;
  if (!GetLastErrorCode()) {
    // Only select the new mask if we were successful in fetching the
    // previous one to avoid blowing away the previous mask on failure.
    XSelectInput(display_, xid, attr.your_event_mask);
  }
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while deselecting input on window "
                 << XidStr(xid) << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::AddPassiveButtonGrabOnWindow(
    XWindow xid, int button, int event_mask) {
  TrapErrors();
  XGrabButton(display_,
              button,
              0,              // modifiers
              xid,
              False,          // owner_events
              event_mask,
              GrabModeSync,   // pointer_mode
              GrabModeAsync,  // keyboard_mode
              None,           // confine_to
              None);          // cursor
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while installing passive grab for button "
                 << button << " on window " << XidStr(xid) << ": "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::RemovePassiveButtonGrabOnWindow(XWindow xid, int button) {
  TrapErrors();
  XUngrabButton(display_, button, None, xid);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while uninstalling passive grab for button "
                 << button << " on window " << XidStr(xid) << ": "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::AddActivePointerGrabForWindow(XWindow xid,
                                                    int event_mask,
                                                    Time timestamp) {
  TrapErrors();
  int result = XGrabPointer(display_,
                            xid,
                            False,          // owner_events
                            event_mask,
                            GrabModeAsync,  // pointer_mode
                            GrabModeAsync,  // keyboard_mode
                            None,           // confine_to
                            None,           // cursor
                            timestamp);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while actively grabbing pointer for window "
                 << XidStr(xid) << ": " << GetErrorText(error);
    return false;
  } else if (result != GrabSuccess) {
    LOG(WARNING) << "Active pointer grab for window " << XidStr(xid)
                 << " failed with " << result;
    return false;
  }
  return true;
}

bool RealXConnection::RemoveActivePointerGrab(bool replay_events) {
  TrapErrors();
  if (replay_events) {
    XAllowEvents(display_, ReplayPointer, CurrentTime);
  } else {
    XUngrabPointer(display_, CurrentTime);
  }
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while repeating grabbed button events: "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::RemoveInputRegionFromWindow(XWindow xid) {
  TrapErrors();
  XShapeCombineRectangles(display_,
                          xid,
                          ShapeInput,
                          0,     // x_off
                          0,     // y_off
                          NULL,  // rectangles
                          0,     // n_rects
                          ShapeSet,
                          Unsorted);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while removing input region from "
                 << XidStr(xid) << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::GetSizeHintsForWindow(
    XWindow xid, XSizeHints* hints, long* supplied_hints) {
  TrapErrors();
  int result = XGetWMNormalHints(display_, xid, hints, supplied_hints);
  int error = UntrapErrors();
  if (!result || error) {
    if (error) {
      LOG(WARNING) << "Got X error while getting hints for " << XidStr(xid)
                   << ": " << GetErrorText(error);
    }
    return false;
  }
  return true;
}

bool RealXConnection::GetTransientHintForWindow(
    XWindow xid, XWindow* owner_out) {
  CHECK(owner_out);
  *owner_out = None;

  TrapErrors();
  XGetTransientForHint(display_, xid, owner_out);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while getting WM_TRANSIENT_FOR for "
                 << "window " << XidStr(xid) << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::GetWindowAttributes(
    XWindow xid, XWindowAttributes* attr_out) {
  TrapErrors();
  int result = XGetWindowAttributes(display_, xid, attr_out);
  int error = UntrapErrors();
  if (!result || error) {
    if (error) {
      LOG(WARNING) << "Got X error while getting attributes for window "
                   << XidStr(xid) << ": " << GetErrorText(error);
    }
    return false;
  }
  return true;
}

bool RealXConnection::RedirectWindowForCompositing(XWindow xid) {
  TrapErrors();
  // TODO: Manual or Automatic here?
  XCompositeRedirectWindow(display_, xid, CompositeRedirectManual);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while redirecting window " << XidStr(xid)
                 << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::UnredirectWindowForCompositing(XWindow xid) {
  TrapErrors();
  // TODO: Manual or Automatic here?  What does the 'update' parameter even
  // mean when we're *un*-redirecting a window?
  XCompositeUnredirectWindow(display_, xid, CompositeRedirectManual);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while unredirecting window " << XidStr(xid)
                 << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

XWindow RealXConnection::GetCompositingOverlayWindow(XWindow root) {
  TrapErrors();
  XWindow xid = XCompositeGetOverlayWindow(display_, root);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while getting overlay window: "
                 << GetErrorText(error);
    return None;
  }
  return xid;
}

XWindow RealXConnection::CreateWindow(
    XWindow parent,
    int x, int y,
    int width, int height,
    bool override_redirect,
    bool input_only,
    int event_mask) {
  CHECK_GT(width, 0);
  CHECK_GT(height, 0);
  CHECK_NE(parent, None);

  TrapErrors();
  XSetWindowAttributes attr;
  attr.override_redirect = override_redirect ? True : False;
  attr.event_mask = event_mask;
  XWindow xid = XCreateWindow(display_,
                              parent,
                              x, y,
                              width, height,
                              0,               // border width
                              CopyFromParent,  // depth
                              input_only ? InputOnly : CopyFromParent,  // class
                              CopyFromParent,  // visual
                              CWOverrideRedirect | CWEventMask,
                              &attr);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while creating window: "
                 << GetErrorText(error);
    return None;
  }
  return xid;
}

bool RealXConnection::DestroyWindow(XWindow xid) {
  TrapErrors();
  XDestroyWindow(display_, xid);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while destroying window " << XidStr(xid)
                 << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::IsWindowShaped(XWindow xid) {
  TrapErrors();
  Bool bounding_shaped = False, clip_shaped = False;
  int x_bounding = 0, y_bounding = 0;
  uint w_bounding = 0, h_bounding = 0;
  int x_clip = 0, y_clip = 0;
  uint w_clip = 0, h_clip = 0;
  XShapeQueryExtents(
      display_, xid,
      &bounding_shaped, &x_bounding, &y_bounding, &w_bounding, &h_bounding,
      &clip_shaped, &x_clip, &y_clip, &w_clip, &h_clip);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while checking if window " << XidStr(xid)
                 << " is shaped: " << GetErrorText(error);
    return false;
  }
  return (bounding_shaped == True);
}

bool RealXConnection::SelectShapeEventsOnWindow(XWindow xid) {
  TrapErrors();
  XShapeSelectInput(display_, xid, ShapeNotifyMask);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while selecting ShapeNotify events on "
                 << XidStr(xid) << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::GetWindowBoundingRegion(XWindow xid, ByteMap* bytemap) {
  TrapErrors();
  int count = 0, ordering = 0;
  XRectangle* rects =
      XShapeGetRectangles(display_, xid, ShapeBounding, &count, &ordering);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while getting bounding rectangles for "
                 << XidStr(xid) << ": " << GetErrorText(error);
    return false;
  }
  bytemap->Clear(0x0);
  for (int i = 0; i < count; ++i) {
    const XRectangle& rect = rects[i];
    bytemap->SetRectangle(rect.x, rect.y, rect.width, rect.height, 0xff);
  }
  XFree(rects);
  return true;
}

bool RealXConnection::SelectXRandREventsOnWindow(XWindow xid) {
  TrapErrors();
  XRRSelectInput(display_, xid, RRScreenChangeNotifyMask);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while selecting RRScreenChangeNotify events "
                 << "on " << XidStr(xid) << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::GetAtom(const string& name, XAtom* atom_out) {
  CHECK(atom_out);
  TrapErrors();
  *atom_out = XInternAtom(display_, const_cast<char*>(name.c_str()), false);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while getting atom \"" << name << "\": "
                 << GetErrorText(error);
    return false;
  }
  if (*atom_out == None) {
    LOG(WARNING) << "Unable to get atom \"" << name << "\"";
    return false;
  }
  return true;
}

bool RealXConnection::GetAtoms(
    const vector<string>& names, vector<XAtom>* atoms_out) {
  CHECK(atoms_out);
  atoms_out->clear();

  char** names_array = new char*[names.size()];
  XAtom* atoms_array = new XAtom[names.size()];

  for (size_t i = 0; i < names.size(); ++i) {
    // Need to const_cast because XInternAtoms() wants a char**.
    names_array[i] = const_cast<char*>(names.at(i).c_str());
  }

  TrapErrors();
  int result = XInternAtoms(display_,
                            names_array,
                            names.size(),
                            False,  // only_if_exists
                            atoms_array);
  int error = UntrapErrors();
  if (!result || error) {
    if (error) {
      LOG(WARNING) << "Got X error while getting atoms: "
                   << GetErrorText(error);
    }
    delete[] names_array;
    delete[] atoms_array;
    return false;
  }

  for (size_t i = 0; i < names.size(); ++i) {
    atoms_out->push_back(atoms_array[i]);
  }
  delete[] names_array;
  delete[] atoms_array;
  return true;
}

bool RealXConnection::GetAtomName(XAtom atom, string* name) {
  CHECK(name);
  TrapErrors();
  char* name_ptr = XGetAtomName(display_, atom);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while looking up atom " << XidStr(atom) << ": "
                 << GetErrorText(error);
    if (name_ptr) {
      XFree(name_ptr);
    }
    return false;
  }
  if (!name_ptr) {
    LOG(WARNING) << "Got NULL string while looking up atom " << XidStr(atom);
    return false;
  }
  name->assign(name_ptr);
  XFree(name_ptr);
  return true;
}

bool RealXConnection::GetIntArrayProperty(
    XWindow xid, XAtom xatom, vector<int>* values) {
  CHECK(values);
  values->clear();

  XAtom type = None;
  int format = 0;  // size in bits of each item in 'property'
  long unsigned int num_items = 0, remaining_bytes = 0;
  unsigned char* property = NULL;

  TrapErrors();
  XGetWindowProperty(display_,
                     xid,
                     xatom,
                     0,                    // offset into property data to read
                     kMaxIntPropertySize,  // length to get in 32-bit quantities
                     False,                // deleted
                     AnyPropertyType,
                     &type,
                     &format,
                     &num_items,
                     &remaining_bytes,
                     &property);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while getting int property " << XidStr(xatom)
                 << " for window " << XidStr(xid) << ": "
                 << GetErrorText(error);
    return false;
  }

  if (format == 0) {
    // Okay, the property just wasn't set.
    return false;
  }

  // Only read data containing a single 32-bit item.
  if (format != kLongFormat) {
    LOG(WARNING) << "Not reading property " << XidStr(xatom) << " for window "
                 << XidStr(xid) << "; it's supposed to have a 32-bit format "
                 << "but is actually " << format << "-bit";
    XFree(property);
    return false;
  }
  if (num_items < 1 || remaining_bytes != 0) {
    LOG(WARNING) << "Not reading property " << XidStr(xatom) << " for window "
                 << XidStr(xid) << "; we got " << num_items << " ints and have "
                 << remaining_bytes << " byte(s) remaining (we expected at "
                 << "least one int)";
    XFree(property);
    return false;
  }

  for (size_t i = 0; i < num_items; ++i) {
    values->push_back(reinterpret_cast<int*>(property)[i]);
  }
  XFree(property);
  return true;
}

bool RealXConnection::SetIntArrayProperty(
    XWindow xid, XAtom xatom, XAtom type, const vector<int>& values) {
  if (values.size() > kMaxIntPropertySize) {
    LOG(WARNING) << "Setting int property " << XidStr(xatom) << " for window "
                 << XidStr(xid) << " with " << values.size()
                 << " values (max is " << kMaxIntPropertySize << ")";
  }

  CHECK(!values.empty());
  int* buf = new int[values.size()];
  for (size_t i = 0; i < values.size(); ++i) {
    buf[i] = values[i];
  }

  TrapErrors();
  XChangeProperty(display_,
                  xid,
                  xatom,
                  type,
                  kLongFormat,  // size in bits of items in 'value'
                  PropModeReplace,
                  reinterpret_cast<const unsigned char*>(buf),
                  values.size());  // num items
  delete[] buf;
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while setting int property " << xatom
                 << " for window " << xid << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::GetStringProperty(XWindow xid, XAtom xatom, string* out) {
  CHECK(out);
  out->clear();

  XAtom type = None;
  int format = 0;  // size in bits of each item in 'property'
  long unsigned int num_items = 0, remaining_bytes = 0;
  unsigned char* property = NULL;

  TrapErrors();
  int result = XGetWindowProperty(display_,
                                  xid,
                                  xatom,
                                  0,      // offset
                                  1024,   // max size
                                  false,  // deleted
                                  AnyPropertyType,
                                  &type,
                                  &format,
                                  &num_items,
                                  &remaining_bytes,
                                  &property);
  int error = UntrapErrors();

  if (result != Success) {
    LOG(WARNING) << "Unable to get property " << XidStr(xatom)
                 << " for window " << XidStr(xid);
    return false;
  }
  if (error) {
    LOG(WARNING) << "Got X error while getting property " << XidStr(xatom)
                 << " for window " << XidStr(xid) << ": "
                 << GetErrorText(error);
    return false;
  }
  // Only read data containing 8-bit items.
  if (format == kByteFormat) {
    if (type != XA_STRING && type != utf8_string_atom_) {
      LOG(WARNING) << "Getting property " << XidStr(xatom)
                   << " with unsupported type " << type
                   << " as string for window " << XidStr(xid);
    }
    out->assign(reinterpret_cast<char*>(property), num_items);
  }
  XFree(property);
  return true;
}

bool RealXConnection::SetStringProperty(
    XWindow xid, XAtom xatom, const string& value) {
  TrapErrors();
  XChangeProperty(display_,
                  xid,
                  xatom,
                  utf8_string_atom_,
                  kByteFormat,  // size in bits of items in 'value'
                  PropModeReplace,
                  reinterpret_cast<const unsigned char*>(value.c_str()),
                  value.size());
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while setting string property "
                 << XidStr(xatom) << " to \"" << value << "\" for window "
                 << XidStr(xid) << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::DeletePropertyIfExists(XWindow xid, XAtom xatom) {
  TrapErrors();
  XDeleteProperty(display_, xid, xatom);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while deleting property " << XidStr(xatom)
                 << " for window " << XidStr(xid) << ": "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::SendEvent(XWindow xid, XEvent* event, int event_mask) {
  CHECK(event);
  TrapErrors();
  XSendEvent(display_,
             xid,
             False,  // propagate
             event_mask,
             event);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while sending message to window "
                 << XidStr(xid) << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::WaitForEvent(
    XWindow xid, int event_mask, XEvent* event_out) {
  CHECK(event_out);
  TrapErrors();
  XWindowEvent(display_, xid, event_mask, event_out);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while waiting for event on window "
                 << XidStr(xid) << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

XWindow RealXConnection::GetSelectionOwner(XAtom atom) {
  TrapErrors();
  XWindow xid = XGetSelectionOwner(display_, atom);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while getting selection owner for "
                 << XidStr(atom) << ": " << GetErrorText(error);
    return None;
  }
  return xid;
}

bool RealXConnection::SetSelectionOwner(
    XAtom atom, XWindow xid, Time timestamp) {
  TrapErrors();
  XSetSelectionOwner(display_, atom, xid, timestamp);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while setting selection owner for "
                 << XidStr(atom) << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::SetWindowCursor(XWindow xid, uint32 shape) {
  TrapErrors();
  XSetWindowAttributes attr;
  attr.cursor = XCreateFontCursor(display_, shape);
  XChangeWindowAttributes(display_, xid, CWCursor, &attr);
  // TODO(derat): Do we need to free the result of XCreateFontCursor above?
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while changing cursor for " << XidStr(xid)
                 << ": " << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::GetChildWindows(XWindow xid,
                                      vector<XWindow>* children_out) {
  CHECK(children_out);
  children_out->clear();

  XWindow root_return, parent_return;
  XWindow* children = NULL;
  unsigned int num_children = 0;

  TrapErrors();
  int result = XQueryTree(display_, xid, &root_return, &parent_return,
                          &children, &num_children);
  int error = UntrapErrors();
  if (!result || error) {
    if (error) {
      LOG(WARNING) << "Got X error while querying windows under " << XidStr(xid)
                   << ": " << GetErrorText(error);
    }
    return false;
  }
  if (!children) {
    return true;
  }

  for (unsigned int i = 0; i < num_children; ++i) {
    children_out->push_back(children[i]);
  }
  XFree(children);
  return true;
}

bool RealXConnection::GetParentWindow(XWindow xid, XWindow* parent) {
  CHECK(parent);
  XWindow root_return, parent_return;
  XWindow* children = NULL;
  unsigned int num_children = 0;

  TrapErrors();
  int result = XQueryTree(display_, xid, &root_return, &parent_return,
                          &children, &num_children);
  *parent = parent_return;
  if (children)
    XFree(children);
  int error = UntrapErrors();
  if (!result || error) {
    if (error) {
      LOG(WARNING) << "Got X error while querying parent window for "
                   << XidStr(xid) << ": " << GetErrorText(error);
    }
    return false;
  }
  return true;
}

KeySym RealXConnection::GetKeySymFromKeyCode(uint32 keycode) {
  return XKeycodeToKeysym(display_, keycode, 0);
}

uint32 RealXConnection::GetKeyCodeFromKeySym(KeySym keysym) {
  return XKeysymToKeycode(display_, keysym);
}

string RealXConnection::GetStringFromKeySym(KeySym keysym) {
  char* ptr = XKeysymToString(keysym);
  if (!ptr) {
    return "";
  }
  return string(ptr);
}

bool RealXConnection::GrabKey(KeyCode keycode, uint32 modifiers) {
  TrapErrors();
  XGrabKey(display_,
           keycode,
           modifiers,
           root_,
           False,           // owner_events
           GrabModeAsync,   // pointer_mode
           GrabModeAsync);  // keyboard_mode
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while grabbing key " << keycode
                 << " with modifiers 0x" << hex << modifiers << ": "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::UngrabKey(KeyCode keycode, uint32 modifiers) {
  TrapErrors();
  XUngrabKey(display_, keycode, modifiers, root_);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while ungrabbing key " << keycode
                 << " with modifiers 0x" << hex << modifiers << ": "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::SetDetectableKeyboardAutoRepeat(bool detectable) {
  Bool supported = False;
  XkbSetDetectableAutoRepeat(
      display_, detectable ? True : False, &supported);
  return (supported == True ? true : false);
}

bool RealXConnection::GrabServerImpl() {
  TrapErrors();
  XGrabServer(display_);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while grabbing server: "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

bool RealXConnection::UngrabServerImpl() {
  TrapErrors();
  XUngrabServer(display_);
  if (int error = UntrapErrors()) {
    LOG(WARNING) << "Got X error while ungrabbing server: "
                 << GetErrorText(error);
    return false;
  }
  return true;
}

void RealXConnection::TrapErrors() {
  CHECK(!old_error_handler) << "X errors are already being trapped";
  old_error_handler = XSetErrorHandler(&HandleXError);
  last_trapped_error_code = 0;
  last_trapped_request_code = 0;
  last_trapped_minor_code = 0;
}

int RealXConnection::UntrapErrors() {
  CHECK(old_error_handler) << "X errors aren't being trapped";
  // Sync in case we sent a request that didn't require a reply.
  // TODO: It seems kinda wasteful to always do this.
  XSync(display_, False);
  XSetErrorHandler(old_error_handler);
  old_error_handler = NULL;
  return last_trapped_error_code;
}

int RealXConnection::GetLastErrorCode() {
  return last_trapped_error_code;
}

string RealXConnection::GetErrorText(int error_code) {
  char str[1024];
  XGetErrorText(display_, error_code, str, sizeof(str));
  return string(str);
}

}  // namespace chromeos
