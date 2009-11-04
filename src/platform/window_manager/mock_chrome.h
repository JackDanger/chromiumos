// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __PLATFORM_WINDOW_MANAGER_MOCK_CHROME_H__
#define __PLATFORM_WINDOW_MANAGER_MOCK_CHROME_H__

#include <map>
#include <vector>
#include <tr1/memory>

#include <gflags/gflags.h>
#include <gtkmm.h>
extern "C" {
#include <gdk/gdkx.h>
}

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "window_manager/util.h"

// This file implements a small app that displays windows containing Chrome
// screenshots and allows tabs to be dragged between them.  Its intent is
// to provide a way to quickly mock out different types of interactions
// between Chrome and the window manager.

typedef ::Window XWindow;

namespace chromeos {
class AtomCache;
class WmIpc;
class XConnection;
}

namespace mock_chrome {

class ChromeWindow;
class MockChrome;
class Panel;

// A tab is just a wrapper around an image.  Each tab is owned by a window
// or by a FloatingTab object.
class Tab {
 public:
  Tab(const std::string& image_filename, const std::string& title);

  const std::string& title() const { return title_; }

  // Draw the tab's image to the passed-in widget.  The image can be
  // positioned and scaled within the widget.
  void RenderToGtkWidget(
      Gtk::Widget* widget, int x, int y, int width, int height);

 private:
  Glib::RefPtr<Gdk::Pixbuf> image_;
  std::string title_;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

// Tab summaries are windows that display scaled-down images of all of the
// tabs in a Chrome window.
class TabSummary : public Gtk::Window {
 public:
  explicit TabSummary(ChromeWindow* parent_win);

  XWindow xid() { return xid_; }
  int width() const { return width_; }
  int height() const { return height_; }
  int insert_index() const { return insert_index_; }

 private:
  // Resize the window to fit its contents.
  void Resize();

  // Redraw the entire window.
  void Draw();

  // Draw a line representing where a floating tab will be inserted.  The
  // top of the line is at the passed-in position.
  void DrawInsertCursor(int x, int y);

  // Handle a notification that a floating tab is above us.
  // We update the position of the insert cursor.
  void HandleFloatingTabMovement(int x, int y);

  bool on_button_press_event(GdkEventButton* event);
  bool on_expose_event(GdkEventExpose* event);
  bool on_client_event(GdkEventClient* event);

  // Dimensions of tab images and the amount of padding that should be
  // placed between them.
  static const int kTabImageWidth;
  static const int kTabImageHeight;
  static const int kPadding;
  static const int kInsertCursorWidth;

  ChromeWindow* parent_win_;

  XWindow xid_;
  int width_;
  int height_;

  // Index into 'parent_win_' where a floating tab should be inserted.
  int insert_index_;

  DISALLOW_COPY_AND_ASSIGN(TabSummary);
};

// A floating tab is a draggable window containing a tab object.
class FloatingTab : public Gtk::Window {
 public:
  FloatingTab(MockChrome* chrome, Tab* tab,
              int initial_x, int initial_y,
              int drag_start_offset_x, int drag_start_offset_y);

  // Tell the window manager to move us.
  void Move(int x, int y);

  // Relinquish ownership of the tab.
  Tab* ReleaseTab() { return tab_.release(); }

 private:
  void Draw();

  bool on_expose_event(GdkEventExpose* event);

  static Glib::RefPtr<Gdk::Pixbuf> image_tab_;

  static const int kWidth;
  static const int kHeight;

  MockChrome* chrome_;  // not owned
  scoped_ptr<Tab> tab_;
  XWindow xid_;

  DISALLOW_COPY_AND_ASSIGN(FloatingTab);
};

// This is an actual GTK+ window that holds a collection of tabs, one of
// which is active and rendered inside of the window.
class ChromeWindow : public Gtk::Window {
 public:
  ChromeWindow(MockChrome* chrome, int width, int height);

  MockChrome* chrome() { return chrome_; }
  XWindow xid() { return xid_; }
  int width() const { return width_; }
  int height() const { return height_; }
  size_t num_tabs() const { return tabs_.size(); }
  Tab* tab(size_t index) { return tabs_[index]->tab.get(); }
  int active_tab_index() const { return active_tab_index_; }
  TabSummary* tab_summary() { return tab_summary_.get(); }

  // Insert a tab into this window.  The window takes ownership of the tab.
  // 'index' values greater than the current number of tabs will result in
  // the tab being appended at the end.
  // TODO: Clean up which methods do redraws and which don't.
  void InsertTab(Tab* tab, size_t index);

  // Remove a tab from the window.  Ownership of the tab is transferred to
  // the caller.
  Tab* RemoveTab(size_t index);

  void ActivateTab(int index);

 private:
  struct TabInfo {
    TabInfo(Tab* tab)
        : tab(tab),
          start_x(0),
          width(0) {
    }

    scoped_ptr<Tab> tab;
    int start_x;
    int width;

    DISALLOW_COPY_AND_ASSIGN(TabInfo);
  };

  // Initialize static image data.
  static void InitImages();

  // Draw the tab strip.  Also updates tab position info inside of 'tabs_'.
  void DrawTabs();

  // Draw the navigation bar underneath the tab strip.
  void DrawNavBar();

  // Draw the page contents.  If 'active_tab_index_' >= 0, this will be the
  // image from the currently-selected tab; otherwise it will just be a
  // gray box.
  void DrawView();

  // Get the number of the tab at the given position, relative to the
  // left side of the window.  The portion of the tab bar to the right of
  // any tabs is given position equal to the number of tabs.  -1 is
  // returned for positions outside of the tab bar.
  int GetTabIndexAtXPosition(int x) const;

  bool on_button_press_event(GdkEventButton* event);
  bool on_button_release_event(GdkEventButton* event);
  bool on_motion_notify_event(GdkEventMotion* event);
  bool on_key_press_event(GdkEventKey* event);
  bool on_expose_event(GdkEventExpose* event);
  bool on_client_event(GdkEventClient* event);
  bool on_configure_event(GdkEventConfigure* event);
  bool on_window_state_event(GdkEventWindowState* event);

  MockChrome* chrome_;  // not owned

  XWindow xid_;

  int width_;
  int height_;

  std::vector<std::tr1::shared_ptr<TabInfo> > tabs_;

  scoped_ptr<TabSummary> tab_summary_;
  scoped_ptr<FloatingTab> floating_tab_;

  int active_tab_index_;

  // Is a tab currently being dragged?
  bool dragging_tab_;

  // Cursor's offset from the upper-left corner of the tab at the start of
  // the drag.
  int tab_drag_start_offset_x_;
  int tab_drag_start_offset_y_;

  // Is the window currently in fullscreen mode?
  bool fullscreen_;

  // TODO: Rename these to e.g. kImageNavBg?
  static Glib::RefPtr<Gdk::Pixbuf> image_nav_bg_;
  static Glib::RefPtr<Gdk::Pixbuf> image_nav_left_;
  static Glib::RefPtr<Gdk::Pixbuf> image_nav_right_;
  static Glib::RefPtr<Gdk::Pixbuf> image_tab_bg_;
  static Glib::RefPtr<Gdk::Pixbuf> image_tab_hl_;
  static Glib::RefPtr<Gdk::Pixbuf> image_tab_nohl_;
  static Glib::RefPtr<Gdk::Pixbuf> image_tab_right_hl_left_nohl_;
  static Glib::RefPtr<Gdk::Pixbuf> image_tab_right_hl_left_none_;
  static Glib::RefPtr<Gdk::Pixbuf> image_tab_right_nohl_left_hl_;
  static Glib::RefPtr<Gdk::Pixbuf> image_tab_right_nohl_left_nohl_;
  static Glib::RefPtr<Gdk::Pixbuf> image_tab_right_nohl_left_none_;
  static Glib::RefPtr<Gdk::Pixbuf> image_tab_right_none_left_hl_;
  static Glib::RefPtr<Gdk::Pixbuf> image_tab_right_none_left_nohl_;

  // Height of the tab and navigation bars.
  static int kTabHeight;
  static int kNavHeight;

  // Distance above and below the tab bar that a tab can be dragged before
  // we detach it.
  static const int kTabDragThreshold;

  static const char* kTabFontFace;
  static const double kTabFontSize;
  static const int kTabFontPadding;

  DISALLOW_COPY_AND_ASSIGN(ChromeWindow);
};

class PanelTitlebar : public Gtk::Window {
 public:
  explicit PanelTitlebar(Panel* panel);

  XWindow xid() const { return xid_; }
  void set_focused(bool focused) { focused_ = focused; }

  void Draw();

 private:
  static const int kWidth;
  static const int kHeight;
  static Glib::RefPtr<Gdk::Pixbuf> image_bg_;
  static Glib::RefPtr<Gdk::Pixbuf> image_bg_focused_;
  static const char* kFontFace;
  static const double kFontSize;
  static const double kFontPadding;
  static const int kDragThreshold;

  bool on_expose_event(GdkEventExpose* event);
  bool on_button_press_event(GdkEventButton* event);
  bool on_button_release_event(GdkEventButton* event);
  bool on_motion_notify_event(GdkEventMotion* event);

  Panel* panel_;  // not owned
  XWindow xid_;

  // Is the mouse button currently down?
  bool mouse_down_;

  // Cursor's absolute position when the mouse button was pressed.
  int mouse_down_abs_x_;
  int mouse_down_abs_y_;

  // Cursor's offset from the upper-left corner of the titlebar when the
  // mouse button was pressed.
  int mouse_down_offset_x_;
  int mouse_down_offset_y_;

  // Is the titlebar currently being dragged?  That is, has the cursor
  // moved more than kDragThreshold away from its starting position?
  bool dragging_;

  // Is this panel focused?  We draw ourselves differently if it is.
  bool focused_;

  DISALLOW_COPY_AND_ASSIGN(PanelTitlebar);
};

class Panel : public Gtk::Window {
 public:
  Panel(MockChrome* chrome,
        const std::string& image_filename,
        const std::string& title);

  XWindow xid() const { return xid_; }
  MockChrome* chrome() { return chrome_; }
  bool expanded() const { return expanded_; }
  const std::string& title() const { return title_; }

 private:
  bool on_expose_event(GdkEventExpose* event);
  bool on_button_press_event(GdkEventButton* event);
  bool on_key_press_event(GdkEventKey* event);
  bool on_client_event(GdkEventClient* event);
  bool on_focus_in_event(GdkEventFocus* event);
  bool on_focus_out_event(GdkEventFocus* event);

  MockChrome* chrome_;  // not owned
  XWindow xid_;
  scoped_ptr<PanelTitlebar> titlebar_;
  Glib::RefPtr<Gdk::Pixbuf> image_;
  int width_;
  int height_;
  bool expanded_;
  std::string title_;

  DISALLOW_COPY_AND_ASSIGN(Panel);
};

class MockChrome {
 public:
  MockChrome();

  chromeos::WmIpc* wm_ipc() { return wm_ipc_.get(); }

  // Create a new window, ownership of which remains with the MockChrome
  // object.
  ChromeWindow* CreateWindow(int width, int height);

  // Close a window.
  void CloseWindow(ChromeWindow* win);

  // Create a new panel, ownership of which remains with the MockChrome
  // object.
  Panel* CreatePanel(const std::string& image_filename,
                     const std::string &title);

  // Close a panel.
  void ClosePanel(Panel* panel);

  // Handle a notification about a floating tab getting moved into or out
  // of a window.  We track this so we'll know which window the tab is in
  // when it gets dropped.
  void NotifyAboutFloatingTab(
      XWindow tab_xid, ChromeWindow* win, bool entered);

  // Deal with a dropped floating tab.  Ownership of 'tab' is passed to
  // this method.
  void HandleDroppedFloatingTab(Tab* tab);

 private:
  scoped_ptr<chromeos::XConnection> xconn_;
  scoped_ptr<chromeos::AtomCache> atom_cache_;
  scoped_ptr<chromeos::WmIpc> wm_ipc_;

  typedef std::map<XWindow, std::tr1::shared_ptr<ChromeWindow> > ChromeWindows;
  ChromeWindows windows_;

  // Map from the panel window's XID to the corresponding Panel object.
  typedef std::map<XWindow, std::tr1::shared_ptr<Panel> > Panels;
  Panels panels_;

  // The window currently under the floating tab.
  ChromeWindow* window_under_floating_tab_;

  DISALLOW_COPY_AND_ASSIGN(MockChrome);
};

}  // namespace mock_chrome

#endif
