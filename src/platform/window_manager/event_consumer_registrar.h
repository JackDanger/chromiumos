// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_EVENT_CONSUMER_REGISTRAR_H_
#define WINDOW_MANAGER_EVENT_CONSUMER_REGISTRAR_H_

#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "window_manager/wm_ipc.h"
#include "window_manager/x_types.h"

namespace window_manager {

class EventConsumer;
class WindowManager;

// This is a RAII-like helper class for EventConsumers that is used to
// register interest in different type of events with the WindowManager
// class.  When EventConsumerRegistrar is destroyed, it unregisters all of
// the interests.
class EventConsumerRegistrar {
 public:
  EventConsumerRegistrar(WindowManager* wm, EventConsumer* event_consumer);
  ~EventConsumerRegistrar();

  // These call the similarly-named
  // WindowManager::RegisterEventConsumerFor*() methods.
  void RegisterForWindowEvents(XWindow xid);
  void RegisterForPropertyChanges(XWindow xid, XAtom xatom);
  void RegisterForChromeMessages(WmIpc::Message::Type message_type);

 private:
  WindowManager* wm_;              // not owned
  EventConsumer* event_consumer_;  // not owned

  // Various interests to be unregistered at destruction.
  std::vector<XWindow> window_event_xids_;
  std::vector<std::pair<XWindow, XAtom> > property_change_pairs_;
  std::vector<WmIpc::Message::Type> chrome_message_types_;

  DISALLOW_COPY_AND_ASSIGN(EventConsumerRegistrar);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_EVENT_CONSUMER_REGISTRAR_H_
