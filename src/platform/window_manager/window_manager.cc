// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/window_manager.h"

#include <queue>

extern "C" {
#include <gdk/gdkx.h>
#include <stdlib.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <X11/Xcursor/Xcursor.h>
}
#include <gflags/gflags.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "chromeos/callback.h"
#include "chromeos/obsolete_logging.h"
#include "chromeos/utility.h"
#include "window_manager/event_consumer.h"
#include "window_manager/hotkey_overlay.h"
#include "window_manager/key_bindings.h"
#include "window_manager/layout_manager.h"
#include "window_manager/metrics_reporter.h"
#include "window_manager/panel_manager.h"
#include "window_manager/stacking_manager.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/x_connection.h"

DEFINE_string(wm_xterm_command, "xterm", "Command for hotkey xterm spawn.");
DEFINE_string(wm_background_image, "", "Background image to display");
DEFINE_string(wm_lock_screen_command, "xscreensaver-command -l",
              "Command to lock the screen");
DEFINE_string(wm_configure_monitor_command,
              "/usr/sbin/monitor_reconfigure",
              "Command to configure an external monitor");
DEFINE_string(screenshot_binary,
              "/usr/bin/screenshot",
              "Path to the screenshot binary");
DEFINE_string(screenshot_output_dir,
              ".", "Output directory for screenshots");

DEFINE_bool(wm_use_compositing, true, "Use compositing");

namespace window_manager {

using chromeos::Closure;
using chromeos::NewPermanentCallback;
using std::list;
using std::make_pair;
using std::map;
using std::set;
using std::string;
using std::tr1::shared_ptr;
using std::vector;

// Time to spend fading the hotkey overlay in or out, in milliseconds.
static const int kHotkeyOverlayAnimMs = 100;

// Interval with which we query the keyboard state from the X server to
// update the hotkey overlay (when it's being shown).
static const int kHotkeyOverlayPollMs = 100;

// Look up the event consumers that have registered interest in 'key' in
// 'consumer_map' (one of the WindowManager::*_event_consumers_ member
// variables), and invoke 'function_call' (e.g.
// "HandleWindowPropertyChange(e.window, e.atom)") on each.  Helper macro
// used by WindowManager's event-handling methods.
#define FOR_EACH_EVENT_CONSUMER(consumer_map, key, function_call)              \
  do {                                                                         \
    typeof(consumer_map.begin()) it = consumer_map.find(key);                  \
    if (it != consumer_map.end()) {                                            \
      for (set<EventConsumer*>::iterator ec_it =                               \
           it->second.begin(); ec_it != it->second.end(); ++ec_it) {           \
        (*ec_it)->function_call;                                               \
      }                                                                        \
    }                                                                          \
  } while (0)

#undef DEBUG_EVENTS  // Turn this on if you want to debug events.
#ifdef DEBUG_EVENTS
static const char* XEventTypeToName(int type) {
  switch (type) {
    CHROMEOS_CASE_RETURN_LABEL(ButtonPress);
    CHROMEOS_CASE_RETURN_LABEL(ButtonRelease);
    CHROMEOS_CASE_RETURN_LABEL(CirculateNotify);
    CHROMEOS_CASE_RETURN_LABEL(CirculateRequest);
    CHROMEOS_CASE_RETURN_LABEL(ClientMessage);
    CHROMEOS_CASE_RETURN_LABEL(ColormapNotify);
    CHROMEOS_CASE_RETURN_LABEL(ConfigureNotify);
    CHROMEOS_CASE_RETURN_LABEL(ConfigureRequest);
    CHROMEOS_CASE_RETURN_LABEL(CreateNotify);
    CHROMEOS_CASE_RETURN_LABEL(DestroyNotify);
    CHROMEOS_CASE_RETURN_LABEL(EnterNotify);
    CHROMEOS_CASE_RETURN_LABEL(Expose);
    CHROMEOS_CASE_RETURN_LABEL(FocusIn);
    CHROMEOS_CASE_RETURN_LABEL(FocusOut);
    CHROMEOS_CASE_RETURN_LABEL(GraphicsExpose);
    CHROMEOS_CASE_RETURN_LABEL(GravityNotify);
    CHROMEOS_CASE_RETURN_LABEL(KeymapNotify);
    CHROMEOS_CASE_RETURN_LABEL(KeyPress);
    CHROMEOS_CASE_RETURN_LABEL(KeyRelease);
    CHROMEOS_CASE_RETURN_LABEL(LeaveNotify);
    CHROMEOS_CASE_RETURN_LABEL(MapNotify);
    CHROMEOS_CASE_RETURN_LABEL(MappingNotify);
    CHROMEOS_CASE_RETURN_LABEL(MapRequest);
    CHROMEOS_CASE_RETURN_LABEL(MotionNotify);
    CHROMEOS_CASE_RETURN_LABEL(NoExpose);
    CHROMEOS_CASE_RETURN_LABEL(PropertyNotify);
    CHROMEOS_CASE_RETURN_LABEL(ReparentNotify);
    CHROMEOS_CASE_RETURN_LABEL(ResizeRequest);
    CHROMEOS_CASE_RETURN_LABEL(SelectionClear);
    CHROMEOS_CASE_RETURN_LABEL(SelectionNotify);
    CHROMEOS_CASE_RETURN_LABEL(SelectionRequest);
    CHROMEOS_CASE_RETURN_LABEL(UnmapNotify);
    CHROMEOS_CASE_RETURN_LABEL(VisibilityNotify);
    default: return "Unknown";
  }
}
#endif

static const char* FocusChangeEventModeToName(int mode) {
  switch (mode) {
    CHROMEOS_CASE_RETURN_LABEL(NotifyNormal);
    CHROMEOS_CASE_RETURN_LABEL(NotifyWhileGrabbed);
    CHROMEOS_CASE_RETURN_LABEL(NotifyGrab);
    CHROMEOS_CASE_RETURN_LABEL(NotifyUngrab);
    default: return "Unknown";
  }
}

static const char* FocusChangeEventDetailToName(int detail) {
  switch (detail) {
    CHROMEOS_CASE_RETURN_LABEL(NotifyAncestor);
    CHROMEOS_CASE_RETURN_LABEL(NotifyVirtual);
    CHROMEOS_CASE_RETURN_LABEL(NotifyInferior);
    CHROMEOS_CASE_RETURN_LABEL(NotifyNonlinear);
    CHROMEOS_CASE_RETURN_LABEL(NotifyNonlinearVirtual);
    CHROMEOS_CASE_RETURN_LABEL(NotifyPointer);
    CHROMEOS_CASE_RETURN_LABEL(NotifyPointerRoot);
    CHROMEOS_CASE_RETURN_LABEL(NotifyDetailNone);
    default: return "Unknown";
  }
}

// Callback that sends GDK events to the window manager.
static GdkFilterReturn FilterEvent(GdkXEvent* xevent,
                                   GdkEvent* event,
                                   gpointer data) {
  WindowManager* wm = reinterpret_cast<WindowManager*>(data);
  wm->HandleEvent(reinterpret_cast<XEvent*>(xevent));
  return GDK_FILTER_CONTINUE;
}

// Callback for a GTK timer that will attempt to send metrics to
// Chrome once every timer interval.  We don't really care if a given
// attempt fails, as we'll just keep aggregating metrics and try again
// next time.
// Returns true, as returning false causes the timer to destroy itself.
static gboolean AttemptMetricsReport(gpointer data) {
  MetricsReporter *reporter = reinterpret_cast<MetricsReporter*>(data);
  reporter->AttemptReport();
  return true;
}


WindowManager::WindowManager(XConnection* xconn, ClutterInterface* clutter)
    : xconn_(xconn),
      clutter_(clutter),
      root_(None),
      width_(0),
      height_(0),
      wm_xid_(None),
      stage_(NULL),
      background_(NULL),
      stage_xid_(None),
      overlay_xid_(None),
      background_xid_(None),
      stacking_manager_(NULL),
      mapped_xids_(new Stacker<XWindow>),
      stacked_xids_(new Stacker<XWindow>),
      active_window_xid_(None),
      query_keyboard_state_timer_(0),
      showing_hotkey_overlay_(false),
      wm_ipc_version_(0),
      layout_manager_x_(0),
      layout_manager_y_(0),
      layout_manager_width_(1),
      layout_manager_height_(1) {
  CHECK(xconn_);
  CHECK(clutter_);
}

WindowManager::~WindowManager() {
  if (wm_xid_)
    xconn_->DestroyWindow(wm_xid_);
  if (background_xid_)
    xconn_->DestroyWindow(background_xid_);
}

bool WindowManager::Init() {
  root_ = xconn_->GetRootWindow();
  xconn_->SelectRandREventsOnWindow(root_);
  XConnection::WindowGeometry root_geometry;
  CHECK(xconn_->GetWindowGeometry(root_, &root_geometry));
  width_ = root_geometry.width;
  height_ = root_geometry.height;

  // Create the atom cache first; RegisterExistence() needs it.
  atom_cache_.reset(new AtomCache(xconn_));

  wm_ipc_.reset(new WmIpc(xconn_, atom_cache_.get()));

  CHECK(RegisterExistence());
  SetEwmhGeneralProperties();
  SetEwmhSizeProperties();

  // Set root window's cursor to left pointer.
  xconn_->SetWindowCursor(root_, XC_left_ptr);

  stage_ = clutter_->GetDefaultStage();
  stage_xid_ = stage_->GetStageXWindow();
  stage_->SetName("stage");
  stage_->SetSize(width_, height_);
  // Color equivalent to "#222"
  stage_->SetStageColor(ClutterInterface::Color(0.12549f, 0.12549f, 0.12549f));
  stage_->SetVisibility(true);

  stacking_manager_.reset(new StackingManager(xconn_, clutter_));

  if (!FLAGS_wm_background_image.empty()) {
    background_.reset(clutter_->CreateImage(FLAGS_wm_background_image));
    background_->SetName("background");
    background_->Move(0, 0, 0);
    background_->SetSize(width_, height_);
    stage_->AddActor(background_.get());
    stacking_manager_->StackActorAtTopOfLayer(
        background_.get(), StackingManager::LAYER_BACKGROUND);
    background_->SetVisibility(true);
  }

  if (FLAGS_wm_use_compositing) {
    // Create the compositing overlay, put the stage's window inside of it,
    // and make events fall through both to the client windows underneath.
    overlay_xid_ = xconn_->GetCompositingOverlayWindow(root_);
    CHECK_NE(overlay_xid_, None);
    VLOG(1) << "Reparenting stage window " << XidStr(stage_xid_)
            << " into Xcomposite overlay window " << XidStr(overlay_xid_);
    CHECK(xconn_->ReparentWindow(stage_xid_, overlay_xid_, 0, 0));
    CHECK(xconn_->RemoveInputRegionFromWindow(overlay_xid_));
    CHECK(xconn_->RemoveInputRegionFromWindow(stage_xid_));
  }

  background_xid_ = CreateInputWindow(0, 0, width_, height_, 0);  // no events
  stacking_manager_->StackXidAtTopOfLayer(
      background_xid_, StackingManager::LAYER_BACKGROUND);

  // Set up keybindings.
  key_bindings_.reset(new KeyBindings(xconn_));
  if (!FLAGS_wm_xterm_command.empty()) {
    key_bindings_->AddAction(
        "launch-terminal",
        NewPermanentCallback(this, &WindowManager::LaunchTerminal),
        NULL, NULL);
    key_bindings_->AddBinding(
        KeyBindings::KeyCombo(
            XK_t, KeyBindings::kControlMask | KeyBindings::kAltMask),
        "launch-terminal");
  }
  key_bindings_->AddAction(
      "toggle-client-window-debugging",
      NewPermanentCallback(this, &WindowManager::ToggleClientWindowDebugging),
      NULL, NULL);
  key_bindings_->AddBinding(
      KeyBindings::KeyCombo(XK_F9), "toggle-client-window-debugging");
  if (!FLAGS_wm_lock_screen_command.empty()) {
    key_bindings_->AddAction(
        "lock-screen",
        NewPermanentCallback(this, &WindowManager::LockScreen),
        NULL, NULL);
    key_bindings_->AddBinding(
        KeyBindings::KeyCombo(
            XK_l, KeyBindings::kControlMask | KeyBindings::kAltMask),
        "lock-screen");
  }
  if (!FLAGS_wm_configure_monitor_command.empty()) {
    key_bindings_->AddAction(
        "configure-monitor",
        NewPermanentCallback(this, &WindowManager::ConfigureExternalMonitor),
        NULL, NULL);
    key_bindings_->AddBinding(
        KeyBindings::KeyCombo(
            XK_m, KeyBindings::kControlMask | KeyBindings::kAltMask),
        "configure-monitor");
  }

  key_bindings_->AddAction(
      "toggle-hotkey-overlay",
      NewPermanentCallback(this, &WindowManager::ToggleHotkeyOverlay),
      NULL, NULL);
  key_bindings_->AddBinding(
      KeyBindings::KeyCombo(XK_F8), "toggle-hotkey-overlay");

  if (!FLAGS_screenshot_binary.empty()) {
    key_bindings_->AddAction(
        "take-root-screenshot",
        NewPermanentCallback(this, &WindowManager::TakeScreenshot, false),
        NULL, NULL);
    key_bindings_->AddAction(
        "take-window-screenshot",
        NewPermanentCallback(this, &WindowManager::TakeScreenshot, true),
        NULL, NULL);
    key_bindings_->AddBinding(
        KeyBindings::KeyCombo(XK_Print), "take-root-screenshot");
    key_bindings_->AddBinding(
        KeyBindings::KeyCombo(XK_Print, KeyBindings::kShiftMask),
        "take-window-screenshot");
  }

  // We need to create the layout manager after the stage so we can stack
  // its input windows correctly.
  layout_manager_x_ = 0;
  layout_manager_y_ = 0;
  layout_manager_width_ = width_;
  layout_manager_height_ = height_;
  layout_manager_.reset(
      new LayoutManager(this, layout_manager_x_, layout_manager_y_,
                        layout_manager_width_, layout_manager_height_));
  event_consumers_.insert(layout_manager_.get());

  panel_manager_.reset(new PanelManager(this));
  event_consumers_.insert(panel_manager_.get());

  hotkey_overlay_.reset(new HotkeyOverlay(xconn_, clutter_));
  stage_->AddActor(hotkey_overlay_->group());
  stacking_manager_->StackActorAtTopOfLayer(
      hotkey_overlay_->group(), StackingManager::LAYER_HOTKEY_OVERLAY);
  hotkey_overlay_->group()->Move(width_ / 2, height_ / 2, 0);

  // Register a callback to get a shot at all the events that come in.
  gdk_window_add_filter(NULL, FilterEvent, this);

  // Look up existing windows (note that this includes windows created
  // earlier in this method) and select window management events.
  //
  // TODO: We should be grabbing the server here, calling
  // ManageExistingWindows(), selecting SubstructureRedirectMask, and then
  // releasing the grab.  The grabs lead to deadlocks, though, since
  // Clutter is using its own X connection (so Clutter calls that we make
  // while grabbing the server via GDK's connection will block forever).
  // We would ideally just use a single X connection for everything, but I
  // haven't found a way to do this without causing issues with Clutter not
  // getting all events.  It doesn't seem to receive any when we call
  // clutter_x11_disable_event_retrieval() and forward all events using
  // clutter_x11_handle_event(), and it seems to be missing some events
  // when we use clutter_x11_add_filter() to sidestep GDK entirely (even if
  // the filter returns CLUTTER_X11_FILTER_CONTINUE for everything).
  //
  // As a workaround, the order of these operations is reversed -- we
  // select SubstructureRedirectMask and then query for all windows, so
  // there's no period where new windows could sneak in unnoticed.  This
  // creates the possibility of us getting double-notified about windows
  // being created or mapped, so HandleCreateNotify() and HandleMapNotify()
  // are careful to bail out early if it looks like they're dealing with a
  // window that was already handled by ManageExistingWindows().
  CHECK(xconn_->SelectInputOnWindow(
            root_,
            SubstructureRedirectMask | StructureNotifyMask |
              SubstructureNotifyMask,
            true));  // preserve GDK's existing event mask
  ManageExistingWindows();

  metrics_reporter_.reset(new MetricsReporter(layout_manager_.get(),
                                              wm_ipc_.get()));
  // Using this function to set the timeout allows for better
  // optimization from the user's perspective at the cost of some
  // timer precision.  We don't really care about timer precision, so
  // that's fine.
  g_timeout_add_seconds(MetricsReporter::kMetricsReportingIntervalInSeconds,
                        AttemptMetricsReport,
                        metrics_reporter_.get());

  return true;
}

void WindowManager::HandleEvent(XEvent* event) {
#ifdef DEBUG_EVENTS
    if (event->type == xconn_->damage_event_base() + XDamageNotify) {
    LOG(INFO) << "Got DAMAGE" << " event (" << event->type << ")";
  } else {
    LOG(INFO) << "Got " << XEventTypeToName(event->type)
              << " event (" << event->type << ") in window manager.";
  }
#endif
  switch (event->type) {
    case ButtonPress:
      HandleButtonPress(event->xbutton); break;
    case ButtonRelease:
      HandleButtonRelease(event->xbutton); break;
    case ClientMessage:
      HandleClientMessage(event->xclient); break;
    case ConfigureNotify:
      HandleConfigureNotify(event->xconfigure); break;
    case ConfigureRequest:
      HandleConfigureRequest(event->xconfigurerequest); break;
    case CreateNotify:
      HandleCreateNotify(event->xcreatewindow); break;
    case DestroyNotify:
      HandleDestroyNotify(event->xdestroywindow); break;
    case EnterNotify:
      HandleEnterNotify(event->xcrossing); break;
    case FocusIn:
    case FocusOut:
      HandleFocusChange(event->xfocus); break;
    case KeyPress:
      HandleKeyPress(event->xkey); break;
    case KeyRelease:
      HandleKeyRelease(event->xkey); break;
    case LeaveNotify:
      HandleLeaveNotify(event->xcrossing); break;
    case MapNotify:
      HandleMapNotify(event->xmap); break;
    case MapRequest:
      HandleMapRequest(event->xmaprequest); break;
    case MappingNotify:
      HandleMappingNotify(event->xmapping); break;
    case MotionNotify:
      HandleMotionNotify(event->xmotion); break;
    case PropertyNotify:
      HandlePropertyNotify(event->xproperty); break;
    case ReparentNotify:
      HandleReparentNotify(event->xreparent); break;
    case UnmapNotify:
      HandleUnmapNotify(event->xunmap); break;
    default:
      if (event->type == xconn_->shape_event_base() + ShapeNotify) {
        HandleShapeNotify(*(reinterpret_cast<XShapeEvent*>(event)));
      } else if (event->type ==
                 xconn_->randr_event_base() + RRScreenChangeNotify) {
        HandleRRScreenChangeNotify(
            *(reinterpret_cast<XRRScreenChangeNotifyEvent*>(event)));
      }
  }
}

XWindow WindowManager::CreateInputWindow(
    int x, int y, int width, int height, int event_mask) {
  XWindow xid = xconn_->CreateWindow(
      root_,  // parent
      x, y,
      width, height,
      true,   // override redirect
      true,   // input only
      event_mask);
  CHECK_NE(xid, None);

  if (FLAGS_wm_use_compositing) {
    // If the stage has been reparented into the overlay window, we need to
    // stack the input window under the overlay instead of under the stage
    // (because the stage isn't a sibling of the input window).
    XWindow top_win = overlay_xid_ ? overlay_xid_ : stage_xid_;
    CHECK(xconn_->StackWindow(xid, top_win, false));
  }
  CHECK(xconn_->MapWindow(xid));
  return xid;
}

bool WindowManager::ConfigureInputWindow(
    XWindow xid, int x, int y, int width, int height) {
  VLOG(1) << "Configuring input window " << XidStr(xid)
          << " to (" << x << ", " << y << ") and size "
          << width << "x" << height;
  return xconn_->ConfigureWindow(xid, x, y, width, height);
}

XAtom WindowManager::GetXAtom(Atom atom) {
  return atom_cache_->GetXAtom(atom);
}

const string& WindowManager::GetXAtomName(XAtom xatom) {
  return atom_cache_->GetName(xatom);
}

XTime WindowManager::GetCurrentTimeFromServer() {
  // Just set a bogus property on our window and wait for the
  // PropertyNotify event so we can get its timestamp.
  CHECK(xconn_->SetIntProperty(
            wm_xid_,
            GetXAtom(ATOM_CHROME_GET_SERVER_TIME),    // atom
            XA_ATOM,                                  // type
            GetXAtom(ATOM_CHROME_GET_SERVER_TIME)));  // value
  XTime timestamp = 0;
  xconn_->WaitForPropertyChange(wm_xid_, &timestamp);
  return timestamp;
}

Window* WindowManager::GetWindow(XWindow xid) {
  return FindWithDefault(client_windows_, xid, shared_ptr<Window>()).get();
}

Window* WindowManager::GetWindowOrDie(XWindow xid) {
  Window* win = GetWindow(xid);
  CHECK(win) << "Unable to find window " << xid;
  return win;
}

void WindowManager::TakeFocus() {
  if (!layout_manager_->TakeFocus() && !panel_manager_->TakeFocus())
    xconn_->FocusWindow(root_, CurrentTime);
}

bool WindowManager::SetActiveWindowProperty(XWindow xid) {
  if (active_window_xid_ == xid)
    return true;

  VLOG(1) << "Setting active window to " << XidStr(xid);
  if (!xconn_->SetIntProperty(
          root_, GetXAtom(ATOM_NET_ACTIVE_WINDOW), XA_WINDOW, xid)) {
    return false;
  }
  active_window_xid_ = xid;
  return true;
}

void WindowManager::HandleLayoutManagerAreaChange(
    int x, int y, int width, int height) {
  layout_manager_x_ = x;
  layout_manager_y_ = y;
  layout_manager_width_ = width;
  layout_manager_height_ = height;
  layout_manager_->MoveAndResize(x, y, width, height);
}

void WindowManager::RegisterEventConsumerForWindowEvents(
    XWindow xid, EventConsumer* event_consumer) {
  DCHECK(event_consumer);
  if (!window_event_consumers_[xid].insert(event_consumer).second) {
    LOG(WARNING) << "Got request to register already-present window event "
                 << "consumer " << event_consumer << " for window "
                 << XidStr(xid);
  }
}

void WindowManager::UnregisterEventConsumerForWindowEvents(
    XWindow xid, EventConsumer* event_consumer) {
  DCHECK(event_consumer);
  WindowEventConsumerMap::iterator it = window_event_consumers_.find(xid);
  if (it == window_event_consumers_.end() ||
      it->second.erase(event_consumer) != 1) {
    LOG(WARNING) << "Got request to unregister not-registered window event "
                 << "consumer " << event_consumer << " for window "
                 << XidStr(xid);
  } else {
    if (it->second.empty())
      window_event_consumers_.erase(it);
  }
}

void WindowManager::RegisterEventConsumerForPropertyChanges(
    XWindow xid, XAtom xatom, EventConsumer* event_consumer) {
  DCHECK(event_consumer);
  if (!property_change_event_consumers_[make_pair(xid, xatom)].insert(
          event_consumer).second) {
    LOG(WARNING) << "Got request to register already-present window property "
                 << "listener " << event_consumer << " for window "
                 << XidStr(xid) << " and atom " << XidStr(xatom) << " ("
                 << GetXAtomName(xatom) << ")";
  }
}

void WindowManager::UnregisterEventConsumerForPropertyChanges(
    XWindow xid, XAtom xatom, EventConsumer* event_consumer) {
  DCHECK(event_consumer);
  PropertyChangeEventConsumerMap::iterator it =
      property_change_event_consumers_.find(make_pair(xid, xatom));
  if (it == property_change_event_consumers_.end() ||
      it->second.erase(event_consumer) != 1) {
    LOG(WARNING) << "Got request to unregister not-registered window property "
                 << "listener " << event_consumer << " for window "
                 << XidStr(xid) << " and atom " << XidStr(xatom) << " ("
                 << GetXAtomName(xatom) << ")";
  } else {
    if (it->second.empty())
      property_change_event_consumers_.erase(it);
  }
}

void WindowManager::RegisterEventConsumerForChromeMessages(
    WmIpc::Message::Type message_type, EventConsumer* event_consumer) {
  DCHECK(event_consumer);
  if (!chrome_message_event_consumers_[message_type].insert(
          event_consumer).second) {
    LOG(WARNING) << "Got request to register already-present Chrome message "
                 << "event consumer " << event_consumer << " for message type "
                 << message_type;
  }
}

void WindowManager::UnregisterEventConsumerForChromeMessages(
    WmIpc::Message::Type message_type, EventConsumer* event_consumer) {
  DCHECK(event_consumer);
  ChromeMessageEventConsumerMap::iterator it =
      chrome_message_event_consumers_.find(message_type);
  if (it == chrome_message_event_consumers_.end() ||
      it->second.erase(event_consumer) != 1) {
    LOG(WARNING) << "Got request to unregister not-registered Chrome message "
                 << "event consumer " << event_consumer << " for message type "
                 << message_type;
  } else {
    if (it->second.empty())
      chrome_message_event_consumers_.erase(it);
  }
}

bool WindowManager::GetManagerSelection(
    XAtom atom, XWindow manager_win, XTime timestamp) {
  // Find the current owner of the selection and select events on it so
  // we'll know when it's gone away.
  XWindow current_manager = xconn_->GetSelectionOwner(atom);
  if (current_manager != None)
    xconn_->SelectInputOnWindow(current_manager, StructureNotifyMask, false);

  // Take ownership of the selection.
  CHECK(xconn_->SetSelectionOwner(atom, manager_win, timestamp));
  if (xconn_->GetSelectionOwner(atom) != manager_win) {
    LOG(WARNING) << "Couldn't take ownership of "
                 << GetXAtomName(atom) << " selection";
    return false;
  }

  // Announce that we're here.
  long data[5];
  memset(data, 0, sizeof(data));
  data[0] = timestamp;
  data[1] = atom;
  data[2] = manager_win;
  CHECK(xconn_->SendClientMessageEvent(
            root_, root_, GetXAtom(ATOM_MANAGER), data, StructureNotifyMask));

  // If there was an old manager running, wait for its window to go away.
  if (current_manager != None)
    CHECK(xconn_->WaitForWindowToBeDestroyed(current_manager));

  return true;
}

bool WindowManager::RegisterExistence() {
  // Create an offscreen window to take ownership of the selection and
  // receive properties.
  wm_xid_ = xconn_->CreateWindow(root_,   // parent
                                 -1, -1,  // position
                                 1, 1,    // dimensions
                                 true,    // override redirect
                                 false,   // input only
                                 PropertyChangeMask);  // event mask
  CHECK_NE(wm_xid_, None);
  VLOG(1) << "Created window " << XidStr(wm_xid_)
          << " for registering ourselves as the window manager";

  // Set the window's title and wait for the notify event so we can get a
  // timestamp from the server.
  CHECK(xconn_->SetStringProperty(
            wm_xid_, GetXAtom(ATOM_NET_WM_NAME), GetWmName()));
  XTime timestamp = 0;
  xconn_->WaitForPropertyChange(wm_xid_, &timestamp);

  if (!GetManagerSelection(GetXAtom(ATOM_WM_S0), wm_xid_, timestamp) ||
      !GetManagerSelection(GetXAtom(ATOM_NET_WM_CM_S0), wm_xid_, timestamp)) {
    return false;
  }

  return true;
}

bool WindowManager::SetEwmhGeneralProperties() {
  bool success = true;

  success &= xconn_->SetIntProperty(
      root_, GetXAtom(ATOM_NET_NUMBER_OF_DESKTOPS), XA_CARDINAL, 1);
  success &= xconn_->SetIntProperty(
      root_, GetXAtom(ATOM_NET_CURRENT_DESKTOP), XA_CARDINAL, 0);

  // Let clients know that we're the current WM and that we at least
  // partially conform to EWMH.
  XAtom check_atom = GetXAtom(ATOM_NET_SUPPORTING_WM_CHECK);
  success &= xconn_->SetIntProperty(root_, check_atom, XA_WINDOW, wm_xid_);
  success &= xconn_->SetIntProperty(wm_xid_, check_atom, XA_WINDOW, wm_xid_);

  // State which parts of EWMH we support.
  vector<int> supported;
  supported.push_back(GetXAtom(ATOM_NET_ACTIVE_WINDOW));
  supported.push_back(GetXAtom(ATOM_NET_CLIENT_LIST));
  supported.push_back(GetXAtom(ATOM_NET_CLIENT_LIST_STACKING));
  supported.push_back(GetXAtom(ATOM_NET_CURRENT_DESKTOP));
  supported.push_back(GetXAtom(ATOM_NET_DESKTOP_GEOMETRY));
  supported.push_back(GetXAtom(ATOM_NET_DESKTOP_VIEWPORT));
  supported.push_back(GetXAtom(ATOM_NET_NUMBER_OF_DESKTOPS));
  supported.push_back(GetXAtom(ATOM_NET_WM_NAME));
  supported.push_back(GetXAtom(ATOM_NET_WM_STATE));
  supported.push_back(GetXAtom(ATOM_NET_WM_STATE_FULLSCREEN));
  supported.push_back(GetXAtom(ATOM_NET_WM_STATE_MODAL));
  supported.push_back(GetXAtom(ATOM_NET_WM_WINDOW_OPACITY));
  supported.push_back(GetXAtom(ATOM_NET_WORKAREA));
  success &= xconn_->SetIntArrayProperty(
      root_, GetXAtom(ATOM_NET_SUPPORTED), XA_ATOM, supported);

  return success;
}

bool WindowManager::SetEwmhSizeProperties() {
  bool success = true;

  // We don't use pseudo-large desktops, so this is just the screen size.
  vector<int> geometry;
  geometry.push_back(width_);
  geometry.push_back(height_);
  success &= xconn_->SetIntArrayProperty(
      root_, GetXAtom(ATOM_NET_DESKTOP_GEOMETRY), XA_CARDINAL, geometry);

  // The viewport (top-left corner of the desktop) is just (0, 0) for us.
  vector<int> viewport(2, 0);
  success &= xconn_->SetIntArrayProperty(
      root_, GetXAtom(ATOM_NET_DESKTOP_VIEWPORT), XA_CARDINAL, viewport);

  // This isn't really applicable to us (EWMH just says that it should be
  // used to determine where desktop icons can be placed), but we set it to
  // the size of the screen.
  vector<int> workarea;
  workarea.push_back(0);  // x
  workarea.push_back(0);  // y
  workarea.push_back(width_);
  workarea.push_back(height_);
  success &= xconn_->SetIntArrayProperty(
      root_, GetXAtom(ATOM_NET_WORKAREA), XA_CARDINAL, workarea);

  return success;
}

ClutterInterface::Actor* WindowManager::CreateActorAbove(
    ClutterInterface::Actor* bottom_actor) {
  ClutterInterface::Actor* actor = clutter_->CreateGroup();
  stage_->AddActor(actor);
  if (bottom_actor) {
    actor->Raise(bottom_actor);
  } else {
    actor->LowerToBottom();
  }
  actor->SetName("stacking reference");
  return actor;
}

bool WindowManager::ManageExistingWindows() {
  vector<XWindow> windows;
  if (!xconn_->GetChildWindows(root_, &windows)) {
    return false;
  }

  VLOG(1) << "Taking ownership of " << windows.size() << " window"
          << (windows.size() == 1 ? "" : "s");
  for (size_t i = 0; i < windows.size(); ++i) {
    XWindow xid = windows[i];
    XConnection::WindowAttributes attr;
    if (!xconn_->GetWindowAttributes(xid, &attr))
      continue;
    // XQueryTree() returns child windows in bottom-to-top stacking order.
    stacked_xids_->AddOnTop(xid);
    Window* win = TrackWindow(xid, attr.override_redirect);
    if (win && win->FetchMapState()) {
      win->set_mapped(true);
      HandleMappedWindow(win);
    }
  }
  UpdateClientListStackingProperty();
  return true;
}

Window* WindowManager::TrackWindow(XWindow xid, bool override_redirect) {
  // Don't manage our internal windows.
  if (IsInternalWindow(xid) || stacking_manager_->IsInternalWindow(xid))
    return NULL;
  for (set<EventConsumer*>::const_iterator it = event_consumers_.begin();
       it != event_consumers_.end(); ++it) {
    if ((*it)->IsInputWindow(xid))
      return NULL;
  }

  // We don't care about InputOnly windows either.
  // TODO: Don't call GetWindowAttributes() so many times; we call in it
  // Window's c'tor as well.
  XConnection::WindowAttributes attr;
  if (xconn_->GetWindowAttributes(xid, &attr) &&
      attr.window_class ==
        XConnection::WindowAttributes::WINDOW_CLASS_INPUT_ONLY)
    return NULL;

  VLOG(1) << "Managing window " << XidStr(xid);
  Window* win = GetWindow(xid);
  if (win) {
    LOG(WARNING) << "Window " << XidStr(xid) << " is already being managed";
  } else {
    shared_ptr<Window> win_ref(new Window(this, xid, override_redirect));
    client_windows_.insert(make_pair(xid, win_ref));
    win = win_ref.get();
  }
  return win;
}

void WindowManager::HandleMappedWindow(Window* win) {
  if (!win->override_redirect()) {
    if (mapped_xids_->Contains(win->xid())) {
      LOG(WARNING) << "Got map notify for " << win->xid_str() << ", which is "
                   << "already listed in 'mapped_xids_'";
    } else {
      mapped_xids_->AddOnTop(win->xid());
      UpdateClientListProperty();
      // This only includes mapped windows, so we need to update it now.
      UpdateClientListStackingProperty();
    }
  }

  // Redirect the window if we didn't already do it in response to a MapNotify
  // event (i.e. previously-existing or override-redirect).
  if (FLAGS_wm_use_compositing && !win->redirected())
    win->Redirect();
  SetWmStateProperty(win->xid(), 1);  // NormalState
  for (set<EventConsumer*>::iterator it = event_consumers_.begin();
       it != event_consumers_.end(); ++it) {
    (*it)->HandleWindowMap(win);
  }
}

bool WindowManager::SetWmStateProperty(XWindow xid, int state) {
  vector<int> values;
  values.push_back(state);
  values.push_back(None);  // we don't use icons
  XAtom xatom = GetXAtom(ATOM_WM_STATE);
  return xconn_->SetIntArrayProperty(xid, xatom, xatom, values);
}

bool WindowManager::UpdateClientListProperty() {
  vector<int> values;
  const list<XWindow>& xids = mapped_xids_->items();
  // We store windows in most-to-least-recently-mapped order, but
  // _NET_CLIENT_LIST is least-to-most-recently-mapped.
  for (list<XWindow>::const_reverse_iterator it = xids.rbegin();
       it != xids.rend(); ++it) {
    const Window* win = GetWindow(*it);
    if (!win || !win->mapped() || win->override_redirect()) {
      LOG(WARNING) << "Skipping "
                   << (!win ? "missing" :
                       (!win->mapped() ? "unmapped" : "override-redirect"))
                   << " window " << XidStr(*it)
                   << " when updating _NET_CLIENT_LIST";
    } else {
      values.push_back(*it);
    }
  }
  if (!values.empty()) {
    return xconn_->SetIntArrayProperty(
        root_, GetXAtom(ATOM_NET_CLIENT_LIST), XA_WINDOW, values);
  } else {
    return xconn_->DeletePropertyIfExists(
        root_, GetXAtom(ATOM_NET_CLIENT_LIST));
  }
}

bool WindowManager::UpdateClientListStackingProperty() {
  vector<int> values;
  const list<XWindow>& xids = stacked_xids_->items();
  // We store windows in top-to-bottom stacking order, but
  // _NET_CLIENT_LIST_STACKING is bottom-to-top.
  for (list<XWindow>::const_reverse_iterator it = xids.rbegin();
       it != xids.rend(); ++it) {
    const Window* win = GetWindow(*it);
    if (win && win->mapped() && !win->override_redirect())
      values.push_back(*it);
  }
  if (!values.empty()) {
    return xconn_->SetIntArrayProperty(
        root_, GetXAtom(ATOM_NET_CLIENT_LIST_STACKING), XA_WINDOW, values);
  } else {
    return xconn_->DeletePropertyIfExists(
        root_, GetXAtom(ATOM_NET_CLIENT_LIST_STACKING));
  }
}

void WindowManager::HandleButtonPress(const XButtonEvent& e) {
  VLOG(1) << "Handling button press in window " << XidStr(e.window)
          << " at relative (" << e.x << ", " << e.y << "), absolute ("
          << e.x_root << ", " << e.y_root << ") with button " << e.button;
  FOR_EACH_EVENT_CONSUMER(window_event_consumers_,
                          e.window,
                          HandleButtonPress(e.window, e.x, e.y, e.x_root,
                                            e.y_root, e.button, e.time));
}

void WindowManager::HandleButtonRelease(const XButtonEvent& e) {
  VLOG(1) << "Handling button release in window " << XidStr(e.window)
          << " at relative (" << e.x << ", " << e.y << "), absolute ("
          << e.x_root << ", " << e.y_root << ") with button " << e.button;
  FOR_EACH_EVENT_CONSUMER(window_event_consumers_,
                          e.window,
                          HandleButtonRelease(e.window, e.x, e.y, e.x_root,
                                              e.y_root, e.button, e.time));
}

void WindowManager::HandleClientMessage(const XClientMessageEvent& e) {
  VLOG(2) << "Handling client message for window " << XidStr(e.window)
          << " with type " << XidStr(e.message_type) << " ("
          << GetXAtomName(e.message_type) << ") and format " << e.format;
  WmIpc::Message msg;
  if (wm_ipc_->GetMessage(e.message_type, e.format, e.data.l, &msg)) {
    if (msg.type() == WmIpc::Message::WM_NOTIFY_IPC_VERSION) {
      wm_ipc_version_ = msg.param(0);
      LOG(INFO) << "Got WM_NOTIFY_IPC_VERSION message saying that Chrome is "
                << "using version " << wm_ipc_version_;
    } else {
      FOR_EACH_EVENT_CONSUMER(chrome_message_event_consumers_,
                              msg.type(),
                              HandleChromeMessage(msg));
    }
  } else {
    if (static_cast<XAtom>(e.message_type) == GetXAtom(ATOM_MANAGER) &&
        e.format == XConnection::kLongFormat &&
        (static_cast<XAtom>(e.data.l[1]) == GetXAtom(ATOM_WM_S0) ||
         static_cast<XAtom>(e.data.l[1]) == GetXAtom(ATOM_NET_WM_CM_S0))) {
      if (static_cast<XWindow>(e.data.l[2]) != wm_xid_) {
        LOG(WARNING) << "Ignoring client message saying that window "
                     << XidStr(e.data.l[2]) << " got the "
                     << GetXAtomName(e.data.l[1]) << " manager selection";
      }
      return;
    }
    if (e.format == XConnection::kLongFormat) {
      FOR_EACH_EVENT_CONSUMER(window_event_consumers_,
                              e.window,
                              HandleClientMessage(
                                  e.window, e.message_type, e.data.l));
    } else {
      LOG(WARNING) << "Ignoring client message event with unsupported format "
                   << e.format << " (we only handle 32-bit data currently)";
    }
  }
}

void WindowManager::HandleConfigureNotify(const XConfigureEvent& e) {
  // Even though _NET_CLIENT_LIST_STACKING only contains client windows
  // that we're managing, we also need to keep track of other (e.g.
  // override-redirect, or even untracked) windows: we receive
  // notifications of the form "X is on top of Y", so we need to know where
  // Y is even if we're not managing it so that we can stack X correctly.

  // If this isn't an immediate child of the root window, ignore the
  // ConfigureNotify.
  if (!stacked_xids_->Contains(e.window))
    return;

  // Did the window get restacked from its previous position in
  // 'stacked_xids_'?
  bool restacked = false;

  // Check whether the stacking order changed.
  const XWindow* prev_above = stacked_xids_->GetUnder(e.window);
  if (// On bottom, but not previously
      (e.above == None && prev_above != NULL) ||
      // Not on bottom, but previously on bottom or above different sibling
      (e.above != None && (prev_above == NULL || *prev_above != e.above))) {
    restacked = true;
    stacked_xids_->Remove(e.window);

    if (e.above != None && stacked_xids_->Contains(e.above)) {
      stacked_xids_->AddAbove(e.window, e.above);
    } else {
      // 'above' being unset means that the window is stacked beneath its
      // siblings.
      if (e.above != None) {
        LOG(WARNING) << "ConfigureNotify for " << XidStr(e.window)
                     << " said that it's stacked above " << XidStr(e.above)
                     << ", which we don't know about";
      }
      stacked_xids_->AddOnBottom(e.window);
    }
  }

  Window* win = GetWindow(e.window);
  if (!win)
    return;
  VLOG(1) << "Handling configure notify for " << XidStr(e.window)
          << " to pos (" << e.x << ", " << e.y << ") and size "
          << e.width << "x" << e.height;

  // There are several cases to consider here:
  //
  // - Override-redirect windows' calls to configure themselves are honored
  //   by the X server without any intervention on our part, so we only
  //   need to update their composited positions here.
  // - Regular non-override-redirect windows' configuration calls are
  //   passed to us as ConfigureRequest events, so we would've already
  //   updated both their X and composited configuration in
  //   HandleConfigureRequest().  We don't need to do anything here.
  // - For both types of window, we may have decided to move or resize the
  //   window ourselves earlier through a direct call to Window::Move() or
  //   Resize().  In that case, we would've already updated their
  //   composited position (or at least started the animation) then.

  if (win->override_redirect()) {
    // TODO: This possibly isn't really correct.  We'll get this
    // notification if we were the ones who moved this window (which I
    // guess we shouldn't be doing, since it's override-redirect), so this
    // will effectively cancel out whatever animation we previously
    // started.
    win->MoveComposited(e.x, e.y, 0);

    win->SaveClientPosition(e.x, e.y);
    win->SaveClientAndCompositedSize(e.width, e.height);

    // When we see a stacking change for an override-redirect window, we
    // attempt to restack its actor correspondingly.  If we don't have an
    // actor for the X window directly under it, we walk down the stack
    // until we find one.  This is primarily needed for things like
    // xscreensaver in don't-use-the-MIT-screensaver-extension mode -- when
    // it activates and raises its screensaver window, we need to make sure
    // that it ends up on top of all other override-redirect windows.
    // TODO: We should do something similar for non-override-redirect
    // windows as well, but it's a) less critical there, since we already
    // restack their composited windows ourselves when we restack the
    // client windows, and b) tricky, because we also need to stack Clutter
    // actors that aren't tied to X windows (e.g. the panel bar, shadows,
    // etc.).
    XWindow above_xid = e.above;
    while (above_xid != None) {
      Window* above_win = GetWindow(above_xid);
      if (above_win) {
        win->StackCompositedAbove(above_win->actor(), NULL, false);
        break;
      }
      const XWindow* above_ptr = stacked_xids_->GetUnder(above_xid);
      above_xid = above_ptr ? *above_ptr : None;
    }
  } else {
    if (restacked) {
      // _NET_CLIENT_LIST_STACKING only includes managed (i.e.
      // non-override-redirect) windows, so we only update it when a
      // managed window's stacking position changed.
      UpdateClientListStackingProperty();
    }
  }
}

void WindowManager::HandleConfigureRequest(const XConfigureRequestEvent& e) {
  Window* win = GetWindow(e.window);
  if (!win)
    return;

  VLOG(1) << "Handling configure request for " << XidStr(e.window)
          << " to pos ("
          << ((e.value_mask & CWX) ? StringPrintf("%d", e.x) : string("undef"))
          << ", "
          << ((e.value_mask & CWY) ? StringPrintf("%d", e.y) : string("undef"))
          << ") and size "
          << ((e.value_mask & CWWidth) ?
                  StringPrintf("%d", e.width) : string("undef "))
          << "x"
          << ((e.value_mask & CWHeight) ?
                  StringPrintf("%d", e.height) : string(" undef"));
  if (win->override_redirect()) {
    LOG(WARNING) << "Huh?  Got a ConfigureRequest event for override-redirect "
                 << "window " << win->xid_str();
  }

  const int req_x = (e.value_mask & CWX) ? e.x : win->client_x();
  const int req_y = (e.value_mask & CWY) ? e.y : win->client_y();
  const int req_width =
      (e.value_mask & CWWidth) ? e.width : win->client_width();
  const int req_height =
      (e.value_mask & CWHeight) ? e.height : win->client_height();

  // The X server should reject bogus requests before they get to us, but
  // just in case...
  if (req_width <= 0 || req_height <= 0) {
    LOG(WARNING) << "Ignoring request to resize window " << win->xid_str()
                 << " to " << req_width << "x" << req_height;
    return;
  }

  if (!win->mapped()) {
    // If the window is unmapped, it's unlikely that any event consumers
    // will know what to do with it.  Do whatever we were asked to do.
    if (req_x != win->client_x() || req_y != win->client_y())
      win->MoveClient(req_x, req_y);
    if (req_width != win->client_width() || req_height != win->client_height())
      win->ResizeClient(req_width, req_height, Window::GRAVITY_NORTHWEST);
  } else {
    FOR_EACH_EVENT_CONSUMER(
        window_event_consumers_,
        e.window,
        HandleWindowConfigureRequest(win, req_x, req_y, req_width, req_height));
  }
}

void WindowManager::HandleCreateNotify(const XCreateWindowEvent& e) {
  if (GetWindow(e.window)) {
    LOG(WARNING) << "Ignoring create notify for already-known window "
                 << XidStr(e.window);
    return;
  }

  VLOG(1) << "Handling create notify for "
          << (e.override_redirect ? "override-redirect" : "normal")
          << " window " << XidStr(e.window) << " at (" << e.x << ", " << e.y
          << ") with size " << e.width << "x" << e.height;

  // Don't bother doing anything else for windows which aren't direct
  // children of the root window.
  if (e.parent != root_)
    return;

  // CreateWindow stacks the new window on top of its siblings.
  DCHECK(!stacked_xids_->Contains(e.window));
  stacked_xids_->AddOnTop(e.window);

  // TODO: We should grab the server while we're getting everything set up
  // so we can make sure that the window doesn't go away on us (if it does,
  // we'll get a huge flood of errors triggered by the X requests in
  // Window's c'tor).  Grabbing the server "too much" should be avoided
  // since it blocks other clients, but this bit of initialization takes at
  // most about a fifth of a millisecond (at least from brief testing).
  // But, we can't currently grab the server here for the same reason
  // described above the call to ManageExistingWindows() in Init().

  XConnection::WindowAttributes attr;
  if (!xconn_->GetWindowAttributes(e.window, &attr)) {
    LOG(WARNING) << "Window " << XidStr(e.window)
                 << " went away while we were handling its CreateNotify event";
    return;
  }

  // override-redirect means that the window manager isn't going to
  // intercept this window's structure events, but we still need to
  // composite the window, so we'll create a Window object for it
  // regardless.
  TrackWindow(e.window, e.override_redirect);
}

void WindowManager::HandleDestroyNotify(const XDestroyWindowEvent& e) {
  VLOG(1) << "Handling destroy notify for " << XidStr(e.window);

  DCHECK(window_event_consumers_.find(e.window) ==
         window_event_consumers_.end())
      << "One or more event consumers are still registered for destroyed "
      << "window " << XidStr(e.window);

  if (stacked_xids_->Contains(e.window))
    stacked_xids_->Remove(e.window);

  // Don't bother doing anything else for windows which aren't direct
  // children of the root window.
  Window* win = GetWindow(e.window);
  if (!win)
    return;

  if (!win->override_redirect())
    UpdateClientListStackingProperty();

  // TODO: If the code to remove a window gets more involved, move it into
  // a separate RemoveWindow() method -- window_test.cc currently erases
  // windows from 'client_windows_' directly to simulate windows being
  // destroyed.
  client_windows_.erase(e.window);
}

void WindowManager::HandleEnterNotify(const XEnterWindowEvent& e) {
  VLOG(1) << "Handling enter notify for " << XidStr(e.window);
  FOR_EACH_EVENT_CONSUMER(
      window_event_consumers_,
      e.window,
      HandlePointerEnter(e.window, e.x, e.y, e.x_root, e.y_root, e.time));
}

void WindowManager::HandleFocusChange(const XFocusChangeEvent& e) {
  bool focus_in = (e.type == FocusIn);
  VLOG(1) << "Handling focus-" << (focus_in ? "in" : "out") << " event for "
          << XidStr(e.window) << " with mode "
          << FocusChangeEventModeToName(e.mode)
          << " and detail " << FocusChangeEventDetailToName(e.detail);

  // Don't bother doing anything when we lose or gain the focus due to a
  // grab, or just hear about it because of the pointer's location.
  // TODO: Trying to add/remove button grabs and update hints in response
  // to FocusIn and FocusOut events is likely hopeless; see
  // http://tronche.com/gui/x/xlib/events/input-focus/normal-and-grabbed.html
  // for the full insanity.  It would probably be better to just update
  // things ourselves when we change the focus and rely on the fact that
  // clients shouldn't be assigning the focus themselves.
  if (e.mode == NotifyGrab ||
      e.mode == NotifyUngrab ||
      e.detail == NotifyPointer) {
    return;
  }

  Window* win = GetWindow(e.window);
  if (win)
    win->set_focused(focus_in);

  FOR_EACH_EVENT_CONSUMER(window_event_consumers_,
                          e.window,
                          HandleFocusChange(e.window, focus_in));
}

void WindowManager::HandleKeyPress(const XKeyEvent& e) {
  KeySym keysym = xconn_->GetKeySymFromKeyCode(e.keycode);
  key_bindings_->HandleKeyPress(keysym, e.state);
}

void WindowManager::HandleKeyRelease(const XKeyEvent& e) {
  KeySym keysym = xconn_->GetKeySymFromKeyCode(e.keycode);
  key_bindings_->HandleKeyRelease(keysym, e.state);
}

void WindowManager::HandleLeaveNotify(const XLeaveWindowEvent& e) {
  VLOG(1) << "Handling leave notify for " << XidStr(e.window);
  FOR_EACH_EVENT_CONSUMER(
      window_event_consumers_,
      e.window,
      HandlePointerLeave(e.window, e.x, e.y, e.x_root, e.y_root, e.time));
}

void WindowManager::HandleMapNotify(const XMapEvent& e) {
  Window* win = GetWindow(e.window);
  if (!win)
    return;

  if (win->mapped()) {
    LOG(WARNING) << "Ignoring map notify for already-handled window "
                 << XidStr(e.window);
    return;
  }

  VLOG(1) << "Handling map notify for " << XidStr(e.window);
  win->set_mapped(true);
  HandleMappedWindow(win);
}

void WindowManager::HandleMapRequest(const XMapRequestEvent& e) {
  VLOG(1) << "Handling map request for " << XidStr(e.window);
  Window* win = GetWindow(e.window);
  if (!win) {
    // This probably won't do much good; if we don't know about the window,
    // we're not going to be compositing it.
    LOG(WARNING) << "Mapping " << XidStr(e.window)
                 << ", which we somehow didn't already know about";
    xconn_->MapWindow(e.window);
    return;
  }

  if (FLAGS_wm_use_compositing)
    win->Redirect();
  if (win->override_redirect()) {
    LOG(WARNING) << "Huh?  Got a MapRequest event for override-redirect "
                 << "window " << XidStr(e.window);
  }
  for (set<EventConsumer*>::iterator it = event_consumers_.begin();
       it != event_consumers_.end(); ++it) {
    if ((*it)->HandleWindowMapRequest(win))
      return;
  }
  LOG(WARNING) << "Not mapping window " << win->xid_str() << " with type "
               << win->type();
}

void WindowManager::HandleMappingNotify(const XMappingEvent& e) {
  XRefreshKeyboardMapping(const_cast<XMappingEvent*>(&e));
  hotkey_overlay_->RefreshKeyMappings();
  // TODO: Also update key bindings.
}

void WindowManager::HandleMotionNotify(const XMotionEvent& e) {
  FOR_EACH_EVENT_CONSUMER(
      window_event_consumers_,
      e.window,
      HandlePointerMotion(e.window, e.x, e.y, e.x_root, e.y_root, e.time));
}

void WindowManager::HandlePropertyNotify(const XPropertyEvent& e) {
  Window* win = GetWindow(e.window);
  if (win) {
    bool deleted = (e.state == PropertyDelete);
    VLOG(2) << "Handling property notify for " << win->xid_str() << " about "
            << (deleted ? "deleted" : "") << " property "
            << XidStr(e.atom) << " (" << GetXAtomName(e.atom) << ")";
    if (e.atom == GetXAtom(ATOM_NET_WM_NAME)) {
      string title;
      if (deleted || !xconn_->GetStringProperty(win->xid(), e.atom, &title))
        win->SetTitle("");
      else
        win->SetTitle(title);
    } else if (e.atom == GetXAtom(ATOM_WM_HINTS)) {
      win->FetchAndApplyWmHints();
    } else if (e.atom == GetXAtom(ATOM_WM_NORMAL_HINTS)) {
      win->FetchAndApplySizeHints();
    } else if (e.atom == GetXAtom(ATOM_WM_TRANSIENT_FOR)) {
      win->FetchAndApplyTransientHint();
    } else if (e.atom == GetXAtom(ATOM_CHROME_WINDOW_TYPE)) {
      win->FetchAndApplyWindowType(true);  // update_shadow
    } else if (e.atom == GetXAtom(ATOM_NET_WM_WINDOW_OPACITY)) {
      win->FetchAndApplyWindowOpacity();
    } else if (e.atom == GetXAtom(ATOM_NET_WM_STATE)) {
      win->FetchAndApplyWmState();
    }
  }

  // Notify any event consumers that were interested in this property.
  FOR_EACH_EVENT_CONSUMER(property_change_event_consumers_,
                          make_pair(e.window, e.atom),
                          HandleWindowPropertyChange(e.window, e.atom));
}

void WindowManager::HandleReparentNotify(const XReparentEvent& e) {
  VLOG(1) << "Handling reparent notify for " << XidStr(e.window)
          << " to " << XidStr(e.parent);

  if (e.parent == root_) {
    // If a window got reparented *to* the root window, we want to track
    // it in the stacking order (we don't bother tracking it as a true
    // top-level window; we don't see this happen much outside of Flash
    // windows).  The window gets stacked on top of its new siblings.
    DCHECK(!stacked_xids_->Contains(e.window));
    stacked_xids_->AddOnTop(e.window);
  } else {
    // Otherwise, if it got reparented away from us, stop tracking it.
    // We ignore windows that aren't immediate children of the root window.
    if (!stacked_xids_->Contains(e.window))
      return;

    stacked_xids_->Remove(e.window);

    if (mapped_xids_->Contains(e.window)) {
      mapped_xids_->Remove(e.window);
      UpdateClientListProperty();
    }

    Window* win = GetWindow(e.window);
    if (win) {
      if (!win->override_redirect())
        UpdateClientListStackingProperty();

      bool was_mapped = false;
      if (win->mapped()) {
        // Make sure that all event consumers know that the window's going
        // away.
        for (set<EventConsumer*>::iterator it = event_consumers_.begin();
             it != event_consumers_.end(); ++it) {
          (*it)->HandleWindowUnmap(win);
        }
        was_mapped = true;
      }

      client_windows_.erase(e.window);

      // We're not going to be compositing the window anymore, so
      // unredirect it so it'll get drawn using the usual path.
      if (FLAGS_wm_use_compositing && was_mapped) {
        // TODO: This is only an issue as long as we have Clutter using a
        // separate X connection -- see
        // http://code.google.com/p/chromium-os/issues/detail?id=1151 .
        LOG(WARNING) << "Possible race condition -- unredirecting "
                     << XidStr(e.window) << " after it was already "
                     << "mapped";
        xconn_->UnredirectWindowForCompositing(e.window);
      }
    }
  }
}

void WindowManager::HandleRRScreenChangeNotify(
    const XRRScreenChangeNotifyEvent& e) {
  VLOG(1) << "Got RRScreenChangeNotify event to " << e.width << "x" << e.height;
  if (e.width == width_ && e.height == height_)
    return;

  width_ = e.width;
  height_ = e.height;
  stage_->SetSize(width_, height_);
  if (background_.get())
    background_->SetSize(width_, height_);
  panel_manager_->HandleScreenResize();
  // TODO: This is ugly.  PanelManager::HandleScreenResize() will recompute
  // the area available for the layout manager and call
  // HandleLayoutManagerAreaChange(), so the next call is essentially a
  // no-op.  Safer to leave it in for now, though.
  layout_manager_->MoveAndResize(layout_manager_x_, layout_manager_y_,
                                 layout_manager_width_, layout_manager_height_);
  hotkey_overlay_->group()->Move(width_ / 2, height_ / 2, 0);
  xconn_->ResizeWindow(background_xid_, width_, height_);

  SetEwmhSizeProperties();
}

void WindowManager::HandleShapeNotify(const XShapeEvent& e) {
  Window* win = GetWindow(e.window);
  if (!win)
    return;

  VLOG(1) << "Handling " << (e.kind == ShapeBounding ? "bounding" : "clip")
          << " shape notify for " << XidStr(e.window);
  if (e.kind == ShapeBounding)
    win->FetchAndApplyShape(true);  // update_shadow
}

void WindowManager::HandleUnmapNotify(const XUnmapEvent& e) {
  VLOG(1) << "Handling unmap notify for " << XidStr(e.window);
  Window* win = GetWindow(e.window);
  if (!win)
    return;

  SetWmStateProperty(e.window, 0);  // WithdrawnState
  win->set_mapped(false);
  win->HideComposited();
  for (set<EventConsumer*>::iterator it = event_consumers_.begin();
       it != event_consumers_.end(); ++it) {
    (*it)->HandleWindowUnmap(win);
  }

  if (mapped_xids_->Contains(win->xid())) {
    mapped_xids_->Remove(win->xid());
    UpdateClientListProperty();
    UpdateClientListStackingProperty();
  }
  if (active_window_xid_ == e.window)
    SetActiveWindowProperty(None);
}

void WindowManager::LaunchTerminal() {
  LOG(INFO) << "Launching xterm via: " << FLAGS_wm_xterm_command;

  const string command = StringPrintf("%s &", FLAGS_wm_xterm_command.c_str());
  if (system(command.c_str()) < 0)
    LOG(WARNING) << "Unable to launch xterm via: " << command;
}

void WindowManager::LockScreen() {
  LOG(INFO) << "Locking screen via: " << FLAGS_wm_lock_screen_command;
  if (system(FLAGS_wm_lock_screen_command.c_str()) < 0)
    LOG(WARNING) << "Unable to lock screen via: "
                 << FLAGS_wm_lock_screen_command;
}

void WindowManager::ConfigureExternalMonitor() {
  LOG(INFO) << "Configuring external monitor via: "
            << FLAGS_wm_configure_monitor_command;
  if (system(FLAGS_wm_configure_monitor_command.c_str()) < 0)
    LOG(WARNING) << "Unable to configure the external monitor via: "
                 << FLAGS_wm_configure_monitor_command;
}

void WindowManager::ToggleClientWindowDebugging() {
  if (!client_window_debugging_actors_.empty()) {
    client_window_debugging_actors_.clear();
    return;
  }

  LOG(INFO) << "Clutter actors:\n" << stage_->GetDebugString();

  vector<XWindow> xids;
  if (!xconn_->GetChildWindows(root_, &xids))
    return;

  static const int kDebugFadeMs = 100;
  static const ClutterInterface::Color kBgColor(1.f, 1.f, 1.f);
  static const ClutterInterface::Color kFgColor(0.f, 0.f, 0.f);

  for (vector<XWindow>::iterator it = xids.begin(); it != xids.end(); ++it) {
    XConnection::WindowGeometry geometry;
    if (!xconn_->GetWindowGeometry(*it, &geometry))
      continue;

    ClutterInterface::ContainerActor* group = clutter_->CreateGroup();
    stage_->AddActor(group);
    stacking_manager_->StackActorAtTopOfLayer(
        group, StackingManager::LAYER_DEBUGGING);
    group->SetName("debug group");
    group->Move(geometry.x, geometry.y, 0);
    group->SetSize(geometry.width, geometry.height);
    group->SetVisibility(true);
    group->SetClip(0, 0, geometry.width, geometry.height);

    ClutterInterface::Actor* rect =
        clutter_->CreateRectangle(kBgColor, kFgColor, 1);
    group->AddActor(rect);
    rect->SetName("debug box");
    rect->Move(0, 0, 0);
    rect->SetSize(geometry.width, geometry.height);
    rect->SetOpacity(0, 0);
    rect->SetOpacity(0.5, kDebugFadeMs);
    rect->SetVisibility(true);

    ClutterInterface::Actor* text =
        clutter_->CreateText("Sans 10pt", XidStr(*it), kFgColor);
    group->AddActor(text);
    text->SetName("debug label");
    text->Move(3, 3, 0);
    text->SetOpacity(0, 0);
    text->SetOpacity(1, kDebugFadeMs);
    text->SetVisibility(true);
    text->Raise(rect);

    client_window_debugging_actors_.push_back(
        shared_ptr<ClutterInterface::Actor>(group));
    client_window_debugging_actors_.push_back(
        shared_ptr<ClutterInterface::Actor>(rect));
    client_window_debugging_actors_.push_back(
        shared_ptr<ClutterInterface::Actor>(text));
  }
}

void WindowManager::ToggleHotkeyOverlay() {
  ClutterInterface::Actor* group = hotkey_overlay_->group();
  showing_hotkey_overlay_ = !showing_hotkey_overlay_;
  if (showing_hotkey_overlay_) {
    QueryKeyboardState();
    group->SetOpacity(0, 0);
    group->SetVisibility(true);
    group->SetOpacity(1, kHotkeyOverlayAnimMs);
    DCHECK(query_keyboard_state_timer_ == 0);
    query_keyboard_state_timer_ =
        g_timeout_add(kHotkeyOverlayPollMs, QueryKeyboardStateThunk, this);
  } else {
    group->SetOpacity(0, kHotkeyOverlayAnimMs);
    DCHECK(query_keyboard_state_timer_ != 0);
    g_source_remove(query_keyboard_state_timer_);
    query_keyboard_state_timer_ = 0;
  }
}

void WindowManager::TakeScreenshot(bool use_active_window) {
  string message;

  XWindow xid = None;
  if (use_active_window) {
    if (active_window_xid_ == None) {
      message = "No active window to use for screenshot";
      LOG(WARNING) << message;
    } else {
      xid = active_window_xid_;
    }
  } else {
    xid = root_;
  }

  if (xid != None) {
    // TODO: Include the date and time in the screenshot.
    string filename =
        StringPrintf("%s/screenshot.png", FLAGS_screenshot_output_dir.c_str());
    const string command =
        StringPrintf("%s %s 0x%lx",
                     FLAGS_screenshot_binary.c_str(), filename.c_str(), xid);
    if (system(command.c_str()) < 0) {
      message = StringPrintf("Taking screenshot via \"%s\" failed",
                             command.c_str());
      LOG(WARNING) << message;
    } else {
      message = StringPrintf("Saved screenshot of window %s to %s",
                             XidStr(xid).c_str(), filename.c_str());
      LOG(INFO) << message;
    }
  }

  // TODO: Display the message onscreen.
}

void WindowManager::QueryKeyboardState() {
  vector<uint8_t> keycodes;
  xconn_->QueryKeyboardState(&keycodes);
  hotkey_overlay_->HandleKeyboardState(keycodes);
}

}  // namespace window_manager
