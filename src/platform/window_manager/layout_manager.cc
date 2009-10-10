// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/layout_manager.h"

#include <algorithm>
#include <cmath>
extern "C" {
#include <X11/Xatom.h>
}
#include <glog/logging.h>

#include "base/callback.h"
#include "base/strutil.h"
#include "window_manager/atom_cache.h"
#include "window_manager/motion_event_coalescer.h"
#include "window_manager/system_metrics.pb.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"
#include "window_manager/x_connection.h"

DEFINE_bool(lm_force_fullscreen_windows, true,
            "Resize each client window to fill the whole screen (otherwise, "
            "it just uses the client app's requested size)");

DEFINE_bool(lm_honor_window_size_hints, false,
            "When --lm_force_fullscreen_windows is enabled, constrain each "
            "client window's size according to the size hints that the "
            "client app has provided (e.g. max size, size increment, etc.) "
            "instead of automatically making it fill the screen");

namespace chromeos {

// Amount of padding that should be used between windows in overview mode.
static const int kWindowPadding = 10;

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

// Maximum fraction of the total height that magnified windows can take up
// in overview mode.
static const double kOverviewHeightFraction = 0.3;

// When animating a window zooming out while switching windows, what size
// should it scale to?
static const double kWindowFadeSizeFraction = 0.5;

LayoutManager::LayoutManager
    (WindowManager* wm, int x, int y, int width, int height)
    : wm_(wm),
      mode_(MODE_ACTIVE),
      x_(x),
      y_(y),
      width_(-1),
      height_(-1),
      overview_height_(-1),
      magnified_window_(NULL),
      active_window_(NULL),
      floating_tab_(NULL),
      window_under_floating_tab_(NULL),
      tab_summary_(NULL),
      create_browser_window_(NULL),
      floating_tab_event_coalescer_(
          new MotionEventCoalescer(
              NewPermanentCallback(this, &LayoutManager::MoveFloatingTab),
              kFloatingTabUpdateMs)) {
  Resize(width, height);

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
      NewPermanentCallback(this, &LayoutManager::CycleActiveWindow, true),
      NULL, NULL);
  kb->AddAction(
      "cycle-active-backward",
      NewPermanentCallback(this, &LayoutManager::CycleActiveWindow, false),
      NULL, NULL);
  kb->AddAction(
      "cycle-magnification-forward",
      NewPermanentCallback(this, &LayoutManager::CycleMagnifiedWindow, true),
      NULL, NULL);
  kb->AddAction(
      "cycle-magnification-backward",
      NewPermanentCallback(this, &LayoutManager::CycleMagnifiedWindow, false),
      NULL, NULL);
  kb->AddAction(
      "switch-to-active-mode-for-magnified",
      NewPermanentCallback(this, &LayoutManager::SwitchToActiveMode, true),
      NULL, NULL);
  for (int i = 0; i < 8; ++i) {
    kb->AddAction(
        StringPrintf("activate-window-with-index-%d", i),
        NewPermanentCallback(this, &LayoutManager::ActivateWindowByIndex, i),
        NULL, NULL);
    kb->AddAction(
        StringPrintf("magnify-window-with-index-%d", i),
        NewPermanentCallback(this, &LayoutManager::MagnifyWindowByIndex, i),
        NULL, NULL);
  }
  kb->AddAction(
      "activate-last-window",
      NewPermanentCallback(this, &LayoutManager::ActivateWindowByIndex, -1),
      NULL, NULL);
  kb->AddAction(
      "magnify-last-window",
      NewPermanentCallback(this, &LayoutManager::MagnifyWindowByIndex, -1),
      NULL, NULL);
  kb->AddAction(
      "delete-active-window",
      NewPermanentCallback(
          this, &LayoutManager::SendDeleteRequestToActiveWindow),
      NULL, NULL);

  SetMode(MODE_ACTIVE);
}

LayoutManager::~LayoutManager() {
  for (WindowInfos::iterator it = windows_.begin();
       it != windows_.end(); ++it) {
    wm_->xconn()->DestroyWindow((*it)->input_win);
  }

  KeyBindings* kb = wm_->key_bindings();
  kb->RemoveAction("switch-to-overview-mode");
  kb->RemoveAction("switch-to-active-mode");
  kb->RemoveAction("cycle-active-forward");
  kb->RemoveAction("cycle-active-backward");
  kb->RemoveAction("cycle-magnification-forward");
  kb->RemoveAction("cycle-magnification-backward");
  kb->RemoveAction("switch-to-active-mode-for-magnified");
  kb->RemoveAction("delete-active-window");

  magnified_window_ = NULL;
  active_window_ = NULL;
  floating_tab_ = NULL;
  window_under_floating_tab_ = NULL;
  tab_summary_ = NULL;
}

bool LayoutManager::IsInputWindow(XWindow xid) const {
  return (GetWindowByInput(xid) != NULL);
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
      win->StackCompositedAbove(wm_->overview_window_depth(), NULL);
      win->SetCompositedOpacity(0, 0);
      win->ShowComposited();
      win->SetCompositedOpacity(1, kWindowAnimMs);
      tab_summary_ = win;
    } else {
      win->ShowComposited();
    }
    return;
  }

  switch (win->type()) {
    // TODO: Remove this.  mock_chrome currently depends on the WM to
    // position tab summary windows, but Chrome just creates
    // override-redirect ("popup") windows and positions them itself.
    case WmIpc::WINDOW_TYPE_CHROME_TAB_SUMMARY: {
      int x = (width_ - win->client_width()) / 2;
      int y = y_ + height_ - overview_height_ - win->client_height() -
          kTabSummaryPadding;
      win->StackCompositedAbove(wm_->overview_window_depth(), NULL);
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
      win->HideComposited();
      win->StackCompositedAbove(wm_->floating_tab_depth(), NULL);
      win->ScaleComposited(1.0, 1.0, 0);
      win->SetCompositedOpacity(0.75, 0);
      // No worries if we were already tracking a different tab; it should
      // get destroyed soon enough.
      if (floating_tab_) {
        floating_tab_->HideComposited();
      }
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
      DCHECK(!create_browser_window_);
      win->HideComposited();
      win->StackCompositedAbove(wm_->overview_window_depth(), NULL);
      create_browser_window_ = win;
      if (mode_ == MODE_OVERVIEW) {
        create_browser_window_->ShowComposited();
        ArrangeOverview();
      }
      break;
    }
    case WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL:
    case WmIpc::WINDOW_TYPE_UNKNOWN: {
      if (win->transient_for_window()) {
        win->ShowComposited();
        win->AddPassiveButtonGrab();
        if (win->transient_for_window() == active_window_ &&
            active_window_->focused() &&
            mode_ == MODE_ACTIVE) {
          // We should only take the focus if we belong to
          // 'active_window_'.
          win->TakeFocus(wm_->GetCurrentTimeFromServer());
          // The _NET_ACTIVE_WINDOW property should already refer to
          // 'active_window_', since it previously had the focus.
        }
        break;
      }

      if (FLAGS_lm_force_fullscreen_windows) {
        int width = width_;
        int height = height_;
        if (FLAGS_lm_honor_window_size_hints)
          win->GetMaxSize(width_, height_, &width, &height);
        win->ResizeClient(width, height, Window::GRAVITY_NORTHWEST);
      }

      // Make sure that we hear about button presses on this window.
      VLOG(1) << "Adding passive button grab for new top-level window "
              << win->xid();
      win->AddPassiveButtonGrab();

      // Create a new offscreen input window.
      ref_ptr<WindowInfo> info(new WindowInfo(
          win, wm_->CreateInputWindow(-1, -1, 1, 1)));
      input_to_window_[info->input_win] = win;
      win->MoveClientOffscreen();
      win->ShowComposited();

      switch (mode_) {
        case MODE_ACTIVE:
          // Activate the new window, adding it to the right of the
          // currently-active window.
          if (active_window_) {
            int old_index = GetWindowIndex(*active_window_);
            CHECK_GE(old_index, 0);
            WindowInfos::iterator it = windows_.begin() + old_index + 1;
            windows_.insert(it, info);
          } else {
            windows_.push_back(info);
          }
          SetActiveWindow(win,
                          WindowInfo::STATE_ACTIVE_MODE_IN_FROM_RIGHT,
                          WindowInfo::STATE_ACTIVE_MODE_OUT_TO_LEFT);
          break;
        case MODE_OVERVIEW:
          // In overview mode, just put new windows on the right.
          windows_.push_back(info);
          ArrangeOverview();
          break;
      }
      break;
    }
    default:
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
      ArrangeOverview();
  }

  const int index = GetWindowIndex(*win);
  if (index >= 0) {
    WindowInfo* info = windows_[index].get();

    wm_->xconn()->DestroyWindow(info->input_win);
    if (magnified_window_ == win)
      SetMagnifiedWindow(NULL);
    if (active_window_ == win)
      active_window_ = NULL;
    if (window_under_floating_tab_ == win)
      window_under_floating_tab_ = NULL;
    CHECK_EQ(input_to_window_.erase(info->input_win), 1);
    windows_.erase(windows_.begin() + index);

    if (mode_ == MODE_OVERVIEW) {
      ArrangeOverview();
    } else if (mode_ == MODE_ACTIVE) {
      // If there's no active window now, then this was probably active
      // previously.  Choose a new active window if possible; relinquish
      // the focus.
      if (!active_window_) {
        if (!windows_.empty()) {
          int new_index = (index + windows_.size() - 1) % windows_.size();
          SetActiveWindow(windows_[new_index]->win,
                          WindowInfo::STATE_ACTIVE_MODE_IN_FROM_LEFT,
                          WindowInfo::STATE_ACTIVE_MODE_OUT_TO_RIGHT);
        } else {
          if (win->focused()) {
            wm_->SetActiveWindowProperty(None);
            wm_->TakeFocus();
          }
        }
      }
    }
  } else if (win->transient_for_window() && win->focused()) {
    // If this was a focused transient window, pass the focus to its owner
    // if possible.
    if (!TakeFocus()) {
      wm_->SetActiveWindowProperty(None);
      wm_->TakeFocus();
    }
  }
}

bool LayoutManager::HandleButtonPress(
    XWindow xid, int x, int y, int button, Time timestamp) {
  // If the press was received by one of our input windows, we should
  // switch to active mode, activating the corresponding client window.
  Window* win = GetWindowByInput(xid);
  if (win) {
    if (button == 1) {
      active_window_ = win;
      SetMode(MODE_ACTIVE);
    }
    return true;
  }

  // Otherwise, it probably means that the user previously focused a panel
  // and then clicked back on a top-level or transient window.  Take back
  // the focus.
  win = wm_->GetWindow(xid);
  if (!win)
    return false;

  if (!GetWindowInfo(*win) && !win->transient_for_window())
    return false;

  if (win == active_window_ || win->transient_for_window()) {
    VLOG(1) << "Got button press for "
            << (win == active_window_ ? "active" : "transient")
            << " window " << xid << "; removing pointer grab";
    wm_->xconn()->RemoveActivePointerGrab(true);

    // If there's a modal transient window, give the focus to it instead.
    FocusWindowOrModalTransient(win, timestamp);
  } else {
    // This is unexpected, but we don't want to leave the pointer
    // grabbed.
    LOG(WARNING) << "Got button press on inactive window " << win->xid()
                 << " (active is "
                 << (active_window_ ? active_window_->xid() : 0) << "); "
                 << "removing pointer grab";
    wm_->xconn()->RemoveActivePointerGrab(true);
  }
  return true;
}

bool LayoutManager::HandlePointerEnter(XWindow xid, Time timestamp) {
  Window* win = GetWindowByInput(xid);
  if (!win)
    return false;
  if (mode_ != MODE_OVERVIEW)
    return true;
  if (win != magnified_window_) {
    SetMagnifiedWindow(win);
    ArrangeOverview();
    SendTabSummaryMessage(win, true);
  }
  return true;
}

bool LayoutManager::HandlePointerLeave(XWindow xid, Time timestamp) {
  // TODO: Decide if we want to unmagnify the window here or not.
  return (GetWindowByInput(xid) != NULL);
}

bool LayoutManager::HandleFocusChange(XWindow xid, bool focus_in) {
  Window* win = wm_->GetWindow(xid);
  if (!win)
    return false;
  // If this is neither a top-level nor transient window, we don't care
  // about the focus change.
  if (!GetWindowInfo(*win) && !win->transient_for_window())
    return false;

  if (focus_in) {
    VLOG(1) << "Got focus-in for "
            << (win->transient_for_window() ? "transient" : "top-level")
            << " window " << xid << "; removing passive button grab";
    win->RemovePassiveButtonGrab();

    // When a transient window gets the focus, we say that its owner is
    // the "active" window (in the _NET_ACTIVE_WINDOW sense).
    if (win->transient_for_window())
      wm_->SetActiveWindowProperty(win->transient_for_window()->xid());
    else
      wm_->SetActiveWindowProperty(win->xid());
  } else {
    // Listen for button presses on this window so we'll know when it
    // should be focused again.
    VLOG(1) << "Got focus-out for "
            << (win->transient_for_window() ? "transient" : "top-level")
            << " window " << xid << "; re-adding passive button grab";
    win->AddPassiveButtonGrab();
  }
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
                     << xid << " (current is "
                     << (floating_tab_ ? floating_tab_->xid() : 0) << ")";
      } else {
        floating_tab_event_coalescer_->StorePosition(x, y);
      }
      break;
    }
    case WmIpc::Message::WM_FOCUS_WINDOW: {
      XWindow xid = msg.param(0);
      Window* win = wm_->GetWindow(xid);
      if (!win || !GetWindowInfo(*win)) {
        // Not our window -- maybe it's a panel.
        return false;
      }
      active_window_ = win;
      SetMode(MODE_ACTIVE);
      break;
    }
    case WmIpc::Message::WM_SWITCH_TO_OVERVIEW_MODE: {
      SetMode(MODE_OVERVIEW);
      Window* window = wm_->GetWindow(msg.param(0));
      SetMagnifiedWindow(window);
      SendTabSummaryMessage(window, true);
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
    VLOG(1) << "Got _NET_ACTIVE_WINDOW request to focus " << e.window
            << " (requestor says its currently-active window is " << e.data.l[2]
            << "; real active window is " << wm_->active_window_xid() << ")";

    Window* new_active_win = NULL;
    if (GetWindowInfo(*win)) {
      new_active_win = win;
    } else if (win->transient_for_window() &&
               GetWindowInfo(*(win->transient_for_window()))) {
      // If we got a _NET_ACTIVE_WINDOW request for a transient, switch to its
      // owner instead.
      new_active_win = win->transient_for_window();
    } else {
      return false;
    }

    CHECK(new_active_win);
    if (new_active_win != active_window_) {
      SetActiveWindow(new_active_win,
                      WindowInfo::STATE_ACTIVE_MODE_IN_FADE,
                      WindowInfo::STATE_ACTIVE_MODE_OUT_FADE);
    }

    // If the _NET_ACTIVE_WINDOW request was for a transient window, give
    // it the focus if its owner doesn't have any modal windows.
    // TODO: We should be passing the timestamp from e.data.l[1] here
    // instead of getting a new one, but we need one that's later than the
    // new one that SetActiveWindow() generated.
    if (win != new_active_win && new_active_win->GetTopModalTransient() == NULL)
      win->TakeFocus(wm_->GetCurrentTimeFromServer());

    return true;
  }

  return false;
}

Window* LayoutManager::GetChromeWindow() {
  for (size_t i = 0; i < windows_.size(); ++i) {
    if (windows_[i]->win->type() == WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL)
      return windows_[i]->win;
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
    Window* win = GetOverviewWindowAtPoint(x, y);

    // If the user is moving the pointer up to the tab summary, pretend
    // like the pointer is still in the magnified window.
    if (!win && magnified_window_) {
      if (PointIsInTabSummary(x, y) ||
          PointIsBetweenMagnifiedWindowAndTabSummary(x, y)) {
        win = magnified_window_;
      }
    }

    // Only allow docking into Chrome windows.
    if (win && win->type() != WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL)
      win = NULL;

    if (win != window_under_floating_tab_) {
      // Notify the old and new windows about the new position.
      if (window_under_floating_tab_) {
        WmIpc::Message msg(
            WmIpc::Message::CHROME_NOTIFY_FLOATING_TAB_OVER_TOPLEVEL);
        msg.set_param(0, floating_tab_->xid());
        msg.set_param(1, 0);  // left
        wm_->wm_ipc()->SendMessage(window_under_floating_tab_->xid(), msg);
      }
      if (win) {
        WmIpc::Message msg(
            WmIpc::Message::CHROME_NOTIFY_FLOATING_TAB_OVER_TOPLEVEL);
        msg.set_param(0, floating_tab_->xid());
        msg.set_param(1, 1);  // entered
        wm_->wm_ipc()->SendMessage(win->xid(), msg);
      }
      window_under_floating_tab_ = win;
      SetMagnifiedWindow(win);
      ArrangeOverview();
      SendTabSummaryMessage(win, true);
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
  if (mode_ != MODE_ACTIVE || !active_window_)
    return false;

  FocusWindowOrModalTransient(active_window_, wm_->GetCurrentTimeFromServer());
  return true;
}

void LayoutManager::Resize(int width, int height) {
  if (width == width_ && height == height_)
    return;

  width_ = width;
  height_ = height;
  overview_height_ = kOverviewHeightFraction * height_;

  if (FLAGS_lm_force_fullscreen_windows) {
    for (WindowInfos::iterator it = windows_.begin();
         it != windows_.end(); ++it) {
      int width = width_;
      int height = height_;
      if (FLAGS_lm_honor_window_size_hints)
        (*it)->win->GetMaxSize(width_, height_, &width, &height);
      (*it)->win->ResizeClient(width, height, Window::GRAVITY_NORTHWEST);
    }
  }

  switch (mode_) {
    case MODE_ACTIVE:
      ArrangeActive();
      break;
    case MODE_OVERVIEW:
      ArrangeOverview();
      break;
    default:
      DCHECK(false) << "Unhandled mode " << mode_ << " during resize";
  }
}

Window* LayoutManager::GetWindowByInput(XWindow xid) const {
  return FindWithDefault(input_to_window_, xid, static_cast<Window*>(NULL));
}

int LayoutManager::GetWindowIndex(const Window& win) const {
  for (size_t i = 0; i < windows_.size(); ++i) {
    if (windows_[i]->win == &win)
      return static_cast<int>(i);
  }
  return -1;
}

LayoutManager::WindowInfo* LayoutManager::GetWindowInfo(const Window& win) {
  int index = GetWindowIndex(win);
  return (index >= 0) ? windows_[index].get() : NULL;
}

void LayoutManager::SetActiveWindow(Window* win,
                                    WindowInfo::State state_for_new_win,
                                    WindowInfo::State state_for_old_win) {
  CHECK(win);

  if (windows_.empty() || mode_ != MODE_ACTIVE || active_window_ == win)
    return;

  if (active_window_) {
    WindowInfo* old_info = GetWindowInfo(*active_window_);
    CHECK(old_info);
    old_info->state = state_for_old_win;
  }

  active_window_ = win;
  WindowInfo* info = GetWindowInfo(*win);
  CHECK(info);
  info->state = state_for_new_win;

  ArrangeActive();
}

void LayoutManager::SwitchToActiveMode(bool activate_magnified_win) {
  if (mode_ == MODE_ACTIVE)
    return;
  if (activate_magnified_win && magnified_window_)
    active_window_ = magnified_window_;
  SetMode(MODE_ACTIVE);
}

void LayoutManager::ActivateWindowByIndex(int index) {
  if (windows_.empty() || mode_ != MODE_ACTIVE)
    return;

  if (index < 0)
    index = static_cast<int>(windows_.size()) + index;

  if (index < 0 || index >= static_cast<int>(windows_.size()))
    return;
  if (windows_[index]->win == active_window_)
    return;

  SetActiveWindow(windows_[index]->win,
                  WindowInfo::STATE_ACTIVE_MODE_IN_FADE,
                  WindowInfo::STATE_ACTIVE_MODE_OUT_FADE);
}

void LayoutManager::MagnifyWindowByIndex(int index) {
  if (windows_.empty() || mode_ != MODE_OVERVIEW)
    return;

  if (index < 0)
    index = static_cast<int>(windows_.size()) + index;

  if (index < 0 || index >= static_cast<int>(windows_.size()))
    return;
  if (windows_[index]->win == magnified_window_)
    return;

  SetMagnifiedWindow(windows_[index]->win);
  ArrangeOverview();
  SendTabSummaryMessage(magnified_window_, true);
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
      if (!active_window_ && magnified_window_)
        active_window_ = magnified_window_;
      if (!active_window_ && !windows_.empty())
        active_window_ = windows_[0]->win;
      SetMagnifiedWindow(NULL);
      ArrangeActive();
      break;
    }
    case MODE_OVERVIEW: {
      if (create_browser_window_)
        create_browser_window_->ShowComposited();
      SetMagnifiedWindow(NULL);
      // Leave 'active_window_' alone, so we can activate the same window
      // if we return to active mode on an Escape keypress.

      if (active_window_ &&
          (active_window_->focused() ||
           active_window_->GetFocusedTransient())) {
        // We need to take the input focus away here; otherwise the
        // previously-focused window would continue to get keyboard events
        // in overview mode.  Let the WindowManager decide what to do with it.
        wm_->SetActiveWindowProperty(None);
        wm_->TakeFocus();
      }
      ArrangeOverview();
      break;
    }
  }
  AddKeyBindingsForMode(mode_);

  // Let all Chrome windows know about the new layout mode.
  for (WindowInfos::iterator it = windows_.begin();
       it != windows_.end(); ++it) {
    Window* win = (*it)->win;
    if (win->type() == WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL)
      SendModeMessage(win);
  }
}

void LayoutManager::GetWindowScaling(const Window* window,
                                     int max_width,
                                     int max_height,
                                     int* width_out,
                                     int* height_out,
                                     double* scale_out) {
  CHECK(window);

  int client_width = window->client_width();
  int client_height = window->client_height();
  double scale_x = max_width / static_cast<double>(client_width);
  double scale_y = max_height / static_cast<double>(client_height);
  double tmp_scale = min(scale_x, scale_y);

  if (width_out)
    *width_out = tmp_scale * client_width;
  if (height_out)
    *height_out = tmp_scale * client_height;
  if (scale_out)
    *scale_out = tmp_scale;
}

void LayoutManager::ArrangeActive() {
  VLOG(1) << "Arranging windows for active mode";
  if (windows_.empty())
    return;
  if (!active_window_)
    active_window_ = windows_[0]->win;

  for (WindowInfos::iterator it = windows_.begin();
       it != windows_.end(); ++it) {
    WindowInfo* info = it->get();
    Window* win = info->win;

    // Center window vertically
    const int y = y_ + max(0, (height_ - win->client_height())) / 2;

    // TODO: This is a pretty huge mess.  Replace it with a saner way of
    // tracking animation state for windows.
    if (win == active_window_) {
      // Center window horizontally
      const int x = max(0, (width_ - win->client_width())) / 2;
      if (info->state == WindowInfo::STATE_NEW ||
          info->state == WindowInfo::STATE_ACTIVE_MODE_OFFSCREEN ||
          info->state == WindowInfo::STATE_ACTIVE_MODE_IN_FROM_RIGHT ||
          info->state == WindowInfo::STATE_ACTIVE_MODE_IN_FROM_LEFT ||
          info->state == WindowInfo::STATE_ACTIVE_MODE_IN_FADE) {
        // If the active window is in a state that requires that it be
        // animated in from a particular location or opacity, move it there
        // immediately.
        if (info->state == WindowInfo::STATE_ACTIVE_MODE_IN_FROM_RIGHT) {
          win->MoveComposited(x_ + width_, y, 0);
          win->SetCompositedOpacity(1.0, 0);
          win->ScaleComposited(1.0, 1.0, 0);
        } else if (info->state == WindowInfo::STATE_ACTIVE_MODE_IN_FROM_LEFT) {
          win->MoveComposited(x_ - win->client_width(), y, 0);
          win->SetCompositedOpacity(1.0, 0);
          win->ScaleComposited(1.0, 1.0, 0);
        } else if (info->state == WindowInfo::STATE_ACTIVE_MODE_IN_FADE) {
          win->SetCompositedOpacity(0, 0);
          win->MoveComposited(
              x_ - 0.5 * kWindowFadeSizeFraction * win->client_width(),
              y_ - 0.5 * kWindowFadeSizeFraction * win->client_height(),
              0);
          win->ScaleComposited(
              1 + kWindowFadeSizeFraction, 1 + kWindowFadeSizeFraction, 0);
        } else {
          // Animate new or offscreen windows as moving up from the bottom
          // of the layout area.
          win->MoveComposited(x, y_ + height_, 0);
          win->ScaleComposited(1.0, 1.0, 0);
        }
      }
      // In any case, give the window input focus and animate it moving to
      // its final location.
      win->MoveClient(x, y);
      // TODO: Doing a re-layout for active mode is often triggered by user
      // input.  We should use the timestamp from the key or mouse event
      // instead of fetching a new one from the server, but it'll require
      // some changes to KeyBindings so that timestamps will be passed
      // through to callbacks.
      FocusWindowOrModalTransient(win, wm_->GetCurrentTimeFromServer());
      win->StackCompositedAbove(wm_->active_window_depth(), NULL);
      win->MoveComposited(x, y, kWindowAnimMs);
      win->ScaleComposited(1.0, 1.0, kWindowAnimMs);
      win->SetCompositedOpacity(1.0, kWindowAnimMs);
      info->state = WindowInfo::STATE_ACTIVE_MODE_ONSCREEN;
    } else {
      if (info->state == WindowInfo::STATE_ACTIVE_MODE_OUT_TO_LEFT) {
        win->MoveComposited(x_ - win->client_width(), y, kWindowAnimMs);
      } else if (info->state == WindowInfo::STATE_ACTIVE_MODE_OUT_TO_RIGHT) {
        win->MoveComposited(x_ + width_, y, kWindowAnimMs);
      } else if (info->state == WindowInfo::STATE_ACTIVE_MODE_OUT_FADE) {
        win->SetCompositedOpacity(0, kWindowAnimMs);
        win->MoveComposited(
            x_ + 0.5 * kWindowFadeSizeFraction * win->client_width(),
            y_ + 0.5 * kWindowFadeSizeFraction * win->client_height(),
            kWindowAnimMs);
        win->ScaleComposited(
            kWindowFadeSizeFraction, kWindowFadeSizeFraction, kWindowAnimMs);
      } else if (info->state == WindowInfo::STATE_ACTIVE_MODE_OFFSCREEN) {
        // No need to move it; it was already moved offscreen.
      } else {
        // Slide the window down offscreen and scale it down to its
        // overview size.
        win->MoveComposited(info->x, y_ + height_, kWindowAnimMs);
        win->ScaleComposited(info->scale, info->scale, kWindowAnimMs);
        win->SetCompositedOpacity(0.5, kWindowAnimMs);
      }
      // Fade out the window's shadow entirely so it won't be visible if
      // the window is just slightly offscreen.
      win->SetShadowOpacity(0, kWindowAnimMs);
      win->StackCompositedAbove(wm_->overview_window_depth(), NULL);
      info->state = WindowInfo::STATE_ACTIVE_MODE_OFFSCREEN;
      win->MoveClientOffscreen();
    }
    // TODO: Maybe just unmap input windows.
    if (info->width > 0 && info->height > 0) {
      wm_->ConfigureInputWindow(info->input_win,
                                wm_->width(), wm_->height(),
                                info->width, info->height);
    }
  }
}

void LayoutManager::ArrangeOverview() {
  VLOG(1) << "Arranging windows for overview mode";
  CalculateOverview();
  for (WindowInfos::iterator it = windows_.begin();
       it != windows_.end(); ++it) {
    WindowInfo* info = it->get();
    Window* win = info->win;
    if (info->state == WindowInfo::STATE_NEW ||
        info->state == WindowInfo::STATE_ACTIVE_MODE_OFFSCREEN) {
      win->MoveComposited(info->x, y_ + height_, 0);
      win->ScaleComposited(info->scale, info->scale, 0);
      win->SetCompositedOpacity(0.5, 0);
    }
    VLOG(2) << "Moving " << win << " to (" << info->x
            << ", " << info->y << ") at size " << info->width
            << "x" << info->height << " (scale " << info->scale << ")";
    win->StackCompositedAbove(wm_->overview_window_depth(), NULL);
    win->MoveComposited(info->x, info->y, kWindowAnimMs);
    win->ScaleComposited(info->scale, info->scale, kWindowAnimMs);
    win->MoveClientOffscreen();
    wm_->ConfigureInputWindow(info->input_win,
                              info->x, info->y,
                              info->width, info->height);
    if (magnified_window_ && win != magnified_window_) {
      win->SetCompositedOpacity(0.75, kWindowAnimMs);
    } else {
      win->SetCompositedOpacity(1.0, kWindowAnimMs);
    }
    info->state = (win == magnified_window_) ?
        WindowInfo::STATE_OVERVIEW_MODE_MAGNIFIED :
        WindowInfo::STATE_OVERVIEW_MODE_NORMAL;
  }
  if (create_browser_window_) {
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

void LayoutManager::CalculateOverview() {
  if (windows_.empty())
    return;

  // First, figure out how much space the magnified window (if any) will
  // take up.
  int mag_width = 0;
  int mag_height = 0;
  double mag_scale = 1.0;
  if (magnified_window_) {
    GetWindowScaling(magnified_window_,
                     width_,  // TODO: Cap this if we end up with wide windows.
                     overview_height_,
                     &mag_width,
                     &mag_height,
                     &mag_scale);
  }

  // Now, figure out the maximum size that we want each unmagnified window
  // to be able to take.
  const int num_unmag_windows =
      windows_.size() - (magnified_window_ ? 1 : 0);
  int create_browser_window_width = 0;
  if (create_browser_window_) {
    create_browser_window_width = create_browser_window_->client_width() +
                                  kWindowPadding;
  }
  const int total_unmag_width =
      width_ - (windows_.size() + 1) * kWindowPadding - mag_width -
      create_browser_window_width;
  const int max_unmag_width =
      num_unmag_windows ?
      (total_unmag_width / num_unmag_windows) :
      0;
  const int max_unmag_height = kMaxWindowHeightRatio * overview_height_;

  // Figure out the actual scaling for each window.
  for (int i = 0; static_cast<size_t>(i) < windows_.size(); ++i) {
    WindowInfo* info = windows_[i].get();
    if (info->win == magnified_window_) {
      info->width = mag_width;
      info->height = mag_height;
      info->scale = mag_scale;
    } else {
      GetWindowScaling(info->win,
                       max_unmag_width,
                       max_unmag_height,
                       &(info->width),
                       &(info->height),
                       &(info->scale));
    }
  }

  // Divide up the remaining space among all of the windows, including
  // padding around the outer windows.
  int total_window_width = 0;
  for (int i = 0; static_cast<size_t>(i) < windows_.size(); ++i)
    total_window_width += windows_[i]->width;
  if (create_browser_window_)
    total_window_width += create_browser_window_->client_width();
  int total_padding = width_ - total_window_width;
  if (total_padding < 0) {
    LOG(WARNING) << "Summed width of scaled windows (" << total_window_width
                 << ") exceeds width of overview area (" << width_ << ")";
    total_padding = 0;
  }
  const double padding = create_browser_window_
      ? total_padding / static_cast<double>(windows_.size() + 2) :
        total_padding / static_cast<double>(windows_.size() + 1);

  // Finally, go through and calculate the final position for each window.
  double running_width = 0;
  for (int i = 0; static_cast<size_t>(i) < windows_.size(); ++i) {
    WindowInfo* info = windows_[i].get();
    info->x = round(x_ + running_width + padding);
    info->y = y_ + height_ - info->height;
    running_width += padding + info->width;
  }
}

Window* LayoutManager::GetOverviewWindowAtPoint(int x, int y) const {
  for (int i = 0; static_cast<size_t>(i) < windows_.size(); ++i) {
    WindowInfo* info = windows_[i].get();
    if (info->x <= x && info->y <= y &&
        info->x + info->width > x && info->y + info->height > y) {
      return info->win;
    }
  }
  return NULL;
}

bool LayoutManager::PointIsInTabSummary(int x, int y) const {
  return (tab_summary_ &&
          x >= tab_summary_->client_x() &&
          y >= tab_summary_->client_y() &&
          x < tab_summary_->client_x() + tab_summary_->client_width() &&
          y < tab_summary_->client_y() + tab_summary_->client_height());
}

bool LayoutManager::PointIsBetweenMagnifiedWindowAndTabSummary(
    int x, int y) const {
  if (!magnified_window_ || !tab_summary_) return false;

  for (int i = 0; static_cast<size_t>(i) < windows_.size(); ++i) {
    WindowInfo* info = windows_[i].get();
    if (info->win != magnified_window_) continue;
    return (y >= tab_summary_->client_y() + tab_summary_->client_height() &&
            y < info->y);
  }
  LOG(WARNING) << "magnified_window_ " << magnified_window_->xid()
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
                       StringPrintf("activate-window-with-index-%d", i));
      }
      kb->AddBinding(KeyBindings::KeyCombo(XK_9, KeyBindings::kAltMask),
                     "activate-last-window");
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
                       StringPrintf("magnify-window-with-index-%d", i));
      }
      kb->AddBinding(KeyBindings::KeyCombo(XK_9, KeyBindings::kAltMask),
                     "magnify-last-window");
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
      break;
  }
}

void LayoutManager::CycleActiveWindow(bool forward) {
  if (mode_ != MODE_ACTIVE) {
    LOG(WARNING) << "Ignoring request to cycle active window outside of "
                 << "active mode (current mode is " << mode_ << ")";
    return;
  }
  if (windows_.empty())
    return;

  Window* win = NULL;
  if (!active_window_) {
    win = forward ? windows_[0]->win : windows_[windows_.size()-1]->win;
  } else {
    if (windows_.size() == 1)
      return;
    int old_index = GetWindowIndex(*active_window_);
    int new_index =
        (windows_.size() + old_index + (forward ? 1 : -1)) % windows_.size();
    win = windows_[new_index]->win;
  }
  CHECK(win);

  SetActiveWindow(
      win,
      forward ?
        WindowInfo::STATE_ACTIVE_MODE_IN_FROM_RIGHT :
        WindowInfo::STATE_ACTIVE_MODE_IN_FROM_LEFT,
      forward ?
        WindowInfo::STATE_ACTIVE_MODE_OUT_TO_LEFT :
        WindowInfo::STATE_ACTIVE_MODE_OUT_TO_RIGHT);
}

void LayoutManager::CycleMagnifiedWindow(bool forward) {
  if (mode_ != MODE_OVERVIEW) {
    LOG(WARNING) << "Ignoring request to cycle magnified window outside of "
                 << "overview mode (current mode is " << mode_ << ")";
    return;
  }
  if (windows_.empty())
    return;
  if (magnified_window_ && windows_.size() == 1)
    return;

  if (!magnified_window_ && !active_window_) {
    // If we have no clue about which window to magnify, just choose the
    // first one.
    SetMagnifiedWindow(windows_[0]->win);
  } else {
    if (!magnified_window_) {
      // If no window is magnified, pretend like the active window was
      // magnified so we'll move either to its left or its right.
      magnified_window_ = active_window_;
    }
    CHECK(magnified_window_);
    int old_index = GetWindowIndex(*magnified_window_);
    int new_index =
        (windows_.size() + old_index + (forward ? 1 : -1)) % windows_.size();
    SetMagnifiedWindow(windows_[new_index]->win);
  }
  ArrangeOverview();

  // Tell the magnified window to display a tab summary now that we've
  // rearranged all of the windows.
  SendTabSummaryMessage(magnified_window_, true);
}

void LayoutManager::SetMagnifiedWindow(Window* win) {
  if (magnified_window_ == win)
    return;
  // Hide the previous window's tab summary.
  if (magnified_window_)
    SendTabSummaryMessage(magnified_window_, false);
  magnified_window_ = win;
}

void LayoutManager::SendTabSummaryMessage(Window* win, bool show) {
  if (!win || win->type() != WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL)
    return;
  const WindowInfo* info = GetWindowInfo(*win);
  if (!info)
    return;
  WmIpc::Message msg(WmIpc::Message::CHROME_SET_TAB_SUMMARY_VISIBILITY);
  msg.set_param(0, show);  // show summary
  if (show)
    msg.set_param(1, info->x + 0.5 * info->width);  // center of window
  wm_->wm_ipc()->SendMessage(win->xid(), msg);
}

void LayoutManager::SendModeMessage(Window* win) {
  if (!win || win->type() != WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL)
    return;

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
  wm_->wm_ipc()->SendMessage(win->xid(), msg);
}

void LayoutManager::SendDeleteRequestToActiveWindow() {
  if (mode_ == MODE_ACTIVE && active_window_)
    active_window_->SendDeleteRequest(wm_->GetCurrentTimeFromServer());
}

void LayoutManager::FocusWindowOrModalTransient(Window* win, Time timestamp) {
  Window* modal_win = win->GetTopModalTransient();
  if (modal_win)
    modal_win->TakeFocus(timestamp);
  else
    win->TakeFocus(timestamp);
}

}  // namespace chromeos
