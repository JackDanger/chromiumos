// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/wm_ipc.h"

#include <cstring>
#include <gdk/gdkx.h>
extern "C" {
#include <X11/Xatom.h>  // for XA_CARDINAL
}

#include "chromeos/obsolete_logging.h"

#include "window_manager/atom_cache.h"
#include "window_manager/util.h"
#include "window_manager/x_connection.h"

namespace chromeos {

WmIpc::WmIpc(XConnection* xconn, AtomCache* cache)
    : xconn_(xconn),
      atom_cache_(cache),
      wm_window_(xconn_->GetSelectionOwner(atom_cache_->GetXAtom(ATOM_WM_S0))) {
  VLOG(1) << "Window manager window is " << XidStr(wm_window_);
}

bool WmIpc::GetWindowType(XWindow xid,
                          WindowType* type,
                          std::vector<int>* params) {
  CHECK(type);
  CHECK(params);

  params->clear();
  std::vector<int> values;
  if (!xconn_->GetIntArrayProperty(
          xid, atom_cache_->GetXAtom(ATOM_CHROME_WINDOW_TYPE), &values)) {
    return false;
  }
  CHECK(!values.empty());
  *type = static_cast<WindowType>(values[0]);
  for (size_t i = 1; i < values.size(); ++i) {
    params->push_back(values[i]);
  }
  return true;
}

bool WmIpc::SetWindowType(
    XWindow xid, WindowType type, const std::vector<int>* params) {
  CHECK_GE(type, 0);
  CHECK_LT(type, kNumWindowTypes);

  std::vector<int> values;
  values.push_back(type);
  if (params) {
    for (size_t i = 0; i < params->size(); ++i) {
      values.push_back((*params)[i]);
    }
  }
  return xconn_->SetIntArrayProperty(
      xid, atom_cache_->GetXAtom(ATOM_CHROME_WINDOW_TYPE), XA_CARDINAL, values);
}

bool WmIpc::SetSystemMetricsProperty(XWindow xid, const std::string& metrics) {
  return xconn_->SetStringProperty(
      xid, atom_cache_->GetXAtom(ATOM_WM_SYSTEM_METRICS), metrics);
}

bool WmIpc::GetMessage(const XClientMessageEvent& e, Message* msg) {
  CHECK(msg);

  // Skip other types of client messages.
  if (e.message_type != atom_cache_->GetXAtom(ATOM_CHROME_WM_MESSAGE)) {
    return false;
  }

  if (e.format != XConnection::kLongFormat) {
    LOG(WARNING) << "Ignoring Chrome OS ClientEventMessage with invalid bit "
                 << "format " << e.format << " (expected 32-bit values)";
    return false;
  }

  msg->set_type(static_cast<Message::Type>(e.data.l[0]));
  if (msg->type() < 0 || msg->type() >= Message::kNumTypes) {
    LOG(WARNING) << "Ignoring Chrome OS ClientEventMessage with invalid "
                 << "message type " << msg->type();
    return false;
  }

  // XClientMessageEvent only gives us five 32-bit items, and we're using
  // the first one for our message type.
  CHECK_LE(msg->max_params(), 4);
  for (int i = 0; i < msg->max_params(); ++i) {
    msg->set_param(i, e.data.l[i+1]);  // l[0] contains message type
  }
  return true;
}

bool WmIpc::GetMessageGdk(const GdkEventClient& e, Message* msg) {
  XEvent xe;
  xe.xclient.type = ClientMessage;
  xe.xclient.serial = 0;  // not provided by GDK
  xe.xclient.send_event = e.send_event;
  xe.xclient.display = GDK_DISPLAY();
  xe.xclient.window = GDK_WINDOW_XWINDOW(e.window);
  xe.xclient.message_type = gdk_x11_atom_to_xatom(e.message_type);
  xe.xclient.format = e.data_format;
  CHECK_EQ(sizeof(e.data.l), sizeof(xe.xclient.data.l));
  memcpy(xe.xclient.data.l, e.data.l, sizeof(xe.xclient.data.l));
  return GetMessage(xe.xclient, msg);
}

bool WmIpc::SendMessage(XWindow xid, const Message& msg) {
  VLOG(2) << "Sending message of type " << msg.type() << " to " << XidStr(xid);

  XEvent e;
  e.xclient.type = ClientMessage;
  e.xclient.window = xid;
  e.xclient.message_type = atom_cache_->GetXAtom(ATOM_CHROME_WM_MESSAGE);
  e.xclient.format = XConnection::kLongFormat;  // 32-bit values
  e.xclient.data.l[0] = msg.type();

  // XClientMessageEvent only gives us five 32-bit items, and we're using
  // the first one for our message type.
  CHECK_LE(msg.max_params(), 4);
  for (int i = 0; i < msg.max_params(); ++i) {
    e.xclient.data.l[i+1] = msg.param(i);
  }

  return xconn_->SendEvent(xid, &e, 0);  // empty event mask
}

}
