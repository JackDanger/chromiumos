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
      magnified_toplevel_(NULL),
      active_toplevel_(NULL),
      floating_tab_(NULL),
      toplevel_under_floating_tab_(NULL),
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

  SetMode(MODE_ACTIVE);
}

LayoutManager::~LayoutManager() {
  KeyBindings* kb = wm_->key_bindings();
  kb->RemoveAction("switch-to-overview-mode");
  kb->RemoveAction("switch-to-active-mode");
  kb->RemoveAction("cycle-active-forward");
  kb->RemoveAction("cycle-active-backward");
  kb->RemoveAction("cycle-magnification-forward");
  kb->RemoveAction("cycle-magnification-backward");
  kb->RemoveAction("switch-to-active-mode-for-magnified");
  kb->RemoveAction("delete-active-window");

  toplevels_.clear();
  magnified_toplevel_ = NULL;
  active_toplevel_ = NULL;
  floating_tab_ = NULL;
  toplevel_under_floating_tab_ = NULL;
  tab_summary_ = NULL;
}

bool LayoutManager::IsInputWindow(XWindow xid) const {
  return (GetToplevelWindowByInputXid(xid) != NULL);
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
        ArrangeToplevelWindowsForOverviewMode();
      }
      break;
    }
    case WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL:
    case WmIpc::WINDOW_TYPE_UNKNOWN: {
      if (win->transient_for_window()) {
        win->ShowComposited();
        win->AddPassiveButtonGrab();
        if (active_toplevel_ != NULL &&
            win->transient_for_window() == active_toplevel_->win() &&
            active_toplevel_->win()->focused() &&
            mode_ == MODE_ACTIVE) {
          // We should only take the focus if we belong to
          // 'active_toplevel_'.
          win->TakeFocus(wm_->GetCurrentTimeFromServer());
          // The _NET_ACTIVE_WINDOW property should already refer to
          // 'active_toplevel_', since it previously had the focus.
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
      VLOG(1) << "Adding passive button grab for new toplevel window "
              << win->xid();
      win->AddPassiveButtonGrab();

      ref_ptr<ToplevelWindow> toplevel(new ToplevelWindow(win, this));
      input_to_toplevel_[toplevel->input_xid()] = toplevel.get();
      win->MoveClientOffscreen();
      win->ShowComposited();

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
          ArrangeToplevelWindowsForOverviewMode();
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
      ArrangeToplevelWindowsForOverviewMode();
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
      ArrangeToplevelWindowsForOverviewMode();
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
  ToplevelWindow* toplevel = GetToplevelWindowByInputXid(xid);
  if (toplevel) {
    if (button == 1) {
      active_toplevel_ = toplevel;
      SetMode(MODE_ACTIVE);
    }
    return true;
  }

  // Otherwise, it probably means that the user previously focused a panel
  // and then clicked back on a toplevel or transient window.  Take back
  // the focus.
  Window* win = wm_->GetWindow(xid);
  if (!win)
    return false;

  toplevel = GetToplevelWindowByWindow(*win);
  if (!toplevel && win->transient_for_window())
    toplevel = GetToplevelWindowByWindow(*(win->transient_for_window()));
  if (!toplevel)
    return false;

  if (toplevel == active_toplevel_) {
    VLOG(1) << "Got button press for "
            << (win == toplevel->win() ? "active" : "transient")
            << " window " << xid << "; removing pointer grab";
    wm_->xconn()->RemoveActivePointerGrab(true);

    // If there's a modal transient window, give the focus to it instead.
    // FIXME: Record the new preferred transient in the ToplevelWindow.
    if (win->transient_for_window())
      win->TakeFocus(timestamp);
    else
      toplevel->FocusWindowOrModalTransient(timestamp);
  } else {
    // This is unexpected, but we don't want to leave the pointer
    // grabbed.
    LOG(WARNING) << "Got button press on inactive window " << win->xid()
                 << " (active is "
                 << (active_toplevel_ ? active_toplevel_->win()->xid() : 0)
                 << "); removing pointer grab";
    wm_->xconn()->RemoveActivePointerGrab(true);
  }
  return true;
}

bool LayoutManager::HandlePointerEnter(XWindow xid, Time timestamp) {
  ToplevelWindow* toplevel = GetToplevelWindowByInputXid(xid);
  if (!toplevel)
    return false;
  if (mode_ != MODE_OVERVIEW)
    return true;
  if (toplevel != magnified_toplevel_) {
    SetMagnifiedToplevelWindow(toplevel);
    ArrangeToplevelWindowsForOverviewMode();
    SendTabSummaryMessage(toplevel, true);
  }
  return true;
}

bool LayoutManager::HandlePointerLeave(XWindow xid, Time timestamp) {
  // TODO: Decide if we want to unmagnify the window here or not.
  return (GetToplevelWindowByInputXid(xid) != NULL);
}

bool LayoutManager::HandleFocusChange(XWindow xid, bool focus_in) {
  Window* win = wm_->GetWindow(xid);
  if (!win)
    return false;
  // If this is neither a toplevel nor transient window, we don't care
  // about the focus change.
  if (!GetToplevelWindowByWindow(*win) && !win->transient_for_window())
    return false;

  if (focus_in) {
    VLOG(1) << "Got focus-in for "
            << (win->transient_for_window() ? "transient" : "toplevel")
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
            << (win->transient_for_window() ? "transient" : "toplevel")
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
                     << msg.param(0) << " while switching to overview mode";
        return true;
      }

      ToplevelWindow* toplevel = GetToplevelWindowByWindow(*win);
      if (!toplevel) {
        LOG(WARNING) << "Ignoring request to magnify non-toplevel window "
                     << msg.param(0) << " while switching to overview mode";
        return true;
      }

      SetMagnifiedToplevelWindow(toplevel);
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
    VLOG(1) << "Got _NET_ACTIVE_WINDOW request to focus " << e.window
            << " (requestor says its currently-active window is " << e.data.l[2]
            << "; real active window is " << wm_->active_window_xid() << ")";
    ToplevelWindow* new_active_toplevel = GetToplevelWindowByWindow(*win);

    // If we got a _NET_ACTIVE_WINDOW request for a transient, switch to
    // its owner instead.
    if (!new_active_toplevel && win->transient_for_window())
      new_active_toplevel =
          GetToplevelWindowByWindow(*(win->transient_for_window()));

    // If we don't know anything about this window, give up.
    if (!new_active_toplevel)
      return false;

    if (new_active_toplevel != active_toplevel_) {
      SetActiveToplevelWindow(new_active_toplevel,
                              ToplevelWindow::STATE_ACTIVE_MODE_IN_FADE,
                              ToplevelWindow::STATE_ACTIVE_MODE_OUT_FADE);
    }

    // If the _NET_ACTIVE_WINDOW request was for a transient window, give
    // it the focus if its owner doesn't have any modal windows.
    // TODO: We should be passing the timestamp from e.data.l[1] here
    // instead of getting a new one, but we need one that's later than the
    // new one that SetActiveWindow() generated.
    if (win != new_active_toplevel->win() &&
        new_active_toplevel->win()->GetTopModalTransient() == NULL) {
      win->TakeFocus(wm_->GetCurrentTimeFromServer());
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
      ArrangeToplevelWindowsForOverviewMode();
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

  active_toplevel_->FocusWindowOrModalTransient(
      wm_->GetCurrentTimeFromServer());
  return true;
}

void LayoutManager::Resize(int width, int height) {
  if (width == width_ && height == height_)
    return;

  width_ = width;
  height_ = height;
  overview_height_ = kOverviewHeightFraction * height_;

  if (FLAGS_lm_force_fullscreen_windows) {
    for (ToplevelWindows::iterator it = toplevels_.begin();
         it != toplevels_.end(); ++it) {
      int width = width_;
      int height = height_;
      if (FLAGS_lm_honor_window_size_hints)
        (*it)->win()->GetMaxSize(width_, height_, &width, &height);
      (*it)->win()->ResizeClient(width, height, Window::GRAVITY_NORTHWEST);
    }
  }

  switch (mode_) {
    case MODE_ACTIVE:
      ArrangeToplevelWindowsForActiveMode();
      break;
    case MODE_OVERVIEW:
      ArrangeToplevelWindowsForOverviewMode();
      break;
    default:
      DCHECK(false) << "Unhandled mode " << mode_ << " during resize";
  }
}


LayoutManager::ToplevelWindow::ToplevelWindow(Window* win,
                                              LayoutManager* layout_manager)
    : win_(win),
      layout_manager_(layout_manager),
      input_xid_(wm()->CreateInputWindow(-1, -1, 1, 1)),
      state_(STATE_NEW),
      overview_x_(0),
      overview_y_(0),
      overview_width_(0),
      overview_height_(0),
      overview_scale_(1.0) {
}

LayoutManager::ToplevelWindow::~ToplevelWindow() {
  wm()->xconn()->DestroyWindow(input_xid_);
  win_ = NULL;
  layout_manager_ = NULL;
  input_xid_ = None;
}

void LayoutManager::ToplevelWindow::ArrangeForActiveMode(
    bool window_is_active) {
  const int layout_x = layout_manager_->x();
  const int layout_y = layout_manager_->y();
  const int layout_width = layout_manager_->width();
  const int layout_height = layout_manager_->height();

  // Center window vertically.
  const int win_y =
      layout_y + max(0, (layout_height - win_->client_height())) / 2;

  // TODO: This is a pretty huge mess.  Replace it with a saner way of
  // tracking animation state for windows.
  if (window_is_active) {
    // Center window horizontally
    const int win_x = max(0, (layout_width - win_->client_width())) / 2;
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
        win_->MoveComposited(win_x, overview_offscreen_y(), 0);
        win_->ScaleComposited(1.0, 1.0, 0);
      }
    }
    // In any case, give the window input focus and animate it moving to
    // its final location.
    win_->MoveClient(win_x, win_y);
    // TODO: Doing a re-layout for active mode is often triggered by user
    // input.  We should use the timestamp from the key or mouse event
    // instead of fetching a new one from the server, but it'll require
    // some changes to KeyBindings so that timestamps will be passed
    // through to callbacks.
    FocusWindowOrModalTransient(wm()->GetCurrentTimeFromServer());
    win_->StackCompositedAbove(wm()->active_window_depth(), NULL);
    win_->MoveComposited(win_x, win_y, kWindowAnimMs);
    win_->ScaleComposited(1.0, 1.0, kWindowAnimMs);
    win_->SetCompositedOpacity(1.0, kWindowAnimMs);
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
      // Slide the window down offscreen and scale it down to its
      // overview size.
      win_->MoveComposited(overview_x_, overview_offscreen_y(), kWindowAnimMs);
      win_->ScaleComposited(overview_scale_, overview_scale_, kWindowAnimMs);
      win_->SetCompositedOpacity(0.5, kWindowAnimMs);
    }
    // Fade out the window's shadow entirely so it won't be visible if
    // the window is just slightly offscreen.
    win_->SetShadowOpacity(0, kWindowAnimMs);
    win_->StackCompositedAbove(wm()->overview_window_depth(), NULL);
    state_ = STATE_ACTIVE_MODE_OFFSCREEN;
    win_->MoveClientOffscreen();
  }
  // TODO: Maybe just unmap input windows.
  wm()->xconn()->ConfigureWindowOffscreen(input_xid_);
}

void LayoutManager::ToplevelWindow::ArrangeForOverviewMode(
    bool window_is_magnified, bool dim_if_unmagnified) {
  if (state_ == STATE_NEW || state_ == STATE_ACTIVE_MODE_OFFSCREEN) {
    win_->MoveComposited(overview_x_, overview_offscreen_y(), 0);
    win_->ScaleComposited(overview_scale_, overview_scale_, 0);
    win_->SetCompositedOpacity(0.5, 0);
  }
  win_->StackCompositedAbove(wm()->overview_window_depth(), NULL);
  win_->MoveComposited(overview_x_, overview_y_, kWindowAnimMs);
  win_->ScaleComposited(overview_scale_, overview_scale_, kWindowAnimMs);
  win_->MoveClientOffscreen();
  wm()->ConfigureInputWindow(input_xid_,
                             overview_x_, overview_y_,
                             overview_width_, overview_height_);
  if (!window_is_magnified && dim_if_unmagnified)
    win_->SetCompositedOpacity(0.75, kWindowAnimMs);
  else
    win_->SetCompositedOpacity(1.0, kWindowAnimMs);

  state_ = window_is_magnified ?
      STATE_OVERVIEW_MODE_MAGNIFIED :
      STATE_OVERVIEW_MODE_NORMAL;
}

void LayoutManager::ToplevelWindow::UpdateOverviewScaling(int max_width,
                                                          int max_height) {
  double scale_x = max_width / static_cast<double>(win_->client_width());
  double scale_y = max_height / static_cast<double>(win_->client_height());
  double tmp_scale = min(scale_x, scale_y);

  overview_width_  = tmp_scale * win_->client_width();
  overview_height_ = tmp_scale * win_->client_height();
  overview_scale_  = tmp_scale;
}

void LayoutManager::ToplevelWindow::FocusWindowOrModalTransient(
    Time timestamp) {
  Window* modal_win = win_->GetTopModalTransient();
  if (modal_win)
    modal_win->TakeFocus(timestamp);
  else
    win_->TakeFocus(timestamp);
}


LayoutManager::ToplevelWindow* LayoutManager::GetToplevelWindowByInputXid(
    XWindow xid) const {
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
  ArrangeToplevelWindowsForActiveMode();
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
  ArrangeToplevelWindowsForOverviewMode();
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
      if (!active_toplevel_ && magnified_toplevel_)
        active_toplevel_ = magnified_toplevel_;
      if (!active_toplevel_ && !toplevels_.empty())
        active_toplevel_ = toplevels_[0].get();
      SetMagnifiedToplevelWindow(NULL);
      ArrangeToplevelWindowsForActiveMode();
      break;
    }
    case MODE_OVERVIEW: {
      if (create_browser_window_)
        create_browser_window_->ShowComposited();
      SetMagnifiedToplevelWindow(NULL);
      // Leave 'active_toplevel_' alone, so we can activate the same window
      // if we return to active mode on an Escape keypress.

      if (active_toplevel_ &&
          (active_toplevel_->win()->focused() ||
           active_toplevel_->win()->GetFocusedTransient())) {
        // We need to take the input focus away here; otherwise the
        // previously-focused window would continue to get keyboard events
        // in overview mode.  Let the WindowManager decide what to do with it.
        wm_->SetActiveWindowProperty(None);
        wm_->TakeFocus();
      }
      ArrangeToplevelWindowsForOverviewMode();
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

void LayoutManager::ArrangeToplevelWindowsForActiveMode() {
  VLOG(1) << "Arranging windows for active mode";
  if (toplevels_.empty())
    return;
  if (!active_toplevel_)
    active_toplevel_ = toplevels_[0].get();

  for (ToplevelWindows::iterator it = toplevels_.begin();
       it != toplevels_.end(); ++it) {
    (*it)->ArrangeForActiveMode(it->get() == active_toplevel_);
  }
}

void LayoutManager::ArrangeToplevelWindowsForOverviewMode() {
  VLOG(1) << "Arranging windows for overview mode";
  CalculateOverview();
  for (ToplevelWindows::iterator it = toplevels_.begin();
       it != toplevels_.end(); ++it) {
    (*it)->ArrangeForOverviewMode(
        (it->get() == magnified_toplevel_),  // window_is_magnified
        (magnified_toplevel_ != NULL));      // dim_if_unmagnified
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
  if (toplevels_.empty())
    return;

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
    int overview_x = round(x_ + running_width + padding);
    int overview_y = y_ + height_ - toplevel->overview_height();
    toplevel->UpdateOverviewPosition(overview_x, overview_y);
    running_width += padding + toplevel->overview_width();
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
            y < toplevel->overview_y());
  }
  LOG(WARNING) << "magnified_toplevel_ " << magnified_toplevel_->win()->xid()
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
  ArrangeToplevelWindowsForOverviewMode();

  // Tell the magnified window to display a tab summary now that we've
  // rearranged all of the windows.
  SendTabSummaryMessage(magnified_toplevel_, true);
}

void LayoutManager::SetMagnifiedToplevelWindow(ToplevelWindow* toplevel) {
  if (magnified_toplevel_ == toplevel)
    return;
  // Hide the previous window's tab summary.
  if (magnified_toplevel_)
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
    msg.set_param(1, toplevel->overview_center_x());
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

}  // namespace chromeos
