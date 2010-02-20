// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/event_consumer_registrar.h"

#include "window_manager/event_consumer.h"
#include "window_manager/window_manager.h"

namespace window_manager {

using std::make_pair;
using std::pair;
using std::vector;

EventConsumerRegistrar::EventConsumerRegistrar(WindowManager* wm,
                                               EventConsumer* event_consumer)
    : wm_(wm),
      event_consumer_(event_consumer) {
}

EventConsumerRegistrar::~EventConsumerRegistrar() {
  for (vector<XWindow>::const_iterator it = window_event_xids_.begin();
       it != window_event_xids_.end(); ++it) {
    wm_->UnregisterEventConsumerForWindowEvents(*it, event_consumer_);
  }
  for (vector<pair<XWindow, XAtom> >::const_iterator it =
         property_change_pairs_.begin();
       it != property_change_pairs_.end(); ++it) {
    wm_->UnregisterEventConsumerForPropertyChanges(
        it->first, it->second, event_consumer_);
  }
  for (vector<WmIpc::Message::Type>::const_iterator it =
         chrome_message_types_.begin();
       it != chrome_message_types_.end(); ++it) {
    wm_->UnregisterEventConsumerForChromeMessages(*it, event_consumer_);
  }

  wm_ = NULL;
  event_consumer_ = NULL;
}

void EventConsumerRegistrar::RegisterForWindowEvents(XWindow xid) {
  wm_->RegisterEventConsumerForWindowEvents(xid, event_consumer_);
  window_event_xids_.push_back(xid);
}

void EventConsumerRegistrar::RegisterForPropertyChanges(XWindow xid,
                                                        XAtom xatom) {
  wm_->RegisterEventConsumerForPropertyChanges(xid, xatom, event_consumer_);
  property_change_pairs_.push_back(make_pair(xid, xatom));
}

void EventConsumerRegistrar::RegisterForChromeMessages(
    WmIpc::Message::Type message_type) {
  wm_->RegisterEventConsumerForChromeMessages(message_type, event_consumer_);
  chrome_message_types_.push_back(message_type);
}

}  // namespace window_manager
