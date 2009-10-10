// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* TODO(sosa@chromium.org) - Don't need all of these depenedencies */
#include "third_party/chromeos_login_manager/login_manager_main.h"
#include "app/app_paths.h"
#include "app/resource_bundle.h"
#include "base/at_exit.h"
#include "base/process_util.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/textfield/native_textfield_gtk.h"
#include "views/focus/accelerator_handler.h"
#include "views/grid_layout.h"
#include "views/widget/widget.h"

#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/screen.h"
#include "views/background.h"
#include "views/border.h"
#include "views/slim_port/image_background.h"
#include "app/gfx/canvas.h"

using views::Background;
using views::Border;
using views::ColumnSet;
using views::GridLayout;
using views::Label;
using views::Textfield;
using views::NativeTextfieldGtk;
using views::TabbedPane;
using views::View;
using views::Widget;
using views::Accelerator;

const char* kBackgroundImage =
  "/usr/share/chromeos-login-manager/background.png";
const char* kPanelImage =
  "/usr/share/chromeos-login-manager/panel.png";
const int kPanelY = 290;
const int kUsernameY = 27;
const int kPanelSpacing = 16;
const int kTextfieldWidth = 275;

/*
 * TODO(sosa@chormium.org) - Once sparent moves glib into chromium, add
 * gflags support for constants
 */
void LoginManagerMain::Run() {
  GError* gerror = NULL;
  GdkPixbuf* background_pixbuf;
  GdkPixbuf* panel_pixbuf;

  // Load panel image from file
  panel_pixbuf = gdk_pixbuf_new_from_file(kPanelImage, &gerror);
  if (panel_pixbuf) {
    // Load background image from file
    background_pixbuf = gdk_pixbuf_new_from_file(kBackgroundImage, &gerror);
  }
  if (!panel_pixbuf || !background_pixbuf) {
    printf("error message: %s\n", gerror->message);
    exit(1);
  }
  // --------------------- Get attributes of images -----------------------
  int background_height = gdk_pixbuf_get_height(background_pixbuf);
  int background_width = gdk_pixbuf_get_width(background_pixbuf);
  int panel_height = gdk_pixbuf_get_height(panel_pixbuf);
  int panel_width = gdk_pixbuf_get_width(panel_pixbuf);

  // --------------------- Set up root window ------------------------------
  base::EnableTerminationOnHeapCorruption();

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  app::RegisterPathProvider();

  // This requires chrome to be built first right now.
  ResourceBundle::InitSharedInstance(L"en-US");
  ResourceBundle::GetSharedInstance().LoadThemeResources();

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  Widget* main_window = CreateTopLevelWidget();
  /* TODO(sosa@chromium.org) - Use this in later releases */
  // const gfx::Rect rect =
      // views::Screen::GetMonitorWorkAreaNearestWindow(NULL);

  main_window->Init(NULL, gfx::Rect(0, 0, background_width, background_height));

  // ---------------------- Set up root View ------------------------------
  View* container = new View();
  container->set_background(new views::ImageBackground(background_pixbuf));

  // Set layout
  GridLayout* layout = new GridLayout(container);
  container->SetLayoutManager(layout);

  main_window->SetContentsView(container);

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddPaddingColumn(1, 0);
  column_set->AddColumn(GridLayout::CENTER, GridLayout::FILL, 0,
                        GridLayout::FIXED, panel_width, panel_width);
  column_set->AddPaddingColumn(1, 0);

  // Row is resized with window (panel page)
  layout->AddPaddingRow(0, kPanelY);

  layout->StartRow(1, 0);
  {
    // Create login_prompt view
    View* login_prompt = new View();
    login_prompt->set_background(new views::ImageBackground(panel_pixbuf));

    // Set layout
    GridLayout* prompt_layout = new GridLayout(login_prompt);
    login_prompt->SetLayoutManager(prompt_layout);
    ColumnSet* prompt_column_set = prompt_layout->AddColumnSet(0);
    prompt_column_set->AddPaddingColumn(1, 0);
    prompt_column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                                 GridLayout::FIXED,
                                 kTextfieldWidth, kTextfieldWidth);
    prompt_column_set->AddPaddingColumn(1, 0);

    Textfield* username = new Textfield();
    /* TODO(sosa@chromium.org) - Why don't password boxes work */
    /*NativeTextfieldGtk *password = new NativeTextfieldGtk(
        new Textfield(Textfield::STYLE_PASSWORD));*/
    Textfield* password = new Textfield(Textfield::STYLE_PASSWORD);

    prompt_layout->AddPaddingRow(0, kUsernameY);
    prompt_layout->StartRow(1, 0);
    prompt_layout->AddView(username);
    prompt_layout->AddPaddingRow(0, kPanelSpacing);
    prompt_layout->StartRow(1, 0);
    prompt_layout->AddView(password);
    prompt_layout->AddPaddingRow(0, kPanelSpacing);

    layout->AddView(login_prompt, 1, 1, GridLayout::CENTER, GridLayout::CENTER,
                    panel_width, panel_height);
  }

  layout->AddPaddingRow(1, 0);

  /* Stub added here to cleanly exit upon hitting enter */
  Accelerator return_accelerator(base::VKEY_RETURN, false, false, false);
  main_window->GetFocusManager()->RegisterAccelerator(
      return_accelerator, this);

  // Show display and start message loop
  main_window->Show();

  // Start the main loop
  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->Run(&accelerator_handler);

  // Delete parent view and its children
  delete main_window;
}

views::Widget* LoginManagerMain::CreateTopLevelWidget() {
  return new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
}

bool LoginManagerMain::AcceleratorPressed(const Accelerator& accelerator) {
  MessageLoopForUI::current()->Quit();
  return true;
}

int main(int argc, char** argv) {
  // Initializes gtk stuff.
  g_thread_init(NULL);
  g_type_init();
  gtk_init(&argc, &argv);

  LoginManagerMain main;
  main.Run();
  return 0;
}
