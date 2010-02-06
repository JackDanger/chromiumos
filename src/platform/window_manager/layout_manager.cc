// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/layout_manager.h"

#include <algorithm>
#include <cmath>
#include <tr1/memory>
extern "C" {
#include <X11/Xatom.h>
}

#include <gflags/gflags.h>

#include "chromeos/callback.h"
#include "base/string_util.h"
#include "base/logging.h"
#include "window_manager/atom_cache.h"
#include "window_manager/motion_event_coalescer.h"
#include "window_manager/stacking_manager.h"
#include "window_manager/system_metrics.pb.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"
#include "window_manager/x_connection.h"

DEFINE_bool(lm_honor_window_size_hints, false,
            "When maximizing a client window, constrain its size according to "
            "the size hints that the client app has provided (e.g. max size, "
            "size increment, etc.) instead of automatically making it fill the "
            "screen");

DEFINE_bool(lm_new_overview_mode, false, "Use the new overview mode");

DEFINE_string(lm_overview_gradient_image,
              "../assets/images/window_overview_gradient.png",
              "Image to use for gradients on inactive windows in "
              "overview mode");

namespace window_manager {

using std::map;
using std::vector;
using std::pair;
using std::make_pair;
using std::list;
using std::tr1::shared_ptr;

using chromeos::NewPermanentCallback;

// Amount of padding that should be used between windows in overview mode.
static const int kWindowPadding = 10;

// What's the maximum fraction of the manager's total size that a window
// should be scaled to in overview mode?
static const double kOverviewWindowMaxSizeRatio = 0.5;

// What fraction of the manager's total width should each window use for
// peeking out underneath the window on top of it in overview mode?
static const double kOverviewExposedWindowRatio = 0.1;

// Animation speed for windows in new overview mode.
static const int kOverviewAnimMs = 100;

// Padding between the create browser window and the bottom of the screen.
static const int kCreateBrowserWindowVerticalPadding = 10;

// Amount of vertical padding that should be used between tab summary
// windows and overview windows.
static const int kTabSummaryPadding = 40;

// Maximum height that an unmagnified window can have in overview mode,
// relative to the height of the entire area used for displaying windows.
static const double kMaxWindowHeightRatio = 0.75;

// Animation speed used for windows.
static const int kWindowAnimMs = 200;

// Duration between position redraws while a tab is being dragged.
static const int kFloatingTabUpdateMs = 50;

// Duration between panning updates while a drag is occurring on the
// background window in overview mode.
static const int kOverviewDragUpdateMs = 50;

// Maximum fraction of the total height that magnified windows can take up
// in overview mode.
static const double kOverviewHeightFraction = 0.3;

// When animating a window zooming out while switching windows, what size
// should it scale to?
static const double kWindowFadeSizeFraction = 0.5;

ClutterInterface::Actor*
    LayoutManager::ToplevelWindow::static_gradient_texture_ = NULL;

LayoutManager::LayoutManager
    (WindowManager* wm, int x, int y, int width, int height)
    : wm_(wm),
      mode_(MODE_ACTIVE),
      x_(x),
      y_(y),
      width_(-1),
      height_(-1),
      overview_height_(-1),
      magnified_toplevel_(NULL),
      active_toplevel_(NULL),
      floating_tab_(NULL),
      toplevel_under_floating_tab_(NULL),
      tab_summary_(NULL),
      create_browser_window_(NULL),
      overview_panning_offset_(0),
      floating_tab_event_coalescer_(
          new MotionEventCoalescer(
              NewPermanentCallback(this, &LayoutManager::MoveFloatingTab),
              kFloatingTabUpdateMs)),
      overview_background_event_coalescer_(
          new MotionEventCoalescer(
              NewPermanentCallback(
                  this, &LayoutManager::UpdateOverviewPanningForMotion),
              kOverviewDragUpdateMs)),
      overview_drag_last_x_(-1),
      saw_map_request_(false) {
  Resize(width, height);

  if (FLAGS_lm_new_overview_mode) {
    int event_mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    wm_->xconn()->AddButtonGrabOnWindow(
        wm_->background_xid(), 1, event_mask, false);
  }

  KeyBindings* kb = wm_->key_bindings();
  kb->AddAction(
      "switch-to-overview-mode",
      NewPermanentCallback(this, &LayoutManager::SetMode, MODE_OVERVIEW),
      NULL, NULL);
  kb->AddAction(
      "switch-to-active-mode",
      NewPermanentCallback(this, &LayoutManager::SwitchToActiveMode, false),
      NULL, NULL);
  kb->AddAction(
      "cycle-active-forward",
      NewPermanentCallback(
          this, &LayoutManager::CycleActiveToplevelWindow, true),
      NULL, NULL);
  kb->AddAction(
      "cycle-active-backward",
      NewPermanentCallback(
          this, &LayoutManager::CycleActiveToplevelWindow, false),
      NULL, NULL);
  kb->AddAction(
      "cycle-magnification-forward",
      NewPermanentCallback(
          this, &LayoutManager::CycleMagnifiedToplevelWindow, true),
      NULL, NULL);
  kb->AddAction(
      "cycle-magnification-backward",
      NewPermanentCallback(
          this, &LayoutManager::CycleMagnifiedToplevelWindow, false),
      NULL, NULL);
  kb->AddAction(
      "switch-to-active-mode-for-magnified",
      NewPermanentCallback(this, &LayoutManager::SwitchToActiveMode, true),
      NULL, NULL);
  for (int i = 0; i < 8; ++i) {
    kb->AddAction(
        StringPrintf("activate-toplevel-with-index-%d", i),
        NewPermanentCallback(
            this, &LayoutManager::ActivateToplevelWindowByIndex, i),
        NULL, NULL);
    kb->AddAction(
        StringPrintf("magnify-toplevel-with-index-%d", i),
        NewPermanentCallback(
            this, &LayoutManager::MagnifyToplevelWindowByIndex, i),
        NULL, NULL);
  }
  kb->AddAction(
      "activate-last-toplevel",
      NewPermanentCallback(
          this, &LayoutManager::ActivateToplevelWindowByIndex, -1),
      NULL, NULL);
  kb->AddAction(
      "magnify-last-toplevel",
      NewPermanentCallback(
          this, &LayoutManager::MagnifyToplevelWindowByIndex, -1),
      NULL, NULL);
  kb->AddAction(
      "delete-active-window",
      NewPermanentCallback(
          this, &LayoutManager::SendDeleteRequestToActiveWindow),
      NULL, NULL);
  kb->AddAction(
      "pan-overview-mode-left",
      NewPermanentCallback(this, &LayoutManager::PanOverviewMode, -50),
      NULL, NULL);
  kb->AddAction(
      "pan-overview-mode-right",
      NewPermanentCallback(this, &LayoutManager::PanOverviewMode, 50),
      NULL, NULL);

  SetMode(MODE_ACTIVE);
}

LayoutManager::~LayoutManager() {
  if (FLAGS_lm_new_overview_mode)
    wm_->xconn()->RemoveButtonGrabOnWindow(wm_->background_xid(), 1);

  KeyBindings* kb = wm_->key_bindings();
  kb->RemoveAction("switch-to-overview-mode");
  kb->RemoveAction("switch-to-active-mode");
  kb->RemoveAction("cycle-active-forward");
  kb->RemoveAction("cycle-active-backward");
  kb->RemoveAction("cycle-magnification-forward");
  kb->RemoveAction("cycle-magnification-backward");
  kb->RemoveAction("switch-to-active-mode-for-magnified");
  kb->RemoveAction("delete-active-window");
  kb->RemoveAction("pan-overview-mode-left");
  kb->RemoveAction("pan-overview-mode-right");

  toplevels_.clear();
  magnified_toplevel_ = NULL;
  active_toplevel_ = NULL;
  floating_tab_ = NULL;
  toplevel_under_floating_tab_ = NULL;
  tab_summary_ = NULL;
}

bool LayoutManager::IsInputWindow(XWindow xid) {
  return (GetToplevelWindowByInputXid(xid) != NULL);
}

bool LayoutManager::HandleWindowMapRequest(Window* win) {
  saw_map_request_ = true;
  if (!IsHandledWindowType(win->type()))
    return false;

  DoInitialSetupForWindow(win);
  win->MapClient();
  return true;
}

void LayoutManager::HandleWindowMap(Window* win) {
  CHECK(win);

  // Just show override-redirect windows; they're already positioned
  // according to client apps' wishes.
  if (win->override_redirect()) {
    // Make tab summary windows fade in -- this hides the period between
    // them getting mapped and them getting painted in response to the
    // first expose event.
    if (win->type() == WmIpc::WINDOW_TYPE_CHROME_TAB_SUMMARY) {
      // TODO: This is wrong (restacking an override-redirect window), but
      // the proper fix is probably to make this window not be
      // override-redirect in Chrome and to give it an alternate mechanism
      // to provide the appropriate position to us.
      wm_->stacking_manager()->StackWindowAtTopOfLayer(
          win, StackingManager::LAYER_TAB_SUMMARY);
      win->SetCompositedOpacity(0, 0);
      win->ShowComposited();
      win->SetCompositedOpacity(1, kWindowAnimMs);
      tab_summary_ = win;
    } else {
      win->ShowComposited();
    }
    return;
  }

  if (!IsHandledWindowType(win->type()))
    return;

  // Perform initial setup of windows that were already mapped at startup
  // (so we never saw MapRequest events for them).
  if (!saw_map_request_)
    DoInitialSetupForWindow(win);

  switch (win->type()) {
    // TODO: Remove this.  mock_chrome currently depends on the WM to
    // position tab summary windows, but Chrome just creates
    // override-redirect ("popup") windows and positions them itself.
    case WmIpc::WINDOW_TYPE_CHROME_TAB_SUMMARY: {
      int x = (width_ - win->client_width()) / 2;
      int y = y_ + height_ - overview_height_ - win->client_height() -
          kTabSummaryPadding;
      win->MoveComposited(x, y, 0);
      win->ScaleComposited(1.0, 1.0, 0);
      win->SetCompositedOpacity(0, 0);
      win->ShowComposited();
      win->SetCompositedOpacity(0.75, kWindowAnimMs);
      win->MoveClient(x, y);
      tab_summary_ = win;
      break;
    }
    case WmIpc::WINDOW_TYPE_CHROME_FLOATING_TAB: {
      wm_->stacking_manager()->StackWindowAtTopOfLayer(
          win, StackingManager::LAYER_FLOATING_TAB);
      win->ScaleComposited(1.0, 1.0, 0);
      win->SetCompositedOpacity(0.75, 0);
      // No worries if we were already tracking a different tab; it should
      // get destroyed soon enough.
      if (floating_tab_)
        floating_tab_->HideComposited();
      floating_tab_ = win;
      if (!floating_tab_event_coalescer_->IsRunning()) {
        // Start redrawing the tab's position if we aren't already.
        VLOG(2) << "Starting update loop for floating tab drag";
        floating_tab_event_coalescer_->Start();
      }
      if (win->type_params().size() >= 2) {
        floating_tab_event_coalescer_->StorePosition(
            win->type_params()[0], win->type_params()[1]);
      }
      break;
    }
    case WmIpc::WINDOW_TYPE_CREATE_BROWSER_WINDOW: {
      if (create_browser_window_) {
        LOG(WARNING) << "Got second create-browser window " << win->xid_str()
                     << " (previous was " << create_browser_window_->xid_str()
                     << ")";
        create_browser_window_->HideComposited();
      }
      create_browser_window_ = win;
      wm_->stacking_manager()->StackWindowAtTopOfLayer(
          create_browser_window_, StackingManager::LAYER_TOPLEVEL_WINDOW);
      if (mode_ == MODE_OVERVIEW) {
        create_browser_window_->ShowComposited();
        LayoutToplevelWindowsForOverviewMode(-1);
      }
      break;
    }
    case WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL:
    case WmIpc::WINDOW_TYPE_CHROME_INFO_BUBBLE:
    case WmIpc::WINDOW_TYPE_UNKNOWN: {
      if (win->transient_for_xid() != None) {
        ToplevelWindow* toplevel_owner =
            GetToplevelWindowByXid(win->transient_for_xid());
        if (toplevel_owner) {
          transient_to_toplevel_[win->xid()] = toplevel_owner;
          toplevel_owner->AddTransientWindow(win);

          if (mode_ == MODE_ACTIVE &&
              active_toplevel_ != NULL &&
              active_toplevel_->IsWindowOrTransientFocused()) {
            active_toplevel_->TakeFocus(wm_->GetCurrentTimeFromServer());
          }
          break;
        } else {
          LOG(WARNING) << "Ignoring " << win->xid_str()
                       << "'s WM_TRANSIENT_FOR hint of "
                       << XidStr(win->transient_for_xid())
                       << ", which isn't a toplevel window";
          // Continue on and treat the transient as a toplevel window.
        }
      }

      shared_ptr<ToplevelWindow> toplevel(
          new ToplevelWindow(win, this));
      input_to_toplevel_[toplevel->input_xid()] = toplevel.get();

      switch (mode_) {
        case MODE_ACTIVE:
          // Activate the new window, adding it to the right of the
          // currently-active window.
          if (active_toplevel_) {
            int old_index = GetIndexForToplevelWindow(*active_toplevel_);
            CHECK_GE(old_index, 0);
            ToplevelWindows::iterator it = toplevels_.begin() + old_index + 1;
            toplevels_.insert(it, toplevel);
          } else {
            toplevels_.push_back(toplevel);
          }
          SetActiveToplevelWindow(
              toplevel.get(),
              ToplevelWindow::STATE_ACTIVE_MODE_IN_FROM_RIGHT,
              ToplevelWindow::STATE_ACTIVE_MODE_OUT_TO_LEFT);
          break;
        case MODE_OVERVIEW:
          // In overview mode, just put new windows on the right.
          toplevels_.push_back(toplevel);
          LayoutToplevelWindowsForOverviewMode(-1);
          break;
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected window type " << win->type();
      break;
  }
}

void LayoutManager::HandleWindowUnmap(Window* win) {
  CHECK(win);

  // If necessary, reset some pointers to non-toplevels windows first.
  if (floating_tab_ == win) {
    if (floating_tab_event_coalescer_->IsRunning()) {
      VLOG(2) << "Stopping update loop for floating tab drag";
      floating_tab_event_coalescer_->Stop();
    }
    floating_tab_ = NULL;
  }
  if (tab_summary_ == win)
    tab_summary_ = NULL;
  if (create_browser_window_ == win) {
    create_browser_window_ = NULL;
    if (mode_ == MODE_OVERVIEW)
      LayoutToplevelWindowsForOverviewMode(-1);
  }

  ToplevelWindow* toplevel_owner = GetToplevelWindowOwningTransientWindow(*win);
  if (toplevel_owner) {
    bool transient_had_focus = win->focused();
    toplevel_owner->RemoveTransientWindow(win);
    if (transient_to_toplevel_.erase(win->xid()) != 1)
      LOG(WARNING) << "No transient-to-toplevel mapping for " << win->xid_str();
    if (transient_had_focus)
      toplevel_owner->TakeFocus(wm_->GetCurrentTimeFromServer());
  }

  ToplevelWindow* toplevel = GetToplevelWindowByWindow(*win);
  if (toplevel) {
    if (magnified_toplevel_ == toplevel)
      SetMagnifiedToplevelWindow(NULL);
    if (active_toplevel_ == toplevel)
      active_toplevel_ = NULL;
    if (toplevel_under_floating_tab_ == toplevel)
      toplevel_under_floating_tab_ = NULL;

    const int index = GetIndexForToplevelWindow(*toplevel);
    CHECK_EQ(input_to_toplevel_.erase(toplevel->input_xid()), 1);
    toplevels_.erase(toplevels_.begin() + index);

    if (mode_ == MODE_OVERVIEW) {
      LayoutToplevelWindowsForOverviewMode(-1);
    } else if (mode_ == MODE_ACTIVE) {
      // If there's no active window now, then this was probably active
      // previously.  Choose a new active window if possible; relinquish
      // the focus otherwise.
      if (!active_toplevel_) {
        if (!toplevels_.empty()) {
          const int new_index =
              (index + toplevels_.size() - 1) % toplevels_.size();
          SetActiveToplevelWindow(
              toplevels_[new_index].get(),
              ToplevelWindow::STATE_ACTIVE_MODE_IN_FROM_LEFT,
              ToplevelWindow::STATE_ACTIVE_MODE_OUT_TO_RIGHT);
        } else {
          if (win->focused()) {
            wm_->SetActiveWindowProperty(None);
            wm_->TakeFocus();
          }
        }
      }
    }
  }
}

bool LayoutManager::HandleWindowConfigureRequest(
    Window* win, int req_x, int req_y, int req_width, int req_height) {
  ToplevelWindow* toplevel_owner = GetToplevelWindowOwningTransientWindow(*win);
  if (toplevel_owner) {
    toplevel_owner->HandleTransientWindowConfigureRequest(
        win, req_x, req_y, req_width, req_height);
    return true;
  }

  // Let toplevel windows resize themselves to work around issue 449, where
  // the Chrome options window doesn't repaint if it doesn't get resized
  // after it's already mapped.
  // TODO: Remove this after Chrome has been fixed.
  ToplevelWindow* toplevel = GetToplevelWindowByWindow(*win);
  if (toplevel) {
    if (req_width != toplevel->win()->client_width() ||
        req_height != toplevel->win()->client_height()) {
      toplevel->win()->ResizeClient(
          req_width, req_height, Window::GRAVITY_NORTHWEST);
    }
    return true;
  }

  return false;
}

bool LayoutManager::HandleButtonPress(XWindow xid,
                                      int x, int y,
                                      int x_root, int y_root,
                                      int button,
                                      Time timestamp) {
  ToplevelWindow* toplevel = GetToplevelWindowByInputXid(xid);
  if (toplevel) {
    if (button == 1) {
      if (FLAGS_lm_new_overview_mode && toplevel != magnified_toplevel_) {
        SetMagnifiedToplevelWindow(toplevel);
        LayoutToplevelWindowsForOverviewMode(std::max(x_root - x_, 0));
      } else {
        active_toplevel_ = toplevel;
        SetMode(MODE_ACTIVE);
      }
    }
    return true;
  }

  if (xid == wm_->background_xid() && button == 1) {
    overview_drag_last_x_ = x;
    overview_background_event_coalescer_->Start();
    return true;
  }

  // Otherwise, it probably means that the user previously focused a panel
  // and then clicked back on a toplevel or transient window.
  Window* win = wm_->GetWindow(xid);
  if (!win)
    return false;
  toplevel = GetToplevelWindowOwningTransientWindow(*win);
  if (!toplevel)
    toplevel = GetToplevelWindowByWindow(*win);
  if (!toplevel)
    return false;

  toplevel->HandleButtonPress(win, timestamp);
  return true;
}

bool LayoutManager::HandleButtonRelease(XWindow xid,
                                        int x, int y,
                                        int x_root, int y_root,
                                        int button,
                                        Time timestamp) {
  if (xid != wm_->background_xid() || button != 1)
    return false;

  // The X server automatically removes our asynchronous pointer grab when
  // the mouse buttons are released.
  overview_background_event_coalescer_->Stop();

  // We need to do one last configure to update the input windows'
  // positions, which we didn't bother doing while panning.
  ConfigureWindowsForOverviewMode(false);

  return true;
}

bool LayoutManager::HandlePointerEnter(XWindow xid,
                                       int x, int y,
                                       int x_root, int y_root,
                                       Time timestamp) {
  ToplevelWindow* toplevel = GetToplevelWindowByInputXid(xid);
  if (!toplevel)
    return false;
  if (mode_ != MODE_OVERVIEW)
    return true;
  if (!FLAGS_lm_new_overview_mode && toplevel != magnified_toplevel_) {
    SetMagnifiedToplevelWindow(toplevel);
    LayoutToplevelWindowsForOverviewMode(-1);
    SendTabSummaryMessage(toplevel, true);
  }
  return true;
}

bool LayoutManager::HandlePointerLeave(XWindow xid,
                                       int x, int y,
                                       int x_root, int y_root,
                                       Time timestamp) {
  // TODO: Decide if we want to unmagnify the window here or not.
  return (GetToplevelWindowByInputXid(xid) != NULL);
}

bool LayoutManager::HandleFocusChange(XWindow xid, bool focus_in) {
  Window* win = wm_->GetWindow(xid);
  if (!win)
    return false;

  ToplevelWindow* toplevel = GetToplevelWindowOwningTransientWindow(*win);
  if (!toplevel)
    toplevel = GetToplevelWindowByWindow(*win);

  // If this is neither a toplevel nor transient window, we don't care
  // about the focus change.
  if (!toplevel)
    return false;
  toplevel->HandleFocusChange(win, focus_in);

  // Announce that the new window is the "active" window (in the
  // _NET_ACTIVE_WINDOW sense), regardless of whether it's a toplevel
  // window or a transient.
  if (focus_in)
    wm_->SetActiveWindowProperty(win->xid());

  return true;
}

bool LayoutManager::HandlePointerMotion(XWindow xid,
                                        int x, int y,
                                        int x_root, int y_root,
                                        Time timestamp) {
  if (xid != wm_->background_xid())
    return false;

  overview_background_event_coalescer_->StorePosition(x, y);
  return true;
}

bool LayoutManager::HandleChromeMessage(const WmIpc::Message& msg) {
  switch (msg.type()) {
    case WmIpc::Message::WM_MOVE_FLOATING_TAB: {
      XWindow xid = msg.param(0);
      int x = msg.param(1);
      int y = msg.param(2);
      if (!floating_tab_ || xid != floating_tab_->xid()) {
        LOG(WARNING) << "Ignoring request to move unknown floating tab "
                     << XidStr(xid) << " (current is "
                     << XidStr(floating_tab_ ? floating_tab_->xid() : 0) << ")";
      } else {
        floating_tab_event_coalescer_->StorePosition(x, y);
      }
      break;
    }
    case WmIpc::Message::WM_FOCUS_WINDOW: {
      XWindow xid = msg.param(0);
      Window* win = wm_->GetWindow(xid);
      if (!win)
        return false;

      ToplevelWindow* toplevel = GetToplevelWindowByWindow(*win);
      if (!toplevel)
        return false;

      active_toplevel_ = toplevel;
      SetMode(MODE_ACTIVE);
      break;
    }
    case WmIpc::Message::WM_SWITCH_TO_OVERVIEW_MODE: {
      SetMode(MODE_OVERVIEW);
      Window* win = wm_->GetWindow(msg.param(0));
      if (!win) {
        LOG(WARNING) << "Ignoring request to magnify unknown window "
                     << XidStr(msg.param(0))
                     << " while switching to overview mode";
        return true;
      }

      ToplevelWindow* toplevel = GetToplevelWindowByWindow(*win);
      if (!toplevel) {
        LOG(WARNING) << "Ignoring request to magnify non-toplevel window "
                     << XidStr(msg.param(0))
                     << " while switching to overview mode";
        return true;
      }

      SetMagnifiedToplevelWindow(toplevel);
      if (!FLAGS_lm_new_overview_mode)
        SendTabSummaryMessage(toplevel, true);
      break;
    }
    default:
      return false;
  }
  return true;
}

bool LayoutManager::HandleClientMessage(const XClientMessageEvent& e) {
  Window* win = wm_->GetWindow(e.window);
  if (!win)
    return false;

  if (e.message_type == wm_->GetXAtom(ATOM_NET_WM_STATE)) {
    win->HandleWmStateMessage(e);
    return true;
  }

  if (e.message_type == wm_->GetXAtom(ATOM_NET_ACTIVE_WINDOW)) {
    if (e.format != XConnection::kLongFormat)
      return true;
    VLOG(1) << "Got _NET_ACTIVE_WINDOW request to focus " << XidStr(e.window)
            << " (requestor says its currently-active window is "
            << XidStr(e.data.l[2]) << "; real active window is "
            << XidStr(wm_->active_window_xid()) << ")";
    // If we got a _NET_ACTIVE_WINDOW request for a transient, switch to
    // its owner instead.
    ToplevelWindow* toplevel = GetToplevelWindowOwningTransientWindow(*win);
    if (toplevel)
      toplevel->SetPreferredTransientWindowToFocus(win);
    else
      toplevel = GetToplevelWindowByWindow(*win);

    // If we don't know anything about this window, give up.
    if (!toplevel)
      return false;

    if (toplevel != active_toplevel_) {
      SetActiveToplevelWindow(toplevel,
                              ToplevelWindow::STATE_ACTIVE_MODE_IN_FADE,
                              ToplevelWindow::STATE_ACTIVE_MODE_OUT_FADE);
    } else {
      toplevel->TakeFocus(e.data.l[1]);
    }
    return true;
  }

  return false;
}

Window* LayoutManager::GetChromeWindow() {
  for (size_t i = 0; i < toplevels_.size(); ++i) {
    if (toplevels_[i]->win()->type() == WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL)
      return toplevels_[i]->win();
  }
  return NULL;
}

void LayoutManager::MoveFloatingTab() {
  // TODO: Making a bunch of calls to clutter_actor_move() (say, to update
  // the floating tab's Clutter actor's position in response to mouse
  // motion) kills the performance of any animations that are going on.
  // This looks like it's correlated to the mouse sampling rate -- it's
  // less of an issue when running under Xephyr, but quite noticeable if
  // we're talking to the real X server.  Always passing a short duration
  // so that we use implicit animations instead doesn't help.  We
  // rate-limit how often this method is invoked to actually move the
  // floating tab as a workaround.

  if (!floating_tab_) {
    LOG(WARNING) << "Ignoring request to animate floating tab since none "
                 << "is present";
    return;
  }

  int x = floating_tab_event_coalescer_->x();
  int y = floating_tab_event_coalescer_->y();

  if (x == floating_tab_->composited_x() &&
      y == floating_tab_->composited_y()) {
    return;
  }

  if (!floating_tab_->composited_shown())
    floating_tab_->ShowComposited();
  int x_offset = 0, y_offset = 0;
  if (floating_tab_->type_params().size() >= 4) {
    x_offset = floating_tab_->type_params()[2];
    y_offset = floating_tab_->type_params()[3];
  }
  floating_tab_->MoveComposited(x - x_offset, y - y_offset, 0);

  if (mode_ == MODE_OVERVIEW) {
    ToplevelWindow* toplevel = GetOverviewToplevelWindowAtPoint(x, y);

    // If the user is moving the pointer up to the tab summary, pretend
    // like the pointer is still in the magnified window.
    if (!toplevel && magnified_toplevel_) {
      if (PointIsInTabSummary(x, y) ||
          PointIsBetweenMagnifiedToplevelWindowAndTabSummary(x, y)) {
        toplevel = magnified_toplevel_;
      }
    }

    // Only allow docking into Chrome windows.
    if (toplevel &&
        toplevel->win()->type() != WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL) {
      toplevel = NULL;
    }

    if (toplevel != toplevel_under_floating_tab_) {
      // Notify the old and new toplevel windows about the new position.
      if (toplevel_under_floating_tab_) {
        WmIpc::Message msg(
            WmIpc::Message::CHROME_NOTIFY_FLOATING_TAB_OVER_TOPLEVEL);
        msg.set_param(0, floating_tab_->xid());
        msg.set_param(1, 0);  // left
        wm_->wm_ipc()->SendMessage(
            toplevel_under_floating_tab_->win()->xid(), msg);
      }
      if (toplevel) {
        WmIpc::Message msg(
            WmIpc::Message::CHROME_NOTIFY_FLOATING_TAB_OVER_TOPLEVEL);
        msg.set_param(0, floating_tab_->xid());
        msg.set_param(1, 1);  // entered
        wm_->wm_ipc()->SendMessage(toplevel->win()->xid(), msg);
      }
      toplevel_under_floating_tab_ = toplevel;
      SetMagnifiedToplevelWindow(toplevel);
      LayoutToplevelWindowsForOverviewMode(-1);
      SendTabSummaryMessage(toplevel, true);
    }

    if (PointIsInTabSummary(x, y)) {
      WmIpc::Message msg(
          WmIpc::Message::CHROME_NOTIFY_FLOATING_TAB_OVER_TAB_SUMMARY);
      msg.set_param(0, floating_tab_->xid());
      msg.set_param(1, 1);  // currently in window
      msg.set_param(2, x - tab_summary_->client_x());
      msg.set_param(3, y - tab_summary_->client_y());
      wm_->wm_ipc()->SendMessage(tab_summary_->xid(), msg);
    }
    // TODO: Also send a message when we move out of the summary.

  } else if (mode_ == MODE_ACTIVE) {
    if (y > (y_ + height_ - (kMaxWindowHeightRatio * overview_height_)) &&
        y < y_ + height_) {
      // Go into overview mode if the tab is dragged into the bottom area.
      SetMode(MODE_OVERVIEW);
    }
  }
}

bool LayoutManager::TakeFocus() {
  if (mode_ != MODE_ACTIVE || !active_toplevel_)
    return false;

  active_toplevel_->TakeFocus(wm_->GetCurrentTimeFromServer());
  return true;
}

void LayoutManager::Resize(int width, int height) {
  if (width == width_ && height == height_)
    return;

  width_ = width;
  height_ = height;
  overview_height_ = kOverviewHeightFraction * height_;

  for (ToplevelWindows::iterator it = toplevels_.begin();
       it != toplevels_.end(); ++it) {
    int width = width_;
    int height = height_;
    if (FLAGS_lm_honor_window_size_hints)
      (*it)->win()->GetMaxSize(width_, height_, &width, &height);
    (*it)->win()->ResizeClient(width, height, Window::GRAVITY_NORTHWEST);
  }

  switch (mode_) {
    case MODE_ACTIVE:
      LayoutToplevelWindowsForActiveMode(false);  // update_focus=false
      break;
    case MODE_OVERVIEW:
      LayoutToplevelWindowsForOverviewMode(-1);
      break;
    default:
      DCHECK(false) << "Unhandled mode " << mode_ << " during resize";
  }
}


LayoutManager::ToplevelWindow::ToplevelWindow(Window* win,
                                              LayoutManager* layout_manager)
    : win_(win),
      layout_manager_(layout_manager),
      input_xid_(
          wm()->CreateInputWindow(
              -1, -1, 1, 1,
              ButtonPressMask | EnterWindowMask | LeaveWindowMask)),
      state_(STATE_NEW),
      overview_x_(0),
      overview_y_(0),
      overview_width_(0),
      overview_height_(0),
      overview_scale_(1.0),
      stacked_transients_(new Stacker<TransientWindow*>),
      transient_to_focus_(NULL) {
  if (FLAGS_lm_new_overview_mode && !static_gradient_texture_) {
    static_gradient_texture_ = wm()->clutter()->CreateImage(
        FLAGS_lm_overview_gradient_image);
    static_gradient_texture_->SetVisibility(false);
    wm()->stage()->AddActor(static_gradient_texture_);
  }

  int width = layout_manager_->width();
  int height = layout_manager_->height();
  if (FLAGS_lm_honor_window_size_hints)
    win->GetMaxSize(width, height, &width, &height);
  win->ResizeClient(width, height, Window::GRAVITY_NORTHWEST);

  wm()->stacking_manager()->StackXidAtTopOfLayer(
      input_xid_, StackingManager::LAYER_TOPLEVEL_WINDOW);

  // Let the window know that it's maximized.
  vector<pair<XAtom, bool> > wm_state;
  wm_state.push_back(
      make_pair(wm()->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_HORZ), true));
  wm_state.push_back(
      make_pair(wm()->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_VERT), true));
  win->ChangeWmState(wm_state);

  win->MoveClientOffscreen();
  win->SetCompositedOpacity(0, 0);
  win->ShowComposited();
  // Make sure that we hear about button presses on this window.
  win->AddButtonGrab();

  if (FLAGS_lm_new_overview_mode) {
    gradient_actor_.reset(
        wm()->clutter()->CloneActor(static_gradient_texture_));
    gradient_actor_->SetOpacity(0, 0);
    gradient_actor_->SetVisibility(true);
    wm()->stage()->AddActor(gradient_actor_.get());
  }
}

LayoutManager::ToplevelWindow::~ToplevelWindow() {
  wm()->xconn()->DestroyWindow(input_xid_);
  win_->RemoveButtonGrab();
  win_ = NULL;
  layout_manager_ = NULL;
  input_xid_ = None;
  transient_to_focus_ = NULL;
}

int LayoutManager::ToplevelWindow::GetAbsoluteOverviewX() const {
  int offset = FLAGS_lm_new_overview_mode ?
      layout_manager_->overview_panning_offset() :
      0;
  return layout_manager_->x() - offset + overview_x_;
}

int LayoutManager::ToplevelWindow::GetAbsoluteOverviewY() const {
  return layout_manager_->y() + overview_y_;
}

void LayoutManager::ToplevelWindow::ConfigureForActiveMode(
    bool window_is_active,
    bool to_left_of_active,
    bool update_focus) {
  const int layout_x = layout_manager_->x();
  const int layout_y = layout_manager_->y();
  const int layout_width = layout_manager_->width();
  const int layout_height = layout_manager_->height();

  // Center window vertically.
  const int win_y =
      layout_y + std::max(0, (layout_height - win_->client_height())) / 2;

  // TODO: This is a pretty huge mess.  Replace it with a saner way of
  // tracking animation state for windows.
  if (window_is_active) {
    // Center window horizontally.
    const int win_x =
        layout_x + std::max(0, (layout_width - win_->client_width())) / 2;
    if (state_ == STATE_NEW ||
        state_ == STATE_ACTIVE_MODE_OFFSCREEN ||
        state_ == STATE_ACTIVE_MODE_IN_FROM_RIGHT ||
        state_ == STATE_ACTIVE_MODE_IN_FROM_LEFT ||
        state_ == STATE_ACTIVE_MODE_IN_FADE) {
      // If the active window is in a state that requires that it be
      // animated in from a particular location or opacity, move it there
      // immediately.
      if (state_ == STATE_ACTIVE_MODE_IN_FROM_RIGHT) {
        win_->MoveComposited(layout_x + layout_width, win_y, 0);
        win_->SetCompositedOpacity(1.0, 0);
        win_->ScaleComposited(1.0, 1.0, 0);
      } else if (state_ == STATE_ACTIVE_MODE_IN_FROM_LEFT) {
        win_->MoveComposited(layout_x - win_->client_width(), win_y, 0);
        win_->SetCompositedOpacity(1.0, 0);
        win_->ScaleComposited(1.0, 1.0, 0);
      } else if (state_ == STATE_ACTIVE_MODE_IN_FADE) {
        win_->SetCompositedOpacity(0, 0);
        win_->MoveComposited(
            layout_x - 0.5 * kWindowFadeSizeFraction * win_->client_width(),
            layout_y - 0.5 * kWindowFadeSizeFraction * win_->client_height(),
            0);
        win_->ScaleComposited(
            1 + kWindowFadeSizeFraction, 1 + kWindowFadeSizeFraction, 0);
      } else {
        // Animate new or offscreen windows as moving up from the bottom
        // of the layout area.
        win_->MoveComposited(win_x, GetAbsoluteOverviewOffscreenY(), 0);
        win_->ScaleComposited(1.0, 1.0, 0);
      }
    }
    MoveAndScaleAllTransientWindows(0);

    // In any case, give the window input focus and animate it moving to
    // its final location.
    win_->MoveClient(win_x, win_y);
    win_->MoveComposited(win_x, win_y, kWindowAnimMs);
    win_->ScaleComposited(1.0, 1.0, kWindowAnimMs);
    win_->SetCompositedOpacity(1.0, kWindowAnimMs);
    if (FLAGS_lm_new_overview_mode)
      gradient_actor_->SetOpacity(0, 0);
    if (update_focus)
      TakeFocus(wm()->GetCurrentTimeFromServer());
    state_ = STATE_ACTIVE_MODE_ONSCREEN;
  } else {
    if (state_ == STATE_ACTIVE_MODE_OUT_TO_LEFT) {
      win_->MoveComposited(
          layout_x - win_->client_width(), win_y, kWindowAnimMs);
    } else if (state_ == STATE_ACTIVE_MODE_OUT_TO_RIGHT) {
      win_->MoveComposited(layout_x + layout_width, win_y, kWindowAnimMs);
    } else if (state_ == STATE_ACTIVE_MODE_OUT_FADE) {
      win_->SetCompositedOpacity(0, kWindowAnimMs);
      win_->MoveComposited(
          layout_x + 0.5 * kWindowFadeSizeFraction * win_->client_width(),
          layout_y + 0.5 * kWindowFadeSizeFraction * win_->client_height(),
          kWindowAnimMs);
      win_->ScaleComposited(
          kWindowFadeSizeFraction, kWindowFadeSizeFraction, kWindowAnimMs);
    } else if (state_ == STATE_ACTIVE_MODE_OFFSCREEN) {
      // No need to move it; it was already moved offscreen.
    } else {
      if (FLAGS_lm_new_overview_mode) {
        int x = to_left_of_active ?
                layout_x - overview_width_ :
                layout_x + layout_width;
        win_->MoveComposited(x, GetAbsoluteOverviewY(), kWindowAnimMs);
        gradient_actor_->Move(x, GetAbsoluteOverviewY(), kWindowAnimMs);
      } else {
        // Slide the window down offscreen and scale it down to its
        // overview size.
        win_->MoveComposited(GetAbsoluteOverviewX(),
                             GetAbsoluteOverviewOffscreenY(),
                             kWindowAnimMs);
      }
      win_->ScaleComposited(overview_scale_, overview_scale_, kWindowAnimMs);
      win_->SetCompositedOpacity(0.5, kWindowAnimMs);
    }
    // Fade out the window's shadow entirely so it won't be visible if
    // the window is just slightly offscreen.
    win_->SetShadowOpacity(0, kWindowAnimMs);
    state_ = STATE_ACTIVE_MODE_OFFSCREEN;
    win_->MoveClientOffscreen();
  }

  ApplyStackingForAllTransientWindows();
  MoveAndScaleAllTransientWindows(kWindowAnimMs);

  // TODO: Maybe just unmap input windows.
  wm()->xconn()->ConfigureWindowOffscreen(input_xid_);
}

void LayoutManager::ToplevelWindow::ConfigureForOverviewMode(
    bool window_is_magnified,
    bool dim_if_unmagnified,
    ToplevelWindow* toplevel_to_stack_under,
    bool incremental) {
  if (FLAGS_lm_new_overview_mode) {
    if (!incremental) {
      if (toplevel_to_stack_under) {
        win_->StackCompositedBelow(
            toplevel_to_stack_under->win()->GetBottomActor(), NULL, false);
        win_->StackClientBelow(toplevel_to_stack_under->win()->xid());
        wm()->xconn()->StackWindow(
            input_xid_, toplevel_to_stack_under->input_xid(), false);
      } else {
        wm()->stacking_manager()->StackWindowAtTopOfLayer(
            win_, StackingManager::LAYER_TOPLEVEL_WINDOW);
        wm()->stacking_manager()->StackXidAtTopOfLayer(
            input_xid_, StackingManager::LAYER_TOPLEVEL_WINDOW);
      }

      // We want to get new windows into their starting state immediately;
      // we animate other windows smoothly.
      const int anim_ms = (state_ == STATE_NEW) ? 0 : kOverviewAnimMs;

      win_->ScaleComposited(overview_scale_, overview_scale_, anim_ms);
      win_->SetCompositedOpacity(1.0, anim_ms);
      win_->MoveClientOffscreen();
      wm()->ConfigureInputWindow(input_xid_,
                                 GetAbsoluteOverviewX(), GetAbsoluteOverviewY(),
                                 overview_width_, overview_height_);
      ApplyStackingForAllTransientWindows();

      gradient_actor_->Raise(
          !stacked_transients_->items().empty() ?
            stacked_transients_->items().front()->win->actor() :
            win_->actor());
      gradient_actor_->SetOpacity(window_is_magnified ? 0 : 1, anim_ms);

      // Make new windows slide in from the right.
      if (state_ == STATE_NEW) {
        const int initial_x = layout_manager_->x() + layout_manager_->width();
        const int initial_y = GetAbsoluteOverviewY();
        win_->MoveComposited(initial_x, initial_y, 0);
        gradient_actor_->Move(initial_x, initial_y, 0);
      }
      state_ = window_is_magnified ?
          STATE_OVERVIEW_MODE_MAGNIFIED :
          STATE_OVERVIEW_MODE_NORMAL;
    }

    win_->MoveComposited(GetAbsoluteOverviewX(), GetAbsoluteOverviewY(),
                         incremental ? 0 : kOverviewAnimMs);
    MoveAndScaleAllTransientWindows(incremental ? 0 : kOverviewAnimMs);
    gradient_actor_->Move(GetAbsoluteOverviewX(), GetAbsoluteOverviewY(),
                          incremental ? 0 : kOverviewAnimMs);
    gradient_actor_->Scale(
        overview_scale_ * win_->client_width() / gradient_actor_->GetWidth(),
        overview_scale_ * win_->client_height() / gradient_actor_->GetHeight(),
        incremental ? 0 : kOverviewAnimMs);
  } else {
    if (state_ == STATE_NEW || state_ == STATE_ACTIVE_MODE_OFFSCREEN) {
      win_->MoveComposited(GetAbsoluteOverviewX(),
                           GetAbsoluteOverviewOffscreenY(),
                           0);
      win_->ScaleComposited(overview_scale_, overview_scale_, 0);
      win_->SetCompositedOpacity(0.5, 0);
      MoveAndScaleAllTransientWindows(0);
    }
    win_->MoveComposited(GetAbsoluteOverviewX(), GetAbsoluteOverviewY(),
                         kOverviewAnimMs);
    win_->ScaleComposited(overview_scale_, overview_scale_, kOverviewAnimMs);
    win_->MoveClientOffscreen();
    wm()->ConfigureInputWindow(input_xid_,
                               GetAbsoluteOverviewX(), GetAbsoluteOverviewY(),
                               overview_width_, overview_height_);
    if (!window_is_magnified && dim_if_unmagnified)
      win_->SetCompositedOpacity(0.75, kOverviewAnimMs);
    else
      win_->SetCompositedOpacity(1.0, kOverviewAnimMs);

    ApplyStackingForAllTransientWindows();
    MoveAndScaleAllTransientWindows(kOverviewAnimMs);

    state_ = window_is_magnified ?
        STATE_OVERVIEW_MODE_MAGNIFIED :
        STATE_OVERVIEW_MODE_NORMAL;
  }
}

void LayoutManager::ToplevelWindow::UpdateOverviewScaling(int max_width,
                                                          int max_height) {
  double scale_x = max_width / static_cast<double>(win_->client_width());
  double scale_y = max_height / static_cast<double>(win_->client_height());
  double tmp_scale = std::min(scale_x, scale_y);

  overview_width_  = tmp_scale * win_->client_width();
  overview_height_ = tmp_scale * win_->client_height();
  overview_scale_  = tmp_scale;
}

void LayoutManager::ToplevelWindow::TakeFocus(Time timestamp) {
  if (transient_to_focus_) {
    RestackTransientWindowOnTop(transient_to_focus_);
    transient_to_focus_->win->TakeFocus(timestamp);
  } else {
    win_->TakeFocus(timestamp);
  }
}

void LayoutManager::ToplevelWindow::SetPreferredTransientWindowToFocus(
    Window* transient_win) {
  if (!transient_win) {
    if (transient_to_focus_ && !transient_to_focus_->win->wm_state_modal())
      transient_to_focus_ = NULL;
    return;
  }

  TransientWindow* transient = GetTransientWindow(*transient_win);
  if (!transient) {
    LOG(ERROR) << "Got request to prefer focusing " << transient_win->xid_str()
               << ", which isn't transient for " << win_->xid_str();
    return;
  }

  if (transient == transient_to_focus_)
    return;

  if (!transient_to_focus_ ||
      !transient_to_focus_->win->wm_state_modal() ||
      transient_win->wm_state_modal())
    transient_to_focus_ = transient;
}

bool LayoutManager::ToplevelWindow::IsWindowOrTransientFocused() const {
  if (win_->focused())
    return true;

  for (map<XWindow, shared_ptr<TransientWindow> >::const_iterator it =
           transients_.begin();
       it != transients_.end(); ++it) {
    if (it->second->win->focused())
      return true;
  }
  return false;
}

void LayoutManager::ToplevelWindow::AddTransientWindow(Window* transient_win) {
  CHECK(transient_win);
  if (transients_.find(transient_win->xid()) != transients_.end()) {
    LOG(ERROR) << "Got request to add already-present transient window "
               << transient_win->xid_str() << " to " << win_->xid_str();
    return;
  }

  shared_ptr<TransientWindow> transient(new TransientWindow(transient_win));
  transients_[transient_win->xid()] = transient;

  // All transient windows other than info bubbles get centered over their
  // owner.
  if (transient_win->type() == WmIpc::WINDOW_TYPE_CHROME_INFO_BUBBLE) {
    transient->SaveOffsetsRelativeToOwnerWindow(win_);
    transient->centered = false;
  } else {
    transient->UpdateOffsetsToCenterOverOwnerWindow(win_);
    transient->centered = true;
  }

  // If the new transient is non-modal, stack it above the top non-modal
  // transient that we have.  If it's modal, just put it on top of all
  // other transients.
  TransientWindow* transient_to_stack_above = NULL;
  for (list<TransientWindow*>::const_iterator it =
         stacked_transients_->items().begin();
       it != stacked_transients_->items().end(); ++it) {
    if (transient_win->wm_state_modal() || !(*it)->win->wm_state_modal()) {
      transient_to_stack_above = (*it);
      break;
    }
  }
  if (transient_to_stack_above)
    stacked_transients_->AddAbove(transient.get(), transient_to_stack_above);
  else
    stacked_transients_->AddOnBottom(transient.get());

  SetPreferredTransientWindowToFocus(transient_win);

  MoveAndScaleTransientWindow(transient.get(), 0);
  ApplyStackingForTransientWindowAboveWindow(
      transient.get(),
      transient_to_stack_above ? transient_to_stack_above->win : win_);

  transient_win->ShowComposited();
  transient_win->AddButtonGrab();
}

void LayoutManager::ToplevelWindow::RemoveTransientWindow(
    Window* transient_win) {
  CHECK(transient_win);
  TransientWindow* transient = GetTransientWindow(*transient_win);
  if (!transient) {
    LOG(ERROR) << "Got request to remove not-present transient window "
               << transient_win->xid_str() << " from " << win_->xid_str();
    return;
  }
  stacked_transients_->Remove(transient);
  CHECK(transients_.erase(transient_win->xid()) == 1);
  transient_win->RemoveButtonGrab();

  if (transient_to_focus_ == transient) {
    transient_to_focus_ = NULL;
    TransientWindow* new_transient = FindTransientWindowToFocus();
    SetPreferredTransientWindowToFocus(
        new_transient ? new_transient->win : NULL);
  }
}

void LayoutManager::ToplevelWindow::HandleTransientWindowConfigureRequest(
    Window* transient_win,
    int req_x, int req_y, int req_width, int req_height) {
  CHECK(transient_win);
  TransientWindow* transient = GetTransientWindow(*transient_win);
  CHECK(transient);

  // Move and resize the transient window as requested.
  bool moved = false;
  if (req_x != transient_win->client_x() ||
      req_y != transient_win->client_y()) {
    transient_win->MoveClient(req_x, req_y);
    transient->SaveOffsetsRelativeToOwnerWindow(win_);
    transient->centered = false;
    moved = true;
  }

  if (req_width != transient_win->client_width() ||
      req_height != transient_win->client_height()) {
    transient_win->ResizeClient(
        req_width, req_height, Window::GRAVITY_NORTHWEST);
    if (transient->centered) {
      transient->UpdateOffsetsToCenterOverOwnerWindow(win_);
      moved = true;
    }
  }

  if (moved)
    MoveAndScaleTransientWindow(transient, 0);
}

void LayoutManager::ToplevelWindow::HandleFocusChange(
    Window* focus_win, bool focus_in) {
  DCHECK(focus_win == win_ || GetTransientWindow(*focus_win));

  if (focus_in) {
    VLOG(1) << "Got focus-in for " << focus_win->xid_str()
            << "; removing passive button grab";
    focus_win->RemoveButtonGrab();
  } else {
    // Listen for button presses on this window so we'll know when it
    // should be focused again.
    VLOG(1) << "Got focus-out for " << focus_win->xid_str()
            << "; re-adding passive button grab";
    focus_win->AddButtonGrab();
  }
}

void LayoutManager::ToplevelWindow::HandleButtonPress(
    Window* button_win, Time timestamp) {
  SetPreferredTransientWindowToFocus(
      GetTransientWindow(*button_win) ? button_win : NULL);
  TakeFocus(timestamp);
  wm()->xconn()->RemovePointerGrab(true, timestamp);  // replay events
}

LayoutManager::ToplevelWindow::TransientWindow*
    LayoutManager::ToplevelWindow::GetTransientWindow(const Window& win) {
  map<XWindow, shared_ptr<TransientWindow> >::iterator it =
      transients_.find(win.xid());
  if (it == transients_.end())
    return NULL;
  return it->second.get();
}

void LayoutManager::ToplevelWindow::MoveAndScaleTransientWindow(
    TransientWindow* transient, int anim_ms) {
  // TODO: Check if 'win_' is offscreen, and make sure that the transient
  // window is offscreen as well if so.
  transient->win->MoveClient(
      win_->client_x() + transient->x_offset,
      win_->client_y() + transient->y_offset);

  transient->win->MoveComposited(
      win_->composited_x() + win_->composited_scale_x() * transient->x_offset,
      win_->composited_y() + win_->composited_scale_y() * transient->y_offset,
      anim_ms);
  transient->win->ScaleComposited(
      win_->composited_scale_x(), win_->composited_scale_y(), anim_ms);
}

void LayoutManager::ToplevelWindow::MoveAndScaleAllTransientWindows(
    int anim_ms) {
  for (map<XWindow, shared_ptr<TransientWindow> >::iterator it =
           transients_.begin();
       it != transients_.end(); ++it) {
    MoveAndScaleTransientWindow(it->second.get(), anim_ms);
  }
}

// static
void LayoutManager::ToplevelWindow::ApplyStackingForTransientWindowAboveWindow(
    TransientWindow* transient, Window* other_win) {
  CHECK(transient);
  CHECK(other_win);
  transient->win->StackClientAbove(other_win->xid());
  transient->win->StackCompositedAbove(other_win->actor(), NULL, false);
}

void LayoutManager::ToplevelWindow::ApplyStackingForAllTransientWindows() {
  Window* prev_win = win_;
  for (list<TransientWindow*>::const_reverse_iterator it =
           stacked_transients_->items().rbegin();
       it != stacked_transients_->items().rend();
       ++it) {
    TransientWindow* transient = *it;
    ApplyStackingForTransientWindowAboveWindow(transient, prev_win);
    prev_win = transient->win;
  }
}

LayoutManager::ToplevelWindow::TransientWindow*
    LayoutManager::ToplevelWindow::FindTransientWindowToFocus() const {
  if (stacked_transients_->items().empty())
    return NULL;

  for (list<TransientWindow*>::const_iterator it =
           stacked_transients_->items().begin();
       it != stacked_transients_->items().end();
       ++it) {
    if ((*it)->win->wm_state_modal())
      return *it;
  }
  return stacked_transients_->items().front();
}

void LayoutManager::ToplevelWindow::RestackTransientWindowOnTop(
    TransientWindow* transient) {
  if (transient == stacked_transients_->items().front())
    return;

  DCHECK(stacked_transients_->Contains(transient));
  DCHECK_GT(stacked_transients_->items().size(), 1U);
  TransientWindow* transient_to_stack_above =
      stacked_transients_->items().front();
  stacked_transients_->Remove(transient);
  stacked_transients_->AddOnTop(transient);
  ApplyStackingForTransientWindowAboveWindow(
      transient, transient_to_stack_above->win);
}


// static
bool LayoutManager::IsHandledWindowType(WmIpc::WindowType type) {
  return (type == WmIpc::WINDOW_TYPE_CHROME_FLOATING_TAB ||
          type == WmIpc::WINDOW_TYPE_CHROME_INFO_BUBBLE ||
          type == WmIpc::WINDOW_TYPE_CHROME_TAB_SUMMARY ||
          type == WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL ||
          type == WmIpc::WINDOW_TYPE_CREATE_BROWSER_WINDOW ||
          type == WmIpc::WINDOW_TYPE_UNKNOWN);
}

LayoutManager::ToplevelWindow* LayoutManager::GetToplevelWindowByInputXid(
    XWindow xid) {
  return FindWithDefault(
      input_to_toplevel_, xid, static_cast<ToplevelWindow*>(NULL));
}

int LayoutManager::GetIndexForToplevelWindow(
    const ToplevelWindow& toplevel) const {
  for (size_t i = 0; i < toplevels_.size(); ++i)
    if (toplevels_[i].get() == &toplevel)
      return static_cast<int>(i);
  return -1;
}

LayoutManager::ToplevelWindow* LayoutManager::GetToplevelWindowByWindow(
    const Window& win) {
  for (size_t i = 0; i < toplevels_.size(); ++i)
    if (toplevels_[i]->win() == &win)
      return toplevels_[i].get();
  return NULL;
}

LayoutManager::ToplevelWindow* LayoutManager::GetToplevelWindowByXid(
    XWindow xid) {
  const Window* win = wm_->GetWindow(xid);
  if (!win)
    return NULL;
  return GetToplevelWindowByWindow(*win);
}

LayoutManager::ToplevelWindow*
    LayoutManager::GetToplevelWindowOwningTransientWindow(const Window& win) {
  return FindWithDefault(
      transient_to_toplevel_, win.xid(), static_cast<ToplevelWindow*>(NULL));
}

XWindow LayoutManager::GetInputXidForWindow(const Window& win) {
  ToplevelWindow* toplevel = GetToplevelWindowByWindow(win);
  return toplevel ? toplevel->input_xid() : None;
}

void LayoutManager::DoInitialSetupForWindow(Window* win) {
  // We preserve info bubbles' initial locations even though they're
  // ultimately transient windows.
  if (win->type() != WmIpc::WINDOW_TYPE_CHROME_INFO_BUBBLE)
    win->MoveClientOffscreen();
  wm_->stacking_manager()->StackWindowAtTopOfLayer(
      win, StackingManager::LAYER_TOPLEVEL_WINDOW);
}

void LayoutManager::SetActiveToplevelWindow(
    ToplevelWindow* toplevel,
    ToplevelWindow::State state_for_new_win,
    ToplevelWindow::State state_for_old_win) {
  CHECK(toplevel);

  if (mode_ != MODE_ACTIVE || active_toplevel_ == toplevel)
    return;

  if (active_toplevel_)
    active_toplevel_->set_state(state_for_old_win);
  toplevel->set_state(state_for_new_win);
  active_toplevel_ = toplevel;
  LayoutToplevelWindowsForActiveMode(true);  // update_focus=true
}

void LayoutManager::SwitchToActiveMode(bool activate_magnified_win) {
  if (mode_ == MODE_ACTIVE)
    return;
  if (activate_magnified_win && magnified_toplevel_)
    active_toplevel_ = magnified_toplevel_;
  SetMode(MODE_ACTIVE);
}

void LayoutManager::ActivateToplevelWindowByIndex(int index) {
  if (toplevels_.empty() || mode_ != MODE_ACTIVE)
    return;

  if (index < 0)
    index = static_cast<int>(toplevels_.size()) + index;
  if (index < 0 || index >= static_cast<int>(toplevels_.size()))
    return;
  if (toplevels_[index].get() == active_toplevel_)
    return;

  SetActiveToplevelWindow(toplevels_[index].get(),
                          ToplevelWindow::STATE_ACTIVE_MODE_IN_FADE,
                          ToplevelWindow::STATE_ACTIVE_MODE_OUT_FADE);
}

void LayoutManager::MagnifyToplevelWindowByIndex(int index) {
  if (toplevels_.empty() || mode_ != MODE_OVERVIEW)
    return;

  if (index < 0)
    index = static_cast<int>(toplevels_.size()) + index;
  if (index < 0 || index >= static_cast<int>(toplevels_.size()))
    return;
  if (toplevels_[index].get() == magnified_toplevel_)
    return;

  SetMagnifiedToplevelWindow(toplevels_[index].get());
  LayoutToplevelWindowsForOverviewMode(0.5 * width_);
  if (!FLAGS_lm_new_overview_mode)
    SendTabSummaryMessage(magnified_toplevel_, true);
}

void LayoutManager::Metrics::Populate(chrome_os_pb::SystemMetrics *metrics_pb) {
  CHECK(metrics_pb);
  metrics_pb->Clear();
  metrics_pb->set_overview_keystroke_count(overview_by_keystroke_count);
  metrics_pb->set_overview_exit_mouse_count(overview_exit_by_mouse_count);
  metrics_pb->set_overview_exit_keystroke_count(
      overview_exit_by_keystroke_count);
  metrics_pb->set_keystroke_window_cycling_count(
      window_cycle_by_keystroke_count);
}

void LayoutManager::SetMode(Mode mode) {
  RemoveKeyBindingsForMode(mode_);
  mode_ = mode;
  switch (mode_) {
    case MODE_ACTIVE: {
      if (create_browser_window_) {
        create_browser_window_->HideComposited();
        create_browser_window_->MoveClientOffscreen();
      }
      if ((FLAGS_lm_new_overview_mode || !active_toplevel_) &&
          magnified_toplevel_) {
        active_toplevel_ = magnified_toplevel_;
      }
      if (!active_toplevel_ && !toplevels_.empty())
        active_toplevel_ = toplevels_[0].get();
      if (!FLAGS_lm_new_overview_mode)
        SetMagnifiedToplevelWindow(NULL);
      LayoutToplevelWindowsForActiveMode(true);  // update_focus=true
      break;
    }
    case MODE_OVERVIEW: {
      if (create_browser_window_)
        create_browser_window_->ShowComposited();
      if (FLAGS_lm_new_overview_mode)
        SetMagnifiedToplevelWindow(active_toplevel_);
      else
        SetMagnifiedToplevelWindow(NULL);
      // Leave 'active_toplevel_' alone, so we can activate the same window
      // if we return to active mode on an Escape keypress.

      if (active_toplevel_ && active_toplevel_->IsWindowOrTransientFocused()) {
        // We need to take the input focus away here; otherwise the
        // previously-focused window would continue to get keyboard events
        // in overview mode.  Let the WindowManager decide what to do with it.
        wm_->SetActiveWindowProperty(None);
        wm_->TakeFocus();
      }
      LayoutToplevelWindowsForOverviewMode(0.5 * width_);
      break;
    }
  }
  AddKeyBindingsForMode(mode_);

  // Let all Chrome windows know about the new layout mode.
  for (ToplevelWindows::iterator it = toplevels_.begin();
       it != toplevels_.end(); ++it) {
    if ((*it)->win()->type() == WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL)
      SendModeMessage(it->get());
  }
}

void LayoutManager::LayoutToplevelWindowsForActiveMode(bool update_focus) {
  VLOG(1) << "Laying out windows for active mode";
  if (toplevels_.empty())
    return;
  if (!active_toplevel_)
    active_toplevel_ = toplevels_[0].get();

  bool saw_active = false;
  for (ToplevelWindows::iterator it = toplevels_.begin();
       it != toplevels_.end(); ++it) {
    bool is_active = it->get() == active_toplevel_;
    (*it)->ConfigureForActiveMode(is_active, !saw_active, update_focus);
    if (is_active)
      saw_active = true;
  }
}

void LayoutManager::LayoutToplevelWindowsForOverviewMode(
    int magnified_x) {
  VLOG(1) << "Laying out windows for overview mode";
  CalculatePositionsForOverviewMode(magnified_x);
  ConfigureWindowsForOverviewMode(false);
}

void LayoutManager::CalculatePositionsForOverviewMode(int magnified_x) {
  if (toplevels_.empty())
    return;

  if (FLAGS_lm_new_overview_mode) {
    const int width_limit =
        std::min(static_cast<double>(width_) / sqrt(toplevels_.size()),
                 kOverviewWindowMaxSizeRatio * width_);
    const int height_limit =
        std::min(static_cast<double>(height_) / sqrt(toplevels_.size()),
                 kOverviewWindowMaxSizeRatio * height_);
    int running_width = kWindowPadding;

    for (int i = 0; static_cast<size_t>(i) < toplevels_.size(); ++i) {
      ToplevelWindow* toplevel = toplevels_[i].get();
      bool is_magnified = (toplevel == magnified_toplevel_);

      toplevel->UpdateOverviewScaling(width_limit, height_limit);
      toplevel->UpdateOverviewPosition(
          running_width, 0.5 * (height_ - toplevel->overview_height()));
      running_width += is_magnified ?
          toplevel->overview_width() :
          (kOverviewExposedWindowRatio * width_ *
            (width_limit / (kOverviewWindowMaxSizeRatio * width_)));
      if (is_magnified && magnified_x >= 0) {
        // If the window will be under 'magnified_x' when centered, just
        // center it.  Otherwise, move it as close to centered as possible
        // while still being under 'magnified_x'.
        if (0.5 * (width_ - toplevel->overview_width()) < magnified_x &&
            0.5 * (width_ + toplevel->overview_width()) >= magnified_x) {
          overview_panning_offset_ =
              toplevel->overview_x() +
              0.5 * toplevel->overview_width() -
              0.5 * width_;
        } else if (0.5 * (width_ - toplevel->overview_width()) > magnified_x) {
          overview_panning_offset_ = toplevel->overview_x() - magnified_x + 1;
        } else {
          overview_panning_offset_ = toplevel->overview_x() - magnified_x +
                                     toplevel->overview_width() - 1;
        }
      }
    }
  } else {
    // First, figure out how much space the magnified window (if any) will
    // take up.
    if (magnified_toplevel_) {
      magnified_toplevel_->UpdateOverviewScaling(
          width_,  // TODO: Cap this if we end up with wide windows.
          overview_height_);
    }

    // Now, figure out the maximum size that we want each unmagnified window
    // to be able to take.
    int num_unmag_windows = toplevels_.size();
    int total_unmag_width = width_ - (toplevels_.size() + 1) * kWindowPadding;

    if (create_browser_window_) {
      total_unmag_width -=
          (create_browser_window_->client_width() + kWindowPadding);
    }
    if (magnified_toplevel_) {
      total_unmag_width -= magnified_toplevel_->overview_width();
      num_unmag_windows -= 1;
    }

    const int max_unmag_width =
        num_unmag_windows ?
        (total_unmag_width / num_unmag_windows) :
        0;
    const int max_unmag_height = kMaxWindowHeightRatio * overview_height_;

    // Figure out the actual scaling for each window.
    for (int i = 0; static_cast<size_t>(i) < toplevels_.size(); ++i) {
      ToplevelWindow* toplevel = toplevels_[i].get();
      // We already computed the dimensions for the magnified window.
      if (toplevel != magnified_toplevel_)
        toplevel->UpdateOverviewScaling(max_unmag_width, max_unmag_height);
    }

    // Divide up the remaining space among all of the windows, including
    // padding around the outer windows.
    int total_window_width = 0;
    for (int i = 0; static_cast<size_t>(i) < toplevels_.size(); ++i)
      total_window_width += toplevels_[i]->overview_width();
    if (create_browser_window_)
      total_window_width += create_browser_window_->client_width();
    int total_padding = width_ - total_window_width;
    if (total_padding < 0) {
      LOG(WARNING) << "Summed width of scaled windows (" << total_window_width
                   << ") exceeds width of overview area (" << width_ << ")";
      total_padding = 0;
    }
    const double padding = create_browser_window_
        ? total_padding / static_cast<double>(toplevels_.size() + 2) :
          total_padding / static_cast<double>(toplevels_.size() + 1);

    // Finally, go through and calculate the final position for each window.
    double running_width = 0;
    for (int i = 0; static_cast<size_t>(i) < toplevels_.size(); ++i) {
      ToplevelWindow* toplevel = toplevels_[i].get();
      int overview_x = round(running_width + padding);
      int overview_y = height_ - toplevel->overview_height();
      toplevel->UpdateOverviewPosition(overview_x, overview_y);
      running_width += padding + toplevel->overview_width();
    }
  }
}

void LayoutManager::ConfigureWindowsForOverviewMode(bool incremental) {
  ToplevelWindow* toplevel_to_right = NULL;
  // We iterate through the windows in descending stacking order
  // (right-to-left).  Otherwise, we'd get spurious pointer enter events as
  // a result of stacking a window underneath the pointer immediately
  // before we stack the window to its right directly on top of it.
  for (ToplevelWindows::reverse_iterator it = toplevels_.rbegin();
       it != toplevels_.rend(); ++it) {
    ToplevelWindow* toplevel = it->get();
    toplevel->ConfigureForOverviewMode(
        (it->get() == magnified_toplevel_),  // window_is_magnified
        (magnified_toplevel_ != NULL),       // dim_if_unmagnified
        toplevel_to_right,
        incremental);
    toplevel_to_right = toplevel;
  }
  if (!incremental && create_browser_window_) {
    // The 'create browser window' is always anchored to the right side
    // of the screen.
    create_browser_window_->MoveComposited(
        x_ + width_ - create_browser_window_->client_width() - kWindowPadding,
        y_ + height_ - create_browser_window_->client_height() -
        kCreateBrowserWindowVerticalPadding,
        0);
    create_browser_window_->MoveClientToComposited();
  }
}

LayoutManager::ToplevelWindow* LayoutManager::GetOverviewToplevelWindowAtPoint(
    int x, int y) const {
  for (int i = 0; static_cast<size_t>(i) < toplevels_.size(); ++i)
    if (toplevels_[i]->OverviewWindowContainsPoint(x, y))
      return toplevels_[i].get();
  return NULL;
}

bool LayoutManager::PointIsInTabSummary(int x, int y) const {
  return (tab_summary_ &&
          x >= tab_summary_->client_x() &&
          y >= tab_summary_->client_y() &&
          x < tab_summary_->client_x() + tab_summary_->client_width() &&
          y < tab_summary_->client_y() + tab_summary_->client_height());
}

bool LayoutManager::PointIsBetweenMagnifiedToplevelWindowAndTabSummary(
    int x, int y) const {
  if (!magnified_toplevel_ || !tab_summary_) return false;

  for (int i = 0; static_cast<size_t>(i) < toplevels_.size(); ++i) {
    ToplevelWindow* toplevel = toplevels_[i].get();
    if (toplevel != magnified_toplevel_)
      continue;
    return (y >= tab_summary_->client_y() + tab_summary_->client_height() &&
            y < toplevel->GetAbsoluteOverviewY());
  }
  LOG(WARNING) << "magnified_toplevel_ "
               << magnified_toplevel_->win()->xid_str()
               << " isn't present in our list of windows";
  return false;
}

void LayoutManager::AddKeyBindingsForMode(Mode mode) {
  VLOG(1) << "Adding key bindings for mode " << mode;
  KeyBindings* kb = wm_->key_bindings();

  switch (mode) {
    case MODE_ACTIVE:
      kb->AddBinding(KeyBindings::KeyCombo(XK_F12, 0),
                     "switch-to-overview-mode");
      kb->AddBinding(KeyBindings::KeyCombo(XK_Tab, KeyBindings::kAltMask),
                     "cycle-active-forward");
      kb->AddBinding(KeyBindings::KeyCombo(XK_F2, KeyBindings::kAltMask),
                     "cycle-active-forward");
      kb->AddBinding(
          KeyBindings::KeyCombo(
              XK_Tab, KeyBindings::kAltMask | KeyBindings::kShiftMask),
          "cycle-active-backward");
      kb->AddBinding(KeyBindings::KeyCombo(XK_F1, KeyBindings::kAltMask),
                     "cycle-active-backward");
      for (int i = 0; i < 8; ++i) {
        kb->AddBinding(KeyBindings::KeyCombo(XK_1 + i, KeyBindings::kAltMask),
                       StringPrintf("activate-toplevel-with-index-%d", i));
      }
      kb->AddBinding(KeyBindings::KeyCombo(XK_9, KeyBindings::kAltMask),
                     "activate-last-toplevel");
      kb->AddBinding(
          KeyBindings::KeyCombo(
              XK_w, KeyBindings::kControlMask | KeyBindings::kShiftMask),
              "delete-active-window");
      break;
    case MODE_OVERVIEW:
      kb->AddBinding(KeyBindings::KeyCombo(XK_Escape, 0),
                     "switch-to-active-mode");
      kb->AddBinding(KeyBindings::KeyCombo(XK_F12, 0),
                     "switch-to-active-mode");
      kb->AddBinding(KeyBindings::KeyCombo(XK_Return, 0),
                     "switch-to-active-mode-for-magnified");
      kb->AddBinding(KeyBindings::KeyCombo(XK_Right, 0),
                     "cycle-magnification-forward");
      kb->AddBinding(KeyBindings::KeyCombo(XK_Tab, KeyBindings::kAltMask),
                     "cycle-magnification-forward");
      kb->AddBinding(KeyBindings::KeyCombo(XK_F2, KeyBindings::kAltMask),
                     "cycle-magnification-forward");
      kb->AddBinding(KeyBindings::KeyCombo(XK_Left, 0),
                     "cycle-magnification-backward");
      kb->AddBinding(
          KeyBindings::KeyCombo(
              XK_Tab, KeyBindings::kAltMask | KeyBindings::kShiftMask),
          "cycle-magnification-backward");
      kb->AddBinding(KeyBindings::KeyCombo(XK_F1, KeyBindings::kAltMask),
                     "cycle-magnification-backward");
      for (int i = 0; i < 8; ++i) {
        kb->AddBinding(KeyBindings::KeyCombo(XK_1 + i, KeyBindings::kAltMask),
                       StringPrintf("magnify-toplevel-with-index-%d", i));
      }
      kb->AddBinding(KeyBindings::KeyCombo(XK_9, KeyBindings::kAltMask),
                     "magnify-last-toplevel");
      kb->AddBinding(KeyBindings::KeyCombo(XK_h, KeyBindings::kAltMask),
                     "pan-overview-mode-left");
      kb->AddBinding(KeyBindings::KeyCombo(XK_l, KeyBindings::kAltMask),
                     "pan-overview-mode-right");
      break;
  }
}

void LayoutManager::RemoveKeyBindingsForMode(Mode mode) {
  VLOG(1) << "Removing key bindings for mode " << mode;
  KeyBindings* kb = wm_->key_bindings();

  switch (mode) {
    case MODE_ACTIVE:
      // TODO: Add some sort of "key bindings group" feature to the
      // KeyBindings class so that we don't need to explicitly repeat all
      // of the bindings from AddKeyBindingsForMode() here.
      kb->RemoveBinding(KeyBindings::KeyCombo(XK_F12, 0));
      kb->RemoveBinding(KeyBindings::KeyCombo(XK_Tab, KeyBindings::kAltMask));
      kb->RemoveBinding(KeyBindings::KeyCombo(XK_F2, KeyBindings::kAltMask));
      kb->RemoveBinding(
          KeyBindings::KeyCombo(
              XK_Tab, KeyBindings::kAltMask | KeyBindings::kShiftMask));
      kb->RemoveBinding(KeyBindings::KeyCombo(XK_F1, KeyBindings::kAltMask));
      for (int i = 0; i < 9; ++i) {
        kb->RemoveBinding(
            KeyBindings::KeyCombo(XK_1 + i, KeyBindings::kAltMask));
      }
      kb->RemoveBinding(
          KeyBindings::KeyCombo(
              XK_w, KeyBindings::kControlMask | KeyBindings::kShiftMask));
      break;
    case MODE_OVERVIEW:
      kb->RemoveBinding(KeyBindings::KeyCombo(XK_Escape, 0));
      kb->RemoveBinding(KeyBindings::KeyCombo(XK_F12, 0));
      kb->RemoveBinding(KeyBindings::KeyCombo(XK_Return, 0));
      kb->RemoveBinding(KeyBindings::KeyCombo(XK_Right, 0));
      kb->RemoveBinding(KeyBindings::KeyCombo(XK_Tab, KeyBindings::kAltMask));
      kb->RemoveBinding(KeyBindings::KeyCombo(XK_F2, KeyBindings::kAltMask));
      kb->RemoveBinding(KeyBindings::KeyCombo(XK_Left, 0));
      kb->RemoveBinding(
          KeyBindings::KeyCombo(
              XK_Tab, KeyBindings::kAltMask | KeyBindings::kShiftMask));
      kb->RemoveBinding(KeyBindings::KeyCombo(XK_F1, KeyBindings::kAltMask));
      for (int i = 0; i < 9; ++i) {
        kb->RemoveBinding(
            KeyBindings::KeyCombo(XK_1 + i, KeyBindings::kAltMask));
      }
      kb->RemoveBinding(KeyBindings::KeyCombo(XK_h, KeyBindings::kAltMask));
      kb->RemoveBinding(KeyBindings::KeyCombo(XK_l, KeyBindings::kAltMask));
      break;
  }
}

void LayoutManager::CycleActiveToplevelWindow(bool forward) {
  if (mode_ != MODE_ACTIVE) {
    LOG(WARNING) << "Ignoring request to cycle active toplevel outside of "
                 << "active mode (current mode is " << mode_ << ")";
    return;
  }
  if (toplevels_.empty())
    return;

  ToplevelWindow* toplevel = NULL;
  if (!active_toplevel_) {
    toplevel = forward ?
        toplevels_[0].get() :
        toplevels_[toplevels_.size()-1].get();
  } else {
    if (toplevels_.size() == 1)
      return;
    int old_index = GetIndexForToplevelWindow(*active_toplevel_);
    int new_index = (toplevels_.size() + old_index + (forward ? 1 : -1)) %
                    toplevels_.size();
    toplevel = toplevels_[new_index].get();
  }
  CHECK(toplevel);

  SetActiveToplevelWindow(
      toplevel,
      forward ?
        ToplevelWindow::STATE_ACTIVE_MODE_IN_FROM_RIGHT :
        ToplevelWindow::STATE_ACTIVE_MODE_IN_FROM_LEFT,
      forward ?
        ToplevelWindow::STATE_ACTIVE_MODE_OUT_TO_LEFT :
        ToplevelWindow::STATE_ACTIVE_MODE_OUT_TO_RIGHT);
}

void LayoutManager::CycleMagnifiedToplevelWindow(bool forward) {
  if (mode_ != MODE_OVERVIEW) {
    LOG(WARNING) << "Ignoring request to cycle magnified toplevel outside of "
                 << "overview mode (current mode is " << mode_ << ")";
    return;
  }
  if (toplevels_.empty())
    return;
  if (magnified_toplevel_ && toplevels_.size() == 1)
    return;

  if (!magnified_toplevel_ && !active_toplevel_) {
    // If we have no clue about which window to magnify, just choose the
    // first one.
    SetMagnifiedToplevelWindow(toplevels_[0].get());
  } else {
    if (!magnified_toplevel_) {
      // If no toplevel window is magnified, pretend like the active
      // toplevel was magnified so we'll move either to its left or its
      // right.
      magnified_toplevel_ = active_toplevel_;
    }
    CHECK(magnified_toplevel_);
    int old_index = GetIndexForToplevelWindow(*magnified_toplevel_);
    int new_index = (toplevels_.size() + old_index + (forward ? 1 : -1)) %
                    toplevels_.size();
    SetMagnifiedToplevelWindow(toplevels_[new_index].get());
  }
  LayoutToplevelWindowsForOverviewMode(0.5 * width_);

  // Tell the magnified window to display a tab summary now that we've
  // rearranged all of the windows.
  if (!FLAGS_lm_new_overview_mode)
    SendTabSummaryMessage(magnified_toplevel_, true);
}

void LayoutManager::SetMagnifiedToplevelWindow(ToplevelWindow* toplevel) {
  if (magnified_toplevel_ == toplevel)
    return;
  // Hide the previous window's tab summary.
  if (!FLAGS_lm_new_overview_mode && magnified_toplevel_)
    SendTabSummaryMessage(magnified_toplevel_, false);
  magnified_toplevel_ = toplevel;
}

void LayoutManager::SendTabSummaryMessage(ToplevelWindow* toplevel, bool show) {
  if (!toplevel ||
      toplevel->win()->type() != WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL) {
    return;
  }
  WmIpc::Message msg(WmIpc::Message::CHROME_SET_TAB_SUMMARY_VISIBILITY);
  msg.set_param(0, show);  // show summary
  if (show)
    msg.set_param(1, toplevel->GetAbsoluteOverviewCenterX());
  wm_->wm_ipc()->SendMessage(toplevel->win()->xid(), msg);
}

void LayoutManager::SendModeMessage(ToplevelWindow* toplevel) {
  if (!toplevel ||
      toplevel->win()->type() != WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL) {
    return;
  }

  WmIpc::Message msg(WmIpc::Message::CHROME_NOTIFY_LAYOUT_MODE);
  switch (mode_) {
    // Set the mode in the message using the appropriate value from wm_ipc.h.
    case MODE_ACTIVE:
      msg.set_param(0, 0);
      break;
    case MODE_OVERVIEW:
      msg.set_param(0, 1);
      break;
    default:
      CHECK(false) << "Unhandled mode " << mode_;
  }
  wm_->wm_ipc()->SendMessage(toplevel->win()->xid(), msg);
}

void LayoutManager::SendDeleteRequestToActiveWindow() {
  // TODO: If there's a focused transient window, the message should get
  // sent to it instead.
  if (mode_ == MODE_ACTIVE && active_toplevel_)
    active_toplevel_->win()->SendDeleteRequest(wm_->GetCurrentTimeFromServer());
}

void LayoutManager::PanOverviewMode(int offset) {
  overview_panning_offset_ += offset;
  if (mode_ == MODE_OVERVIEW)
    ConfigureWindowsForOverviewMode(false);  // incremental=false
}

void LayoutManager::UpdateOverviewPanningForMotion() {
  int dx = overview_background_event_coalescer_->x() - overview_drag_last_x_;
  overview_drag_last_x_ = overview_background_event_coalescer_->x();
  overview_panning_offset_ -= dx;
  ConfigureWindowsForOverviewMode(true);  // incremental=true
}

}  // namespace window_manager
