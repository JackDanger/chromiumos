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
    actor->SetName(StringPrintf("%s layer", LayerToName(layer)));
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
  DCHECK(win);

  ClutterInterface::Actor* layer_actor = GetActorForLayer(layer);

  // Find the next-lowest layer so we can stack the window's shadow
  // directly above it.
  // TODO: This won't work for the bottom layer; write additional code to
  // handle it if it ever becomes necessary.
  ClutterInterface::Actor* lower_layer_actor =
      GetActorForLayer(static_cast<Layer>(layer + 1));
  win->StackCompositedBelow(layer_actor, lower_layer_actor, true);

  XWindow layer_xid = GetXidForLayer(layer);
  return win->StackClientBelow(layer_xid);
}

bool StackingManager::StackXidAtTopOfLayer(XWindow xid, Layer layer) {
  XWindow layer_xid = GetXidForLayer(layer);
  return xconn_->StackWindow(xid, layer_xid, false);  // above=false
}

void StackingManager::StackActorAtTopOfLayer(
    ClutterInterface::Actor* actor, Layer layer) {
  DCHECK(actor);
  ClutterInterface::Actor* layer_actor = GetActorForLayer(layer);
  actor->Lower(layer_actor);
}

bool StackingManager::StackWindowRelativeToOtherWindow(
    Window* win, Window* sibling, bool above, Layer layer) {
  DCHECK(win);
  DCHECK(sibling);

  ClutterInterface::Actor* lower_layer_actor =
      GetActorForLayer(static_cast<Layer>(layer + 1));
  if (above)
    win->StackCompositedAbove(sibling->actor(), lower_layer_actor, true);
  else
    win->StackCompositedBelow(sibling->actor(), lower_layer_actor, true);

  return above ?
         win->StackClientAbove(sibling->xid()) :
         win->StackClientBelow(sibling->xid());
}

// static
const char* StackingManager::LayerToName(Layer layer) {
  switch (layer) {
    case LAYER_DEBUGGING:                return "debugging";
    case LAYER_HOTKEY_OVERLAY:           return "hotkey overlay";
    case LAYER_DRAGGED_PANEL:            return "dragged panel";
    case LAYER_ACTIVE_TRANSIENT_WINDOW:  return "active transient window";
    case LAYER_PANEL_BAR_INPUT_WINDOW:   return "panel bar input window";
    case LAYER_STATIONARY_PANEL_IN_BAR:  return "static panel in bar";
    case LAYER_STATIONARY_PANEL_IN_DOCK: return "stationary panel in dock";
    case LAYER_FLOATING_TAB:             return "floating tab";
    case LAYER_TAB_SUMMARY:              return "tab summary";
    case LAYER_TOPLEVEL_WINDOW:          return "toplevel window";
    case LAYER_BACKGROUND:               return "background";
    default:                             return "unknown";
  }
}

ClutterInterface::Actor* StackingManager::GetActorForLayer(Layer layer) {
  shared_ptr<ClutterInterface::Actor> layer_actor =
      FindWithDefault(
          layer_to_actor_, layer, shared_ptr<ClutterInterface::Actor>());
  CHECK(layer_actor.get()) << "Invalid layer " << layer;
  return layer_actor.get();
}

XWindow StackingManager::GetXidForLayer(Layer layer) {
  XWindow xid = FindWithDefault(
      layer_to_xid_, layer, static_cast<XWindow>(None));
  CHECK(xid != None) << "Invalid layer " << layer;
  return xid;
}

}  // namespace window_manager
