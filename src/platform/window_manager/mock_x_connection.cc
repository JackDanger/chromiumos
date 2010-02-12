// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/mock_x_connection.h"

#include "base/logging.h"

#include "window_manager/util.h"

namespace window_manager {

using chromeos::Closure;
using std::list;
using std::make_pair;
using std::map;
using std::pair;
using std::string;
using std::tr1::shared_ptr;
using std::vector;

MockXConnection::MockXConnection()
    : windows_(),
      stacked_xids_(new Stacker<XWindow>),
      next_window_(1),
      // TODO: Replace magic numbers.
      root_(CreateWindow(None, 0, 0, 1024, 768, true, false, 0)),
      overlay_(CreateWindow(root_, 0, 0, 1024, 768, true, false, 0)),
      next_atom_(1000),
      focused_xid_(None),
      pointer_grab_xid_(None),
      pointer_x_(0),
      pointer_y_(0) {
  // Arbitrary large numbers unlikely to be used by other events.
  shape_event_base_ = 432432;
  randr_event_base_ = 543251;
  damage_event_base_ = 683827;
  damage_error_base_ = 728384;
}

MockXConnection::~MockXConnection() {}

XVisualInfo* MockXConnection::GetVisualInfo(long mask,
                                            XVisualInfo* visual_template,
                                            int* item_count) {
  *item_count = 1;
  return visual_template;
}

bool MockXConnection::GetWindowGeometry(XWindow xid, WindowGeometry* geom_out) {
  CHECK(geom_out);
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  geom_out->x = info->x;
  geom_out->y = info->y;
  geom_out->width = info->width;
  geom_out->height = info->height;
  geom_out->border_width = 0;
  geom_out->depth = 32;
  return true;
}

bool MockXConnection::MapWindow(XWindow xid) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->mapped = true;
  info->changed = true;
  return true;
}

bool MockXConnection::UnmapWindow(XWindow xid) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->mapped = false;
  if (focused_xid_ == xid)
    focused_xid_ = None;
  info->changed = true;
  return true;
}

bool MockXConnection::MoveWindow(XWindow xid, int x, int y) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->x = x;
  info->y = y;
  info->changed = true;
  return true;
}

bool MockXConnection::ResizeWindow(XWindow xid, int width, int height) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->width = width;
  info->height = height;
  info->changed = true;
  return true;
}

bool MockXConnection::RaiseWindow(XWindow xid) {
  if (!stacked_xids_->Contains(xid))
    return false;
  stacked_xids_->Remove(xid);
  stacked_xids_->AddOnTop(xid);
  return true;
}

bool MockXConnection::FocusWindow(XWindow xid, Time event_time) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  focused_xid_ = xid;
  return true;
}

bool MockXConnection::StackWindow(XWindow xid, XWindow other, bool above) {
  if (!stacked_xids_->Contains(xid) || !stacked_xids_->Contains(other))
    return false;
  stacked_xids_->Remove(xid);
  if (above)
    stacked_xids_->AddAbove(xid, other);
  else
    stacked_xids_->AddBelow(xid, other);
  return true;
}

bool MockXConnection::AddButtonGrabOnWindow(
    XWindow xid, int button, int event_mask, bool synchronous) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->button_grabs[button] =
      WindowInfo::ButtonGrabInfo(event_mask, synchronous);
  return true;
}

bool MockXConnection::SelectInputOnWindow(
    XWindow xid, int event_mask, bool preserve_existing) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->event_mask = preserve_existing ?
      (info->event_mask | event_mask) : event_mask;
  return true;
}

bool MockXConnection::DeselectInputOnWindow(XWindow xid, int event_mask) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->event_mask &= ~event_mask;
  return true;
}

bool MockXConnection::RemoveButtonGrabOnWindow(XWindow xid, int button) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->button_grabs.erase(button);
  return true;
}

bool MockXConnection::AddPointerGrabForWindow(
    XWindow xid, int event_mask, Time timestamp) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  if (pointer_grab_xid_ != None) {
    LOG(ERROR) << "Pointer is already grabbed for " << XidStr(pointer_grab_xid_)
               << "; ignoring request to grab it for " << XidStr(xid);
    return false;
  }
  pointer_grab_xid_ = xid;
  return true;
}

bool MockXConnection::RemovePointerGrab(bool replay_events, Time timestamp) {
  pointer_grab_xid_ = None;
  return true;
}

bool MockXConnection::GetSizeHintsForWindow(XWindow xid, SizeHints* hints_out) {
  CHECK(hints_out);
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  *hints_out = info->size_hints;
  return true;
}

bool MockXConnection::GetTransientHintForWindow(
    XWindow xid, XWindow* owner_out) {
  CHECK(owner_out);
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  *owner_out = info->transient_for;
  return true;
}

bool MockXConnection::GetWindowAttributes(
    XWindow xid, WindowAttributes* attr_out) {
  CHECK(attr_out);
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;

  attr_out->window_class = info->input_only ?
      WindowAttributes::WINDOW_CLASS_INPUT_ONLY :
      WindowAttributes::WINDOW_CLASS_INPUT_OUTPUT;
  attr_out->map_state = info->mapped ?
      WindowAttributes::MAP_STATE_VIEWABLE :
      WindowAttributes::MAP_STATE_UNMAPPED;
  attr_out->override_redirect = info->override_redirect;
  return true;
}

bool MockXConnection::RedirectWindowForCompositing(XWindow xid) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->redirected = true;
  return true;
}

bool MockXConnection::UnredirectWindowForCompositing(XWindow xid) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->redirected = false;
  return true;
}

XWindow MockXConnection::CreateWindow(
    XWindow parent,
    int x, int y,
    int width, int height,
    bool override_redirect,
    bool input_only,
    int event_mask) {
  XWindow xid = next_window_;
  next_window_++;
  shared_ptr<WindowInfo> info(new WindowInfo(xid, parent));
  info->x = x;
  info->y = y;
  info->width = width;
  info->height = height;
  info->override_redirect = override_redirect;
  info->input_only = input_only;
  info->event_mask = event_mask;
  windows_[xid] = info;
  stacked_xids_->AddOnTop(xid);
  return xid;
}

bool MockXConnection::DestroyWindow(XWindow xid) {
  map<XWindow, shared_ptr<WindowInfo> >::iterator it = windows_.find(xid);
  if (it == windows_.end())
    return false;
  windows_.erase(it);
  stacked_xids_->Remove(xid);
  if (focused_xid_ == xid)
    focused_xid_ = None;

  // Release any selections held by this window.
  vector<XAtom> orphaned_selections;
  for (map<XAtom, XWindow>::const_iterator it = selection_owners_.begin();
       it != selection_owners_.end(); ++it) {
    if (it->second == xid)
      orphaned_selections.push_back(it->first);
  }
  for (vector<XAtom>::const_iterator it = orphaned_selections.begin();
       it != orphaned_selections.end(); ++it) {
    selection_owners_.erase(*it);
  }

  return true;
}

bool MockXConnection::IsWindowShaped(XWindow xid) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  return (info->shape.get() != NULL);
}

bool MockXConnection::SelectShapeEventsOnWindow(XWindow xid) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->shape_events_selected = true;
  return true;
}

bool MockXConnection::GetWindowBoundingRegion(XWindow xid, ByteMap* bytemap) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  if (info->shape.get())
    bytemap->Copy(*(info->shape.get()));
  else
    bytemap->SetRectangle(0, 0, info->width, info->height, 0xff);
  return true;
}

bool MockXConnection::SelectRandREventsOnWindow(XWindow xid) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->randr_events_selected = true;
  return true;
}

bool MockXConnection::GetAtoms(
    const vector<string>& names, vector<XAtom>* atoms_out) {
  CHECK(atoms_out);
  atoms_out->clear();
  for (vector<string>::const_iterator name_it = names.begin();
       name_it != names.end(); ++name_it) {
    map<string, XAtom>::const_iterator find_it = name_to_atom_.find(*name_it);
    if (find_it != name_to_atom_.end()) {
      atoms_out->push_back(find_it->second);
    } else {
      XAtom atom = next_atom_;
      atoms_out->push_back(atom);
      name_to_atom_[*name_it] = atom;
      atom_to_name_[atom] = *name_it;
      next_atom_++;
    }
  }
  return true;
}

bool MockXConnection::GetIntArrayProperty(
    XWindow xid, XAtom xatom, vector<int>* values) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  map<XAtom, vector<int> >::const_iterator it =
      info->int_properties.find(xatom);
  if (it == info->int_properties.end())
    return false;
  *values = it->second;
  return true;
}

bool MockXConnection::SetIntArrayProperty(
    XWindow xid, XAtom xatom, XAtom type, const vector<int>& values) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->int_properties[xatom] = values;
  // TODO: Also save type.
  Closure* cb =
      FindWithDefault(property_callbacks_,
                      make_pair(xid, xatom),
                      shared_ptr<Closure>(static_cast<Closure*>(NULL))).get();
  if (cb)
    cb->Run();
  return true;
}

bool MockXConnection::GetStringProperty(XWindow xid, XAtom xatom, string* out) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  map<XAtom, string>::const_iterator it = info->string_properties.find(xatom);
  if (it == info->string_properties.end())
    return false;
  *out = it->second;
  return true;
}

bool MockXConnection::SetStringProperty(
    XWindow xid, XAtom xatom, const string& value) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->string_properties[xatom] = value;
  Closure* cb =
      FindWithDefault(property_callbacks_,
                      make_pair(xid, xatom),
                      shared_ptr<Closure>(static_cast<Closure*>(NULL))).get();
  if (cb)
    cb->Run();
  return true;
}

bool MockXConnection::DeletePropertyIfExists(XWindow xid, XAtom xatom) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->int_properties.erase(xatom);
  return true;
}

bool MockXConnection::SendEvent(XWindow xid, XEvent* event, int event_mask) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;

  if (event->type == ClientMessage)
    info->client_messages.push_back(event->xclient);
  return true;
}

bool MockXConnection::GetAtomName(XAtom atom, string* name) {
  CHECK(name);
  map<XAtom, string>::const_iterator it = atom_to_name_.find(atom);
  if (it == atom_to_name_.end())
    return false;
  *name = it->second;
  return true;
}

XWindow MockXConnection::GetSelectionOwner(XAtom atom) {
  map<XAtom, XWindow>::const_iterator it = selection_owners_.find(atom);
  return (it == selection_owners_.end()) ? None : it->second;
}

bool MockXConnection::SetSelectionOwner(
    XAtom atom, XWindow xid, Time timestamp) {
  selection_owners_[atom] = xid;
  return true;
}

bool MockXConnection::SetWindowCursor(XWindow xid, uint32 shape) {
  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;
  info->cursor = shape;
  return true;
}

bool MockXConnection::GrabKey(KeyCode keycode, uint32 modifiers) {
  grabbed_keys_.insert(make_pair(keycode, modifiers));
  return true;
}

bool MockXConnection::UngrabKey(KeyCode keycode, uint32 modifiers) {
  grabbed_keys_.erase(make_pair(keycode, modifiers));
  return true;
}

bool MockXConnection::QueryPointerPosition(int* x_root, int* y_root) {
  if (x_root)
    *x_root = pointer_x_;
  if (y_root)
    *y_root = pointer_y_;
  return true;
}

bool MockXConnection::GetChildWindows(XWindow xid,
                                      vector<XWindow>* children_out) {
  CHECK(children_out);
  children_out->clear();

  WindowInfo* info = GetWindowInfo(xid);
  if (!info)
    return false;

  // Add the children in bottom-to-top order to match XQueryTree().
  for (list<XWindow>::const_reverse_iterator it =
         stacked_xids_->items().rbegin();
       it != stacked_xids_->items().rend(); ++it) {
    const WindowInfo* child_info = GetWindowInfo(*it);
    CHECK(child_info) << "No info found for window " << *it;
    if (child_info->parent == xid)
      children_out->push_back(*it);
  }
  return true;
}

MockXConnection::WindowInfo::WindowInfo(XWindow xid, XWindow parent)
    : xid(xid),
      parent(parent),
      x(-1),
      y(-1),
      width(1),
      height(1),
      border_width(0),
      depth(32),
      mapped(false),
      override_redirect(false),
      input_only(false),
      redirected(false),
      event_mask(0),
      transient_for(None),
      cursor(0),
      shape(NULL),
      shape_events_selected(false),
      randr_events_selected(false),
      changed(false) {
}

MockXConnection::WindowInfo::~WindowInfo() {}

MockXConnection::WindowInfo* MockXConnection::GetWindowInfo(XWindow xid) {
  map<XWindow, shared_ptr<WindowInfo> >::iterator it = windows_.find(xid);
  return (it != windows_.end()) ? it->second.get() : NULL;
}

void MockXConnection::RegisterPropertyCallback(
    XWindow xid, XAtom xatom, Closure* cb) {
  CHECK(cb);
  CHECK(property_callbacks_.insert(
            make_pair(make_pair(xid, xatom), shared_ptr<Closure>(cb))).second);
}

// static
void MockXConnection::InitButtonEvent(XEvent* event,
                                      const WindowInfo& info,
                                      int x, int y, int button,
                                      bool press) {
  CHECK(event);
  XButtonEvent* button_event = &(event->xbutton);
  memset(button_event, 0, sizeof(*button_event));
  button_event->type = press ? ButtonPress : ButtonRelease;
  button_event->window = info.xid;
  button_event->x = x;
  button_event->y = y;
  button_event->x_root = info.x + x;
  button_event->y_root = info.y + y;
  button_event->button = button;
}

// static
void MockXConnection::InitClientMessageEvent(XEvent* event,
                                             XWindow xid,
                                             XAtom type,
                                             long arg1,
                                             long arg2,
                                             long arg3,
                                             long arg4) {
  CHECK(event);
  XClientMessageEvent* client_event = &(event->xclient);
  memset(client_event, 0, sizeof(*client_event));
  client_event->type = ClientMessage;
  client_event->window = xid;
  client_event->message_type = type;
  client_event->format = kLongFormat;
  client_event->data.l[0] = arg1;
  client_event->data.l[1] = arg2;
  client_event->data.l[2] = arg3;
  client_event->data.l[3] = arg4;
}

// static
void MockXConnection::InitConfigureNotifyEvent(
    XEvent* event, const WindowInfo& info) {
  CHECK(event);
  XConfigureEvent* conf_event = &(event->xconfigure);
  memset(conf_event, 0, sizeof(*conf_event));
  conf_event->type = ConfigureNotify;
  conf_event->window = info.xid;
  conf_event->above = None;  // TODO: Handle stacking.
  conf_event->override_redirect = info.override_redirect;
  conf_event->x = info.x;
  conf_event->y = info.y;
  conf_event->width = info.width;
  conf_event->height = info.height;
}

// static
void MockXConnection::InitConfigureRequestEvent(
    XEvent* event, XWindow xid, int x, int y, int width, int height) {
  CHECK(event);
  XConfigureRequestEvent* conf_event = &(event->xconfigurerequest);
  memset(conf_event, 0, sizeof(*conf_event));
  conf_event->type = ConfigureRequest;
  conf_event->window = xid;
  conf_event->x = x;
  conf_event->y = y;
  conf_event->width = width;
  conf_event->height = height;
  conf_event->value_mask = CWX | CWY | CWWidth | CWHeight;
}

// static
void MockXConnection::InitCreateWindowEvent(XEvent* event,
                                            const WindowInfo& info) {
  CHECK(event);
  XCreateWindowEvent* create_event = &(event->xcreatewindow);
  memset(create_event, 0, sizeof(*create_event));
  create_event->type = CreateNotify;
  create_event->parent = info.parent;
  create_event->window = info.xid;
  create_event->x = info.x;
  create_event->y = info.y;
  create_event->width = info.width;
  create_event->height = info.height;
  create_event->border_width = info.border_width;
  create_event->override_redirect = info.override_redirect ? True : False;
}

// static
void MockXConnection::InitDestroyWindowEvent(XEvent* event, XWindow xid) {
  CHECK(event);
  XDestroyWindowEvent* destroy_event = &(event->xdestroywindow);
  memset(destroy_event, 0, sizeof(*destroy_event));
  destroy_event->type = DestroyNotify;
  destroy_event->window = xid;
}

// static
void MockXConnection::InitEnterOrLeaveWindowEvent(XEvent* event,
                                                  const WindowInfo& info,
                                                  int x, int y, bool enter) {
  CHECK(event);
  XEnterWindowEvent* enter_event = &(event->xcrossing);
  memset(enter_event, 0, sizeof(*enter_event));
  enter_event->type = enter ? EnterNotify : LeaveNotify;
  enter_event->window = info.xid;
  enter_event->x = x;
  enter_event->y = y;
  enter_event->x_root = info.x + x;
  enter_event->y_root = info.y + y;
  // Leave everything else blank for now; we don't use it.
}

// static
void MockXConnection::InitFocusEvent(
    XEvent* event, XWindow xid, int mode, int detail, bool focus_in) {
  CHECK(event);
  XFocusChangeEvent* focus_event = &(event->xfocus);
  memset(focus_event, 0, sizeof(*focus_event));
  focus_event->type = focus_in ? FocusIn : FocusOut;
  focus_event->window = xid;
  focus_event->mode = mode;
  focus_event->detail = detail;
}

// static
void MockXConnection::InitMapEvent(XEvent* event, XWindow xid) {
  CHECK(event);
  XMapEvent* map_event = &(event->xmap);
  memset(map_event, 0, sizeof(*map_event));
  map_event->type = MapNotify;
  map_event->window = xid;
}

// static
void MockXConnection::InitMapRequestEvent(XEvent* event,
                                          const WindowInfo& info) {
  CHECK(event);
  XMapRequestEvent* req_event = &(event->xmaprequest);
  memset(req_event, 0, sizeof(*req_event));
  req_event->type = MapRequest;
  req_event->window = info.xid;
  req_event->parent = info.parent;
}

// static
void MockXConnection::InitMotionNotifyEvent(XEvent* event,
                                            const WindowInfo& info,
                                            int x, int y) {
  CHECK(event);
  XMotionEvent* motion_event = &(event->xmotion);
  memset(motion_event, 0, sizeof(*motion_event));
  motion_event->type = MotionNotify;
  motion_event->window = info.xid;
  motion_event->x = x;
  motion_event->y = y;
  motion_event->x_root = info.x + x;
  motion_event->y_root = info.y + y;
  // Leave everything else blank for now; we don't use it.
}

// static
void MockXConnection::InitUnmapEvent(XEvent* event, XWindow xid) {
  CHECK(event);
  XUnmapEvent* unmap_event = &(event->xunmap);
  memset(unmap_event, 0, sizeof(*unmap_event));
  unmap_event->type = UnmapNotify;
  unmap_event->window = xid;
}

}  // namespace window_manager
