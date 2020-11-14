/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * WWW    : http://sourceforge.net/projects/es40
 * E-mail : camiel@camicom.com
 *
 *  This file is based upon Bochs.
 *
 *  Copyright (C) 2002  MandrakeSoft S.A.
 *
 *    MandrakeSoft S.A.
 *    43, rue d'Aboukir
 *    75002 Paris - France
 *    http://www.linux-mandrake.com/
 *    http://www.mandrakesoft.com/
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

 */

/**
 * \file
 * X-Windows GUI implementation. Allows use of gfx without SDL on Linux.
 *
 * $Id: gui_x11.cpp,v 1.5 2008/03/26 19:19:53 iamcamiel Exp $
 *
 * X-1.5        Camiel Vanderhoeven                             26-MAR-2008
 *      Fix compiler warnings.
 *
 * X-1.4        Pepito Grillo                                   15-MAR-2008
 *      Fixed FAILURE macro
 *
 * X-1.3        Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting
 *
 * X-1.2        Camiel Vanderhoeven                             12-FEB-2008
 *      Moved keyboard code into it's own class (CKeyboard)
 *
 * X-1.1        Camiel Vanderhoeven                             20-JAN-2008
 *      Initial version for ES40 emulator.
 *
 **/
#define XK_PUBLISHING
#define XK_TECHNICAL

#include "../StdAfx.h"

#if defined(HAVE_X11)
#include "../Configurator.h"
#include "../Keyboard.h"
#include "../VGA.h"
#include "gui.h"
#include "keymap.h"

extern "C" {
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
}
#include "gui_win32_font.h"

class bx_x11_gui_c : public bx_gui_c {
public:
  bx_x11_gui_c(CConfigurator *cfg) {
    myCfg = cfg;
    bx_keymap = new bx_keymap_c(cfg);
  };

  virtual void specific_init(unsigned x_tilesize, unsigned y_tilesize);
  virtual void text_update(u8 *old_text, u8 *new_text, unsigned long cursor_x,
                           unsigned long cursor_y, bx_vga_tminfo_t tm_info,
                           unsigned rows);
  virtual void graphics_tile_update(u8 *snapshot, unsigned x, unsigned y);
  virtual void handle_events(void);
  virtual void flush(void);
  virtual void clear_screen(void);
  virtual bool palette_change(unsigned index, unsigned red, unsigned green,
                              unsigned blue);
  virtual void dimension_update(unsigned x, unsigned y, unsigned fheight = 0,
                                unsigned fwidth = 0, unsigned bpp = 8);
  virtual void mouse_enabled_changed_specific(bool val);
  virtual void exit(void);
  virtual bx_svga_tileinfo_t *graphics_tile_info(bx_svga_tileinfo_t *info);
  virtual u8 *graphics_tile_get(unsigned x, unsigned y, unsigned *w,
                                unsigned *h);
  virtual void graphics_tile_update_in_place(unsigned x, unsigned y, unsigned w,
                                             unsigned h);
  virtual void get_capabilities(u16 *xres, u16 *yres, u16 *bpp);

private:
  CConfigurator *myCfg;
};

// declare one instance of the gui object and call macro to insert the
// plugin code
static bx_x11_gui_c *theGui = NULL;
IMPLEMENT_GUI_PLUGIN_CODE(x11)
#define MAX_MAPPED_STRING_LENGTH 10

/* These are used as arguments to nearly every Xlib routine, so it saves
 * routine arguments to declare them global.  If there were
 * additional source files, they would be declared extern there. */
Display *bx_x_display;
int bx_x_screen_num;
static Visual *default_visual;
static Colormap default_cmap;
static unsigned long white_pixel = 0, black_pixel = 0;

static char *progname; /* name this program was invoked by */

static unsigned int text_rows = 25, text_cols = 80;
static u8 h_panning = 0, v_panning = 0;
static u16 line_compare = 1023;

static Window win;
static GC gc, gc_inv;
static unsigned font_width, font_height;
static unsigned dimension_x = 0, dimension_y = 0;
static unsigned vga_bpp = 8;

static XImage *ximage = NULL;
static unsigned imDepth, imWide, imBPP;

// current cursor coordinates
static int prev_x = -1, prev_y = -1;
static int current_x = -1, current_y = -1, current_z = 0;
static unsigned mouse_button_state = 0;
static bool CTRL_pressed = 0;

static unsigned prev_cursor_x = 0;
static unsigned prev_cursor_y = 0;

static int warp_home_x = 200;
static int warp_home_y = 200;
static int mouse_enable_x = 0;
static int mouse_enable_y = 0;
static int warp_dx = 0;
static int warp_dy = 0;

static void warp_cursor(int dx, int dy);
static void disable_cursor();
static void enable_cursor();

static u32 convertStringToXKeysym(const char *string);

static bool x_init_done = false;

static Pixmap vgafont[256];

static void send_keyboard_mouse_status(void);

static bool x_keymapping;
static bool x_private_colormap;

u32 ascii_to_key_event[0x5f] = {
    //  !"#$%&'
    BX_KEY_SPACE, BX_KEY_1, BX_KEY_SINGLE_QUOTE, BX_KEY_3, BX_KEY_4, BX_KEY_5,
    BX_KEY_7, BX_KEY_SINGLE_QUOTE,

    // ()*+,-./
    BX_KEY_9, BX_KEY_0, BX_KEY_8, BX_KEY_EQUALS, BX_KEY_COMMA, BX_KEY_MINUS,
    BX_KEY_PERIOD, BX_KEY_SLASH,

    // 01234567
    BX_KEY_0, BX_KEY_1, BX_KEY_2, BX_KEY_3, BX_KEY_4, BX_KEY_5, BX_KEY_6,
    BX_KEY_7,

    // 89:;<=>?
    BX_KEY_8, BX_KEY_9, BX_KEY_SEMICOLON, BX_KEY_SEMICOLON, BX_KEY_COMMA,
    BX_KEY_EQUALS, BX_KEY_PERIOD, BX_KEY_SLASH,

    // @ABCDEFG
    BX_KEY_2, BX_KEY_A, BX_KEY_B, BX_KEY_C, BX_KEY_D, BX_KEY_E, BX_KEY_F,
    BX_KEY_G,

    // HIJKLMNO
    BX_KEY_H, BX_KEY_I, BX_KEY_J, BX_KEY_K, BX_KEY_L, BX_KEY_M, BX_KEY_N,
    BX_KEY_O,

    // PQRSTUVW
    BX_KEY_P, BX_KEY_Q, BX_KEY_R, BX_KEY_S, BX_KEY_T, BX_KEY_U, BX_KEY_V,
    BX_KEY_W,

    // XYZ[\]^_
    BX_KEY_X, BX_KEY_Y, BX_KEY_Z, BX_KEY_LEFT_BRACKET, BX_KEY_BACKSLASH,
    BX_KEY_RIGHT_BRACKET, BX_KEY_6, BX_KEY_MINUS,

    // `abcdefg
    BX_KEY_GRAVE, BX_KEY_A, BX_KEY_B, BX_KEY_C, BX_KEY_D, BX_KEY_E, BX_KEY_F,
    BX_KEY_G,

    // hijklmno
    BX_KEY_H, BX_KEY_I, BX_KEY_J, BX_KEY_K, BX_KEY_L, BX_KEY_M, BX_KEY_N,
    BX_KEY_O,

    // pqrstuvw
    BX_KEY_P, BX_KEY_Q, BX_KEY_R, BX_KEY_S, BX_KEY_T, BX_KEY_U, BX_KEY_V,
    BX_KEY_W,

    // xyz{|}~
    BX_KEY_X, BX_KEY_Y, BX_KEY_Z, BX_KEY_LEFT_BRACKET, BX_KEY_BACKSLASH,
    BX_KEY_RIGHT_BRACKET, BX_KEY_GRAVE};

extern u8 graphics_snapshot[32 * 1024];

static void create_internal_vga_font(void);
static void xkeypress(KeySym keysym, int press_release);

// extern "C" void select_visual(void);
#define ROUNDUP(nbytes, pad) ((((nbytes) + ((pad)-1)) / (pad)) * ((pad) >> 3))
#define MAX_VGA_COLORS 256

unsigned long col_vals[MAX_VGA_COLORS]; // 256 VGA colors
unsigned curr_foreground, curr_background;

static unsigned x_tilesize, y_tilesize;

// BxEvent *x11_notify_callback (void *unused, BxEvent *event);
// bxevent_handler old_callback = NULL;
// void *old_callback_arg = NULL;

// Try to allocate NCOLORS at once in the colormap provided.  If it can
// be done, return true.  If not, return false.  (In either case, free
// up the color cells so that we don't add to the problem!)  This is used
// to determine whether Bochs should use a private colormap even when the

// user did not specify it.
static bool test_alloc_colors(Colormap cmap, u32 n_tries) {
  XColor color;
  unsigned long pixel[MAX_VGA_COLORS];
  bool pixel_valid[MAX_VGA_COLORS];
  u32 n_allocated = 0;
  u32 i;
  color.flags = DoRed | DoGreen | DoBlue;
  for (i = 0; i < n_tries; i++) {

    // choose weird color values that are unlikely to already be in the
    // colormap.
    color.red = ((i + 41) % MAX_VGA_COLORS) << 8;
    color.green = ((i + 42) % MAX_VGA_COLORS) << 8;
    color.blue = ((i + 43) % MAX_VGA_COLORS) << 8;
    pixel_valid[i] = false;
    if (XAllocColor(bx_x_display, cmap, &color)) {
      pixel[i] = color.pixel;
      pixel_valid[i] = true;
      n_allocated++;
    }
  }

  BX_INFO(("test_alloc_colors: %d colors available out of %d colors tried",
           n_allocated, n_tries));

  // now free them all
  for (i = 0; i < n_tries; i++) {
    if (pixel_valid[i])
      XFreeColors(bx_x_display, cmap, &pixel[i], 1, 0);
  }

  return (n_allocated == n_tries);
}

void bx_x11_gui_c::specific_init(unsigned tilewidth, unsigned tileheight) {
  unsigned i;
  int x;
  int y;                         /* window position */
  unsigned int border_width = 4; /* four pixels */
  const char *window_name = "ES40 Emulator";
  const char *icon_name = "ES40";
  XSizeHints size_hints;
  char *display_name = NULL;

  /* create GC for text and drawing */
  unsigned long valuemask = 0; /* ignore XGCvalues and use defaults */
  XGCValues values;
  int default_depth;
  XEvent report;
  XSetWindowAttributes win_attr;
  unsigned long plane_masks_return[1];
  XColor color;

  x_tilesize = tilewidth;
  y_tilesize = tileheight;

  /* connect to X server */
  if ((bx_x_display = XOpenDisplay(display_name)) == NULL && progname != nullptr) {
    BX_PANIC(("%s: cannot connect to X server %s", progname,
              XDisplayName(display_name)));
  }

  /* get screen size from display structure macro */
  bx_x_screen_num = DefaultScreen(bx_x_display);

  /* Note that in a real application, x and y would default to 0
   * but would be settable from the command line or resource database.
   */
  x = y = 0;

  // Temporary values so we can create the window
  font_width = 8;
  font_height = 16;

  dimension_x = text_cols * font_width;
  dimension_y = text_rows * font_height;

  /* create opaque window */
  win = XCreateSimpleWindow(bx_x_display,
                            RootWindow(bx_x_display, bx_x_screen_num), x, y,
                            dimension_x, dimension_y, border_width,
                            BlackPixel(bx_x_display, bx_x_screen_num),
                            BlackPixel(bx_x_display, bx_x_screen_num));

  // (attempt to) enable backing store
  win_attr.save_under = 1;
  win_attr.backing_store = Always;
  XChangeWindowAttributes(bx_x_display, win, CWSaveUnder | CWBackingStore,
                          &win_attr);

  default_depth = DefaultDepth(bx_x_display, bx_x_screen_num);
  default_visual = DefaultVisual(bx_x_display, bx_x_screen_num);

  x_private_colormap = myCfg->get_bool_value("private_colormap", true);
  if (!x_private_colormap) {

    // if (!SIM->get_param_bool(BXPN_PRIVATE_COLORMAP)->get()) {
    default_cmap = DefaultColormap(bx_x_display, bx_x_screen_num);

    // try to use default colormap.  If not enough colors are available,
    // then switch to private colormap despite the user setting.  There
    // are too many cases when no colors are available and ES40 Emulator
    // simply draws everything in black on black.
    if (!test_alloc_colors(default_cmap, 16)) {
      printf("xxx: I can't allocate 16 colors\n");
      x_private_colormap = true;
    }

    col_vals[0] = BlackPixel(bx_x_display, bx_x_screen_num);
    col_vals[15] = WhitePixel(bx_x_display, bx_x_screen_num);
    for (i = 1; i < MAX_VGA_COLORS; i++) {
      if (i == 15)
        continue;
      col_vals[i] = col_vals[0];
    }
  }

  if (x_private_colormap) {

    // if (SIM->get_param_bool(BXPN_PRIVATE_COLORMAP)->get()) {
    default_cmap =
        XCreateColormap(bx_x_display, DefaultRootWindow(bx_x_display),
                        default_visual, AllocNone);
    if (XAllocColorCells(bx_x_display, default_cmap, False, plane_masks_return,
                         0, col_vals, MAX_VGA_COLORS) == 0) {
      BX_PANIC(("XAllocColorCells returns error. Maybe your screen does not "
                "support a private colormap?"));
    }

    win_attr.colormap = default_cmap;
    XChangeWindowAttributes(bx_x_display, win, CWColormap, &win_attr);

    color.flags = DoRed | DoGreen | DoBlue;

    for (i = 0; i < MAX_VGA_COLORS; i++) {
      color.pixel = i;
      if (i == 15) {
        color.red = 0xffff;
        color.green = 0xffff;
        color.blue = 0xffff;
      } else {
        color.red = 0;
        color.green = 0;
        color.blue = 0;
      }

      XStoreColor(bx_x_display, default_cmap, &color);
    }
  }

  // convenience variables which hold the black & white color indeces
  black_pixel = col_vals[0];
  white_pixel = col_vals[15];

  BX_INFO(("font %u wide x %u high, display depth = %d", (unsigned)font_width,
           (unsigned)font_height, default_depth));

  // select_visual();

  /* Set size hints for window manager.  The window manager may
   * override these settings.  Note that in a real
   * application if size or position were set by the user
   * the flags would be UPosition and USize, and these would
   * override the window manager's preferences for this window.
   */

  /* x, y, width, and height hints are now taken from
   * the actual settings of the window when mapped. Note
   * that PPosition and PSize must be specified anyway.
   */
  size_hints.flags = PPosition | PSize | PMinSize | PMaxSize;
  size_hints.max_width = size_hints.min_width = dimension_x;
  size_hints.max_height = size_hints.min_height = dimension_y;
  {
    XWMHints wm_hints;
    XClassHint class_hints;

    /* format of the window name and icon name
     * arguments has changed in R4
     */
    XTextProperty windowName;

    /* format of the window name and icon name
     * arguments has changed in R4
     */
    XTextProperty iconName;

    /* These calls store window_name and icon_name into
     * XTextProperty structures and set their other
     * fields properly. */
    if (XStringListToTextProperty((char **)&window_name, 1, &windowName) == 0 && progname != nullptr) {
      BX_PANIC(("%s: structure allocation for windowName failed.", progname));
    }

    if (XStringListToTextProperty((char **)&icon_name, 1, &iconName) == 0 && progname != nullptr) {
      BX_PANIC(("%s: structure allocation for iconName failed.", progname));
    }

    wm_hints.initial_state = NormalState;
    wm_hints.input = True;
    class_hints.res_name = progname;
    class_hints.res_class = (char *)"ES40 Emulator";

    XSetWMProperties(bx_x_display, win, &windowName, &iconName, NULL /*argv*/,
                     0 /*argc*/, &size_hints, &wm_hints, &class_hints);
    XFree(windowName.value);
    XFree(iconName.value);

    Atom wm_delete = XInternAtom(bx_x_display, "WM_DELETE_WINDOW", 1);
    XSetWMProtocols(bx_x_display, win, &wm_delete, 1);
  }

  /* Select event types wanted */
  XSelectInput(bx_x_display, win,
               ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                   ButtonReleaseMask | StructureNotifyMask | PointerMotionMask |
                   EnterWindowMask | LeaveWindowMask);

  /* Create default Graphics Context */
  gc = XCreateGC(bx_x_display, win, valuemask, &values);
  gc_inv = XCreateGC(bx_x_display, win, valuemask, &values);

  XSetState(bx_x_display, gc, white_pixel, black_pixel, GXcopy, AllPlanes);

  XSetState(bx_x_display, gc_inv, black_pixel, white_pixel, GXinvert,
            AllPlanes);

  /* Display window */
  XMapWindow(bx_x_display, win);
  XSync(bx_x_display, /* no discard */ 0);

  BX_DEBUG(("waiting for MapNotify"));
  while (1) {
    XNextEvent(bx_x_display, &report);
    if (report.type == MapNotify)
      break;
  }

  BX_DEBUG(("MapNotify found."));

  // Create the VGA font
  create_internal_vga_font();
  {
    char *imagedata;

    ximage = XCreateImage(bx_x_display, default_visual,
                          default_depth,          // depth of image (bitplanes)
                          ZPixmap, 0,             // offset
                          NULL,                   // malloc() space after
                          x_tilesize, y_tilesize, // x & y size of image
                          32,                     // # bits of padding
                          0); // bytes_per_line, let X11 calculate
    if (!ximage)
      BX_PANIC(("vga: couldn't XCreateImage()"));

    imDepth = default_depth;
    imWide = ximage->bytes_per_line;
    imBPP = ximage->bits_per_pixel;

    imagedata = (char *)malloc((size_t)(ximage->bytes_per_line * y_tilesize));
    if (!imagedata)
      BX_PANIC(("imagedata: malloc returned error"));

    ximage->data = imagedata;

    if (imBPP < imDepth) {
      BX_PANIC(("vga_x: bits_per_pixel < depth ?"));
    }

    x_init_done = true;
  }

  curr_background = 0;
  XSetBackground(bx_x_display, gc, col_vals[curr_background]);
  curr_foreground = 1;
  XSetForeground(bx_x_display, gc, col_vals[curr_foreground]);

  // XGrabPointer( bx_x_display, win, True, 0, GrabModeAsync, GrabModeAsync,
  //  win, None, CurrentTime );
  XFlush(bx_x_display);

  // loads keymap for x11
  x_keymapping = myCfg->get_bool_value("keyboard.use_mapping", false);
  if (x_keymapping) {

    // if (SIM->get_param_bool(BXPN_KBD_USEMAPPING)->get()) {
    bx_keymap->loadKeymap(convertStringToXKeysym);
  }

  new_gfx_api = 1;
}

// This is called whenever the mouse_enabled parameter changes.  It
// can change because of a gui event such as clicking on the mouse-enable
// bitmap or pressing the middle button, or from the configuration interface.

// In all those cases, setting the parameter value will get you here.
void bx_x11_gui_c::mouse_enabled_changed_specific(bool val) {
  BX_DEBUG(("mouse_enabled=%d, x11 specific code", val ? 1 : 0));
  if (val) {
    BX_INFO(("[x] Mouse on"));
    mouse_enable_x = current_x;
    mouse_enable_y = current_y;
    disable_cursor();

    // Move the cursor to a 'safe' place
    warp_cursor(warp_home_x - current_x, warp_home_y - current_y);
  } else {
    BX_INFO(("[x] Mouse off"));
    enable_cursor();
    warp_cursor(mouse_enable_x - current_x, mouse_enable_y - current_y);
  }
}

/**
 * Create a bitmap for VGA font data
 **/
void create_internal_vga_font(void) {

  // Default values
  font_width = 8;
  font_height = 16;

  for (int i = 0; i < 256; i++) {
    vgafont[i] = XCreateBitmapFromData(bx_x_display, win,
                                       (const char *)bx_vgafont[i].data,
                                       font_width, font_height);
    if (vgafont[i] == None)
      BX_PANIC(("Can't create vga font [%d]", i));
  }
}

void bx_x11_gui_c::handle_events(void) {
  XEvent report;
  XKeyEvent *key_event;
  KeySym keysym;
  XComposeStatus compose;
  char buffer[MAX_MAPPED_STRING_LENGTH];
  int bufsize = MAX_MAPPED_STRING_LENGTH;
  int charcount;
  bool mouse_update;
  int y;
  int height;

  XPointerMovedEvent *pointer_event;
  XEnterWindowEvent *enter_event;
  XLeaveWindowEvent *leave_event;
  XButtonEvent *button_event;
  XExposeEvent *expose_event;

  // current_x = -1;
  // current_y = -1;
  mouse_update = 0;

  while (XPending(bx_x_display) > 0) {
    XNextEvent(bx_x_display, &report);
    current_z = 0;
    switch (report.type) {
    case Expose:
      expose_event = &report.xexpose;

      /* Adjust y.*/
      y = expose_event->y;
      height = expose_event->height;
      if (y < 0) {
        height += y;
        y = 0;
      }

      theVGA->redraw_area((unsigned)expose_event->x, y,
                          (unsigned)expose_event->width, height);

      break;

    case ConfigureNotify:
      BX_DEBUG(("ConfigureNotify Xevent"));
      break;

    case ButtonPress:
      button_event = (XButtonEvent *)&report;
      BX_DEBUG(("xxx: buttonpress"));
      current_x = button_event->x;
      current_y = button_event->y;
      mouse_update = 1;
      BX_DEBUG(("xxx:   x,y=(%d,%d)", current_x, current_y));
      switch (button_event->button) {
      case Button1:
        mouse_button_state |= 0x01;
        send_keyboard_mouse_status();
        mouse_update = 0;
        break;

      case Button2:
        if (CTRL_pressed) {

          //            toggle_mouse_enable();
        } else {
          mouse_button_state |= 0x04;
          send_keyboard_mouse_status();
          mouse_update = 0;
        }
        break;

      case Button3:
        mouse_button_state |= 0x02;
        send_keyboard_mouse_status();
        mouse_update = 0;
        break;
      }
      break;

    case ButtonRelease:
      button_event = (XButtonEvent *)&report;
      current_x = button_event->x;
      current_y = button_event->y;
      mouse_update = 1;
      switch (button_event->button) {
      case Button1:
        mouse_button_state &= ~0x01;
        send_keyboard_mouse_status();
        mouse_update = 0;
        break;

      case Button2:
        mouse_button_state &= ~0x04;
        send_keyboard_mouse_status();
        mouse_update = 0;
        break;

      case Button3:
        mouse_button_state &= ~0x02;
        send_keyboard_mouse_status();
        mouse_update = 0;
        break;

      case Button4:
        current_z = 1;
        send_keyboard_mouse_status();
        mouse_update = 0;
        break;

      case Button5:
        current_z = -1;
        send_keyboard_mouse_status();
        mouse_update = 0;
        break;
      }
      break;

    case KeyPress:
      key_event = (XKeyEvent *)&report;
      charcount = XLookupString(key_event, buffer, bufsize, &keysym, &compose);
      xkeypress(keysym, 0);
      break;

    case KeyRelease:
      key_event = (XKeyEvent *)&report;
      charcount = XLookupString(key_event, buffer, bufsize, &keysym, &compose);
      xkeypress(keysym, 1);
      break;

    case MotionNotify:
      pointer_event = (XPointerMovedEvent *)&report;
      current_x = pointer_event->x;
      current_y = pointer_event->y;
      mouse_update = 1;
      break;

    case EnterNotify:
      enter_event = (XEnterWindowEvent *)&report;
      prev_x = current_x = enter_event->x;
      prev_y = current_y = enter_event->y;
      break;

    case LeaveNotify:
      leave_event = (XLeaveWindowEvent *)&report;
      prev_x = current_x = -1;
      prev_y = current_y = -1;
      break;

    case MapNotify:

      /* screen needs redraw, since X would have tossed previous
       * requests before window mapped
       */

      // retval = 1;
      break;

    case ClientMessage:
      if (!strcmp(XGetAtomName(bx_x_display, report.xclient.message_type),
                  "WM_PROTOCOLS")) {
        FAILURE(Graceful, "Emulator stopped from X");

        // bx_stop_simulation();
      }
      break;

    default:

      // (mch) Ignore...
      BX_DEBUG(("XXX: default Xevent type"));

      /* all events selected by StructureNotifyMask are thrown away here,
       * since nothing is done with them */
      break;
    } /* end switch */
  }   /* end while */

  if (mouse_update) {
    BX_DEBUG(("handle_events(): send mouse status"));
    send_keyboard_mouse_status();
  }
}

void send_keyboard_mouse_status(void) {
  BX_DEBUG(
      ("XXX: prev=(%d,%d) curr=(%d,%d)", prev_x, prev_y, current_x, current_y));

  if (((prev_x != -1) && (current_x != -1) && (prev_y != -1) &&
       (current_y != -1)) ||
      (current_z != 0)) {
    int dx;

    int dy;

    int dz;

    // (mch) consider warping here
    dx = current_x - prev_x - warp_dx;
    dy = -(current_y - prev_y - warp_dy);
    dz = current_z;
    warp_cursor(warp_home_x - current_x, warp_home_y - current_y);

    // DEV_mouse_motion_ext (dx, dy, dz, mouse_button_state);
    prev_x = current_x;
    prev_y = current_y;
  } else {
    if ((current_x != -1) && (current_y != -1)) {
      prev_x = current_x;
      prev_y = current_y;
    } else {
      prev_x = current_x = -1;
      prev_y = current_y = -1;
    }
  }
}

void bx_x11_gui_c::flush(void) {
  if (bx_x_display)
    XFlush(bx_x_display);
}

void xkeypress(KeySym keysym, int press_release) {
  u32 key_event;

  if ((keysym == XK_Control_L) || (keysym == XK_Control_R)) {
    CTRL_pressed = !press_release;
  }

  /* Old (no mapping) behavior */
  if (!x_keymapping) {

    // if (!SIM->get_param_bool(BXPN_KBD_USEMAPPING)->get()) {
    // this depends on the fact that the X11 keysyms which
    // correspond to the ascii characters space .. tilde
    // are in consequtive order.
    if ((keysym >= XK_space) && (keysym <= XK_asciitilde)) {
      key_event = ascii_to_key_event[keysym - XK_space];
    } else {
      switch (keysym) {
      case XK_KP_1:
#ifdef XK_KP_End

      case XK_KP_End:
#endif
        key_event = BX_KEY_KP_END;
        break;

      case XK_KP_2:
#ifdef XK_KP_Down

      case XK_KP_Down:
#endif
        key_event = BX_KEY_KP_DOWN;
        break;

      case XK_KP_3:
#ifdef XK_KP_Page_Down

      case XK_KP_Page_Down:
#endif
        key_event = BX_KEY_KP_PAGE_DOWN;
        break;

      case XK_KP_4:
#ifdef XK_KP_Left

      case XK_KP_Left:
#endif
        key_event = BX_KEY_KP_LEFT;
        break;

      case XK_KP_5:
#ifdef XK_KP_Begin

      case XK_KP_Begin:
#endif
        key_event = BX_KEY_KP_5;
        break;

      case XK_KP_6:
#ifdef XK_KP_Right

      case XK_KP_Right:
#endif
        key_event = BX_KEY_KP_RIGHT;
        break;

      case XK_KP_7:
#ifdef XK_KP_Home

      case XK_KP_Home:
#endif
        key_event = BX_KEY_KP_HOME;
        break;

      case XK_KP_8:
#ifdef XK_KP_Up

      case XK_KP_Up:
#endif
        key_event = BX_KEY_KP_UP;
        break;

      case XK_KP_9:
#ifdef XK_KP_Page_Up

      case XK_KP_Page_Up:
#endif
        key_event = BX_KEY_KP_PAGE_UP;
        break;

      case XK_KP_0:
#ifdef XK_KP_Insert

      case XK_KP_Insert:
#endif
        key_event = BX_KEY_KP_INSERT;
        break;

      case XK_KP_Decimal:
#ifdef XK_KP_Delete

      case XK_KP_Delete:
#endif
        key_event = BX_KEY_KP_DELETE;
        break;

#ifdef XK_KP_Enter

      case XK_KP_Enter:
        key_event = BX_KEY_KP_ENTER;
        break;
#endif

      case XK_KP_Subtract:
        key_event = BX_KEY_KP_SUBTRACT;
        break;

      case XK_KP_Add:
        key_event = BX_KEY_KP_ADD;
        break;

      case XK_KP_Multiply:
        key_event = BX_KEY_KP_MULTIPLY;
        break;

      case XK_KP_Divide:
        key_event = BX_KEY_KP_DIVIDE;
        break;

      case XK_Up:
        key_event = BX_KEY_UP;
        break;

      case XK_Down:
        key_event = BX_KEY_DOWN;
        break;

      case XK_Left:
        key_event = BX_KEY_LEFT;
        break;

      case XK_Right:
        key_event = BX_KEY_RIGHT;
        break;

      case XK_Delete:
        key_event = BX_KEY_DELETE;
        break;

      case XK_BackSpace:
        key_event = BX_KEY_BACKSPACE;
        break;

      case XK_Tab:
        key_event = BX_KEY_TAB;
        break;
#ifdef XK_ISO_Left_Tab

      case XK_ISO_Left_Tab:
        key_event = BX_KEY_TAB;
        break;
#endif

      case XK_Return:
        key_event = BX_KEY_ENTER;
        break;

      case XK_Escape:
        key_event = BX_KEY_ESC;
        break;

      case XK_F1:
        key_event = BX_KEY_F1;
        break;

      case XK_F2:
        key_event = BX_KEY_F2;
        break;

      case XK_F3:
        key_event = BX_KEY_F3;
        break;

      case XK_F4:
        key_event = BX_KEY_F4;
        break;

      case XK_F5:
        key_event = BX_KEY_F5;
        break;

      case XK_F6:
        key_event = BX_KEY_F6;
        break;

      case XK_F7:
        key_event = BX_KEY_F7;
        break;

      case XK_F8:
        key_event = BX_KEY_F8;
        break;

      case XK_F9:
        key_event = BX_KEY_F9;
        break;

      case XK_F10:
        key_event = BX_KEY_F10;
        break;

      case XK_F11:
        key_event = BX_KEY_F11;
        break;

      case XK_F12:
        key_event = BX_KEY_F12;
        break;

      case XK_Control_L:
        key_event = BX_KEY_CTRL_L;
        break;
#ifdef XK_Control_R

      case XK_Control_R:
        key_event = BX_KEY_CTRL_R;
        break;
#endif

      case XK_Shift_L:
        key_event = BX_KEY_SHIFT_L;
        break;

      case XK_Shift_R:
        key_event = BX_KEY_SHIFT_R;
        break;

      case XK_Alt_L:
        key_event = BX_KEY_ALT_L;
        break;
#ifdef XK_Alt_R

      case XK_Alt_R:
        key_event = BX_KEY_ALT_R;
        break;
#endif

      case XK_Caps_Lock:
        key_event = BX_KEY_CAPS_LOCK;
        break;

      case XK_Num_Lock:
        key_event = BX_KEY_NUM_LOCK;
        break;
#ifdef XK_Scroll_Lock

      case XK_Scroll_Lock:
        key_event = BX_KEY_SCRL_LOCK;
        break;
#endif
#ifdef XK_Print

      case XK_Print:
        key_event = BX_KEY_PRINT;
        break;
#endif
#ifdef XK_Pause

      case XK_Pause:
        key_event = BX_KEY_PAUSE;
        break;
#endif

      case XK_Insert:
        key_event = BX_KEY_INSERT;
        break;

      case XK_Home:
        key_event = BX_KEY_HOME;
        break;

      case XK_End:
        key_event = BX_KEY_END;
        break;

      case XK_Page_Up:
        key_event = BX_KEY_PAGE_UP;
        break;

      case XK_Page_Down:
        key_event = BX_KEY_PAGE_DOWN;
        break;

      default:
        BX_ERROR(("xkeypress(): keysym %x unhandled!", (unsigned)keysym));
        return;
        break;
      }
    }
  } else {

    /* use mapping */
    BXKeyEntry *entry = bx_keymap->findHostKey(keysym);
    if (!entry) {
      BX_ERROR(("xkeypress(): keysym %x unhandled!", (unsigned)keysym));
      return;
    }

    key_event = entry->baseKey;
  }

  if (press_release)
    key_event |= BX_KEY_RELEASED;

  theKeyboard->gen_scancode(key_event);
}

void bx_x11_gui_c::clear_screen(void) {
  XClearArea(bx_x_display, win, 0, 0, dimension_x, dimension_y, 0);
}

void bx_x11_gui_c::text_update(u8 *old_text, u8 *new_text,
                               unsigned long cursor_x, unsigned long cursor_y,
                               bx_vga_tminfo_t tm_info, unsigned nrows) {
  u8 *old_line;

  u8 *new_line;

  u8 *text_base;
  u8 cChar;
  unsigned int curs;
  unsigned int hchars;
  unsigned int i;
  unsigned int j;
  unsigned int offset;
  unsigned int rows;
  unsigned int x;
  unsigned int y;
  unsigned int xc;
  unsigned int yc;
  unsigned int yc2;
  unsigned int cs_y;
  unsigned new_foreground;
  unsigned new_background;
  u8 cfwidth;
  u8 cfheight;
  u8 cfheight2;
  u8 font_col;
  u8 font_row;
  u8 font_row2;
  u8 split_textrow;
  u8 split_fontrows;
  bool forceUpdate = 0;
  bool split_screen;
  unsigned char cell[64];
  unsigned long text_palette[16];

  if (charmap_updated) {
    BX_INFO(("charmap update. Font Height is %d", font_height));
    for (unsigned c = 0; c < 256; c++) {
      if (char_changed[c]) {
        XFreePixmap(bx_x_display, vgafont[c]);

        bool gfxchar = tm_info.line_graphics && ((c & 0xE0) == 0xC0);
        j = 0;
        memset(cell, 0, sizeof(cell));
        for (i = 0; i < font_height * 2; i += 2) {
          cell[i] |= ((vga_charmap[(c << 5) + j] & 0x01) << 7);
          cell[i] |= ((vga_charmap[(c << 5) + j] & 0x02) << 5);
          cell[i] |= ((vga_charmap[(c << 5) + j] & 0x04) << 3);
          cell[i] |= ((vga_charmap[(c << 5) + j] & 0x08) << 1);
          cell[i] |= ((vga_charmap[(c << 5) + j] & 0x10) >> 1);
          cell[i] |= ((vga_charmap[(c << 5) + j] & 0x20) >> 3);
          cell[i] |= ((vga_charmap[(c << 5) + j] & 0x40) >> 5);
          cell[i] |= ((vga_charmap[(c << 5) + j] & 0x80) >> 7);
          if (gfxchar) {
            cell[i + 1] = (vga_charmap[(c << 5) + j] & 0x01);
          }

          j++;
        }

        vgafont[c] = XCreateBitmapFromData(bx_x_display, win,
                                           (const char *)cell, 9, font_height);
        if (vgafont[c] == None)
          BX_PANIC(("Can't create vga font [%d]", c));
        char_changed[c] = 0;
      }
    }

    forceUpdate = 1;
    charmap_updated = 0;
  }

  for (i = 0; i < 16; i++) {
    text_palette[i] = col_vals[theVGA->get_actl_palette_idx(i)];
  }

  if ((tm_info.h_panning != h_panning) || (tm_info.v_panning != v_panning)) {
    forceUpdate = 1;
    h_panning = tm_info.h_panning;
    v_panning = tm_info.v_panning;
  }

  if (tm_info.line_compare != line_compare) {
    forceUpdate = 1;
    line_compare = tm_info.line_compare;
  }

  // first invalidate character at previous and new cursor location
  if ((prev_cursor_y < text_rows) && (prev_cursor_x < text_cols)) {
    curs = prev_cursor_y * tm_info.line_offset + prev_cursor_x * 2;
    old_text[curs] = ~new_text[curs];
  }

  if ((tm_info.cs_start <= tm_info.cs_end) &&
      (tm_info.cs_start < font_height) && (cursor_y < text_rows) &&
      (cursor_x < text_cols)) {
    curs = cursor_y * tm_info.line_offset + cursor_x * 2;
    old_text[curs] = ~new_text[curs];
  } else {
    curs = 0xffff;
  }

  rows = text_rows;
  if (v_panning)
    rows++;
  y = 0;
  cs_y = 0;
  text_base = new_text - tm_info.start_address;
  split_textrow = (line_compare + v_panning) / font_height;
  split_fontrows = ((line_compare + v_panning) % font_height) + 1;
  split_screen = 0;
  do {
    hchars = text_cols;
    if (h_panning)
      hchars++;
    if (split_screen) {
      yc = line_compare + cs_y * font_height + 1;
      font_row = 0;
      if (rows == 1) {
        cfheight = (dimension_y - line_compare - 1) % font_height;
        if (cfheight == 0)
          cfheight = font_height;
      } else {
        cfheight = font_height;
      }
    } else if (v_panning) {
      if (y == 0) {
        yc = 0;
        font_row = v_panning;
        cfheight = font_height - v_panning;
      } else {
        yc = y * font_height - v_panning;
        font_row = 0;
        if (rows == 1) {
          cfheight = v_panning;
        } else {
          cfheight = font_height;
        }
      }
    } else {
      yc = y * font_height;
      font_row = 0;
      cfheight = font_height;
    }

    if (!split_screen && (y == split_textrow)) {
      if (split_fontrows < cfheight)
        cfheight = split_fontrows;
    }

    new_line = new_text;
    old_line = old_text;
    x = 0;
    offset = cs_y * tm_info.line_offset;
    do {
      if (h_panning) {
        if (hchars > text_cols) {
          xc = 0;
          font_col = h_panning;
          cfwidth = font_width - h_panning;
        } else {
          xc = x * font_width - h_panning;
          font_col = 0;
          if (hchars == 1) {
            cfwidth = h_panning;
          } else {
            cfwidth = font_width;
          }
        }
      } else {
        xc = x * font_width;
        font_col = 0;
        cfwidth = font_width;
      }

      if (forceUpdate || (old_text[0] != new_text[0]) ||
          (old_text[1] != new_text[1])) {
        cChar = new_text[0];
        new_foreground = new_text[1] & 0x0f;
        new_background = (new_text[1] & 0xf0) >> 4;

        XSetForeground(bx_x_display, gc, text_palette[new_foreground]);
        XSetBackground(bx_x_display, gc, text_palette[new_background]);

        XCopyPlane(bx_x_display, vgafont[cChar], win, gc, font_col, font_row,
                   cfwidth, cfheight, xc, yc, 1);
        if (offset == curs) {
          XSetForeground(bx_x_display, gc, text_palette[new_background]);
          XSetBackground(bx_x_display, gc, text_palette[new_foreground]);
          if (font_row == 0) {
            yc2 = yc + tm_info.cs_start;
            font_row2 = tm_info.cs_start;
            cfheight2 = tm_info.cs_end - tm_info.cs_start + 1;
            if ((yc2 + cfheight2) > (dimension_y)) {
              cfheight2 = dimension_y - yc2;
            }
          } else {
            if (v_panning > tm_info.cs_start) {
              yc2 = yc;
              font_row2 = font_row;
              cfheight2 = tm_info.cs_end - v_panning + 1;
            } else {
              yc2 = yc + tm_info.cs_start - v_panning;
              font_row2 = tm_info.cs_start;
              cfheight2 = tm_info.cs_end - tm_info.cs_start + 1;
            }
          }

          if (yc2 < (dimension_y)) {
            XCopyPlane(bx_x_display, vgafont[cChar], win, gc, font_col,
                       font_row2, cfwidth, cfheight2, xc, yc2, 1);
          }
        }
      }

      x++;
      new_text += 2;
      old_text += 2;
      offset += 2;
    } while (--hchars);
    if (!split_screen && (y == split_textrow)) {
      new_text = text_base;
      forceUpdate = 1;
      cs_y = 0;
      if (tm_info.split_hpanning)
        h_panning = 0;
      rows = ((dimension_y - line_compare + font_height - 2) / font_height) + 1;
      split_screen = 1;
    } else {
      y++;
      cs_y++;
      new_text = new_line + tm_info.line_offset;
      old_text = old_line + tm_info.line_offset;
    }
  } while (--rows);

  h_panning = tm_info.h_panning;
  prev_cursor_x = cursor_x;
  prev_cursor_y = cursor_y;

  XFlush(bx_x_display);
}

void bx_x11_gui_c::graphics_tile_update(u8 *tile, unsigned x0, unsigned y0) {
  unsigned x;

  unsigned y;

  unsigned y_size;
  unsigned color;
  unsigned offset;
  u8 b0;
  u8 b1;
  u8 b2;
  u8 b3;

  if ((y0 + y_tilesize) > dimension_y) {
    y_size = dimension_y - y0;
  } else {
    y_size = y_tilesize;
  }

  switch (vga_bpp) {
  case 8: // 8 bits per pixel
    for (y = 0; y < y_size; y++) {
      for (x = 0; x < x_tilesize; x++) {
        color = col_vals[tile[y * x_tilesize + x]];
        switch (imBPP) {
        case 8: // 8 bits per pixel
          ximage->data[imWide * y + x] = color;
          break;

        case 16: // 16 bits per pixel
          offset = imWide * y + 2 * x;
          b0 = color >> 0;
          b1 = color >> 8;
          if (ximage->byte_order == LSBFirst) {
            ximage->data[offset + 0] = b0;
            ximage->data[offset + 1] = b1;
          } else { // MSBFirst
            ximage->data[offset + 0] = b1;
            ximage->data[offset + 1] = b0;
          }
          break;

        case 24: // 24 bits per pixel
          offset = imWide * y + 3 * x;
          b0 = color >> 0;
          b1 = color >> 8;
          b2 = color >> 16;
          if (ximage->byte_order == LSBFirst) {
            ximage->data[offset + 0] = b0;
            ximage->data[offset + 1] = b1;
            ximage->data[offset + 2] = b2;
          } else { // MSBFirst
            ximage->data[offset + 0] = b2;
            ximage->data[offset + 1] = b1;
            ximage->data[offset + 2] = b0;
          }
          break;

        case 32: // 32 bits per pixel
          offset = imWide * y + 4 * x;
          b0 = color >> 0;
          b1 = color >> 8;
          b2 = color >> 16;
          b3 = color >> 24;
          if (ximage->byte_order == LSBFirst) {
            ximage->data[offset + 0] = b0;
            ximage->data[offset + 1] = b1;
            ximage->data[offset + 2] = b2;
            ximage->data[offset + 3] = b3;
          } else { // MSBFirst
            ximage->data[offset + 0] = b3;
            ximage->data[offset + 1] = b2;
            ximage->data[offset + 2] = b1;
            ximage->data[offset + 3] = b0;
          }
          break;

        default:
          BX_PANIC(("X_graphics_tile_update: bits_per_pixel %u not implemented",
                    (unsigned)imBPP));
          return;
        }
      }
    }
    break;

  default:
    BX_PANIC((
        "X_graphics_tile_update: bits_per_pixel %u handled by new graphics API",
        (unsigned)vga_bpp));
    return;
  }

  XPutImage(bx_x_display, win, gc, ximage, 0, 0, x0, y0, x_tilesize, y_size);
}

bx_svga_tileinfo_t *bx_x11_gui_c::graphics_tile_info(bx_svga_tileinfo_t *info) {
  if (!info) {
    info = (bx_svga_tileinfo_t *)malloc(sizeof(bx_svga_tileinfo_t));
    if (!info) {
      return NULL;
    }
  }

  info->bpp = ximage->bits_per_pixel;
  info->pitch = ximage->bytes_per_line;
  info->red_shift = 0;
  info->green_shift = 0;
  info->blue_shift = 0;
  info->red_mask = ximage->red_mask;
  info->green_mask = ximage->green_mask;
  info->blue_mask = ximage->blue_mask;

  int i;

  int rf;

  int gf;

  int bf;
  unsigned long red;
  unsigned long green;
  unsigned long blue;

  i = rf = gf = bf = 0;
  red = ximage->red_mask;
  green = ximage->green_mask;
  blue = ximage->blue_mask;

  while (red || rf || green || gf || blue || bf) {
    if (rf) {
      if (!(red & 1)) {
        info->red_shift = i;
        rf = 0;
      }
    } else {
      if (red & 1) {
        rf = 1;
      }
    }

    if (gf) {
      if (!(green & 1)) {
        info->green_shift = i;
        gf = 0;
      }
    } else {
      if (green & 1) {
        gf = 1;
      }
    }

    if (bf) {
      if (!(blue & 1)) {
        info->blue_shift = i;
        bf = 0;
      }
    } else {
      if (blue & 1) {
        bf = 1;
      }
    }

    i++;
    red >>= 1;
    green >>= 1;
    blue >>= 1;
  }

  info->is_indexed = (default_visual->c_class != TrueColor) &&
                     (default_visual->c_class != DirectColor);
  info->is_little_endian = (ximage->byte_order == LSBFirst);

  return info;
}

u8 *bx_x11_gui_c::graphics_tile_get(unsigned x0, unsigned y0, unsigned *w,
                                    unsigned *h) {
  if (x0 + x_tilesize > dimension_x) {
    *w = dimension_x - x0;
  } else {
    *w = x_tilesize;
  }

  if (y0 + y_tilesize > dimension_y) {
    *h = dimension_y - y0;
  } else {
    *h = y_tilesize;
  }

  return (u8 *)ximage->data + ximage->xoffset * ximage->bits_per_pixel / 8;
}

void bx_x11_gui_c::graphics_tile_update_in_place(unsigned x0, unsigned y0,
                                                 unsigned w, unsigned h) {
  XPutImage(bx_x_display, win, gc, ximage, 0, 0, x0, y0, w, h);
}

bool bx_x11_gui_c::palette_change(unsigned index, unsigned red, unsigned green,
                                  unsigned blue) {

  // returns: 0=no screen update needed (color map change has direct effect)
  //          1=screen updated needed (redraw using current colormap)
  XColor color;

  color.flags = DoRed | DoGreen | DoBlue;
  color.red = red << 8;
  color.green = green << 8;
  color.blue = blue << 8;

  if (x_private_colormap) {

    // if (SIM->get_param_bool(BXPN_PRIVATE_COLORMAP)->get()) {
    color.pixel = index;
    XStoreColor(bx_x_display, default_cmap, &color);
    return (0); // no screen update needed
  } else {
    XAllocColor(bx_x_display, DefaultColormap(bx_x_display, bx_x_screen_num),
                &color);
    col_vals[index] = color.pixel;
    return (1); // screen update needed
  }
}

void bx_x11_gui_c::dimension_update(unsigned x, unsigned y, unsigned fheight,
                                    unsigned fwidth, unsigned bpp) {
  if ((bpp == 8) || (bpp == 15) || (bpp == 16) || (bpp == 24) || (bpp == 32)) {
    vga_bpp = bpp;
  } else {
    BX_PANIC(("%d bpp graphics mode not supported", bpp));
  }

  if (fheight > 0) {
    font_height = fheight;
    font_width = fwidth;
    text_cols = x / font_width;
    text_rows = y / font_height;
  }

  if ((x != dimension_x) || (y != dimension_y)) {
    XSizeHints hints;
    long supplied_return;

    if (XGetWMNormalHints(bx_x_display, win, &hints, &supplied_return) &&
        supplied_return & PMaxSize) {
      hints.max_width = hints.min_width = x;
      hints.max_height = hints.min_height = y;
      XSetWMNormalHints(bx_x_display, win, &hints);
    }

    XResizeWindow(bx_x_display, win, x, y);
    dimension_x = x;
    dimension_y = y;
  }
}

void bx_x11_gui_c::exit(void) {
  if (!x_init_done)
    return;

  // Delete the font bitmaps
  for (int i = 0; i < 256; i++) {

    // if (vgafont[i] != NULL)
    XFreePixmap(bx_x_display, vgafont[i]);
  }

  if (bx_x_display)
    XCloseDisplay(bx_x_display);
  BX_INFO(("Exit."));
}

static void warp_cursor(int dx, int dy) {
  if (warp_dx || warp_dy || dx || dy) {
    warp_dx = dx;
    warp_dy = dy;
    XWarpPointer(bx_x_display, None, None, 0, 0, 0, 0, dx, dy);
  }
}

static void disable_cursor() {
  static Cursor cursor;
  static unsigned cursor_created = 0;

  static int shape_width = 16;
  static int shape_height = 16;
  static int mask_width = 16;
  static int mask_height = 16;

  static u32 shape_bits[(16 * 16) / 32] = {
      0x00000000, 0x00000000, 0x00000000, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000,
  };
  static u32 mask_bits[(16 * 16) / 32] = {
      0x00000000, 0x00000000, 0x00000000, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000,
  };

  if (!cursor_created) {
    Pixmap shape;

    Pixmap mask;
    XColor white;
    XColor black;
    shape = XCreatePixmapFromBitmapData(
        bx_x_display, RootWindow(bx_x_display, bx_x_screen_num),
        (char *)shape_bits, shape_width, shape_height, 1, 0, 1);
    mask = XCreatePixmapFromBitmapData(
        bx_x_display, RootWindow(bx_x_display, bx_x_screen_num),
        (char *)mask_bits, mask_width, mask_height, 1, 0, 1);
    XParseColor(bx_x_display, default_cmap, "black", &black);
    XParseColor(bx_x_display, default_cmap, "white", &white);
    cursor =
        XCreatePixmapCursor(bx_x_display, shape, mask, &white, &black, 1, 1);
    cursor_created = 1;
  }

  XDefineCursor(bx_x_display, win, cursor);
}

static void enable_cursor() { XUndefineCursor(bx_x_display, win); }

/* convertStringToXKeysym is a keymap callback
 * used when reading the keymap file.
 * It converts a Symblic String to a GUI Constant
 *
 * It returns a u32 constant or BX_KEYMAP_UNKNOWN if it fails
 */
static u32 convertStringToXKeysym(const char *string) {
  if (strncmp("XK_", string, 3) != 0)
    return BX_KEYMAP_UNKNOWN;

  KeySym keysym = XStringToKeysym(string + 3);

  // failure, return unknown
  if (keysym == NoSymbol)
    return BX_KEYMAP_UNKNOWN;

  return ((u32)keysym);
}

void bx_x11_gui_c::get_capabilities(u16 *xres, u16 *yres, u16 *bpp) {
  *xres = 1024;
  *yres = 768;
  *bpp = 32;
}
#endif /* if BX_WITH_X11 */
