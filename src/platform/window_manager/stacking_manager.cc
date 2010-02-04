// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/stacking_manager.h"

#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/x_connection.h"

using std::map;
using std::set;
using std::tr1::shared_ptr;

namespace window_manager {

StackingManager::StackingManager(XConnection* xconn, ClutterInterface* clutter)
    : xconn_(xconn) {
  XWindow root = xconn_->GetRootWindow();

  for (int i = kNumLayers - 1; i >= 0; --i) {
    Layer layer = static_cast<Layer>(i);

    XWindow xid = xconn_->CreateWindow(root, -1, -1, 1, 1, true, true, 0);
    layer_to_xid_[layer] = xid;
    xids_.insert(xid);

    shared_ptr<ClutterInterface::Actor> actor(clutter->CreateGroup());
    actor->SetName(LayerToName(layer));
    actor->SetVisibility(false);
    clutter->GetDefaultStage()->AddActor(actor.get());
    actor->RaiseToTop();
    layer_to_actor_[layer] = actor;
  }
}

StackingManager::~StackingManager() {
  for (set<XWindow>::const_iterator it = xids_.begin(); it != xids_.end(); ++it)
    xconn_->DestroyWindow(*it);
}

bool StackingManager::StackWindowAtTopOfLayer(Window* win, Layer layer) {
  CHECK(win);

  XWindow layer_xid =
      FindWithDefault(layer_to_xid_, layer, static_cast<XWindow>(None));
  CHECK(layer_xid != None) << "Invalid layer " << layer;
  bool result = win->StackClientBelow(layer_xid);

  shared_ptr<ClutterInterface::Actor> layer_actor =
      FindWithDefault(
          layer_to_actor_, layer, shared_ptr<ClutterInterface::Actor>());
  CHECK(layer_actor.get()) << "Invalid layer " << layer;

  // Find the next-lowest layer so we can stack the window's shadow
  // directly above it.
  // TODO: This won't work for the bottom layer; write additional code to
  // handle it if it ever becomes necessary.
  Layer lower_layer = static_cast<Layer>(layer + 1);
  shared_ptr<ClutterInterface::Actor> lower_layer_actor =
      FindWithDefault(
          layer_to_actor_, lower_layer, shared_ptr<ClutterInterface::Actor>());
  win->StackCompositedBelow(layer_actor.get(), lower_layer_actor.get(), true);

  return result;
}

bool StackingManager::StackXidAtTopOfLayer(XWindow xid, Layer layer) {
  XWindow ref_xid = FindWithDefault(
      layer_to_xid_, layer, static_cast<XWindow>(None));
  CHECK(ref_xid != None) << "Invalid layer " << layer;
  return xconn_->StackWindow(xid, ref_xid, false);  // above=false
}

void StackingManager::StackActorAtTopOfLayer(
    ClutterInterface::Actor* actor, Layer layer) {
  shared_ptr<ClutterInterface::Actor> ref_actor =
      FindWithDefault(
          layer_to_actor_, layer, shared_ptr<ClutterInterface::Actor>());
  CHECK(ref_actor.get()) << "Invalid layer " << layer;
  actor->Lower(ref_actor.get());
}

// static
const char* StackingManager::LayerToName(Layer layer) {
  switch (layer) {
    case LAYER_DEBUGGING:        return "debugging layer";
    case LAYER_HOTKEY_OVERLAY:   return "hotkey overlay layer";
    case LAYER_PANEL_ANCHOR:     return "panel anchor layer";
    case LAYER_DRAGGED_PANEL:    return "dragged panel layer";
    case LAYER_STATIONARY_PANEL: return "stationary panel layer";
    case LAYER_FLOATING_TAB:     return "floating tab layer";
    case LAYER_TAB_SUMMARY:      return "tab summary layer";
    case LAYER_TOPLEVEL_WINDOW:  return "toplevel window layer";
    case LAYER_BACKGROUND:       return "background layer";
    default:                     return "unknown layer";
  }
}

}  // namespace window_manager
