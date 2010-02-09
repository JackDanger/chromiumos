// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/mock_chrome.h"

#include <algorithm>

#include <cairomm/context.h>

#include "base/command_line.h"
#include "base/string_util.h"
#include "chromeos/obsolete_logging.h"
#include "chromeos/string.h"
#include "window_manager/atom_cache.h"
#include "window_manager/real_x_connection.h"
#include "window_manager/util.h"
#include "window_manager/wm_ipc.h"

DEFINE_string(image_dir, "data/", "Path to static image files");
DEFINE_string(new_panel_image, "data/panel_chat.png",
              "Image to use when creating a new panel");
DEFINE_int32(num_panels, 3, "Number of panels to open");
DEFINE_int32(num_windows, 3, "Number of windows to open");
DEFINE_string(panel_images, "data/panel_chat.png",
              "Comma-separated images to use for panels");
DEFINE_string(panel_titles, "Chat",
              "Comma-separated titles to use for panels");
DEFINE_string(tab_images,
              "data/chrome_page_google.png,"
              "data/chrome_page_gmail.png,"
              "data/chrome_page_chrome.png",
              "Comma-separated images to use for tabs");
DEFINE_string(tab_titles, "Google,Gmail,Google Chrome",
              "Comma-separated titles to use for tabs");
DEFINE_int32(tabs_per_window, 3, "Number of tabs to add to each window");
DEFINE_int32(window_height, 640, "Window height");
DEFINE_int32(window_width, 920, "Window width");

using chromeos::SplitStringUsing;
using std::max;
using window_manager::AtomCache;
using window_manager::GetCurrentTime;
using window_manager::RealXConnection;
using window_manager::WmIpc;

namespace mock_chrome {

const int TabSummary::kTabImageWidth = 160;
const int TabSummary::kTabImageHeight = 120;
const int TabSummary::kPadding = 20;
const int TabSummary::kInsertCursorWidth = 2;

const int FloatingTab::kWidth = 240;
const int FloatingTab::kHeight = 180;

const int PanelTitlebar::kWidth = 200;
const int PanelTitlebar::kHeight = 26;
Glib::RefPtr<Gdk::Pixbuf> PanelTitlebar::image_bg_;
Glib::RefPtr<Gdk::Pixbuf> PanelTitlebar::image_bg_focused_;
const char* PanelTitlebar::kFontFace = "Arial";
const double PanelTitlebar::kFontSize = 13;
const double PanelTitlebar::kFontPadding = 6;
const int PanelTitlebar::kDragThreshold = 10;

const int ChromeWindow::kTabDragThreshold = 10;
const char* ChromeWindow::kTabFontFace = "DejaVu Sans";
const double ChromeWindow::kTabFontSize = 13;
const int ChromeWindow::kTabFontPadding = 5;

// Static images.
Glib::RefPtr<Gdk::Pixbuf> ChromeWindow::image_nav_bg_;
Glib::RefPtr<Gdk::Pixbuf> ChromeWindow::image_nav_left_;
Glib::RefPtr<Gdk::Pixbuf> ChromeWindow::image_nav_right_;
Glib::RefPtr<Gdk::Pixbuf> ChromeWindow::image_tab_bg_;
Glib::RefPtr<Gdk::Pixbuf> ChromeWindow::image_tab_hl_;
Glib::RefPtr<Gdk::Pixbuf> ChromeWindow::image_tab_nohl_;
Glib::RefPtr<Gdk::Pixbuf> ChromeWindow::image_tab_right_hl_left_nohl_;
Glib::RefPtr<Gdk::Pixbuf> ChromeWindow::image_tab_right_hl_left_none_;
Glib::RefPtr<Gdk::Pixbuf> ChromeWindow::image_tab_right_nohl_left_hl_;
Glib::RefPtr<Gdk::Pixbuf> ChromeWindow::image_tab_right_nohl_left_nohl_;
Glib::RefPtr<Gdk::Pixbuf> ChromeWindow::image_tab_right_nohl_left_none_;
Glib::RefPtr<Gdk::Pixbuf> ChromeWindow::image_tab_right_none_left_hl_;
Glib::RefPtr<Gdk::Pixbuf> ChromeWindow::image_tab_right_none_left_nohl_;

int ChromeWindow::kTabHeight = 0;
int ChromeWindow::kNavHeight = 0;

static void DrawImage(Glib::RefPtr<Gdk::Pixbuf>& image,
                      Gtk::Widget* widget,
                      int dest_x, int dest_y,
                      int dest_width, int dest_height) {
  CHECK(widget);
  CHECK_GT(dest_width, 0);
  CHECK_GT(dest_height, 0);

  // Only scale the original image if we have to.
  Glib::RefPtr<Gdk::Pixbuf> scaled_image = image;
  if (dest_width != image->get_width() || dest_height != image->get_height()) {
    scaled_image = image->scale_simple(
        dest_width, dest_height, Gdk::INTERP_BILINEAR);
  }
  scaled_image->render_to_drawable(
      widget->get_window(),
      widget->get_style()->get_black_gc(),
      0, 0,             // src position
      dest_x, dest_y,   // dest position
      dest_width, dest_height,
      Gdk::RGB_DITHER_NONE,
      0, 0);  // x and y dither
}

Tab::Tab(const std::string& image_filename, const std::string& title)
    : image_(Gdk::Pixbuf::create_from_file(image_filename)),
      title_(title) {
}

void Tab::RenderToGtkWidget(Gtk::Widget* widget,
                            int x, int y,
                            int width, int height) {
  DrawImage(image_, widget, x, y, width, height);
}

TabSummary::TabSummary(ChromeWindow* parent_win)
    : parent_win_(parent_win),
      width_(1),
      height_(1),
      insert_index_(-1) {
  Resize();

  // Calling realize() creates the underlying X window; we need to do this
  // early on instead of relying on show_all() to do it for us, so that we
  // can set the window's type property before it gets mapped so the WM
  // knows how to handle it.
  realize();
  xid_ = GDK_WINDOW_XWINDOW(Glib::unwrap(get_window()));
  CHECK(parent_win_->chrome()->wm_ipc()->SetWindowType(
            xid(), WmIpc::WINDOW_TYPE_CHROME_TAB_SUMMARY, NULL));

  add_events(Gdk::BUTTON_PRESS_MASK);
  show_all();

}

void TabSummary::Resize() {
  width_ = parent_win_->num_tabs() * kTabImageWidth +
           (parent_win_->num_tabs() + 1) * kPadding;
  if (insert_index_ >= 0) {
    width_ += kInsertCursorWidth + kPadding;
  }
  height_ = kTabImageHeight + 2 * kPadding;
  set_size_request(width_, height_);
}

void TabSummary::Draw() {
  get_window()->clear();

  Cairo::RefPtr<Cairo::Context> cr =
      get_window()->create_cairo_context();
  cr->set_line_width(1.0);

  int x = kPadding;
  for (size_t i = 0; i < parent_win_->num_tabs(); ++i) {
    if (static_cast<int>(i) == insert_index_) {
      DrawInsertCursor(x, kPadding);
      x += kInsertCursorWidth + kPadding;
    }

    parent_win_->tab(i)->RenderToGtkWidget(
        this, x, kPadding, kTabImageWidth, kTabImageHeight);

    double alpha =
        (static_cast<int>(i) == parent_win_->active_tab_index()) ?  0.75 : 0.25;
    cr->set_source_rgba(0, 0, 0, alpha);
    // Cairo places coordinates at the edges of pixels.  So that we don't
    // end up with ugly two-pixel-wide antialiased lines, we need to
    // specify our positions in the center of pixels.
    cr->rectangle(x + 0.5, kPadding + 0.5, kTabImageWidth, kTabImageHeight);
    cr->stroke();
    x += kTabImageWidth + kPadding;
  }

  if (insert_index_ == static_cast<int>(parent_win_->num_tabs())) {
    DrawInsertCursor(x, kPadding);
  }
}

void TabSummary::DrawInsertCursor(int x, int y) {
  Cairo::RefPtr<Cairo::Context> cr = get_window()->create_cairo_context();
  cr->set_line_width(kInsertCursorWidth);
  cr->set_source_rgba(0, 0, 0, 1.0);
  cr->move_to(x + 1, y);
  cr->line_to(x + 1, y + kTabImageHeight);
  cr->stroke();
}

void TabSummary::HandleFloatingTabMovement(int x, int y) {
  int old_index = insert_index_;
  // TODO: This isn't really correct; we need to take the insert cursor
  // into account too.
  insert_index_ = static_cast<double>(x) / (width_ - kTabImageWidth) *
      parent_win_->num_tabs();
  if (insert_index_ != old_index) {
    if (old_index < 0) Resize();
    Draw();
  }
}

bool TabSummary::on_button_press_event(GdkEventButton* event) {
  if (event->button != 1) {
    return false;
  }
  int index = static_cast<double>(event->x) / width_ * parent_win_->num_tabs();
  if (index < static_cast<int>(parent_win_->num_tabs())) {
    parent_win_->ActivateTab(index);
  }
  Draw();
  WmIpc::Message msg(WmIpc::Message::WM_FOCUS_WINDOW);
  msg.set_param(0, parent_win_->xid());
  CHECK(parent_win_->chrome()->wm_ipc()->SendMessage(
            parent_win_->chrome()->wm_ipc()->wm_window(), msg));
  return true;
}

bool TabSummary::on_expose_event(GdkEventExpose* event) {
  Draw();
  return true;
}

bool TabSummary::on_client_event(GdkEventClient* event) {
  WmIpc::Message msg;
  if (!parent_win_->chrome()->wm_ipc()->GetMessageGdk(*event, &msg)) {
    return false;
  }

  VLOG(2) << "Got message of type " << msg.type();
  switch (msg.type()) {
    case WmIpc::Message::CHROME_NOTIFY_FLOATING_TAB_OVER_TAB_SUMMARY: {
      HandleFloatingTabMovement(msg.param(2), msg.param(3));
      break;
    }
    default:
      LOG(WARNING) << "Ignoring WM message of unknown type " << msg.type();
      return false;
  }
  return true;
}

FloatingTab::FloatingTab(MockChrome* chrome, Tab* tab,
                         int initial_x, int initial_y,
                         int drag_start_offset_x, int drag_start_offset_y)
    : chrome_(chrome),
      tab_(tab) {
  set_size_request(kWidth, kHeight);

  // TODO: We should really be calling realize() and setting the window
  // type property before calling show_all() to avoid a race condition, but
  // doing so seems to lead to another race condition -- a good portion of
  // the time, the window ends up being blank instead of containing the tab
  // image.
  show_all();
  xid_ = GDK_WINDOW_XWINDOW(Glib::unwrap(get_window()));
  std::vector<int> type_params;
  type_params.push_back(initial_x);
  type_params.push_back(initial_y);
  type_params.push_back(drag_start_offset_x);
  type_params.push_back(drag_start_offset_y);
  CHECK(chrome_->wm_ipc()->SetWindowType(
            xid_, WmIpc::WINDOW_TYPE_CHROME_FLOATING_TAB, &type_params));

}

void FloatingTab::Move(int x, int y) {
  VLOG(2) << "Asking WM to move floating tab " << xid_ << " to ("
          << x << ", " << y << ")";
  WmIpc::Message msg(WmIpc::Message::WM_MOVE_FLOATING_TAB);
  msg.set_param(0, xid_);
  msg.set_param(1, x);
  msg.set_param(2, y);
  CHECK(chrome_->wm_ipc()->SendMessage(chrome_->wm_ipc()->wm_window(), msg));
}

bool FloatingTab::on_expose_event(GdkEventExpose* event) {
  tab_->RenderToGtkWidget(this, 0, 0, kWidth, kHeight);
  return true;
}

ChromeWindow::ChromeWindow(MockChrome* chrome, int width, int height)
    : chrome_(chrome),
      width_(width),
      height_(height),
      active_tab_index_(-1),
      dragging_tab_(false),
      tab_drag_start_offset_x_(0),
      tab_drag_start_offset_y_(0),
      fullscreen_(false) {
  if (!image_nav_bg_) {
    InitImages();
  }

  set_size_request(width_, height_);

  realize();
  xid_ = GDK_WINDOW_XWINDOW(Glib::unwrap(get_window()));
  CHECK(chrome_->wm_ipc()->SetWindowType(
            xid(), WmIpc::WINDOW_TYPE_CHROME_TOPLEVEL, NULL));
  add_events(Gdk::BUTTON_PRESS_MASK |
             Gdk::BUTTON_RELEASE_MASK |
             Gdk::POINTER_MOTION_MASK);

  show_all();
}

void ChromeWindow::InsertTab(Tab* tab, size_t index) {
  std::tr1::shared_ptr<TabInfo> info(new TabInfo(tab));
  if (index > tabs_.size()) {
    index = tabs_.size();
  }

  tabs_.insert(tabs_.begin() + index, info);
  if (static_cast<int>(index) <= active_tab_index_) {
    active_tab_index_++;
  }
  if (active_tab_index_ < 0) {
    active_tab_index_ = 0;
    DrawView();
  }
  DrawTabs();
  if (tab_summary_.get()) {
    tab_summary_.reset(new TabSummary(this));
  }
}

Tab* ChromeWindow::RemoveTab(size_t index) {
  CHECK_LT(index, tabs_.size());
  std::tr1::shared_ptr<TabInfo> info = tabs_[index];
  tabs_.erase(tabs_.begin() + index);
  if (active_tab_index_ >= static_cast<int>(tabs_.size())) {
    active_tab_index_ = static_cast<int>(tabs_.size()) - 1;
  }
  return info->tab.release();
}

void ChromeWindow::ActivateTab(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, static_cast<int>(tabs_.size()));
  if (index == active_tab_index_) {
    return;
  }
  active_tab_index_ = index;
  DrawTabs();
  DrawView();
}

/* static */ void ChromeWindow::InitImages() {
  CHECK(!image_nav_bg_);

  image_nav_bg_ = Gdk::Pixbuf::create_from_file(
      FLAGS_image_dir + "chrome_nav_bg.png");
  image_nav_left_ = Gdk::Pixbuf::create_from_file(
      FLAGS_image_dir + "chrome_nav_left.png");
  image_nav_right_ = Gdk::Pixbuf::create_from_file(
      FLAGS_image_dir + "chrome_nav_right.png");
  image_tab_bg_ = Gdk::Pixbuf::create_from_file(
      FLAGS_image_dir + "chrome_tab_bg.png");
  image_tab_hl_ = Gdk::Pixbuf::create_from_file(
      FLAGS_image_dir + "chrome_tab_hl.png");
  image_tab_nohl_ = Gdk::Pixbuf::create_from_file(
      FLAGS_image_dir + "chrome_tab_nohl.png");
  image_tab_right_hl_left_nohl_ = Gdk::Pixbuf::create_from_file(
      FLAGS_image_dir + "chrome_tab_right_hl_left_nohl.png");
  image_tab_right_hl_left_none_ = Gdk::Pixbuf::create_from_file(
      FLAGS_image_dir + "chrome_tab_right_hl_left_none.png");
  image_tab_right_nohl_left_hl_ = Gdk::Pixbuf::create_from_file(
      FLAGS_image_dir + "chrome_tab_right_nohl_left_hl.png");
  image_tab_right_nohl_left_nohl_ = Gdk::Pixbuf::create_from_file(
      FLAGS_image_dir + "chrome_tab_right_nohl_left_nohl.png");
  image_tab_right_nohl_left_none_ = Gdk::Pixbuf::create_from_file(
      FLAGS_image_dir + "chrome_tab_right_nohl_left_none.png");
  image_tab_right_none_left_hl_ = Gdk::Pixbuf::create_from_file(
      FLAGS_image_dir + "chrome_tab_right_none_left_hl.png");
  image_tab_right_none_left_nohl_ = Gdk::Pixbuf::create_from_file(
      FLAGS_image_dir + "chrome_tab_right_none_left_nohl.png");

  kTabHeight = image_tab_hl_->get_height();
  kNavHeight = image_nav_left_->get_height();
}

void ChromeWindow::DrawTabs() {
  Cairo::RefPtr<Cairo::Context> cr =
      get_window()->create_cairo_context();
  cr->select_font_face(kTabFontFace,
                       Cairo::FONT_SLANT_NORMAL,
                       Cairo::FONT_WEIGHT_NORMAL);
  cr->set_font_size(kTabFontSize);

  Cairo::FontOptions font_options;
  font_options.set_hint_style(Cairo::HINT_STYLE_MEDIUM);
  font_options.set_hint_metrics(Cairo::HINT_METRICS_ON);
  font_options.set_antialias(Cairo::ANTIALIAS_GRAY);
  cr->set_font_options(font_options);

  Cairo::FontExtents extents;
  cr->get_font_extents(extents);

  cr->set_source_rgb(0, 0, 0);

  int x_offset = 0;
  for (int i = 0; static_cast<size_t>(i) < tabs_.size(); ++i) {
    bool active = (i == active_tab_index_);
    tabs_[i]->start_x = x_offset;

    // Draw the image on the left.
    if (i == 0) {
      Glib::RefPtr<Gdk::Pixbuf> left_image =
          active ?
          image_tab_right_none_left_hl_ :
          image_tab_right_none_left_nohl_;
      DrawImage(left_image,
                this,
                x_offset, 0,
                left_image->get_width(), left_image->get_height());
      x_offset += left_image->get_width();
    }

    // Draw the tab's background and its title.
    Glib::RefPtr<Gdk::Pixbuf> image = active ? image_tab_hl_ : image_tab_nohl_;
    DrawImage(image,
              this,
              x_offset, 0,
              image->get_width(), image->get_height());
    cr->move_to(x_offset + kTabFontPadding, extents.ascent + kTabFontPadding);
    cr->show_text(tabs_[i]->tab->title());
    x_offset += image->get_width();

    // Draw the image on the right.
    Glib::RefPtr<Gdk::Pixbuf> right_image;
    if (static_cast<size_t>(i) == tabs_.size() - 1) {
      // Last tab.
      if (active) {
        right_image = image_tab_right_hl_left_none_;
      } else {
        right_image = image_tab_right_nohl_left_none_;
      }
    } else if (active) {
      // Active tab.
      right_image = image_tab_right_hl_left_nohl_;
    } else if (i + 1 == active_tab_index_) {
      // Next tab is active.
      right_image = image_tab_right_nohl_left_hl_;
    } else {
      // Neither tab is active.
      right_image = image_tab_right_nohl_left_nohl_;
    }
    DrawImage(right_image,
              this,
              x_offset, 0,  // x, y
              right_image->get_width(), right_image->get_height());
    x_offset += right_image->get_width();

    tabs_[i]->width = x_offset - tabs_[i]->start_x;
  }

  if (x_offset < width_) {
    DrawImage(image_tab_bg_, this,
              x_offset, 0,  // x, y
              width_ - x_offset, image_tab_bg_->get_height());
  }
}

void ChromeWindow::DrawNavBar() {
  DrawImage(image_nav_bg_, this,
            0, kTabHeight,  // x, y
            width_, image_nav_bg_->get_height());

  DrawImage(image_nav_left_, this,
            0, kTabHeight,  // x, y
            image_nav_left_->get_width(), image_nav_left_->get_height());
  DrawImage(image_nav_right_, this,
            width_ - image_nav_right_->get_width(), kTabHeight,  // x, y
            image_nav_right_->get_width(), image_nav_right_->get_height());
}

void ChromeWindow::DrawView() {
  int x = 0;
  int y = kTabHeight + kNavHeight;
  int width = width_;
  int height = height_ - y;

  if (active_tab_index_ >= 0) {
    CHECK_LT(active_tab_index_, static_cast<int>(tabs_.size()));
    tabs_[active_tab_index_]->tab->RenderToGtkWidget(this, x, y, width, height);
  } else {
    get_window()->clear_area(x, y, width, height);
  }
}

int ChromeWindow::GetTabIndexAtXPosition(int x) const {
  if (x < 0) {
    return -1;
  }

  for (int i = 0; static_cast<size_t>(i) < tabs_.size(); ++i) {
    if (x >= tabs_[i]->start_x && x < tabs_[i]->start_x + tabs_[i]->width) {
      return i;
    }
  }

  return (x < get_width() ? tabs_.size() : -1);
}

bool ChromeWindow::on_button_press_event(GdkEventButton* event) {
  if (event->button == 2) {
    chrome_->CloseWindow(this);
    return true;
  } else if (event->button != 1) {
    return false;
  }

  VLOG(2) << "Got mouse down at (" << event->x << ", " << event->y << ")";
  if (event->y < 0 || event->y > kTabHeight) {
    // Don't do anything for clicks outside of the tab bar.
    return false;
  }

  int tab_index = GetTabIndexAtXPosition(event->x);
  if (tab_index < 0 || tab_index >= static_cast<int>(tabs_.size())) {
    // Ignore clicks outside of tabs.
    return false;
  }

  dragging_tab_ = true;
  tab_drag_start_offset_x_ = event->x - tabs_[tab_index]->start_x;
  tab_drag_start_offset_y_ = event->y;
  if (tab_index != active_tab_index_) {
    CHECK_LT(tab_index, static_cast<int>(tabs_.size()));
    active_tab_index_ = tab_index;
    DrawTabs();
    DrawView();
  }
  return true;
}

bool ChromeWindow::on_button_release_event(GdkEventButton* event) {
  if (event->button != 1) {
    return false;
  }
  VLOG(2) << "Got mouse up at (" << event->x << ", " << event->y << ")";
  if (floating_tab_.get()) {
    // Why do we have a floating tab if we weren't dragging a tab?
    CHECK(dragging_tab_);
    chrome_->HandleDroppedFloatingTab(floating_tab_->ReleaseTab());
    floating_tab_.reset();
  }
  dragging_tab_ = false;
  return true;
}

bool ChromeWindow::on_motion_notify_event(GdkEventMotion* event) {
  if (!dragging_tab_) return false;

  VLOG(2) << "Got motion at (" << event->x << ", " << event->y << ")";
  if (floating_tab_.get()) {
    // TODO: We should send these events up to the MockChrome object.  If
    // the user detaches a tab and switches windows in focused mode, they
    // should be able to insert the tab into the new window; currently they
    // can only insert it into the one that it was originally detached
    // from.
    int tab_index = GetTabIndexAtXPosition(event->x);
    if (tab_index >= 0 && event->y >= 0 && event->y < kTabHeight) {
      // If the floating tab has moved back into the tab bar, re-add it to
      // the window and make it active.
      InsertTab(floating_tab_->ReleaseTab(), tab_index);
      floating_tab_.reset();
      active_tab_index_ = tab_index;
      DrawTabs();
      DrawView();
    } else {
      // Otherwise, just tell the window manager to move the floating tab.
      floating_tab_->Move(event->x_root, event->y_root);
    }
  } else if (active_tab_index_ >= 0) {
    int tab_index = GetTabIndexAtXPosition(event->x);
    if (tab_index < 0 ||
        event->y < -kTabDragThreshold ||
        event->y >= kTabHeight + kTabDragThreshold) {
      // The tab has been moved out of the tab bar (including the threshold
      // around it); detach it.
      Tab* tab = RemoveTab(active_tab_index_);
      floating_tab_.reset(
          new FloatingTab(chrome_, tab,
                          event->x_root, event->y_root,
                          tab_drag_start_offset_x_, tab_drag_start_offset_y_));
      DrawTabs();
      DrawView();
    } else {
      // The tab is still within the tab bar; move it to a new position.
      if (tab_index >= static_cast<int>(tabs_.size())) {
        // GetTabIndexAtXPosition() returns tabs_.size() for positions in
        // the empty space at the right of the tab bar, but we need to
        // treat that space as belonging to the last tab when reordering.
        tab_index = tabs_.size() - 1;
      }
      if (tab_index != active_tab_index_) {
        Tab* tab = RemoveTab(active_tab_index_);
        InsertTab(tab, tab_index);
        active_tab_index_ = tab_index;
        DrawTabs();
      }
    }
  }
  return true;
}

bool ChromeWindow::on_key_press_event(GdkEventKey* event) {
  if (strcmp(event->string, "p") == 0) {
    chrome_->CreatePanel(FLAGS_new_panel_image, "New Panel", true);
  } else if (strcmp(event->string, "w") == 0) {
    chrome_->CreateWindow(width_, height_);
  } else if (strcmp(event->string, "f") == 0) {
    if (fullscreen_) {
      unfullscreen();
    } else {
      fullscreen();
    }
  }
  return true;
}

bool ChromeWindow::on_expose_event(GdkEventExpose* event) {
  DrawTabs();
  DrawNavBar();
  DrawView();
  return true;
}

bool ChromeWindow::on_client_event(GdkEventClient* event) {
  WmIpc::Message msg;
  if (!chrome_->wm_ipc()->GetMessageGdk(*event, &msg)) {
    return false;
  }

  VLOG(2) << "Got message of type " << msg.type();
  switch (msg.type()) {
    case WmIpc::Message::CHROME_SET_TAB_SUMMARY_VISIBILITY: {
      if (msg.param(0)) {
        if (!tab_summary_.get()) {
          tab_summary_.reset(new TabSummary(this));
        }
      } else {
        tab_summary_.reset(NULL);
      }
      break;
    }
    case WmIpc::Message::CHROME_NOTIFY_FLOATING_TAB_OVER_TOPLEVEL: {
      // Inform the MockChrome object that the tab has entered or exited a
      // window.
      chrome_->NotifyAboutFloatingTab(msg.param(0), this, msg.param(1));
      break;
    }
    default:
      LOG(WARNING) << "Ignoring WM message of unknown type " << msg.type();
      return false;
  }
  return true;
}

bool ChromeWindow::on_configure_event(GdkEventConfigure* event) {
  width_ = event->width;
  height_ = event->height;
  DrawView();
  return true;
}

bool ChromeWindow::on_window_state_event(GdkEventWindowState* event) {
  fullscreen_ = (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN);
  VLOG(1) << "Fullscreen mode set to " << fullscreen_;
  return true;
}

PanelTitlebar::PanelTitlebar(Panel* panel)
    : panel_(panel),
      mouse_down_(false),
      mouse_down_abs_x_(0),
      mouse_down_abs_y_(0),
      mouse_down_offset_x_(0),
      mouse_down_offset_y_(0),
      dragging_(false),
      focused_(false) {
  if (!image_bg_) {
    image_bg_ = Gdk::Pixbuf::create_from_file(
        FLAGS_image_dir + "panel_titlebar_bg.png");
    image_bg_focused_ = Gdk::Pixbuf::create_from_file(
        FLAGS_image_dir + "panel_titlebar_bg_focused.png");
  }
  set_size_request(kWidth, kHeight);
  realize();
  xid_ = GDK_WINDOW_XWINDOW(Glib::unwrap(get_window()));
  CHECK(panel_->chrome()->wm_ipc()->SetWindowType(
            xid_, WmIpc::WINDOW_TYPE_CHROME_PANEL_TITLEBAR, NULL));
  add_events(Gdk::BUTTON_PRESS_MASK |
             Gdk::BUTTON_RELEASE_MASK |
             Gdk::POINTER_MOTION_MASK);
  show_all();
}

void PanelTitlebar::Draw() {
  DrawImage((focused_ ? image_bg_focused_ : image_bg_),
            this, 0, 0, get_width(), get_height());

  Cairo::RefPtr<Cairo::Context> cr =
      get_window()->create_cairo_context();
  cr->select_font_face(kFontFace,
                       Cairo::FONT_SLANT_NORMAL,
                       Cairo::FONT_WEIGHT_BOLD);
  cr->set_font_size(kFontSize);

  Cairo::FontOptions font_options;
  font_options.set_hint_style(Cairo::HINT_STYLE_MEDIUM);
  font_options.set_hint_metrics(Cairo::HINT_METRICS_ON);
  font_options.set_antialias(Cairo::ANTIALIAS_GRAY);
  cr->set_font_options(font_options);

  Cairo::FontExtents extents;
  cr->get_font_extents(extents);
  int x = kFontPadding;
  int y = kFontPadding + extents.ascent;

  cr->set_source_rgb(1, 1, 1);
  cr->move_to(x, y);
  cr->show_text(panel_->title());
}

bool PanelTitlebar::on_expose_event(GdkEventExpose* event) {
  Draw();
  return true;
}

bool PanelTitlebar::on_button_press_event(GdkEventButton* event) {
  if (event->button == 2) {
    panel_->chrome()->ClosePanel(panel_);
    return true;
  } else if (event->button != 1) {
    return false;
  }
  mouse_down_ = true;
  mouse_down_abs_x_ = event->x_root;
  mouse_down_abs_y_ = event->y_root;

  int width = 1, height = 1;
  get_size(width, height);
  mouse_down_offset_x_ = event->x - width;
  mouse_down_offset_y_ = event->y;
  dragging_ = false;
  return true;
}

bool PanelTitlebar::on_button_release_event(GdkEventButton* event) {
  if (event->button != 1) {
    return false;
  }
  // Only handle clicks that started in our window.
  if (!mouse_down_) {
    return false;
  }

  mouse_down_ = false;
  if (!dragging_) {
    WmIpc::Message msg(WmIpc::Message::WM_SET_PANEL_STATE);
    msg.set_param(0, panel_->xid());
    msg.set_param(1, !(panel_->expanded()));
    CHECK(panel_->chrome()->wm_ipc()->SendMessage(
              panel_->chrome()->wm_ipc()->wm_window(), msg));

    // If the panel is getting expanded, tell the WM to focus it.
    if (!panel_->expanded()) {
      WmIpc::Message focus_msg(WmIpc::Message::WM_FOCUS_WINDOW);
      focus_msg.set_param(0, panel_->xid());
      CHECK(panel_->chrome()->wm_ipc()->SendMessage(
                panel_->chrome()->wm_ipc()->wm_window(), focus_msg));
    }
  } else {
    WmIpc::Message msg(WmIpc::Message::WM_NOTIFY_PANEL_DRAG_COMPLETE);
    msg.set_param(0, panel_->xid());
    CHECK(panel_->chrome()->wm_ipc()->SendMessage(
              panel_->chrome()->wm_ipc()->wm_window(), msg));
    dragging_ = false;
  }
  return true;
}

bool PanelTitlebar::on_motion_notify_event(GdkEventMotion* event) {
  if (!mouse_down_) {
    return false;
  }

  if (!dragging_) {
    if (abs(event->x_root - mouse_down_abs_x_) >= kDragThreshold ||
        abs(event->y_root - mouse_down_abs_y_) >= kDragThreshold) {
      dragging_ = true;
    }
  }
  if (dragging_) {
    WmIpc::Message msg(WmIpc::Message::WM_NOTIFY_PANEL_DRAGGED);
    msg.set_param(0, panel_->xid());
    msg.set_param(1, event->x_root - mouse_down_offset_x_);
    msg.set_param(2, event->y_root - mouse_down_offset_y_);
    CHECK(panel_->chrome()->wm_ipc()->SendMessage(
              panel_->chrome()->wm_ipc()->wm_window(), msg));
  }
  return true;
}

Panel::Panel(MockChrome* chrome,
             const std::string& image_filename,
             const std::string& title,
             bool expanded)
    : chrome_(chrome),
      titlebar_(new PanelTitlebar(this)),
      image_(Gdk::Pixbuf::create_from_file(image_filename)),
      width_(image_->get_width()),
      height_(image_->get_height()),
      expanded_(false),
      title_(title) {
  set_size_request(width_, height_);
  realize();
  xid_ = GDK_WINDOW_XWINDOW(Glib::unwrap(get_window()));
  std::vector<int> type_params;
  type_params.push_back(titlebar_->xid());
  type_params.push_back(expanded);
  CHECK(chrome_->wm_ipc()->SetWindowType(
            xid_, WmIpc::WINDOW_TYPE_CHROME_PANEL_CONTENT, &type_params));
  add_events(Gdk::BUTTON_PRESS_MASK);
  show_all();
}

bool Panel::on_expose_event(GdkEventExpose* event) {
  DrawImage(image_, this, 0, 0, width_, height_);
  return true;
}

bool Panel::on_button_press_event(GdkEventButton* event) {
  VLOG(1) << "Panel " << xid_ << " got button " << event->button;
  if (event->button == 2) {
    chrome_->ClosePanel(this);
  }
  return true;
}

bool Panel::on_key_press_event(GdkEventKey* event) {
  if (strcmp(event->string, "+") == 0) {
    width_ += 10;
    height_ += 10;
    resize(width_, height_);
  } else if (strcmp(event->string, "-") == 0) {
    width_ = max(width_ - 10, 1);
    height_ = max(height_ - 10, 1);
    resize(width_, height_);
  } else {
    VLOG(1) << "Panel " << xid_ << " got key press " << event->string;
  }
  return true;
}

bool Panel::on_client_event(GdkEventClient* event) {
  WmIpc::Message msg;
  if (!chrome_->wm_ipc()->GetMessageGdk(*event, &msg)) {
    return false;
  }

  VLOG(2) << "Got message of type " << msg.type();
  switch (msg.type()) {
    case WmIpc::Message::CHROME_NOTIFY_PANEL_STATE: {
      expanded_ = msg.param(0);
      break;
    }
    default:
      LOG(WARNING) << "Ignoring WM message of unknown type " << msg.type();
      return false;
  }
  return true;
}

bool Panel::on_focus_in_event(GdkEventFocus* event) {
  titlebar_->set_focused(true);
  titlebar_->Draw();
  return true;
}

bool Panel::on_focus_out_event(GdkEventFocus* event) {
  titlebar_->set_focused(false);
  titlebar_->Draw();
  return true;
}

MockChrome::MockChrome()
    : xconn_(new RealXConnection(GDK_DISPLAY())),
      atom_cache_(new AtomCache(xconn_.get())),
      wm_ipc_(new WmIpc(xconn_.get(), atom_cache_.get())),
      window_under_floating_tab_(NULL) {
  WmIpc::Message msg(WmIpc::Message::WM_NOTIFY_IPC_VERSION);
  msg.set_param(0, 1);
  wm_ipc_->SendMessage(wm_ipc_->wm_window(), msg);
}

ChromeWindow* MockChrome::CreateWindow(int width, int height) {
  std::tr1::shared_ptr<ChromeWindow> win(new ChromeWindow(this, width, height));
  CHECK(windows_.insert(std::make_pair(win->xid(), win)).second);
  return win.get();
}

void MockChrome::CloseWindow(ChromeWindow* win) {
  CHECK(win);
  CHECK_EQ(windows_.erase(win->xid()), 1);
}

Panel* MockChrome::CreatePanel(const std::string& image_filename,
                               const std::string& title,
                               bool expanded) {
  std::tr1::shared_ptr<Panel> panel(
      new Panel(this, image_filename, title, expanded));
  CHECK(panels_.insert(std::make_pair(panel->xid(), panel)).second);
  return panel.get();
}

void MockChrome::ClosePanel(Panel* panel) {
  CHECK(panel);
  CHECK_EQ(panels_.erase(panel->xid()), 1);
}

void MockChrome::NotifyAboutFloatingTab(
    XWindow tab_xid, ChromeWindow* win, bool entered) {
  CHECK(win);
  if (!entered) {
    if (win == window_under_floating_tab_) {
      window_under_floating_tab_ = NULL;
    }
  } else {
    window_under_floating_tab_ = win;
  }
}

void MockChrome::HandleDroppedFloatingTab(Tab* tab) {
  if (!window_under_floating_tab_) {
    VLOG(1) << "Creating new window for tab";
    ChromeWindow* win = CreateWindow(FLAGS_window_width, FLAGS_window_height);
    win->InsertTab(tab, 0);
  } else {
    VLOG(1) << "Inserting tab into window "
            << window_under_floating_tab_->xid();
    TabSummary* summary = window_under_floating_tab_->tab_summary();
    int index = window_under_floating_tab_->num_tabs();
    if (summary && summary->insert_index() >= 0) {
      index = summary->insert_index();
    }
    window_under_floating_tab_->InsertTab(tab, index);
  }
}

}  // namespace mock_chrome


int main(int argc, char** argv) {
  Gtk::Main kit(argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  CommandLine::Init(argc, argv);
  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);

  std::vector<std::string> filenames;
  SplitStringUsing(FLAGS_tab_images, ",", &filenames);
  CHECK(!filenames.empty())
      << "At least one image must be supplied using --tab_images";

  std::vector<std::string> titles;
  SplitStringUsing(FLAGS_tab_titles, ",", &titles);
  CHECK_EQ(filenames.size(), titles.size())
      << "Must specify same number of tab images and titles";

  mock_chrome::MockChrome mock_chrome;
  for (int i = 0; i < FLAGS_num_windows; ++i) {
    mock_chrome::ChromeWindow* win =
        mock_chrome.CreateWindow(FLAGS_window_width, FLAGS_window_height);
    for (int j = 0; j < FLAGS_tabs_per_window; ++j) {
      win->InsertTab(new mock_chrome::Tab(filenames[j % filenames.size()],
                                          titles[j % titles.size()]),
                     win->num_tabs());
    }
    win->ActivateTab(i % win->num_tabs());
  }

  filenames.clear();
  SplitStringUsing(FLAGS_panel_images, ",", &filenames);
  CHECK(!filenames.empty())
      << "At least one image must be supplied using --panel_images";

  titles.clear();
  SplitStringUsing(FLAGS_panel_titles, ",", &titles);
  CHECK_EQ(filenames.size(), titles.size())
      << "Must specify same number of panel images and titles";

  for (int i = 0; i < FLAGS_num_panels; ++i) {
    mock_chrome.CreatePanel(filenames[i % filenames.size()],
                            titles[i % titles.size()],
                            false);  // expanded=false
  }

  Gtk::Main::run();
  return 0;
}
