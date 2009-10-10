// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
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
#include <glog/logging.h>

#include "base/callback.h"
#include "base/strutil.h"
#include "window_manager/event_consumer.h"
#include "window_manager/hotkey_overlay.h"
#include "window_manager/key_bindings.h"
#include "window_manager/layout_manager.h"
#include "window_manager/metrics_reporter.h"
#include "window_manager/panel_bar.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/x_connection.h"

// TODO: Make this flag unecessary! On at least one machine, the X Server is
// reporting an error if an overlay window is used.
DEFINE_bool(wm_use_overlay_window, true, "Create and use an overlay window.");

DEFINE_bool(wm_spawn_chrome_on_start, false,
            "Spawn chromium-browser on startup? Requires wm_chrome_command "
            "(note that \"--loop\" gets appended to it).");
DEFINE_string(wm_xterm_command, "xterm", "Command for hotkey xterm spawn.");
DEFINE_string(wm_chrome_command, "google-chrome",
              "Command for hotkey chrome spawn.");
DEFINE_string(wm_background_image, "", "Background image to display");
DEFINE_string(wm_lock_screen_command, "xscreensaver-command -l",
              "Command to lock the screen");

DEFINE_bool(wm_use_compositing, true, "Use compositing");

namespace chromeos {

// Height for the panel bar.
static const int kPanelBarHeight = 18;

// Time to spend fading the hotkey overlay in or out, in milliseconds.
static const int kHotkeyOverlayAnimMs = 100;

static const char* XEventTypeToName(int type) {
  switch (type) {
    CASE_RETURN_LABEL(ButtonPress);
    CASE_RETURN_LABEL(ButtonRelease);
    CASE_RETURN_LABEL(CirculateNotify);
    CASE_RETURN_LABEL(CirculateRequest);
    CASE_RETURN_LABEL(ClientMessage);
    CASE_RETURN_LABEL(ColormapNotify);
    CASE_RETURN_LABEL(ConfigureNotify);
    CASE_RETURN_LABEL(ConfigureRequest);
    CASE_RETURN_LABEL(CreateNotify);
    CASE_RETURN_LABEL(DestroyNotify);
    CASE_RETURN_LABEL(EnterNotify);
    CASE_RETURN_LABEL(Expose);
    CASE_RETURN_LABEL(FocusIn);
    CASE_RETURN_LABEL(FocusOut);
    CASE_RETURN_LABEL(GraphicsExpose);
    CASE_RETURN_LABEL(GravityNotify);
    CASE_RETURN_LABEL(KeymapNotify);
    CASE_RETURN_LABEL(KeyPress);
    CASE_RETURN_LABEL(KeyRelease);
    CASE_RETURN_LABEL(LeaveNotify);
    CASE_RETURN_LABEL(MapNotify);
    CASE_RETURN_LABEL(MappingNotify);
    CASE_RETURN_LABEL(MapRequest);
    CASE_RETURN_LABEL(MotionNotify);
    CASE_RETURN_LABEL(NoExpose);
    CASE_RETURN_LABEL(PropertyNotify);
    CASE_RETURN_LABEL(ReparentNotify);
    CASE_RETURN_LABEL(ResizeRequest);
    CASE_RETURN_LABEL(SelectionClear);
    CASE_RETURN_LABEL(SelectionNotify);
    CASE_RETURN_LABEL(SelectionRequest);
    CASE_RETURN_LABEL(UnmapNotify);
    CASE_RETURN_LABEL(VisibilityNotify);
    default: return "Unknown";
  }
}

static const char* FocusChangeEventModeToName(int mode) {
  switch (mode) {
    CASE_RETURN_LABEL(NotifyNormal);
    CASE_RETURN_LABEL(NotifyWhileGrabbed);
    CASE_RETURN_LABEL(NotifyGrab);
    CASE_RETURN_LABEL(NotifyUngrab);
    default: return "Unknown";
  }
}

static const char* FocusChangeEventDetailToName(int detail) {
  switch (detail) {
    CASE_RETURN_LABEL(NotifyAncestor);
    CASE_RETURN_LABEL(NotifyVirtual);
    CASE_RETURN_LABEL(NotifyInferior);
    CASE_RETURN_LABEL(NotifyNonlinear);
    CASE_RETURN_LABEL(NotifyNonlinearVirtual);
    CASE_RETURN_LABEL(NotifyPointer);
    CASE_RETURN_LABEL(NotifyPointerRoot);
    CASE_RETURN_LABEL(NotifyDetailNone);
    default: return "Unknown";
  }
}

// Callback that sends GDK events to the window manager.
static GdkFilterReturn FilterEvent(GdkXEvent* xevent,
                                   GdkEvent* event,
                                   gpointer data) {
  WindowManager* wm = reinterpret_cast<WindowManager*>(data);
  return wm->HandleEvent(reinterpret_cast<XEvent*>(xevent)) ?
      GDK_FILTER_REMOVE :
      GDK_FILTER_CONTINUE;
}

// Utility method to invoke a passed in closure and return false to indicate
// that the queued callback should be moved from the gdk run loop.
static gboolean callback_runner_once(gpointer data) {
  Closure* const closure = static_cast<Closure *>(data);
  CHECK(closure) << "Null closure passed to callback_runner_once.";
  closure->Run();
  return FALSE;
}

// Callback for a gtk timer that will attempt to send metrics to
// chrome once every timer interval.  We don't really care if a given
// attempt fails, as we'll just keep aggregating metrics and try again
// next time.
// Returns true, as returning false causes the timer to destroy itself.
static gboolean attempt_metrics_report(gpointer data) {
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
      wm_window_(None),
      stage_(NULL),
      background_(NULL),
      stage_window_(None),
      overlay_window_(None),
      overview_window_depth_(NULL),
      active_window_depth_(NULL),
      expanded_panel_depth_(NULL),
      panel_bar_depth_(NULL),
      collapsed_panel_depth_(NULL),
      floating_tab_depth_(NULL),
      overlay_depth_(NULL),
      client_stacking_win_(None),
      mapped_xids_(new Stacker<XWindow>),
      stacked_xids_(new Stacker<XWindow>),
      active_window_xid_(None),
      snooping_key_events_(false),
      showing_hotkey_overlay_(false) {
  CHECK(xconn_);
  CHECK(clutter_);
}

WindowManager::~WindowManager() {
  xconn_->DestroyWindow(client_stacking_win_);
}

bool WindowManager::Init() {
  root_ = xconn_->GetRootWindow();
  CHECK(xconn_->GetWindowGeometry(root_, NULL, NULL, &width_, &height_));

  // Create the atom cache first; RegisterExistence() needs it.
  atom_cache_.reset(new AtomCache(xconn_));

  wm_ipc_.reset(new WmIpc(xconn_, atom_cache_.get()));

  CHECK(RegisterExistence());
  SetEWMHProperties();

  // Set root window's cursor to left pointer.
  if (0 != access("/tmp/use_ugly_x_cursor", F_OK)) {
    // If we get here, the file doesn't exist.
    xconn_->SetWindowCursor(root_, XC_left_ptr);
  }

  stage_ = clutter_->GetDefaultStage();
  stage_window_ = stage_->GetStageXWindow();
  stage_->SetSize(width_, height_);
  stage_->SetStageColor("#222");
  stage_->SetVisibility(true);

  if (!FLAGS_wm_background_image.empty()) {
    background_.reset(clutter_->CreateImage(FLAGS_wm_background_image));
    background_->Move(0, 0, 0);
    background_->SetSize(width_, height_);
    background_->SetVisibility(true);
    stage_->AddActor(background_.get());
  }

  // Set up reference points for stacking.
  // TODO: There has to be a better way to do this.
  overview_window_depth_.reset(CreateActorAbove(background_.get()));
  active_window_depth_.reset(CreateActorAbove(overview_window_depth_.get()));
  expanded_panel_depth_.reset(CreateActorAbove(active_window_depth_.get()));
  panel_bar_depth_.reset(CreateActorAbove(expanded_panel_depth_.get()));
  collapsed_panel_depth_.reset(CreateActorAbove(panel_bar_depth_.get()));
  floating_tab_depth_.reset(CreateActorAbove(collapsed_panel_depth_.get()));
  overlay_depth_.reset(CreateActorAbove(floating_tab_depth_.get()));

  if (FLAGS_wm_use_compositing) {
    if (FLAGS_wm_use_overlay_window) {
      // Create the compositing overlay, put the stage's window inside of it,
      // and make events fall through both to the client windows underneath.
      overlay_window_ = xconn_->GetCompositingOverlayWindow(root_);
      CHECK_NE(overlay_window_, None);
      VLOG(1) << "Reparenting stage window " << stage_window_
              << " into Xcomposite overlay window " << overlay_window_;
      CHECK(xconn_->ReparentWindow(stage_window_, overlay_window_, 0, 0));
      CHECK(xconn_->RemoveInputRegionFromWindow(overlay_window_));
    } else {
      CHECK(xconn_->RaiseWindow(stage_window_));
    }
    CHECK(xconn_->RemoveInputRegionFromWindow(stage_window_));
  }

  // Create a window to use as a reference for stacking client windows.
  client_stacking_win_ = xconn_->CreateWindow(
      root_,   // parent
      -1, -1,  // x, y
      1, 1,    // width, height
      true,    // override_redirect
      false,   // input_only
      0);      // event_mask
  CHECK(client_stacking_win_ != None);
  xconn_->MapWindow(client_stacking_win_);
  xconn_->RaiseWindow(client_stacking_win_);

  // Set up keybindings.
  key_bindings_.reset(new KeyBindings(xconn_));
  if (!FLAGS_wm_xterm_command.empty()) {
    key_bindings_->AddAction(
        "launch-terminal",
        NewPermanentCallback(this, &WindowManager::LaunchTerminalCallback),
        NULL, NULL);
    key_bindings_->AddBinding(
        KeyBindings::KeyCombo(
            XK_t, KeyBindings::kControlMask | KeyBindings::kAltMask),
        "launch-terminal");
  }
  if (!FLAGS_wm_chrome_command.empty()) {
    key_bindings_->AddAction(
        "launch-chrome",
        NewPermanentCallback(this, &WindowManager::LaunchChromeCallback, false),
        NULL, NULL);
    key_bindings_->AddBinding(
        KeyBindings::KeyCombo(
            XK_n, KeyBindings::kControlMask | KeyBindings::kAltMask),
        "launch-chrome");
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

  key_bindings_->AddAction(
      "toggle-hotkey-overlay",
      NewPermanentCallback(this, &WindowManager::ToggleHotkeyOverlay),
      NULL, NULL);
  key_bindings_->AddBinding(
      KeyBindings::KeyCombo(XK_F8), "toggle-hotkey-overlay");

  // We need to create the layout manager after the stage so we can stack
  // its input windows correctly.
  layout_manager_.reset(new LayoutManager(this, 0, 0, width_, height_));
  event_consumers_.insert(layout_manager_.get());

  panel_bar_.reset(new PanelBar(this, kPanelBarHeight));
  event_consumers_.insert(panel_bar_.get());

  hotkey_overlay_.reset(new HotkeyOverlay(clutter_));
  stage_->AddActor(hotkey_overlay_->group());
  hotkey_overlay_->group()->Lower(overlay_depth_.get());
  hotkey_overlay_->group()->Move(
      stage_->GetWidth() / 2, stage_->GetHeight() / 2, 0);

  // Register a callback to get a shot at all the events that come in.
  gdk_window_add_filter(NULL, FilterEvent, this);

  // Look up existing windows (note that this includes windows created
  // earlier in this method) and then select window management events.
  //
  // TODO: We should be grabbing the server here and then releasing it
  // after selecting SubstructureRedirectMask.  This doesn't work, though,
  // since Clutter is using its own X connection (so Clutter calls that we
  // make while grabbing the server via GDK's connection will block
  // forever).  We would ideally just use a single X connection for
  // everything, but I haven't found a way to do this without causing
  // issues with Clutter not getting all events.  It doesn't seem to
  // receive any when we call clutter_x11_disable_event_retrieval() and
  // forward all events using clutter_x11_handle_event(), and it seems to
  // be missing some events when we use clutter_x11_add_filter() to
  // sidestep GDK entirely (even if the filter returns
  // CLUTTER_X11_FILTER_CONTINUE for everything).
  ManageExistingWindows();
  CHECK(xconn_->SelectInputOnWindow(
            root_,
            SubstructureRedirectMask|StructureNotifyMask|SubstructureNotifyMask,
            true));  // preserve GDK's existing event mask

  UpdateClientListProperty();
  UpdateClientListStackingProperty();

  if (FLAGS_wm_spawn_chrome_on_start) {
    CHECK(!FLAGS_wm_chrome_command.empty());
    g_idle_add(callback_runner_once,
               NewCallback(this, &WindowManager::LaunchChromeCallback, true));
  }

  metrics_reporter_.reset(new MetricsReporter(layout_manager_.get(),
                                              wm_ipc_.get()));
  // Using this function to set the timeout allows for better
  // optimization from the user's perspective at the cost of some
  // timer precision.  We don't really care about timer precision, so
  // that's fine.
  g_timeout_add_seconds(MetricsReporter::kMetricsReportingIntervalInSeconds,
                        attempt_metrics_report,
                        metrics_reporter_.get());

  return true;
}

bool WindowManager::HandleEvent(XEvent* event) {
  VLOG(2) << "Got " << XEventTypeToName(event->type) << " event";
  switch (event->type) {
    case ButtonPress:
      return HandleButtonPress(event->xbutton);
    case ButtonRelease:
      return HandleButtonRelease(event->xbutton);
    case ClientMessage:
      return HandleClientMessage(event->xclient);
    case ConfigureNotify:
      return HandleConfigureNotify(event->xconfigure);
    case ConfigureRequest:
      return HandleConfigureRequest(event->xconfigurerequest);
    case CreateNotify:
      return HandleCreateNotify(event->xcreatewindow);
    case DestroyNotify:
      return HandleDestroyNotify(event->xdestroywindow);
    case EnterNotify:
      return HandleEnterNotify(event->xcrossing);
    case FocusIn:
      // fallthrough
    case FocusOut:
      return HandleFocusChange(event->xfocus);
    case KeyPress:
      return HandleKeyPress(event->xkey);
    case KeyRelease:
      return HandleKeyRelease(event->xkey);
    case LeaveNotify:
      return HandleLeaveNotify(event->xcrossing);
    case MappingNotify:
      XRefreshKeyboardMapping(&(event->xmapping));
      return true;
    case MapNotify:
      return HandleMapNotify(event->xmap);
    case MapRequest:
      return HandleMapRequest(event->xmaprequest);
    case MotionNotify:
      return HandleMotionNotify(event->xmotion);
    case PropertyNotify:
      return HandlePropertyNotify(event->xproperty);
    case ReparentNotify:
      return HandleReparentNotify(event->xreparent);
    case UnmapNotify:
      return HandleUnmapNotify(event->xunmap);
    default:
      if (event->type == xconn_->shape_event_base() + ShapeNotify)
        return HandleShapeNotify(*(reinterpret_cast<XShapeEvent*>(event)));
      return false;
  }
}

XWindow WindowManager::CreateInputWindow(
    int x, int y, int width, int height) {
  XWindow xid = xconn_->CreateWindow(
      root_,  // parent
      x, y,
      width, height,
      true,   // override redirect
      true,   // input only
      ButtonPressMask|EnterWindowMask|LeaveWindowMask);
  CHECK_NE(xid, None);

  if (FLAGS_wm_use_compositing) {
    // If the stage has been reparented into the overlay window, we need to
    // stack the input window under the overlay instead of under the stage
    // (because the stage isn't a sibling of the input window).
    XWindow top_win = overlay_window_ ? overlay_window_ : stage_window_;
    CHECK(xconn_->StackWindow(xid, top_win, false));
  }
  CHECK(xconn_->MapWindow(xid));
  return xid;
}

bool WindowManager::ConfigureInputWindow(
    XWindow xid, int x, int y, int width, int height) {
  VLOG(1) << "Configuring input window " << xid
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

Time WindowManager::GetCurrentTimeFromServer() {
  // Just set a bogus property on our window and wait for the
  // PropertyNotify event so we can get its timestamp.
  CHECK(xconn_->SetIntProperty(
            wm_window_,
            GetXAtom(ATOM_CHROME_GET_SERVER_TIME),    // atom
            XA_ATOM,                                  // type
            GetXAtom(ATOM_CHROME_GET_SERVER_TIME)));  // value
  XEvent event;
  xconn_->WaitForEvent(wm_window_, PropertyChangeMask, &event);
  return event.xproperty.time;
}

Window* WindowManager::GetWindow(XWindow xid) {
  return FindWithDefault(client_windows_, xid, ref_ptr<Window>()).get();
}

void WindowManager::LockScreen() {
  LOG(INFO) << "Locking screen via: " << FLAGS_wm_lock_screen_command;

  const string command = StringPrintf(
                          "%s", FLAGS_wm_lock_screen_command.c_str());
  if (system(command.c_str()) < 0)
    LOG(WARNING) << "Unable to lock screen via: " << command;
}

void WindowManager::TakeFocus() {
  if (!layout_manager_->TakeFocus() &&
      !panel_bar_->TakeFocus()) {
    xconn_->FocusWindow(root_, CurrentTime);
  }
}

void WindowManager::HandlePanelBarVisibilityChange(bool visible) {
  if (visible) {
    layout_manager_->Resize(width_, height_ - kPanelBarHeight);
  } else {
    layout_manager_->Resize(width_, height_);
  }
}

bool WindowManager::SetActiveWindowProperty(XWindow xid) {
  VLOG(1) << "Setting active window to " << xid;
  if (!xconn_->SetIntProperty(
          root_, GetXAtom(ATOM_NET_ACTIVE_WINDOW), XA_WINDOW, xid)) {
    return false;
  }
  active_window_xid_ = xid;
  return true;
}

bool WindowManager::GetManagerSelection(
    XAtom atom, XWindow manager_win, Time timestamp) {
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
  XClientMessageEvent msg;
  msg.type = ClientMessage;
  msg.window = root_;
  msg.type = GetXAtom(ATOM_MANAGER);
  msg.format = XConnection::kLongFormat;
  msg.data.l[0] = timestamp;
  msg.data.l[1] = atom;
  CHECK(xconn_->SendEvent(root_,
                          reinterpret_cast<XEvent*>(&msg),
                          StructureNotifyMask));

  // If there was an old manager running, wait for its window to go away.
  if (current_manager != None) {
    XEvent event;
    do {
      CHECK(xconn_->WaitForEvent(current_manager, StructureNotifyMask, &event));
    } while (event.type != DestroyNotify);
  }

  return true;
}

bool WindowManager::RegisterExistence() {
  // Create an offscreen window to take ownership of the selection and
  // receive properties.
  wm_window_ = xconn_->CreateWindow(root_,   // parent
                                    -1, -1,  // position
                                    1, 1,    // dimensions
                                    true,    // override redirect
                                    false,   // input only
                                    PropertyChangeMask);  // event mask
  CHECK_NE(wm_window_, None);
  VLOG(1) << "Created window " << wm_window_
          << " for registering ourselves as the window manager";

  // Set the window's title and wait for the notify event so we can get a
  // timestamp from the server.
  CHECK(xconn_->SetStringProperty(
            wm_window_, GetXAtom(ATOM_NET_WM_NAME), "wm"));
  XEvent event;
  xconn_->WaitForEvent(wm_window_, PropertyChangeMask, &event);
  Time timestamp = event.xproperty.time;

  if (!GetManagerSelection(GetXAtom(ATOM_WM_S0), wm_window_, timestamp) ||
      !GetManagerSelection(
          GetXAtom(ATOM_NET_WM_CM_S0), wm_window_, timestamp)) {
    return false;
  }

  return true;
}

bool WindowManager::SetEWMHProperties() {
  bool success = true;

  success &= xconn_->SetIntProperty(
      root_, GetXAtom(ATOM_NET_NUMBER_OF_DESKTOPS), XA_CARDINAL, 1);
  success &= xconn_->SetIntProperty(
      root_, GetXAtom(ATOM_NET_CURRENT_DESKTOP), XA_CARDINAL, 0);

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
  // the size of the screen minus the panel bar's area.
  vector<int> workarea;
  workarea.push_back(0);  // x
  workarea.push_back(0);  // y
  workarea.push_back(width_);
  workarea.push_back(height_ - kPanelBarHeight);
  success &= xconn_->SetIntArrayProperty(
      root_, GetXAtom(ATOM_NET_WORKAREA), XA_CARDINAL, workarea);

  // Let clients know that we're the current WM and that we at least
  // partially conform to EWMH.
  XAtom check_atom = GetXAtom(ATOM_NET_SUPPORTING_WM_CHECK);
  success &= xconn_->SetIntProperty(
      root_, check_atom, XA_WINDOW, wm_window_);
  success &= xconn_->SetIntProperty(
      wm_window_, check_atom, XA_WINDOW, wm_window_);

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

ClutterInterface::Actor* WindowManager::CreateActorAbove(
    ClutterInterface::Actor* bottom_actor) {
  ClutterInterface::Actor* actor = clutter_->CreateGroup();
  stage_->AddActor(actor);
  if (bottom_actor) {
    actor->Raise(bottom_actor);
  } else {
    actor->LowerToBottom();
  }
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
    // XQueryTree() returns child windows in bottom-to-top stacking order.
    stacked_xids_->AddOnTop(xid);
    Window* win = TrackWindow(xid);
    if (win && win->mapped())
      HandleMappedWindow(win);
  }
  return true;
}

Window* WindowManager::TrackWindow(XWindow xid) {
  // Don't manage our internal windows.
  if (IsInternalWindow(xid))
    return NULL;
  for (set<EventConsumer*>::const_iterator it = event_consumers_.begin();
       it != event_consumers_.end(); ++it) {
    if ((*it)->IsInputWindow(xid))
      return NULL;
  }

  // We don't care about InputOnly windows either.
  // TODO: Don't call GetWindowAttributes() so many times; we call in it
  // Window's c'tor as well.
  XWindowAttributes attr;
  if (xconn_->GetWindowAttributes(xid, &attr) && attr.c_class == InputOnly)
    return NULL;

  VLOG(1) << "Managing window " << xid;
  Window* win = GetWindow(xid);
  if (win) {
    LOG(WARNING) << "Window " << xid << " is already being managed";
  } else {
    ref_ptr<Window> win_ref(new Window(this, xid));
    client_windows_.insert(make_pair(xid, win_ref));
    win = win_ref.get();
    if (!win->override_redirect() && !win->transient_for_window()) {
      // Move non-override-redirect windows offscreen before they get
      // mapped so they won't accidentally receive events.  We also don't
      // need to move transient windows; they'll get positioned over their
      // parents in Window's constructor.
      win->MoveClientOffscreen();
    }
  }
  return win;
}

void WindowManager::HandleMappedWindow(Window* win) {
  // _NET_CLIENT_LIST contains mapped, managed (i.e.
  // non-override-redirect) client windows.  We store all mapped, tracked
  // (i.e. composited) windows in 'mapped_xids_', since override-redirect
  // can be turned on and off, but we only need to update the property if
  // it's actually changing.
  if (mapped_xids_->Contains(win->xid())) {
    LOG(WARNING) << "Got map notify for " << win->xid() << ", which is "
                 << "already listed in 'mapped_xids_'";
  } else {
    mapped_xids_->AddOnTop(win->xid());
  }

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
    if (win && win->mapped() && !win->override_redirect())
      values.push_back(*it);
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

void WindowManager::SetKeyEventSnooping(bool snoop) {
  if (snooping_key_events_ == snoop)
    return;

  snooping_key_events_ = snoop;
  CHECK(xconn_->GrabServer());
  SelectKeyEventsOnTree(root_, snooping_key_events_);
  CHECK(xconn_->UngrabServer());
}

void WindowManager::SelectKeyEventsOnTree(XWindow root, bool snoop) {
  queue<XWindow> windows;
  windows.push(root);
  while (!windows.empty()) {
    XWindow window = windows.front();
    windows.pop();

    vector<XWindow> children;
    if (xconn_->GetChildWindows(window, &children)) {
      for (vector<XWindow>::const_iterator child = children.begin();
           child != children.end(); ++child) {
        windows.push(*child);
      }
    }

    // Make sure that we don't remove SubstructureNotifyMask from the
    // actual root window; it was already selected (and needs to be so
    // that we'll learn about new toplevel client windows).
    int event_mask = KeyPressMask|KeyReleaseMask;
    if (window != root_)
      event_mask |= SubstructureNotifyMask;

    if (snooping_key_events_)
      xconn_->SelectInputOnWindow(window, event_mask, true);
    else
      xconn_->DeselectInputOnWindow(window, event_mask);
  }
}

bool WindowManager::HandleButtonPress(const XButtonEvent& e) {
  VLOG(1) << "Handling button press in window " << e.window;
  // TODO: Also have consumers register the windows that they're interested
  // in, so we don't need to offer the event to all of them here?
  for (set<EventConsumer*>::iterator it = event_consumers_.begin();
       it != event_consumers_.end(); ++it) {
    if ((*it)->HandleButtonPress(e.window, e.x, e.y, e.button, e.time))
      return true;
  }
  return false;
}

bool WindowManager::HandleButtonRelease(const XButtonEvent& e) {
  VLOG(1) << "Handling button release in window " << e.window;
  for (set<EventConsumer*>::iterator it = event_consumers_.begin();
       it != event_consumers_.end(); ++it) {
    if ((*it)->HandleButtonRelease(e.window, e.x, e.y, e.button, e.time))
      return true;
  }
  return false;
}

bool WindowManager::HandleClientMessage(const XClientMessageEvent& e) {
  VLOG(2) << "Handling client message";
  WmIpc::Message msg;
  if (wm_ipc_->GetMessage(e, &msg)) {
    for (set<EventConsumer*>::iterator it = event_consumers_.begin();
         it != event_consumers_.end(); ++it) {
      if ((*it)->HandleChromeMessage(msg))
        return true;
    }
    LOG(WARNING) << "Ignoring unhandled WM message of type " << msg.type();
  } else {
    for (set<EventConsumer*>::iterator it = event_consumers_.begin();
         it != event_consumers_.end(); ++it) {
      if ((*it)->HandleClientMessage(e))
        return true;
    }
    LOG(WARNING) << "Ignoring unhandled X ClientMessage of type "
                 << e.message_type << " ("
                 << GetXAtomName(e.message_type) << ")";
  }
  return false;
}

bool WindowManager::HandleConfigureNotify(const XConfigureEvent& e) {
  // Even though _NET_CLIENT_LIST_STACKING only contains client windows
  // that we're managing, we also need to keep track of other (e.g.
  // override-redirect, or even untracked) windows: we receive
  // notifications of the form "X is on top of Y", so we need to know where
  // Y is even if we're not managing it so that we can stack X correctly.
  if (!stacked_xids_->Contains(e.window)) {
    // If this isn't an immediate child of the root window (e.g. it's a
    // child of an immediate child and we're hearing about it because we're
    // snooping key events and selected SubstructureNotify on the immediate
    // child), ignore the ConfigureNotify.
    return false;
  }

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
      stacked_xids_->AddAbove(e.above, e.window);
    } else {
      // 'above' being unset means that the window is stacked beneath its
      // siblings.
      if (e.above != None) {
        LOG(WARNING) << "ConfigureNotify for " << e.window << " said that it's "
                     << "stacked above " << e.above << ", which we don't know "
                     << "about";
      }
      stacked_xids_->AddOnBottom(e.window);
    }
  }

  Window* win = GetWindow(e.window);
  if (!win)
    return false;
  VLOG(1) << "Handling configure notify for " << e.window << " to pos ("
          << e.x << ", " << e.y << ") and size " << e.width << "x" << e.height;

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
        win->StackCompositedAbove(above_win->actor(), NULL);
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

  return true;
}

bool WindowManager::HandleConfigureRequest(const XConfigureRequestEvent& e) {
  Window* win = GetWindow(e.window);
  if (!win)
    return false;

  VLOG(1) << "Handling configure request for " << e.window << " to pos ("
          << e.x << ", " << e.y << ") and size " << e.width << "x" << e.height;
  if (win->override_redirect()) {
    LOG(WARNING) << "Huh?  Got a ConfigureRequest event for override-redirect "
                 << "window " << e.window;
  }

  // We only let clients move transient windows (requests for
  // override-redirect windows won't be redirected to us in the first
  // place).
  if (win->transient_for_window()) {
    win->MoveClient(e.x, e.y);
    win->transient_for_window()->MoveAndScaleCompositedTransientWindow(win, 0);
  }

  // GTK sometimes sends us goofy ConfigureRequests that ask for the
  // default size (200x200) even when it falls outside of the min- and
  // max-size hints that were specified earlier.  Stick to the hints here.
  int win_width = e.width;
  int win_height = e.height;
  win->GetMaxSize(e.width, e.height, &win_width, &win_height);

  // Check if any of the event consumers want to modify this request.
  for (set<EventConsumer*>::iterator it = event_consumers_.begin();
       it != event_consumers_.end(); ++it) {
    int permitted_width = win_width;
    int permitted_height = win_height;
    if ((*it)->HandleWindowResizeRequest(
            win, &permitted_width, &permitted_height)) {
      DCHECK_GT(permitted_width, 0);
      DCHECK_GT(permitted_height, 0);
      win_width = permitted_width;
      win_height = permitted_height;
      break;
    }
  }

  if (win_width != win->client_width() || win_height != win->client_height())
    win->ResizeClient(win_width, win_height, Window::GRAVITY_NORTHWEST);
  return true;
}

bool WindowManager::HandleCreateNotify(const XCreateWindowEvent& e) {
  VLOG(1) << "Handling create notify for " << e.window;

  if (snooping_key_events_) {
    VLOG(1) << "Selecting key events on " << e.window;
    CHECK(xconn_->GrabServer());
    SelectKeyEventsOnTree(e.window, true);
    CHECK(xconn_->UngrabServer());
  }

  // Don't bother doing anything else for windows which aren't direct
  // children of the root window.
  XWindow parent = None;
  if (!xconn_->GetParentWindow(e.window, &parent) || parent != root_)
    return false;

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

  XWindowAttributes attr;
  if (!xconn_->GetWindowAttributes(e.window, &attr)) {
    LOG(WARNING) << "Window " << e.window << " went away while we were "
                 << "handling its CreateNotify event";
  } else {
    if (!GetWindow(e.window)) {
      // override-redirect means that the window manager isn't going to
      // intercept this window's structure events, but we still need to
      // composite the window, so we'll create a Window object for it
      // regardless.
      TrackWindow(e.window);
    } else {
      LOG(WARNING) << "That's weird; got a create notify for " << e.window
                   << ", which we already know about";
    }
  }

  return true;
}

bool WindowManager::HandleDestroyNotify(const XDestroyWindowEvent& e) {
  VLOG(1) << "Handling destroy notify for " << e.window;

  if (stacked_xids_->Contains(e.window))
    stacked_xids_->Remove(e.window);

  // Don't bother doing anything else for windows which aren't direct
  // children of the root window.
  if (!GetWindow(e.window))
    return false;

  // TODO: If the code to remove a window gets more involved, move it into
  // a separate RemoveWindow() method -- window_test.cc currently erases
  // windows from 'client_windows_' directly to simulate windows being
  // destroyed.
  client_windows_.erase(e.window);
  return true;
}

bool WindowManager::HandleEnterNotify(const XEnterWindowEvent& e) {
  VLOG(1) << "Handling enter notify for " << e.window;
  for (set<EventConsumer*>::iterator it = event_consumers_.begin();
       it != event_consumers_.end(); ++it) {
    if ((*it)->HandlePointerEnter(e.window, e.time))
      return true;
  }
  return false;
}

bool WindowManager::HandleFocusChange(const XFocusChangeEvent& e) {
  // Don't bother doing anything when we lose or gain the focus due to a
  // grab.
  if (e.mode != NotifyNormal && e.mode != NotifyWhileGrabbed)
    return false;

  bool focus_in = (e.type == FocusIn);
  VLOG(1) << "Handling focus-" << (focus_in ? "in" : "out") << " event for "
          << e.window << " with mode " << FocusChangeEventModeToName(e.mode)
          << " and detail " << FocusChangeEventDetailToName(e.detail);

  Window* win = GetWindow(e.window);
  if (win)
    win->set_focused(focus_in);
  for (set<EventConsumer*>::iterator it = event_consumers_.begin();
       it != event_consumers_.end(); ++it) {
    if ((*it)->HandleFocusChange(e.window, focus_in))
      return true;
  }
  return false;
}

bool WindowManager::HandleKeyPress(const XKeyEvent& e) {
  KeySym keysym = xconn_->GetKeySymFromKeyCode(e.keycode);

  if (snooping_key_events_)
    hotkey_overlay_->HandleKeyPress(keysym);

  if (key_bindings_.get()) {
    if (key_bindings_->HandleKeyPress(keysym, e.state))
      return true;
  }

  return false;
}

bool WindowManager::HandleKeyRelease(const XKeyEvent& e) {
  KeySym keysym = xconn_->GetKeySymFromKeyCode(e.keycode);

  if (snooping_key_events_)
    hotkey_overlay_->HandleKeyRelease(keysym);

  if (key_bindings_.get()) {
    if (key_bindings_->HandleKeyRelease(keysym, e.state))
      return true;
  }

  return false;
}

bool WindowManager::HandleLeaveNotify(const XLeaveWindowEvent& e) {
  VLOG(1) << "Handling leave notify for " << e.window;
  for (set<EventConsumer*>::iterator it = event_consumers_.begin();
       it != event_consumers_.end(); ++it) {
    if ((*it)->HandlePointerLeave(e.window, e.time))
      return true;
  }
  return false;
}

bool WindowManager::HandleMapNotify(const XMapEvent& e) {
  Window* win = GetWindow(e.window);
  if (!win)
    return false;
  VLOG(1) << "Handling map notify for " << e.window;
  win->set_mapped(true);
  HandleMappedWindow(win);

  // If _NET_CLIENT_LIST changed (i.e. a non-override-redirect window just
  // got mapped), update the property.
  if (!win->override_redirect())
    UpdateClientListProperty();
  return true;
}

bool WindowManager::HandleMapRequest(const XMapRequestEvent& e) {
  VLOG(1) << "Handling map request for " << e.window;
  Window* win = GetWindow(e.window);
  if (!win) {
    // This probably won't do much good; if we don't know about the window,
    // we're not going to be compositing it.
    LOG(WARNING) << "Mapping " << e.window << ", which we somehow didn't "
                 << "already know about";
    xconn_->MapWindow(e.window);
  } else {
    if (win->override_redirect()) {
      LOG(WARNING) << "Huh?  Got a MapRequest event for override-redirect "
                   << "window " << e.window;
    }
    win->MapClient();
    win->StackClientBelow(client_stacking_win_);
  }
  return true;
}

bool WindowManager::HandleMotionNotify(const XMotionEvent& e) {
  for (set<EventConsumer*>::iterator it = event_consumers_.begin();
       it != event_consumers_.end(); ++it) {
    if ((*it)->HandlePointerMotion(e.window, e.x, e.y, e.time))
      return true;
  }
  return false;
}

bool WindowManager::HandlePropertyNotify(const XPropertyEvent& e) {
  Window* win = GetWindow(e.window);
  if (!win)
    return false;

  bool deleted = (e.state == PropertyDelete);
  VLOG(2) << "Handling property notify for " << win->xid() << " about "
          << (deleted ? "deleted" : "added") << " property " << e.atom
          << " (" << GetXAtomName(e.atom) << ")";
  if (e.atom == GetXAtom(ATOM_NET_WM_NAME)) {
    string title;
    if (deleted || !xconn_->GetStringProperty(win->xid(), e.atom, &title)) {
      win->set_title("");
    } else {
      win->set_title(title);
    }
    return true;
  } else if (e.atom == GetXAtom(ATOM_WM_NORMAL_HINTS)) {
    win->FetchAndApplySizeHints();
    return true;
  } else if (e.atom == GetXAtom(ATOM_WM_TRANSIENT_FOR)) {
    // TODO: Ignore these if the window is already mapped?
    win->FetchAndApplyTransientHint();
    return true;
  } else if (e.atom == GetXAtom(ATOM_CHROME_WINDOW_TYPE)) {
    win->FetchAndApplyWindowType(true);  // update_shadow
    return true;
  } else if (e.atom == GetXAtom(ATOM_NET_WM_WINDOW_OPACITY)) {
    win->FetchAndApplyWindowOpacity();
    return true;
  } else if (e.atom == GetXAtom(ATOM_NET_WM_STATE)) {
    win->FetchAndApplyWmState();
    return true;
  } else {
    return false;
  }
}

bool WindowManager::HandleReparentNotify(const XReparentEvent& e) {
  VLOG(1) << "Handling reparent notify for " << e.window << " to " << e.parent;

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
      return false;

    stacked_xids_->Remove(e.window);

    if (GetWindow(e.window)) {
      if (mapped_xids_->Contains(e.window))
        mapped_xids_->Remove(e.window);
      client_windows_.erase(e.window);

      // We're not going to be compositing the window anymore, so
      // unredirect it so it'll get drawn using the usual path.
      // TODO: It might be cleaner to defer redirecting client windows
      // until we get their initial MapRequest so that we don't need to do
      // anything special for windows that get reparented before they're
      // mapped, but we'll need to special-case override-redirected windows
      // in that case.
      xconn_->UnredirectWindowForCompositing(e.window);

      // If the window was already mapped, we need to remap it for the
      // redirection change to take effect.
      // TODO: Check that this is really the case here -- it seems to be so
      // when redirecting a window, at least (if the remap call in
      // ManageExistingWindows() is removed, the windows never get drawn;
      // this may be Clutter-specific).
      xconn_->RemapWindowIfMapped(e.window);
    }
  }
  return true;
}

bool WindowManager::HandleShapeNotify(const XShapeEvent& e) {
  Window* win = GetWindow(e.window);
  if (!win)
    return false;

  VLOG(1) << "Handling " << (e.kind == ShapeBounding ? "bounding" : "clip")
          << " shape notify for " << e.window;
  if (e.kind == ShapeBounding)
    win->FetchAndApplyShape(true);  // update_shadow
  return true;
}

bool WindowManager::HandleUnmapNotify(const XUnmapEvent& e) {
  VLOG(1) << "Handling unmap notify for " << e.window;
  Window* win = GetWindow(e.window);
  if (!win)
    return false;
  SetWmStateProperty(e.window, 0);  // WithdrawnState
  win->set_mapped(false);
  win->HideComposited();
  for (set<EventConsumer*>::iterator it = event_consumers_.begin();
       it != event_consumers_.end(); ++it) {
    (*it)->HandleWindowUnmap(win);
  }

  if (mapped_xids_->Contains(win->xid())) {
    mapped_xids_->Remove(win->xid());
    if (!win->override_redirect())
      UpdateClientListProperty();
  }
  return true;
}

void WindowManager::LaunchTerminalCallback() {
  LOG(INFO) << "Launching xterm via: " << FLAGS_wm_xterm_command;

  const string command = StringPrintf("%s &", FLAGS_wm_xterm_command.c_str());
  if (system(command.c_str()) < 0)
    LOG(WARNING) << "Unable to launch xterm via: " << command;
}

void WindowManager::LaunchChromeCallback(bool loop) {
  const string command = StringPrintf(
      "%s %s&", FLAGS_wm_chrome_command.c_str(), (loop ? "--loop " : ""));
  LOG(INFO) << "Launching chrome via: " << command;
  if (system(command.c_str()) < 0)
    LOG(WARNING) << "Unable to launch chrome via: " << command;
}

void WindowManager::ToggleClientWindowDebugging() {
  if (!client_window_debugging_actors_.empty()) {
    client_window_debugging_actors_.clear();
    return;
  }

  vector<XWindow> xids;
  if (!xconn_->GetChildWindows(root_, &xids))
    return;

  static const int kDebugFadeMs = 100;
  static const char* kBgColor = "#fff";
  static const char* kFgColor = "#000";

  for (vector<XWindow>::iterator it = xids.begin(); it != xids.end(); ++it) {
    int x = 0, y = 0, width = 0, height = 0;
    if (!xconn_->GetWindowGeometry(*it, &x, &y, &width, &height))
      continue;

    ClutterInterface::ContainerActor* group = clutter_->CreateGroup();
    stage_->AddActor(group);
    group->Lower(overlay_depth_.get());
    group->Move(x, y, 0);
    group->SetSize(width, height);
    group->SetVisibility(true);
    group->SetClip(0, 0, width, height);

    ClutterInterface::Actor* rect =
        clutter_->CreateRectangle(kBgColor, kFgColor, 1);
    group->AddActor(rect);
    rect->Move(0, 0, 0);
    rect->SetSize(width, height);
    rect->SetOpacity(0, 0);
    rect->SetOpacity(0.5, kDebugFadeMs);
    rect->SetVisibility(true);

    ClutterInterface::Actor* text = clutter_->CreateText(
        "Sans 10pt", StringPrintf("%u (0x%x)", *it, *it), kFgColor);
    group->AddActor(text);
    text->Move(3, 3, 0);
    text->SetOpacity(0, 0);
    text->SetOpacity(1, kDebugFadeMs);
    text->SetVisibility(true);
    text->Raise(rect);

    client_window_debugging_actors_.push_back(
        ref_ptr<ClutterInterface::Actor>(group));
    client_window_debugging_actors_.push_back(
        ref_ptr<ClutterInterface::Actor>(rect));
    client_window_debugging_actors_.push_back(
        ref_ptr<ClutterInterface::Actor>(text));
  }
}

void WindowManager::ToggleHotkeyOverlay() {
  ClutterInterface::Actor* group = hotkey_overlay_->group();
  showing_hotkey_overlay_ = !showing_hotkey_overlay_;
  if (showing_hotkey_overlay_) {
    hotkey_overlay_->Reset();
    group->SetOpacity(0, 0);
    group->SetVisibility(true);
    group->SetOpacity(1, kHotkeyOverlayAnimMs);
    SetKeyEventSnooping(true);
  } else {
    group->SetOpacity(0, kHotkeyOverlayAnimMs);
    SetKeyEventSnooping(false);
  }
}

}  // namespace chromeos
