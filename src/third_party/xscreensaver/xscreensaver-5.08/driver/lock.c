/* lock.c --- handling the password dialog for locking-mode.
 * xscreensaver, Copyright (c) 1993-2008 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

/* Athena locking code contributed by Jon A. Christopher <jac8782@tamu.edu> */
/* Copyright 1997, with the same permissions as above. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <X11/Intrinsic.h>
#include <X11/cursorfont.h>
#include <X11/Xos.h>		/* for time() */
#include <time.h>
#include <sys/time.h>
#include "xscreensaver.h"
#include "resources.h"
#include "mlstring.h"
#include "auth.h"

#ifndef NO_LOCKING              /* (mostly) whole file */

#ifdef HAVE_XHPDISABLERESET
# include <X11/XHPlib.h>
  static void hp_lock_reset (saver_info *si, Bool lock_p);
#endif /* HAVE_XHPDISABLERESET */

#ifdef HAVE_XF86VMODE
# include <X11/extensions/xf86vmode.h>
  static void xfree_lock_mode_switch (saver_info *si, Bool lock_p);
#endif /* HAVE_XF86VMODE */

#ifdef HAVE_XF86MISCSETGRABKEYSSTATE
# include <X11/extensions/xf86misc.h>
  static void xfree_lock_grab_smasher (saver_info *si, Bool lock_p);
#endif /* HAVE_XF86MISCSETGRABKEYSSTATE */


#ifdef _VROOT_H_
ERROR!  You must not include vroot.h in this file.
#endif

#ifdef HAVE_UNAME
# include <sys/utsname.h> /* for hostname info */
#endif /* HAVE_UNAME */
#include <ctype.h>

#ifndef VMS
# include <pwd.h>
#else /* VMS */

extern char *getenv(const char *name);
extern int validate_user(char *name, char *password);

static Bool
vms_passwd_valid_p(char *pw, Bool verbose_p)
{
  return (validate_user (getenv("USER"), typed_passwd) == 1);
}
# undef passwd_valid_p
# define passwd_valid_p vms_passwd_valid_p

#endif /* VMS */

#define SAMPLE_INPUT "MMMMMMMMMMMM"

#undef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))

typedef struct info_dialog_data info_dialog_data;

/* sosa@chromium.org - added to handle screen size with external display */
extern void get_current_screen_size(saver_info *si, saver_screen_info* ssi);

struct passwd_dialog_data {

  saver_screen_info *prompt_screen;
  int previous_mouse_x, previous_mouse_y;

  char typed_passwd [80];
  XtIntervalId timer;

  int i_beam;
  float ratio;

  Position x, y;
  Dimension width;
  Dimension height;

  Bool echo_input;
  Bool show_stars_p;
  Bool draw_password_prompt;

  char *passwd_string;
  Bool passwd_changed_p; /* Whether the user entry field needs redrawing */
  Bool caps_p;		 /* Whether we saw a keypress with caps-lock on */

  XFontStruct *uname_font;
  XFontStruct *passwd_font;

  Pixel foreground;
  Pixel background;
  Pixel passwd_foreground;
  Pixel passwd_background;

  Dimension uname_field_x, uname_field_y;
  Dimension passwd_field_x, passwd_field_y;
  Dimension passwd_field_width, passwd_field_height;

  Pixmap logo_pixmap;
  Pixmap logo_clipmask;
  int logo_npixels;
  unsigned long *logo_pixels;

  Cursor passwd_cursor;

  Pixmap save_under;
  Pixmap user_entry_pixmap;
};
/* TODO(sosa@chromium.org) - need to document these methods */
static void draw_passwd_window (saver_info *si);
static void update_passwd_window (saver_info *si, const char *printed_passwd,
				  float ratio);
static void destroy_passwd_window (saver_info *si);
static void undo_vp_motion (saver_info *si);
static void finished_typing_passwd (saver_info *si, passwd_dialog_data *pw);
static void cleanup_passwd_window (saver_info *si);
static void restore_background (saver_info *si);

extern void xss_authenticate(saver_info *si, Bool verbose_p);

static int
new_passwd_window (saver_info *si)
{
  passwd_dialog_data *pw;
  Screen *screen;
  Colormap cmap;
  char *f;  /* temp variable for user with getting strings from config */
  saver_screen_info *ssi = &si->screens [mouse_screen (si)];

  pw = (passwd_dialog_data *) calloc (1, sizeof(*pw));
  if (!pw)
    return -1;

  pw->passwd_cursor = XCreateFontCursor (si->dpy, XC_top_left_arrow);

  pw->prompt_screen = ssi;

  screen = pw->prompt_screen->screen;
  cmap = DefaultColormapOfScreen (screen);

  pw->show_stars_p = get_boolean_resource(si->dpy, "passwd.asterisks",
					  "Boolean");

  pw->passwd_string = strdup("");

  f = get_string_resource(si->dpy, "passwd.passwdFont", "Dialog.Font");
  pw->passwd_font = XLoadQueryFont (si->dpy, (f ? f : "fixed"));
  if (!pw->passwd_font) pw->passwd_font = XLoadQueryFont (si->dpy, "fixed");
  if (f) free (f);

  f = get_string_resource(si->dpy, "passwd.unameFont", "Dialog.Font");
  pw->uname_font = XLoadQueryFont (si->dpy, (f ? f : "fixed"));
  if (!pw->uname_font) pw->uname_font = XLoadQueryFont (si->dpy, "fixed");
  if (f) free (f);

  pw->foreground = get_pixel_resource (si->dpy, cmap,
				       "passwd.foreground",
				       "Dialog.Foreground" );
  pw->background = get_pixel_resource (si->dpy, cmap,
				       "passwd.background",
				       "Dialog.Background" );

  if (pw->foreground == pw->background)
    {
      /* Make sure the error messages show up. */
      pw->foreground = BlackPixelOfScreen (screen);
      pw->background = WhitePixelOfScreen (screen);
    }

  pw->passwd_foreground = get_pixel_resource (si->dpy, cmap,
					      "passwd.text.foreground",
					      "Dialog.Text.Foreground" );
  pw->passwd_background = get_pixel_resource (si->dpy, cmap,
					      "passwd.text.background",
					      "Dialog.Text.Background" );
  /* sosa@chromium.org - Get Chrome OS specific options */
  pw->passwd_field_width = get_integer_resource(si->dpy, "chromeos.password.width", "Integer");
  pw->passwd_field_height = get_integer_resource(si->dpy, "chromeos.password.height", "Integer");
  pw->width = get_integer_resource(si->dpy, "chromeos.background.width", "Integer");
  pw->height = get_integer_resource(si->dpy, "chromeos.background.height", "Integer");

  {
    Window pointer_root, pointer_child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;
    pw->previous_mouse_x = 0;
    pw->previous_mouse_y = 0;
    if (XQueryPointer (si->dpy, RootWindowOfScreen (pw->prompt_screen->screen),
                       &pointer_root, &pointer_child,
                       &root_x, &root_y, &win_x, &win_y, &mask))
      {
        pw->previous_mouse_x = root_x;
        pw->previous_mouse_y = root_y;
        if (si->prefs.verbose_p)
          fprintf (stderr, "%s: %d: mouse is at %d,%d.\n",
                   blurb(), pw->prompt_screen->number,
                   pw->previous_mouse_x, pw->previous_mouse_y);
      }
    else if (si->prefs.verbose_p)
      fprintf (stderr, "%s: %d: unable to determine mouse position?\n",
               blurb(), pw->prompt_screen->number);
  }

  /* Before mapping the window, save a pixmap of the current screen.
     When we lower the window, we
     restore these bits.  This works, because the running screenhack
     has already been sent SIGSTOP, so we know nothing else is drawing
     right now! */
  {
    XGCValues gcv;
    GC gc;
    pw->save_under = XCreatePixmap (si->dpy,
				    pw->prompt_screen->screensaver_window,
				    pw->prompt_screen->width,
				    pw->prompt_screen->height,
				    pw->prompt_screen->current_depth);
    gcv.function = GXcopy;
    gc = XCreateGC (si->dpy, pw->save_under, GCFunction, &gcv);
    XCopyArea (si->dpy, pw->prompt_screen->screensaver_window,
	       pw->save_under, gc,
	       0, 0,
	       pw->prompt_screen->width, pw->prompt_screen->height,
	       0, 0);
    XFreeGC (si->dpy, gc);
  }

  si->pw_data = pw;
  return 0;
}

/**
 * info_msg and prompt may be NULL.
 */
static int
make_passwd_window (saver_info *si,
		    const char *info_msg,
		    const char *prompt,
		    Bool echo)
{
  passwd_dialog_data *pw;
  Screen *screen;
  Colormap cmap;
  saver_screen_info *ssi = &si->screens [mouse_screen (si)];
  cleanup_passwd_window (si);

  if (! ssi)
    return -1;

  /* sosa@chromium.org - fix for mirroring with external monitor */
  get_current_screen_size(si, ssi);

  if (!si->pw_data)
    if (new_passwd_window (si) < 0)
      return -1;

  if (!(pw = si->pw_data))
    return -1;

  pw->ratio = 1.0;
  pw->prompt_screen = ssi;

  /* sosa@chromium.org
   * Figure out where on the desktop to place the window so that it will
   * actually be visible and centered with respect to screen
   */
  pw->x = MAX((ssi->width - pw->width) / 2, 0);
  pw->y = MAX((ssi->height - pw->height) / 2, 0);

  /* sosa@chromium.org - Get the rest of the resources from the config file */
  pw->uname_field_x = pw->x + get_integer_resource(si->dpy,
                                "chromeos.username.x", "Integer");
  pw->uname_field_y = pw->y + get_integer_resource(si->dpy,
                                "chromeos.username.y", "Integer");
  pw->passwd_field_x = pw->x + get_integer_resource(si->dpy,
                                 "chromeos.password.x", "Integer");
  pw->passwd_field_y = pw->y + get_integer_resource(si->dpy,
                                 "chromeos.password.y", "Integer");
  if (si->prefs.verbose_p)
    fprintf (stderr, "%s: %d: creating password dialog (\"%s\")\n",
             blurb(), pw->prompt_screen->number,
             info_msg ? info_msg : "");

  screen = pw->prompt_screen->screen;
  cmap = DefaultColormapOfScreen (screen);

  pw->echo_input = echo;
  pw->draw_password_prompt = (NULL != prompt);

  /* Only create the window the first time around */
  if (!si->passwd_dialog)
    {
      XSetWindowAttributes attrs;
      unsigned long attrmask = CWOverrideRedirect | CWEventMask;

      attrs.override_redirect = True;
      attrs.event_mask = (ExposureMask | KeyPressMask |
                          ButtonPressMask | ButtonReleaseMask);

      si->passwd_dialog =
	XCreateWindow (si->dpy,
		       RootWindowOfScreen(screen),
		       ssi->x, ssi->y, ssi->width, ssi->height, 0,
		       DefaultDepthOfScreen (screen), InputOutput,
		       DefaultVisualOfScreen(screen),
		       attrmask, &attrs);
      XSetWindowBackground (si->dpy, si->passwd_dialog, pw->background);

      /* We use the default visual, not ssi->visual, so that the logo pixmap's
	 visual matches that of the si->passwd_dialog window. */
      pw->logo_pixmap = xscreensaver_logo (ssi->screen,
					   DefaultVisualOfScreen(screen),
					   si->passwd_dialog, cmap,
					   pw->background,
					   &pw->logo_pixels, &pw->logo_npixels,
					   &pw->logo_clipmask, True);
    }
  else /* On successive prompts, just resize the window */
    {
      XWindowChanges wc;
      unsigned int mask = CWX | CWY | CWWidth | CWHeight;

      wc.x = ssi->x;
      wc.y = ssi->y;
      wc.width = ssi->width;
      wc.height = ssi->height;

      XConfigureWindow (si->dpy, si->passwd_dialog, mask, &wc);
    }

  restore_background(si);

  XMapRaised (si->dpy, si->passwd_dialog);
  XSync (si->dpy, False);

  move_mouse_grab (si, si->passwd_dialog,
                   pw->passwd_cursor,
                   pw->prompt_screen->number);
  undo_vp_motion (si);

  si->pw_data = pw;

  if (cmap)
    XInstallColormap (si->dpy, cmap);
  draw_passwd_window (si);

  return 0;
}


static void
draw_passwd_window (saver_info *si)
{
  passwd_dialog_data *pw = si->pw_data;
  XGCValues gcv;
  GC gc1, gc2;

  /* Force redraw */
  pw->passwd_changed_p = True;

  gcv.foreground = pw->foreground;
  gc1 = XCreateGC (si->dpy, si->passwd_dialog, GCForeground, &gcv);
  gc2 = XCreateGC (si->dpy, si->passwd_dialog, GCForeground, &gcv);

  if (pw->logo_pixmap)
    {
      Window root;
      int x, y;
      unsigned int w, h, bw, d;
      XGetGeometry (si->dpy, pw->logo_pixmap, &root, &x, &y, &w, &h, &bw, &d);
      XSetForeground (si->dpy, gc1, pw->foreground);
      XSetBackground (si->dpy, gc1, pw->background);
      XSetClipMask (si->dpy, gc1, pw->logo_clipmask);

      /* sosa@chromium.org - Let's auto center / crop this background image */
      if (w > pw->width) {
        x = MAX((w - pw->width) / 2, 0);
        w = pw->width;
      }
      if (h > pw->height) {
        y = MAX((h - pw->height)/ 2, 0);
        h = pw->height;
      }
      XSetClipOrigin (si->dpy, gc1, pw->x, pw->y);
      if (d == 1)
        XCopyPlane (si->dpy, pw->logo_pixmap, si->passwd_dialog, gc1,
                    x, y, w, h, pw->x, pw->y, 1);
      else
        XCopyArea (si->dpy, pw->logo_pixmap, si->passwd_dialog, gc1,
                   x, y, w, h, pw->x, pw->y);
    }

  XFreeGC (si->dpy, gc1);
  XFreeGC (si->dpy, gc2);

  update_passwd_window (si, pw->passwd_string, pw->ratio);
}

static void
update_passwd_window (saver_info *si, const char *printed_passwd, float ratio)
{
  passwd_dialog_data *pw = si->pw_data;
  XGCValues gcv;
  GC gc1, gc2;
  int x, y;

  pw->ratio = ratio;
  gcv.foreground = pw->passwd_foreground;
  gcv.font = pw->passwd_font->fid;
  gc1 = XCreateGC (si->dpy, si->passwd_dialog, GCForeground|GCFont, &gcv);
  gcv.foreground = pw->passwd_background;
  gc2 = XCreateGC (si->dpy, si->passwd_dialog, GCForeground, &gcv);

  if (printed_passwd)
    {
      char *s = strdup (printed_passwd);
      if (pw->passwd_string) free (pw->passwd_string);
      pw->passwd_string = s;
    }

  /* sosa@chromium.org - Added to redraw username here */
  XDrawString (si->dpy, si->passwd_dialog, gc1,
                    pw->uname_field_x,
                    pw->uname_field_y,
                    si->user, strlen(si->user));

  if (pw->draw_password_prompt)
    {
      /* The user entry (password) field is double buffered.
       * This avoids flickering, particularly in synchronous mode. */

      if (pw->passwd_changed_p)
        {
	  pw->passwd_changed_p = False;

	  if (pw->user_entry_pixmap)
	    {
	      XFreePixmap(si->dpy, pw->user_entry_pixmap);
	      pw->user_entry_pixmap = 0;
	    }

	  pw->user_entry_pixmap =
            XCreatePixmap (si->dpy, si->passwd_dialog,
                           pw->passwd_field_width, pw->passwd_field_height,
                           DefaultDepthOfScreen (pw->prompt_screen->screen));

	  XFillRectangle (si->dpy, pw->user_entry_pixmap, gc2,
			  0, 0, pw->passwd_field_width,
			  pw->passwd_field_height);

	  XDrawString (si->dpy, pw->user_entry_pixmap, gc1,
		       0,
		       pw->passwd_font->ascent,
		       pw->passwd_string, strlen(pw->passwd_string));

	  /* Ensure the new pixmap gets copied to the window */
	  pw->i_beam = 0;

	}

      /* The I-beam
       */
      if (pw->i_beam == 0)
	{
	  /* Make the I-beam disappear */

	  XCopyArea(si->dpy, pw->user_entry_pixmap, si->passwd_dialog, gc2,
		    0, 0, pw->passwd_field_width, pw->passwd_field_height,
		    pw->passwd_field_x, pw->passwd_field_y);
	}
      else if (pw->i_beam == 1)
	{
	  x = (pw->passwd_field_x + 0 +
	                 string_width (pw->passwd_font, pw->passwd_string));
	            y = pw->passwd_field_y + 0;
	  if (x > pw->passwd_field_x + pw->passwd_field_width - 1)
	    x = pw->passwd_field_x + pw->passwd_field_width - 1;
	  XDrawLine (si->dpy, si->passwd_dialog, gc1,
		     x, y,
		     x, y+pw->passwd_font->ascent + pw->passwd_font->descent-1);
	}
      pw->i_beam = (pw->i_beam + 1) % 4;
    }
  XFreeGC (si->dpy, gc1);
  XFreeGC (si->dpy, gc2);
  XSync (si->dpy, False);
}


void
restore_background (saver_info *si)
{
  passwd_dialog_data *pw = si->pw_data;
  saver_screen_info *ssi = pw->prompt_screen;
  XGCValues gcv;
  GC gc;

  gcv.function = GXcopy;

  gc = XCreateGC (si->dpy, ssi->screensaver_window, GCFunction, &gcv);

  XCopyArea (si->dpy, pw->save_under,
	     ssi->screensaver_window, gc,
	     0, 0,
	     ssi->width, ssi->height,
	     0, 0);

  XFreeGC (si->dpy, gc);
}


/* Frees anything created by make_passwd_window */
static void
cleanup_passwd_window (saver_info *si)
{
  passwd_dialog_data *pw;

  if (!(pw = si->pw_data))
    return;

  memset (pw->typed_passwd, 0, sizeof(pw->typed_passwd));
  memset (pw->passwd_string, 0, strlen(pw->passwd_string));

  if (pw->timer)
    {
      XtRemoveTimeOut (pw->timer);
      pw->timer = 0;
    }

  if (pw->user_entry_pixmap)
    {
      XFreePixmap(si->dpy, pw->user_entry_pixmap);
      pw->user_entry_pixmap = 0;
    }
}


static void
destroy_passwd_window (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  passwd_dialog_data *pw = si->pw_data;
  saver_screen_info *ssi = pw->prompt_screen;
  Colormap cmap = DefaultColormapOfScreen (ssi->screen);
  Pixel black = BlackPixelOfScreen (ssi->screen);
  Pixel white = WhitePixelOfScreen (ssi->screen);
  XEvent event;

  cleanup_passwd_window (si);

  if (si->cached_passwd)
    {
      char *wipe = si->cached_passwd;

      while (*wipe)
	*wipe++ = '\0';

      free(si->cached_passwd);
      si->cached_passwd = NULL;
    }

  move_mouse_grab (si, RootWindowOfScreen (ssi->screen),
                   ssi->cursor, ssi->number);

  if (pw->passwd_cursor)
    XFreeCursor (si->dpy, pw->passwd_cursor);

  if (p->verbose_p)
    fprintf (stderr, "%s: %d: moving mouse back to %d,%d.\n",
             blurb(), ssi->number,
             pw->previous_mouse_x, pw->previous_mouse_y);

  XWarpPointer (si->dpy, None, RootWindowOfScreen (ssi->screen),
                0, 0, 0, 0,
                pw->previous_mouse_x, pw->previous_mouse_y);

  while (XCheckMaskEvent (si->dpy, PointerMotionMask, &event))
    if (p->verbose_p)
      fprintf (stderr, "%s: discarding MotionNotify event.\n", blurb());

  if (si->passwd_dialog)
    {
      if (si->prefs.verbose_p)
        fprintf (stderr, "%s: %d: destroying password dialog.\n",
                 blurb(), pw->prompt_screen->number);

      XDestroyWindow (si->dpy, si->passwd_dialog);
      si->passwd_dialog = 0;
    }

  if (pw->save_under)
    {
      restore_background(si);
      XFreePixmap (si->dpy, pw->save_under);
      pw->save_under = 0;
    }

  if (pw->passwd_string) free (pw->passwd_string);
  if (pw->passwd_font)  XFreeFont (si->dpy, pw->passwd_font);
  if (pw->uname_font)   XFreeFont (si->dpy, pw->uname_font);
  if (pw->foreground != black && pw->foreground != white)
    XFreeColors (si->dpy, cmap, &pw->foreground, 1, 0L);
  if (pw->background != black && pw->background != white)
    XFreeColors (si->dpy, cmap, &pw->background, 1, 0L);
  if (pw->passwd_foreground != black && pw->passwd_foreground != white)
    XFreeColors (si->dpy, cmap, &pw->passwd_foreground, 1, 0L);
  if (pw->passwd_background != black && pw->passwd_background != white)
    XFreeColors (si->dpy, cmap, &pw->passwd_background, 1, 0L);

  if (pw->logo_pixmap)
    XFreePixmap (si->dpy, pw->logo_pixmap);
  if (pw-> logo_clipmask)
    XFreePixmap (si->dpy, pw->logo_clipmask);
  if (pw->logo_pixels)
    {
      if (pw->logo_npixels)
        XFreeColors (si->dpy, cmap, pw->logo_pixels, pw->logo_npixels, 0L);
      free (pw->logo_pixels);
      pw->logo_pixels = 0;
      pw->logo_npixels = 0;
    }

  if (pw->save_under)
    XFreePixmap (si->dpy, pw->save_under);

  if (cmap)
    XInstallColormap (si->dpy, cmap);

  memset (pw, 0, sizeof(*pw));
  free (pw);
  si->pw_data = 0;
}


static Bool error_handler_hit_p = False;

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  return 0;
}


#ifdef HAVE_XHPDISABLERESET
/* This function enables and disables the C-Sh-Reset hot-key, which
   normally resets the X server (logging out the logged-in user.)
   We don't want random people to be able to do that while the
   screen is locked.
 */
static void
hp_lock_reset (saver_info *si, Bool lock_p)
{
  static Bool hp_locked_p = False;

  /* Calls to XHPDisableReset and XHPEnableReset must be balanced,
     or BadAccess errors occur.  (It's ok for this to be global,
     since it affects the whole machine, not just the current screen.)
  */
  if (hp_locked_p == lock_p)
    return;

  if (lock_p)
    XHPDisableReset (si->dpy);
  else
    XHPEnableReset (si->dpy);
  hp_locked_p = lock_p;
}
#endif /* HAVE_XHPDISABLERESET */


#ifdef HAVE_XF86MISCSETGRABKEYSSTATE

/* This function enables and disables the Ctrl-Alt-KP_star and
   Ctrl-Alt-KP_slash hot-keys, which (in XFree86 4.2) break any
   grabs and/or kill the grabbing client.  That would effectively
   unlock the screen, so we don't like that.

   The Ctrl-Alt-KP_star and Ctrl-Alt-KP_slash hot-keys only exist
   if AllowDeactivateGrabs and/or AllowClosedownGrabs are turned on
   in XF86Config.  I believe they are disabled by default.

   This does not affect any other keys (specifically Ctrl-Alt-BS or
   Ctrl-Alt-F1) but I wish it did.  Maybe it will someday.
 */
static void
xfree_lock_grab_smasher (saver_info *si, Bool lock_p)
{
  saver_preferences *p = &si->prefs;
  int status;
  int event, error;
  XErrorHandler old_handler;

  if (!XF86MiscQueryExtension(si->dpy, &event, &error))
    return;

  XSync (si->dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);
  XSync (si->dpy, False);
  status = XF86MiscSetGrabKeysState (si->dpy, !lock_p);
  XSync (si->dpy, False);
  if (error_handler_hit_p) status = 666;

  if (!lock_p && status == MiscExtGrabStateAlready)
    status = MiscExtGrabStateSuccess;  /* shut up, consider this success */

  if (p->verbose_p && status != MiscExtGrabStateSuccess)
    fprintf (stderr, "%s: error: XF86MiscSetGrabKeysState(%d) returned %s\n",
             blurb(), !lock_p,
             (status == MiscExtGrabStateSuccess ? "MiscExtGrabStateSuccess" :
              status == MiscExtGrabStateLocked  ? "MiscExtGrabStateLocked"  :
              status == MiscExtGrabStateAlready ? "MiscExtGrabStateAlready" :
              status == 666 ? "an X error" :
              "unknown value"));

  XSync (si->dpy, False);
  XSetErrorHandler (old_handler);
  XSync (si->dpy, False);
}
#endif /* HAVE_XF86MISCSETGRABKEYSSTATE */



/* This function enables and disables the C-Alt-Plus and C-Alt-Minus
   hot-keys, which normally change the resolution of the X server.
   We don't want people to be able to switch the server resolution
   while the screen is locked, because if they switch to a higher
   resolution, it could cause part of the underlying desktop to become
   exposed.
 */
#ifdef HAVE_XF86VMODE

static void
xfree_lock_mode_switch (saver_info *si, Bool lock_p)
{
  static Bool any_mode_locked_p = False;
  saver_preferences *p = &si->prefs;
  int screen;
  int real_nscreens = ScreenCount (si->dpy);
  int event, error;
  Bool status;
  XErrorHandler old_handler;

  if (any_mode_locked_p == lock_p)
    return;
  if (!XF86VidModeQueryExtension (si->dpy, &event, &error))
    return;

  for (screen = 0; screen < real_nscreens; screen++)
    {
      XSync (si->dpy, False);
      old_handler = XSetErrorHandler (ignore_all_errors_ehandler);
      error_handler_hit_p = False;
      status = XF86VidModeLockModeSwitch (si->dpy, screen, lock_p);
      XSync (si->dpy, False);
      XSetErrorHandler (old_handler);
      if (error_handler_hit_p) status = False;

      if (status)
        any_mode_locked_p = lock_p;

      if (!status && (p->verbose_p || !lock_p))
        /* Only print this when verbose, or when we locked but can't unlock.
           I tried printing this message whenever it comes up, but
           mode-locking always fails if DontZoom is set in XF86Config. */
        fprintf (stderr, "%s: %d: unable to %s mode switching!\n",
                 blurb(), screen, (lock_p ? "lock" : "unlock"));
      else if (p->verbose_p)
        fprintf (stderr, "%s: %d: %s mode switching.\n",
                 blurb(), screen, (lock_p ? "locked" : "unlocked"));
    }
}
#endif /* HAVE_XF86VMODE */


/* If the viewport has been scrolled since the screen was blanked,
   then scroll it back to where it belongs.  This function only exists
   to patch over a very brief race condition.
 */
static void
undo_vp_motion (saver_info *si)
{
#ifdef HAVE_XF86VMODE
  saver_preferences *p = &si->prefs;
  int screen;
  int real_nscreens = ScreenCount (si->dpy);
  int event, error;

  if (!XF86VidModeQueryExtension (si->dpy, &event, &error))
    return;

  for (screen = 0; screen < real_nscreens; screen++)
    {
      saver_screen_info *ssi = &si->screens[screen];
      int x, y;
      Bool status;

      if (ssi->blank_vp_x == -1 && ssi->blank_vp_y == -1)
        break;
      if (!XF86VidModeGetViewPort (si->dpy, screen, &x, &y))
        return;
      if (ssi->blank_vp_x == x && ssi->blank_vp_y == y)
        return;

      /* We're going to move the viewport.  The mouse has just been grabbed on
         (and constrained to, thus warped to) the password window, so it is no
         longer near the edge of the screen.  However, wait a bit anyway, just
         to make sure the server drains its last motion event, so that the
         screen doesn't continue to scroll after we've reset the viewport.
       */
      XSync (si->dpy, False);
      usleep (250000);  /* 1/4 second */
      XSync (si->dpy, False);

      status = XF86VidModeSetViewPort (si->dpy, screen,
                                       ssi->blank_vp_x, ssi->blank_vp_y);

      if (!status)
        fprintf (stderr,
                 "%s: %d: unable to move vp from (%d,%d) back to (%d,%d)!\n",
                 blurb(), screen, x, y, ssi->blank_vp_x, ssi->blank_vp_y);
      else if (p->verbose_p)
        fprintf (stderr,
                 "%s: %d: vp moved to (%d,%d); moved it back to (%d,%d).\n",
                 blurb(), screen, x, y, ssi->blank_vp_x, ssi->blank_vp_y);
    }
#endif /* HAVE_XF86VMODE */
}



/* Interactions
 */

static void
passwd_animate_timer (XtPointer closure, XtIntervalId *id)
{
  saver_info *si = (saver_info *) closure;
  int tick = 166;
  passwd_dialog_data *pw = si->pw_data;

  if (!pw) return;

  pw->ratio -= (1.0 / ((double) si->prefs.passwd_timeout / (double) tick));
  if (pw->ratio < 0)
    {
      pw->ratio = 0;
      if (si->unlock_state == ul_read)
	si->unlock_state = ul_time;
    }

  update_passwd_window (si, 0, pw->ratio);

  if (si->unlock_state == ul_read)
    pw->timer = XtAppAddTimeOut (si->app, tick, passwd_animate_timer,
				 (XtPointer) si);
  else
    pw->timer = 0;

  idle_timer ((XtPointer) si, 0);
}


static XComposeStatus *compose_status;

static void
finished_typing_passwd (saver_info *si, passwd_dialog_data *pw)
{
  if (si->unlock_state == ul_read)
    {
      update_passwd_window (si, "Checking...", pw->ratio);
      XSync (si->dpy, False);

      si->unlock_state = ul_finished;
      update_passwd_window (si, "", pw->ratio);
    }
}

static void
handle_passwd_key (saver_info *si, XKeyEvent *event)
{
  passwd_dialog_data *pw = si->pw_data;
  int pw_size = sizeof (pw->typed_passwd) - 1;
  char *typed_passwd = pw->typed_passwd;
  char s[2];
  char *stars = 0;
  int i;
  int size = XLookupString (event, s, 1, 0, compose_status);

  if (size != 1) return;

  s[1] = 0;

  pw->passwd_changed_p = True;

  /* Add 10% to the time remaining every time a key is pressed. */
  pw->ratio += 0.1;
  if (pw->ratio > 1) pw->ratio = 1;

  switch (*s)
    {
    case '\010': case '\177':				/* Backspace */
      if (!*typed_passwd)
	XBell (si->dpy, 0);
      else
	typed_passwd [strlen(typed_passwd)-1] = 0;
      break;

    case '\025': case '\030':				/* Erase line */
      memset (typed_passwd, 0, pw_size);
      break;

    case '\012': case '\015':				/* Enter */
      finished_typing_passwd(si, pw);
      break;

    case '\033':					/* Escape */
      si->unlock_state = ul_cancel;
      break;

    default:
      /* Though technically the only illegal characters in Unix passwords
         are LF and NUL, most GUI programs (e.g., GDM) use regular text-entry
         fields that only let you type printable characters.  So, people
         who use funky characters in their passwords are already broken.
         We follow that precedent.
       */
      if (isprint ((unsigned char) *s))
        {
          i = strlen (typed_passwd);
          if (i >= pw_size-1)
            XBell (si->dpy, 0);
          else
            {
              typed_passwd [i] = *s;
              typed_passwd [i+1] = 0;
            }
        }
      else
        XBell (si->dpy, 0);
      break;
    }

  if (pw->echo_input)
    {
      /* If the input is wider than the text box, only show the last portion.
       * This simulates a horizontally scrolling text field. */
      int chars_in_pwfield = (pw->passwd_field_width /
			      pw->passwd_font->max_bounds.width);

      if (strlen(typed_passwd) > chars_in_pwfield)
	typed_passwd += (strlen(typed_passwd) - chars_in_pwfield);

      update_passwd_window(si, typed_passwd, pw->ratio);
    }
  else if (pw->show_stars_p)
    {
      i = strlen(typed_passwd);
      stars = (char *) malloc(i+1);
      memset (stars, '*', i);
      stars[i] = 0;
      update_passwd_window (si, stars, pw->ratio);
      free (stars);
    }
  else
    {
      update_passwd_window (si, "", pw->ratio);
    }
}


static void
passwd_event_loop (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  char *msg = 0;
  XEvent event;

  passwd_animate_timer ((XtPointer) si, 0);

  while (si->unlock_state == ul_read)
    {
      XtAppNextEvent (si->app, &event);
      if (event.xany.window == si->passwd_dialog && event.xany.type == Expose)
	draw_passwd_window (si);
      else if (event.xany.type == KeyPress)
        {
          handle_passwd_key (si, &event.xkey);
          si->pw_data->caps_p = (event.xkey.state & LockMask);
        }
      else if (event.xany.type == ButtonPress ||
               event.xany.type == ButtonRelease)
	{
	  /* WE DO NOTHING :D */
	}
      else
	XtDispatchEvent (&event);
    }

  switch (si->unlock_state)
    {
    case ul_cancel: msg = ""; break;
    case ul_time: msg = "Timed out!"; break;
    case ul_finished: msg = "Checking..."; break;
    default: msg = 0; break;
    }

  if (p->verbose_p)
    switch (si->unlock_state) {
    case ul_cancel:
      fprintf (stderr, "%s: input cancelled.\n", blurb()); break;
    case ul_time:
      fprintf (stderr, "%s: input timed out.\n", blurb()); break;
    case ul_finished:
      fprintf (stderr, "%s: input finished.\n", blurb()); break;
    default: break;
    }

  if (msg)
    {
      si->pw_data->i_beam = 0;
      update_passwd_window (si, msg, 0.0);
      XSync (si->dpy, False);

      /* Swallow all pending KeyPress/KeyRelease events. */
      {
	XEvent e;
	while (XCheckMaskEvent (si->dpy, KeyPressMask|KeyReleaseMask, &e))
	  ;
      }
    }
}


static void
handle_typeahead (saver_info *si)
{
  passwd_dialog_data *pw = si->pw_data;
  int i;
  if (!si->unlock_typeahead)
    return;

  pw->passwd_changed_p = True;

  i = strlen (si->unlock_typeahead);
  if (i >= sizeof(pw->typed_passwd) - 1)
    i = sizeof(pw->typed_passwd) - 1;

  memcpy (pw->typed_passwd, si->unlock_typeahead, i);
  pw->typed_passwd [i] = 0;

  memset (si->unlock_typeahead, '*', strlen(si->unlock_typeahead));
  si->unlock_typeahead[i] = 0;
  update_passwd_window (si, si->unlock_typeahead, pw->ratio);

  free (si->unlock_typeahead);
  si->unlock_typeahead = 0;
}


/**
 * Returns a copy of the input string with trailing whitespace removed.
 * Whitespace is anything considered so by isspace().
 * It is safe to call this with NULL, in which case NULL will be returned.
 * The returned string (if not NULL) should be freed by the caller with free().
 */
static char *
remove_trailing_whitespace(const char *str)
{
  size_t len;
  char *newstr, *chr;

  if (!str)
    return NULL;

  len = strlen(str);

  newstr = malloc(len + 1);
  (void) strcpy(newstr, str);

  if (!newstr)
    return NULL;

  chr = newstr + len;
  while (isspace(*--chr) && chr >= newstr)
    *chr = '\0';

  return newstr;
}


/*
 * The authentication conversation function.
 * Like a PAM conversation function, this accepts multiple messages in a single
 * round. It then splits them into individual messages for display on the
 * passwd dialog. A message sequence of info or error followed by a prompt will
 * be reduced into a single dialog window.
 *
 * Returns 0 on success or -1 if some problem occurred (canceled auth, OOM, ...)
 */
int
gui_auth_conv(int num_msg,
	  const struct auth_message auth_msgs[],
	  struct auth_response **resp,
	  saver_info *si)
{
  int i;
  const char *info_msg, *prompt;
  struct auth_response *responses;

  if (si->unlock_state == ul_cancel ||
      si->unlock_state == ul_time)
    /* If we've already canceled or timed out in this PAM conversation,
       don't prompt again even if PAM asks us to! */
    return -1;

  if (!(responses = calloc(num_msg, sizeof(struct auth_response))))
    goto fail;

  for (i = 0; i < num_msg; ++i)
    {
      info_msg = prompt = NULL;

      /* See if there is a following message that can be shown at the same
       * time */
      if (auth_msgs[i].type == AUTH_MSGTYPE_INFO
	  && i+1 < num_msg
	  && (   auth_msgs[i+1].type == AUTH_MSGTYPE_PROMPT_NOECHO
	      || auth_msgs[i+1].type == AUTH_MSGTYPE_PROMPT_ECHO)
	 )
	{
	  info_msg = auth_msgs[i].msg;
	  prompt = auth_msgs[++i].msg;
	}
      else
        {
	  if (   auth_msgs[i].type == AUTH_MSGTYPE_INFO
	      || auth_msgs[i].type == AUTH_MSGTYPE_ERROR)
	    info_msg = auth_msgs[i].msg;
	  else
	    prompt = auth_msgs[i].msg;
	}

      {
	char *info_msg_trimmed, *prompt_trimmed;

        /* Trailing whitespace looks bad in a GUI */
	info_msg_trimmed = remove_trailing_whitespace(info_msg);
	prompt_trimmed = remove_trailing_whitespace(prompt);

	if (make_passwd_window(si, info_msg_trimmed, prompt_trimmed,
                               auth_msgs[i].type == AUTH_MSGTYPE_PROMPT_ECHO
                               ? True : False)
            < 0)
          goto fail;

	if (info_msg_trimmed)
	  free(info_msg_trimmed);

	if (prompt_trimmed)
	  free(prompt_trimmed);
      }

      compose_status = calloc (1, sizeof (*compose_status));
      if (!compose_status)
	goto fail;

      si->unlock_state = ul_read;

      handle_typeahead (si);
      passwd_event_loop (si);

      if (si->unlock_state == ul_cancel)
	goto fail;

      responses[i].response = strdup(si->pw_data->typed_passwd);

      /* Cache the first response to a PROMPT_NOECHO to save prompting for
       * each auth mechanism. */
      if (si->cached_passwd == NULL &&
	  auth_msgs[i].type == AUTH_MSGTYPE_PROMPT_NOECHO)
	si->cached_passwd = strdup(responses[i].response);

      free (compose_status);
      compose_status = 0;
    }

  *resp = responses;

  return (si->unlock_state == ul_finished) ? 0 : -1;

fail:
  if (compose_status)
    free (compose_status);

  if (responses)
    {
      for (i = 0; i < num_msg; ++i)
	if (responses[i].response)
	  free (responses[i].response);
      free (responses);
    }

  return -1;
}


void
auth_finished_cb (saver_info *si)
{
  char buf[1024];
  const char *s;

  /* If we have something to say, put the dialog back up for a few seconds
     to display it.  Otherwise, don't bother.
   */

  if (si->unlock_state == ul_fail &&		/* failed with caps lock on */
      si->pw_data && si->pw_data->caps_p)
    s = "Authentication failed (Caps Lock?)";
  else if (si->unlock_state == ul_fail)		/* failed without caps lock */
    s = "Authentication failed!";
  else if (si->unlock_state == ul_success &&	/* good, but report failures */
           si->unlock_failures > 0)
    {
      if (si->unlock_failures == 1)
        s = "There has been\n1 failed login attempt.";
      else
        {
          sprintf (buf, "There have been\n%d failed login attempts.",
                   si->unlock_failures);
          s = buf;
        }
      si->unlock_failures = 0;
    }
  else						/* good, with no failures, */
    goto END;					/* or timeout, or cancel. */

  make_passwd_window (si, s, NULL, True);
  XSync (si->dpy, False);

  {
    int secs = 4;
    time_t start = time ((time_t *) 0);
    XEvent event;
    while (time ((time_t *) 0) < start + secs)
      if (XPending (si->dpy))
        {
          XNextEvent (si->dpy, &event);
          if (event.xany.window == si->passwd_dialog &&
              event.xany.type == Expose)
            draw_passwd_window (si);
          else if (event.xany.type == ButtonPress ||
                   event.xany.type == KeyPress)
            break;
          XSync (si->dpy, False);
        }
      else
        usleep (250000);  /* 1/4 second */
  }

 END:
  if (si->pw_data)
    destroy_passwd_window (si);
}


Bool
unlock_p (saver_info *si)
{
  saver_preferences *p = &si->prefs;

  if (!si->unlock_cb)
    {
      fprintf(stderr, "%s: Error: no unlock function specified!\n", blurb());
      return False;
    }

  raise_window (si, True, True, True);

  xss_authenticate(si, p->verbose_p);

  return (si->unlock_state == ul_success);
}


void
set_locked_p (saver_info *si, Bool locked_p)
{
  si->locked_p = locked_p;

#ifdef HAVE_XHPDISABLERESET
  hp_lock_reset (si, locked_p);                 /* turn off/on C-Sh-Reset */
#endif
#ifdef HAVE_XF86VMODE
  xfree_lock_mode_switch (si, locked_p);        /* turn off/on C-Alt-Plus */
#endif
#ifdef HAVE_XF86MISCSETGRABKEYSSTATE
  xfree_lock_grab_smasher (si, locked_p);       /* turn off/on C-Alt-KP-*,/ */
#endif

  store_saver_status (si);			/* store locked-p */
}


#else  /*  NO_LOCKING -- whole file */

void
set_locked_p (saver_info *si, Bool locked_p)
{
  if (locked_p) abort();
}

#endif /* !NO_LOCKING */
