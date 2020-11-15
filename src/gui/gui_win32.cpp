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
 *
 *  Much of this file was written by:
 *  David Ross
 *  dross@pobox.com
 */

/**
 * \file
 * Win32 GUI implementation. Allows use of gfx without SDL on WIndows
 *
 * $Id: gui_win32.cpp,v 1.8 2008/03/14 15:31:29 iamcamiel Exp $
 *
 * X-1.5        Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.4        Camiel Vanderhoeven                             12-FEB-2008
 *      Moved keyboard code into it's own class (CKeyboard)
 *
 * X-1.3        Camiel Vanderhoeven                             22-JAN-2008
 *      Commented out mousewheel-code, doesn't work with older win32
 *      versions.
 *
 * X-1.2        Camiel Vanderhoeven                             19-JAN-2008
 *      Changed window title.
 *
 * X-1.1        Camiel Vanderhoeven                             19-JAN-2008
 *      Initial version for ES40 emulator.
 *
 **/
#include "../StdAfx.hpp"

#if defined(_WIN32)
#include <process.h>

#include "../Configurator.hpp"
#include "../Keyboard.hpp"
#include "../VGA.hpp"
#include "gui_win32_font.hpp"
#include "keymap.hpp"

#include "gui.hpp"

class bx_win32_gui_c : public bx_gui_c {
public:
  bx_win32_gui_c(CConfigurator *cfg) {
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

  virtual void get_capabilities(u16 *xres, u16 *yres, u16 *bpp);

private:
  CConfigurator *myCfg;
};

// declare one instance of the gui object and call macro to insert the
// plugin code
static bx_win32_gui_c *theGui = NULL;
IMPLEMENT_GUI_PLUGIN_CODE(win32)
#define EXIT_GUI_SHUTDOWN 1
#define EXIT_GMH_FAILURE 2
#define EXIT_FONT_BITMAP_ERROR 3
#define EXIT_NORMAL 4
#define EXIT_HEADER_BITMAP_ERROR 5

// Keyboard/mouse stuff
#define SCANCODE_BUFSIZE 20
#define MOUSE_PRESSED 0x20000000
#define MOUSE_MOTION 0x22000000
#define BX_SYSKEY (KF_UP | KF_REPEAT | KF_ALTDOWN)
void enq_key_event(u32, u32);
void enq_mouse_event(void);

struct QueueEvent {
  u32 key_event;
  int mouse_x;
  int mouse_y;
  int mouse_z;
  int mouse_button_state;
};
QueueEvent *deq_key_event(void);

static QueueEvent keyevents[SCANCODE_BUFSIZE];
static unsigned head = 0, tail = 0;
static int mouse_button_state = 0;
static int ms_xdelta = 0, ms_ydelta = 0, ms_zdelta = 0;
static int ms_lastx = 0, ms_lasty = 0;
static int ms_savedx = 0, ms_savedy = 0;
static BOOL mouseCaptureMode, mouseCaptureNew, mouseToggleReq;
static unsigned long workerThread = 0;
static DWORD workerThreadID = 0;
static int mouse_buttons = 3;

// Graphics screen stuff
static unsigned x_tilesize = 0, y_tilesize = 0;
static BITMAPINFO *bitmap_info = (BITMAPINFO *)0;
static RGBQUAD *cmap_index; // indeces into system colormap
static HBITMAP MemoryBitmap = NULL;
static HDC MemoryDC = NULL;
static RECT updated_area;
static BOOL updated_area_valid = false;
static HWND desktopWindow;
static RECT desktop;
static BOOL queryFullScreen = false;
static int desktop_x, desktop_y;

// Text mode screen stuff
static unsigned prev_cursor_x = 0;
static unsigned prev_cursor_y = 0;
static HBITMAP vgafont[256];
static int xChar = 8, yChar = 16;
static unsigned int text_rows = 25, text_cols = 80;
static u8 text_pal_idx[16];

static u8 h_panning = 0, v_panning = 0;
static u16 line_compare = 1023;

// Misc stuff
static unsigned dimension_x, dimension_y, current_bpp;
static unsigned stretched_x, stretched_y;
static unsigned stretch_factor = 1;
static BOOL BxTextMode = true;
static BOOL fix_size = false;
static HWND hotKeyReceiver = NULL;
static HWND saveParent = NULL;

static char szAppName[] = "ES40 Emulator";
static char szWindowName[] = "ES40 Emulator";

typedef struct {
  HINSTANCE hInstance;

  CRITICAL_SECTION drawCS;
  CRITICAL_SECTION keyCS;
  CRITICAL_SECTION mouseCS;

  int kill; // reason for terminateEmul(int)
  BOOL UIinited;
  HWND mainWnd;
  HWND simWnd;
} sharedThreadInfo;

sharedThreadInfo stInfo;

LRESULT CALLBACK mainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK simWndProc(HWND, UINT, WPARAM, LPARAM);
VOID UIThread(PVOID);
void terminateEmul(int);
void create_vga_font(void);
static unsigned char reverse_bitorder(unsigned char);
void DrawBitmap(HDC, HBITMAP, int, int, int, int, int, int, DWORD,
                unsigned char);
void DrawChar(HDC, unsigned char, int, int, unsigned char cColor, int, int);
void updateUpdated(int, int, int, int);

u32 win32_to_bx_key[2][0x100] = {
    {
        /* normal-keys */
        /* 0x00 - 0x0f */
        0,
        BX_KEY_ESC,
        BX_KEY_1,
        BX_KEY_2,
        BX_KEY_3,
        BX_KEY_4,
        BX_KEY_5,
        BX_KEY_6,
        BX_KEY_7,
        BX_KEY_8,
        BX_KEY_9,
        BX_KEY_0,
        BX_KEY_MINUS,
        BX_KEY_EQUALS,
        BX_KEY_BACKSPACE,
        BX_KEY_TAB,

        /* 0x10 - 0x1f */
        BX_KEY_Q,
        BX_KEY_W,
        BX_KEY_E,
        BX_KEY_R,
        BX_KEY_T,
        BX_KEY_Y,
        BX_KEY_U,
        BX_KEY_I,
        BX_KEY_O,
        BX_KEY_P,
        BX_KEY_LEFT_BRACKET,
        BX_KEY_RIGHT_BRACKET,
        BX_KEY_ENTER,
        BX_KEY_CTRL_L,
        BX_KEY_A,
        BX_KEY_S,

        /* 0x20 - 0x2f */
        BX_KEY_D,
        BX_KEY_F,
        BX_KEY_G,
        BX_KEY_H,
        BX_KEY_J,
        BX_KEY_K,
        BX_KEY_L,
        BX_KEY_SEMICOLON,
        BX_KEY_SINGLE_QUOTE,
        BX_KEY_GRAVE,
        BX_KEY_SHIFT_L,
        BX_KEY_BACKSLASH,
        BX_KEY_Z,
        BX_KEY_X,
        BX_KEY_C,
        BX_KEY_V,

        /* 0x30 - 0x3f */
        BX_KEY_B,
        BX_KEY_N,
        BX_KEY_M,
        BX_KEY_COMMA,
        BX_KEY_PERIOD,
        BX_KEY_SLASH,
        BX_KEY_SHIFT_R,
        BX_KEY_KP_MULTIPLY,
        BX_KEY_ALT_L,
        BX_KEY_SPACE,
        BX_KEY_CAPS_LOCK,
        BX_KEY_F1,
        BX_KEY_F2,
        BX_KEY_F3,
        BX_KEY_F4,
        BX_KEY_F5,

        /* 0x40 - 0x4f */
        BX_KEY_F6,
        BX_KEY_F7,
        BX_KEY_F8,
        BX_KEY_F9,
        BX_KEY_F10,
        BX_KEY_PAUSE,
        BX_KEY_SCRL_LOCK,
        BX_KEY_KP_HOME,
        BX_KEY_KP_UP,
        BX_KEY_KP_PAGE_UP,
        BX_KEY_KP_SUBTRACT,
        BX_KEY_KP_LEFT,
        BX_KEY_KP_5,
        BX_KEY_KP_RIGHT,
        BX_KEY_KP_ADD,
        BX_KEY_KP_END,

        /* 0x50 - 0x5f */
        BX_KEY_KP_DOWN,
        BX_KEY_KP_PAGE_DOWN,
        BX_KEY_KP_INSERT,
        BX_KEY_KP_DELETE,
        0,
        0,
        BX_KEY_LEFT_BACKSLASH,
        BX_KEY_F11,
        BX_KEY_F12,
        0,
        0,
        0,
        0,
        0,
        0,
        0,

        /* 0x60 - 0x6f */
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,

        /* 0x70 - 0x7f */
        0, /* Todo: "Katakana" key ( ibm 133 ) for Japanese 106 keyboard */
        0,
        0,
        0, /* Todo: "Ro" key ( ibm 56 ) for Japanese 106 keyboard */
        0,
        0,
        0,
        0,
        0,
        0, /* Todo: "convert" key ( ibm 132 ) for Japanese 106 keyboard */
        0,
        0, /* Todo: "non-convert" key ( ibm 131 ) for Japanese 106 keyboard */
        0,
        0, /* Todo: "Yen" key ( ibm 14 ) for Japanese 106 keyboard */
        0,
        0,
    },
    {
        /* extended-keys */
        /* 0x00 - 0x0f */
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,

        /* 0x10 - 0x1f */
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        BX_KEY_KP_ENTER,
        BX_KEY_CTRL_R,
        0,
        0,

        /* 0x20 - 0x2f */
        0,
        BX_KEY_POWER_CALC,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,

        /* 0x30 - 0x3f */
        0,
        0,
        BX_KEY_INT_HOME,
        0,
        0,
        BX_KEY_KP_DIVIDE,
        0,
        BX_KEY_PRINT,
        BX_KEY_ALT_R,
        0,
        0,
        0,
        0,
        0,
        0,
        0,

        /* 0x40 - 0x4f */
        0,
        0,
        0,
        0,
        0,
        BX_KEY_NUM_LOCK,
        0,
        BX_KEY_HOME,
        BX_KEY_UP,
        BX_KEY_PAGE_UP,
        0,
        BX_KEY_LEFT,
        0,
        BX_KEY_RIGHT,
        0,
        BX_KEY_END,

        /* 0x50 - 0x5f */
        BX_KEY_DOWN,
        BX_KEY_PAGE_DOWN,
        BX_KEY_INSERT,
        BX_KEY_DELETE,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        BX_KEY_WIN_L,
        BX_KEY_WIN_R,
        BX_KEY_MENU,
        BX_KEY_POWER_POWER,
        BX_KEY_POWER_SLEEP,

        /* 0x60 - 0x6f */
        0,
        0,
        0,
        BX_KEY_POWER_WAKE,
        0,
        BX_KEY_INT_SEARCH,
        BX_KEY_INT_FAV,
        0,
        BX_KEY_INT_STOP,
        BX_KEY_INT_FORWARD,
        BX_KEY_INT_BACK,
        BX_KEY_POWER_MYCOMP,
        BX_KEY_INT_MAIL,
        0,
        0,
        0,
    }};

/* Macro to convert WM_ button state to BX button state */
#if defined(__MINGW32__) || defined(_MSC_VER)
VOID CALLBACK MyTimer(HWND, UINT, UINT, DWORD);
void alarm(int);
void bx_signal_handler(int);
#endif
static void processMouseXY(int x, int y, int z, int windows_state,
                           int implied_state_change) {
  int bx_state;
  int old_bx_state;
  EnterCriticalSection(&stInfo.mouseCS);
  bx_state = ((windows_state & MK_LBUTTON) ? 1 : 0) +
             ((windows_state & MK_RBUTTON) ? 2 : 0) +
             ((windows_state & MK_MBUTTON) ? 4 : 0);
  old_bx_state = bx_state ^ implied_state_change;
  if (old_bx_state != mouse_button_state) {

    /* Make up for missing message */
    BX_INFO(("&&&missing mouse state change"));
    EnterCriticalSection(&stInfo.keyCS);
    enq_mouse_event();
    mouse_button_state = old_bx_state;
    enq_key_event(mouse_button_state, MOUSE_PRESSED);
    LeaveCriticalSection(&stInfo.keyCS);
  }

  ms_ydelta = ms_savedy - y;
  ms_xdelta = x - ms_savedx;
  ms_zdelta = z;
  ms_lastx = x;
  ms_lasty = y;
  if (bx_state != mouse_button_state) {
    EnterCriticalSection(&stInfo.keyCS);
    enq_mouse_event();
    mouse_button_state = bx_state;
    enq_key_event(mouse_button_state, MOUSE_PRESSED);
    LeaveCriticalSection(&stInfo.keyCS);
  }

  LeaveCriticalSection(&stInfo.mouseCS);
}

static void resetDelta() {
  EnterCriticalSection(&stInfo.mouseCS);
  ms_savedx = ms_lastx;
  ms_savedy = ms_lasty;
  ms_ydelta = ms_xdelta = ms_zdelta = 0;
  LeaveCriticalSection(&stInfo.mouseCS);
}

static void cursorWarped() {
  EnterCriticalSection(&stInfo.mouseCS);
  EnterCriticalSection(&stInfo.keyCS);
  enq_mouse_event();
  LeaveCriticalSection(&stInfo.keyCS);
  ms_lastx = stretched_x / 2;
  ms_lasty = stretched_y / 2;
  ms_savedx = ms_lastx;
  ms_savedy = ms_lasty;
  LeaveCriticalSection(&stInfo.mouseCS);
}

// GUI thread must be dead/done in order to call terminateEmul
void terminateEmul(int reason) {

  // We know that Critical Sections were inited when x_tilesize has been set
  // See bx_win32_gui_c::specific_init
  if (x_tilesize != 0) {
    DeleteCriticalSection(&stInfo.drawCS);
    DeleteCriticalSection(&stInfo.keyCS);
    DeleteCriticalSection(&stInfo.mouseCS);
  }

  x_tilesize = 0;

  if (MemoryDC)
    DeleteDC(MemoryDC);
  if (MemoryBitmap)
    DeleteObject(MemoryBitmap);

  if (bitmap_info)
    delete[](char *) bitmap_info;

  for (unsigned c = 0; c < 256; c++)
    if (vgafont[c])
      DeleteObject(vgafont[c]);

  switch (reason) {
  case EXIT_GUI_SHUTDOWN:
    FAILURE(Graceful, "Window closed, exiting!");
    break;

  case EXIT_GMH_FAILURE:
    FAILURE(Win32, "GetModuleHandle failure!");
    break;

  case EXIT_FONT_BITMAP_ERROR:
    FAILURE(Win32, "Font bitmap creation failure!");
    break;

  case EXIT_NORMAL:
    break;
  }
}

// ::SPECIFIC_INIT()
//
// Called from gui.cc, once upon program startup, to allow for the
// specific GUI code (X11, BeOS, ...) to be initialized.
//
// tilewidth, tileheight: for optimization, graphics_tile_update() passes
//     only updated regions of the screen to the gui code to be redrawn.

//     These define the dimensions of a region (tile).
void bx_win32_gui_c::specific_init(unsigned tilewidth, unsigned tileheight) {
  int i;

  // prepare for possible fullscreen mode
  desktopWindow = GetDesktopWindow();
  GetWindowRect(desktopWindow, &desktop);
  desktop_x = desktop.right - desktop.left;
  desktop_y = desktop.bottom - desktop.top;
  hotKeyReceiver = stInfo.simWnd;
  BX_INFO(("Desktop Window dimensions: %d x %d", desktop_x, desktop_y));

  static RGBQUAD black_quad = {0, 0, 0, 0};
  stInfo.kill = 0;
  stInfo.UIinited = false;
  InitializeCriticalSection(&stInfo.drawCS);
  InitializeCriticalSection(&stInfo.keyCS);
  InitializeCriticalSection(&stInfo.mouseCS);

  x_tilesize = tilewidth;
  y_tilesize = tileheight;

  mouseCaptureMode = false;
  mouseCaptureNew = false;
  mouseToggleReq = false;

  mouse_buttons = GetSystemMetrics(SM_CMOUSEBUTTONS);
  BX_INFO(("Number of Mouse Buttons = %d", mouse_buttons));

  stInfo.hInstance = GetModuleHandle(NULL);

  dimension_x = 640;
  dimension_y = 480;
  current_bpp = 8;
  stretched_x = dimension_x;
  stretched_y = dimension_y;
  stretch_factor = 1;

  for (unsigned c = 0; c < 256; c++)
    vgafont[c] = NULL;
  create_vga_font();

  bitmap_info =
      (BITMAPINFO
           *)new char[sizeof(BITMAPINFOHEADER) +
                      259 * sizeof(RGBQUAD)]; // 256 + 3 entries for 16 bpp mode
  bitmap_info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info->bmiHeader.biWidth = x_tilesize;

  // Height is negative for top-down bitmap
  bitmap_info->bmiHeader.biHeight = -(LONG)y_tilesize;
  bitmap_info->bmiHeader.biPlanes = 1;
  bitmap_info->bmiHeader.biBitCount = 8;
  bitmap_info->bmiHeader.biCompression = BI_RGB;
  bitmap_info->bmiHeader.biSizeImage = x_tilesize * y_tilesize * 4;

  // I think these next two figures don't matter; saying 45 pixels/centimeter
  bitmap_info->bmiHeader.biXPelsPerMeter = 4500;
  bitmap_info->bmiHeader.biYPelsPerMeter = 4500;
  bitmap_info->bmiHeader.biClrUsed = 256;
  bitmap_info->bmiHeader.biClrImportant = 0;
  cmap_index = bitmap_info->bmiColors;

  // start out with all color map indeces pointing to Black
  cmap_index[0] = black_quad;
  for (i = 1; i < 259; i++) {
    cmap_index[i] = cmap_index[0];
  }

  if (stInfo.hInstance)
    workerThread = _beginthread(UIThread, 0, NULL);
  else
    terminateEmul(EXIT_GMH_FAILURE);

  // Wait for a window before continuing
  if ((stInfo.kill == 0) && (FindWindow(szAppName, NULL) == NULL))
    Sleep(500);

  // Now set this thread's priority to below normal because this is where
  //  the emulated CPU runs, and it hogs the real CPU
  // SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
  // load keymap tables
  if (myCfg->get_bool_value("keyboard.use_mapping")) {
    bx_keymap->loadKeymap(
        NULL); // I have no function to convert X windows symbols
  }
}

void resize_main_window() {
  RECT R;
  unsigned long mainStyle;

  // stretched_x and stretched_y were set in dimension_update()
  // if we need to do any additional resizing, do it now
  if ((desktop_y > 0) && (stretched_y >= (unsigned)desktop_y)) {
    if (!queryFullScreen) {
      MessageBox(NULL, "Going into fullscreen mode -- Alt-Enter to revert",
                 "Going fullscreen", MB_APPLMODAL);
      queryFullScreen = true;
    }

    // hide title bar
    mainStyle = GetWindowLong(stInfo.mainWnd, GWL_STYLE);
    mainStyle &= ~(WS_CAPTION | WS_BORDER);
    SetWindowLong(stInfo.mainWnd, GWL_STYLE, mainStyle);

    // maybe need to adjust stInfo.simWnd here also?
    if (saveParent = SetParent(stInfo.mainWnd, desktopWindow)) {
      BX_DEBUG(("Saved parent window"));
      SetWindowPos(stInfo.mainWnd, HWND_TOPMOST, desktop.left, desktop.top,
                   desktop.right, desktop.bottom, SWP_SHOWWINDOW);
    }
  } else {
    if (saveParent) {
      BX_DEBUG(("Restoring parent window"));
      SetParent(stInfo.mainWnd, saveParent);
      saveParent = NULL;
    }

    // put back the title bar, border, etc...
    mainStyle = GetWindowLong(stInfo.mainWnd, GWL_STYLE);
    mainStyle |= WS_CAPTION | WS_BORDER;
    SetWindowLong(stInfo.mainWnd, GWL_STYLE, mainStyle);
    SetRect(&R, 0, 0, stretched_x, stretched_y);

    DWORD style = GetWindowLong(stInfo.simWnd, GWL_STYLE);
    DWORD exstyle = GetWindowLong(stInfo.simWnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&R, style, false, exstyle);
    style = GetWindowLong(stInfo.mainWnd, GWL_STYLE);
    AdjustWindowRect(&R, style, false);
    SetWindowPos(stInfo.mainWnd, HWND_TOP, 0, 0, R.right - R.left,
                 R.bottom - R.top, SWP_NOMOVE | SWP_NOZORDER);
  }

  fix_size = false;
}

// This thread controls the GUI window.
VOID UIThread(PVOID pvoid) {
  MSG msg;
  HDC hdc;
  WNDCLASS wndclass;
  RECT wndRect;

  workerThreadID = GetCurrentThreadId();

  GetClassInfo(NULL, WC_DIALOG, &wndclass);
  wndclass.style = CS_HREDRAW | CS_VREDRAW;
  wndclass.lpfnWndProc = mainWndProc;
  wndclass.cbClsExtra = 0;
  wndclass.cbWndExtra = 0;
  wndclass.hInstance = stInfo.hInstance;
  wndclass.lpszMenuName = NULL;
  wndclass.lpszClassName = szAppName;

  RegisterClass(&wndclass);

  wndclass.style = CS_HREDRAW | CS_VREDRAW;
  wndclass.lpfnWndProc = simWndProc;
  wndclass.cbClsExtra = 0;
  wndclass.cbWndExtra = 0;
  wndclass.hInstance = stInfo.hInstance;
  wndclass.hIcon = NULL;
  wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
  wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  wndclass.lpszMenuName = NULL;
  wndclass.lpszClassName = "SIMWINDOW";

  RegisterClass(&wndclass);

  SetRect(&wndRect, 0, 0, stretched_x, stretched_y);

  DWORD sim_style = WS_CHILD;
  DWORD sim_exstyle = WS_EX_CLIENTEDGE;
  AdjustWindowRectEx(&wndRect, sim_style, false, sim_exstyle);

  DWORD main_style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  AdjustWindowRect(&wndRect, main_style, false);
  stInfo.mainWnd = CreateWindow(
      szAppName, szWindowName, main_style, CW_USEDEFAULT, CW_USEDEFAULT,
      wndRect.right - wndRect.left, wndRect.bottom - wndRect.top, NULL, NULL,
      stInfo.hInstance, NULL);

  if (stInfo.mainWnd) {

    //    InitCommonControls();
    stInfo.simWnd =
        CreateWindowEx(sim_exstyle, "SIMWINDOW", "", sim_style, 0, 0, 0, 0,
                       stInfo.mainWnd, NULL, stInfo.hInstance, NULL);

    /* needed for the Japanese versions of Windows */
    if (stInfo.simWnd) {
      HMODULE hm;
      hm = GetModuleHandle("USER32");
      if (hm) {
        BOOL(WINAPI * enableime)(HWND, BOOL);
        enableime =
            (BOOL(WINAPI *)(HWND, BOOL))GetProcAddress(hm, "WINNLSEnableIME");
        if (enableime) {
          enableime(stInfo.simWnd, false);
          BX_INFO(("IME disabled"));
        }
      }
    }

    ShowWindow(stInfo.simWnd, SW_SHOW);
    SetFocus(stInfo.simWnd);

    ShowCursor(!mouseCaptureMode);

    POINT pt = {0, 0};
    ClientToScreen(stInfo.simWnd, &pt);
    SetCursorPos(pt.x + stretched_x / 2, pt.y + stretched_y / 2);
    cursorWarped();

    hdc = GetDC(stInfo.simWnd);
    MemoryBitmap = CreateCompatibleBitmap(hdc, BX_MAX_XRES, BX_MAX_YRES);
    MemoryDC = CreateCompatibleDC(hdc);
    ReleaseDC(stInfo.simWnd, hdc);

    if (MemoryBitmap && MemoryDC) {
      resize_main_window();
      ShowWindow(stInfo.mainWnd, SW_SHOW);
#if BX_DEBUGGER
      if (windebug) {
        InitDebugDialog(stInfo.mainWnd);
      }
#endif
      stInfo.UIinited = true;

      bx_gui->clear_screen();

      while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  }

  stInfo.kill = EXIT_GUI_SHUTDOWN;

  _endthread();
}

LRESULT CALLBACK mainWndProc(HWND hwnd, UINT iMsg, WPARAM wParam,
                             LPARAM lParam) {
  switch (iMsg) {
  case WM_SETFOCUS:
    SetFocus(stInfo.simWnd);
    return 0;

  case WM_CLOSE:
    SendMessage(stInfo.simWnd, WM_CLOSE, 0, 0);
    break;

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;

  case WM_SIZE: {
    int x;

    int y;

    // now fit simWindow to mainWindow
    //      int rect_data[] = { 1, 0, 0, 0 };
    RECT R;

    // GetEffectiveClientRect( hwnd, &R, rect_data );
    GetClientRect(hwnd, &R);
    x = R.right - R.left;
    y = R.bottom - R.top;
    MoveWindow(stInfo.simWnd, R.left, R.top, x, y, true);
    GetClientRect(stInfo.simWnd, &R);
    x = R.right - R.left;
    y = R.bottom - R.top;
    if ((x != (int)stretched_x) || (y != (int)stretched_y)) {
      BX_ERROR(("Sim client size(%d, %d) != stretched size(%d, %d)!", x, y,
                stretched_x, stretched_y));
      if (!saveParent)
        fix_size = true; // no fixing if fullscreen
    }
  } break;
  }

  return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

void SetMouseCapture() {
  POINT pt = {0, 0};

  if (mouseToggleReq) {
    mouseCaptureMode = mouseCaptureNew;
    mouseToggleReq = false;
  } else {

    //  SIM->get_param_bool(BXPN_MOUSE_ENABLED)->set(mouseCaptureMode);
  }

  ShowCursor(!mouseCaptureMode);
  ShowCursor(!mouseCaptureMode); // somehow one didn't do the trick (win98)
  ClientToScreen(stInfo.simWnd, &pt);
  SetCursorPos(pt.x + stretched_x / 2, pt.y + stretched_y / 2);
  cursorWarped();
}

LRESULT CALLBACK simWndProc(HWND hwnd, UINT iMsg, WPARAM wParam,
                            LPARAM lParam) {
  HDC hdc;

  HDC hdcMem;
  PAINTSTRUCT ps;
  POINT pt;
  static BOOL mouseModeChange = false;

  switch (iMsg) {
  case WM_CREATE:
#if BX_USE_WINDOWS_FONTS
    InitFont();
#endif
    SetTimer(hwnd, 1, 330, NULL);
    return 0;

  case WM_TIMER:
    if (mouseToggleReq && (GetActiveWindow() == stInfo.mainWnd)) {
      SetMouseCapture();
    }

    // If mouse escaped, bring it back
    if (mouseCaptureMode) {
      pt.x = 0;
      pt.y = 0;
      ClientToScreen(hwnd, &pt);
      SetCursorPos(pt.x + stretched_x / 2, pt.y + stretched_y / 2);
      cursorWarped();
    }

    if (fix_size) {
      resize_main_window();
    }

    return 0;

  case WM_PAINT:
    EnterCriticalSection(&stInfo.drawCS);
    hdc = BeginPaint(hwnd, &ps);

    hdcMem = CreateCompatibleDC(hdc);
    SelectObject(hdcMem, MemoryBitmap);

    if (stretch_factor == 1) {
      BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
             ps.rcPaint.right - ps.rcPaint.left + 1,
             ps.rcPaint.bottom - ps.rcPaint.top + 1, hdcMem, ps.rcPaint.left,
             ps.rcPaint.top, SRCCOPY);
    } else {
      StretchBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
                 ps.rcPaint.right - ps.rcPaint.left + 1,
                 ps.rcPaint.bottom - ps.rcPaint.top + 1, hdcMem,
                 ps.rcPaint.left / stretch_factor, ps.rcPaint.top,
                 (ps.rcPaint.right - ps.rcPaint.left + 1) / stretch_factor,
                 (ps.rcPaint.bottom - ps.rcPaint.top + 1), SRCCOPY);
    }

    DeleteDC(hdcMem);
    EndPaint(hwnd, &ps);
    LeaveCriticalSection(&stInfo.drawCS);
    return 0;

  case WM_MOUSEMOVE:
    if (!mouseModeChange) {
      processMouseXY(LOWORD(lParam), HIWORD(lParam), 0, wParam, 0);
    }

    return 0;

  // case WM_MOUSEWHEEL:
  //  if (!mouseModeChange) {
  //    // WM_MOUSEWHEEL returns x and y relative to the main screen.
  //    // WM_MOUSEMOVE below returns x and y relative to the current view.
  //    POINT pt;
  //    pt.x = LOWORD(lParam);
  //    pt.y = HIWORD(lParam);
  //    ScreenToClient(stInfo.simWnd, &pt);
  //    processMouseXY( pt.x, pt.y, (s16) HIWORD(wParam) / 120, LOWORD(wParam),
  //    0);
  //  }
  //  return 0;
  case WM_LBUTTONDOWN:
  case WM_LBUTTONDBLCLK:
  case WM_LBUTTONUP:
    if (mouse_buttons == 2) {
      if (wParam == (MK_CONTROL | MK_LBUTTON | MK_RBUTTON)) {
        mouseCaptureMode = !mouseCaptureMode;
        SetMouseCapture();
        mouseModeChange = true;
      } else if (mouseModeChange && (iMsg == WM_LBUTTONUP)) {
        mouseModeChange = false;
      } else {
        processMouseXY(LOWORD(lParam), HIWORD(lParam), 0, wParam, 1);
      }

      return 0;
    }

    processMouseXY(LOWORD(lParam), HIWORD(lParam), 0, wParam, 1);
    return 0;

  case WM_MBUTTONDOWN:
  case WM_MBUTTONDBLCLK:
  case WM_MBUTTONUP:
    if (wParam == (MK_CONTROL | MK_MBUTTON)) {
      mouseCaptureMode = !mouseCaptureMode;
      SetMouseCapture();
      mouseModeChange = true;
    } else if (mouseModeChange && (iMsg == WM_MBUTTONUP)) {
      mouseModeChange = false;
    } else {
      processMouseXY(LOWORD(lParam), HIWORD(lParam), 0, wParam, 4);
    }

    return 0;

  case WM_RBUTTONDOWN:
  case WM_RBUTTONDBLCLK:
  case WM_RBUTTONUP:
    if (mouse_buttons == 2) {
      if (wParam == (MK_CONTROL | MK_LBUTTON | MK_RBUTTON)) {
        mouseCaptureMode = !mouseCaptureMode;
        SetMouseCapture();
        mouseModeChange = true;
      } else if (mouseModeChange && (iMsg == WM_RBUTTONUP)) {
        mouseModeChange = false;
      } else {
        processMouseXY(LOWORD(lParam), HIWORD(lParam), 0, wParam, 2);
      }

      return 0;
    }

    processMouseXY(LOWORD(lParam), HIWORD(lParam), 0, wParam, 2);
    return 0;

  case WM_CLOSE:
    return DefWindowProc(hwnd, iMsg, wParam, lParam);

  case WM_DESTROY:
    KillTimer(hwnd, 1);
    stInfo.UIinited = false;
#if BX_USE_WINDOWS_FONTS
    DestroyFont();
#endif
    return 0;

  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    EnterCriticalSection(&stInfo.keyCS);
    enq_key_event(HIWORD(lParam) & 0x01FF, BX_KEY_PRESSED);
    LeaveCriticalSection(&stInfo.keyCS);
    return 0;

  case WM_KEYUP:
  case WM_SYSKEYUP:

    // check if it's keyup, alt key, non-repeat
    // see http://msdn2.microsoft.com/en-us/library/ms646267.aspx
    if (wParam == VK_RETURN) {
      if ((HIWORD(lParam) & BX_SYSKEY) == (KF_ALTDOWN | KF_UP)) {
        if (!saveParent) {
          BX_INFO(("entering fullscreen mode"));
          theGui->dimension_update(desktop_x, desktop_y, 0, 0, current_bpp);
        } else {
          BX_INFO(("leaving fullscreen mode"));
          theGui->dimension_update(dimension_x, desktop_y - 1, 0, 0,
                                   current_bpp);
        }
      }
    } else {
      EnterCriticalSection(&stInfo.keyCS);
      enq_key_event(HIWORD(lParam) & 0x01FF, BX_KEY_RELEASED);
      LeaveCriticalSection(&stInfo.keyCS);
    }

    return 0;

  case WM_SYSCHAR:

    // check if it's keydown, alt key, non-repeat
    // see http://msdn2.microsoft.com/en-us/library/ms646267.aspx
    if (wParam == VK_RETURN) {
      if ((HIWORD(lParam) & BX_SYSKEY) == KF_ALTDOWN) {
        if (!saveParent) {
          BX_INFO(("entering fullscreen mode"));
          theGui->dimension_update(desktop_x, desktop_y, 0, 0, current_bpp);
        } else {
          BX_INFO(("leaving fullscreen mode"));
          theGui->dimension_update(dimension_x, desktop_y - 1, 0, 0,
                                   current_bpp);
        }
      }
    }

  case WM_CHAR:
  case WM_DEADCHAR:
  case WM_SYSDEADCHAR:
    return 0;
  }

  return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

void enq_key_event(u32 key, u32 press_release) {
  static BOOL alt_pressed_l = false;
  static BOOL alt_pressed_r = false;
  static BOOL ctrl_pressed_l = false;
  static BOOL ctrl_pressed_r = false;
  static BOOL shift_pressed_l = false;
  static BOOL shift_pressed_r = false;

  // Windows generates multiple keypresses when holding down these keys
  if (press_release == BX_KEY_PRESSED) {
    switch (key) {
    case 0x1d:
      if (ctrl_pressed_l)
        return;
      ctrl_pressed_l = true;
      break;

    case 0x2a:
      if (shift_pressed_l)
        return;
      shift_pressed_l = true;
      break;

    case 0x36:
      if (shift_pressed_r)
        return;
      shift_pressed_r = true;
      break;

    case 0x38:
      if (alt_pressed_l)
        return;
      alt_pressed_l = true;
      break;

    case 0x011d:
      if (ctrl_pressed_r)
        return;
      ctrl_pressed_r = true;
      break;

    case 0x0138:
      if (alt_pressed_r)
        return;

      // This makes the "AltGr" key on European keyboards work
      if (ctrl_pressed_l) {
        enq_key_event(0x1d, BX_KEY_RELEASED);
      }

      alt_pressed_r = true;
      break;
    }
  } else {
    switch (key) {
    case 0x1d:
      if (!ctrl_pressed_l)
        return;
      ctrl_pressed_l = false;
      break;
    case 0x2a:
      shift_pressed_l = false;
      break;
    case 0x36:
      shift_pressed_r = false;
      break;
    case 0x38:
      alt_pressed_l = false;
      break;
    case 0x011d:
      ctrl_pressed_r = false;
      break;
    case 0x0138:
      alt_pressed_r = false;
      break;
    }
  }

  if (((tail + 1) % SCANCODE_BUFSIZE) == head) {
    BX_ERROR(("enq_scancode: buffer full"));
    return;
  }

  keyevents[tail].key_event = key | press_release;
  tail = (tail + 1) % SCANCODE_BUFSIZE;
}

void enq_mouse_event(void) {
  EnterCriticalSection(&stInfo.mouseCS);
  if (ms_xdelta || ms_ydelta || ms_zdelta) {
    if (((tail + 1) % SCANCODE_BUFSIZE) == head) {
      BX_ERROR(("enq_scancode: buffer full"));
      return;
    }

    QueueEvent &current = keyevents[tail];
    current.key_event = MOUSE_MOTION;
    current.mouse_x = ms_xdelta;
    current.mouse_y = ms_ydelta;
    current.mouse_z = ms_zdelta;
    current.mouse_button_state = mouse_button_state;
    resetDelta();
    tail = (tail + 1) % SCANCODE_BUFSIZE;
  }

  LeaveCriticalSection(&stInfo.mouseCS);
}

QueueEvent *deq_key_event(void) {
  QueueEvent *key;

  if (head == tail) {
    BX_ERROR(("deq_scancode: buffer empty"));
    return ((QueueEvent *)0);
  }

  key = &keyevents[head];
  head = (head + 1) % SCANCODE_BUFSIZE;

  return (key);
}

// ::HANDLE_EVENTS()
//
// Called periodically (vga_update_interval in .bochsrc) so the
// the gui code can poll for keyboard, mouse, and other

// relevant events.
void bx_win32_gui_c::handle_events(void) {
  u32 key;
  u32 key_event;

  if (stInfo.kill)
    terminateEmul(stInfo.kill);

  // Handle mouse moves
  enq_mouse_event();

  // Handle keyboard and mouse clicks
  EnterCriticalSection(&stInfo.keyCS);
  while (head != tail) {
    QueueEvent *queue_event = deq_key_event();
    if (!queue_event)
      break;
    key = queue_event->key_event;
    if (key == MOUSE_MOTION) {

      //      DEV_mouse_motion_ext( queue_event->mouse_x,
      //        queue_event->mouse_y, queue_event->mouse_z,
      //        queue_event->mouse_button_state);
    }

    // Check for mouse buttons first
    else if (key & MOUSE_PRESSED) {

      //      DEV_mouse_motion_ext( 0, 0, 0, LOWORD(key));
    } else {
      key_event = win32_to_bx_key[(key & 0x100) ? 1 : 0][key & 0xff];
      if (key & BX_KEY_RELEASED)
        key_event |= BX_KEY_RELEASED;

      // DEV_kbd_gen_scancode(key_event);
      theKeyboard->gen_scancode(key_event);
    }
  }

  LeaveCriticalSection(&stInfo.keyCS);
}

// ::FLUSH()
//
// Called periodically, requesting that the gui code flush all pending

// screen update requests.
void bx_win32_gui_c::flush(void) {
  EnterCriticalSection(&stInfo.drawCS);
  if (updated_area_valid) {

    // slight bugfix
    updated_area.right++;
    updated_area.bottom++;
    InvalidateRect(stInfo.simWnd, &updated_area, false);
    updated_area_valid = false;
  }

  LeaveCriticalSection(&stInfo.drawCS);
}

// ::CLEAR_SCREEN()
//

// Called to request that the VGA region is cleared.
void bx_win32_gui_c::clear_screen(void) {
  HGDIOBJ oldObj;

  if (!stInfo.UIinited)
    return;

  EnterCriticalSection(&stInfo.drawCS);

  oldObj = SelectObject(MemoryDC, MemoryBitmap);
  PatBlt(MemoryDC, 0, 0, stretched_x, stretched_y, BLACKNESS);
  SelectObject(MemoryDC, oldObj);

  updateUpdated(0, 0, dimension_x - 1, dimension_y - 1);

  LeaveCriticalSection(&stInfo.drawCS);
}

// ::TEXT_UPDATE()
//
// Called in a VGA text mode, to update the screen with
// new content.
//
// old_text: array of character/attributes making up the contents
//           of the screen from the last call.  See below
// new_text: array of character/attributes making up the current
//           contents, which should now be displayed.  See below
//
// format of old_text & new_text: each is tm_info.line_offset*text_rows
//     bytes long. Each character consists of 2 bytes.  The first by is
//     the character value, the second is the attribute byte.
//
// cursor_x: new x location of cursor
// cursor_y: new y location of cursor
// tm_info:  this structure contains information for additional
//           features in text mode (cursor shape, line offset,...)

// nrows:    number of text rows (unused here)
void bx_win32_gui_c::text_update(u8 *old_text, u8 *new_text,
                                 unsigned long cursor_x, unsigned long cursor_y,
                                 bx_vga_tminfo_t tm_info, unsigned nrows) {
  HDC hdc;
  unsigned char data[64];
  u8 *old_line;
  u8 *new_line;
  u8 cAttr;
  u8 cChar;
  unsigned int curs;
  unsigned int hchars;
  unsigned int i;
  unsigned int offset;
  unsigned int rows;
  unsigned int x;
  unsigned int y;
  unsigned int xc;
  unsigned int yc;
  BOOL forceUpdate = false;
#if !BX_USE_WINDOWS_FONTS
  u8 *text_base;
  u8 cfwidth;
  u8 cfheight;
  u8 cfheight2;
  u8 font_col;
  u8 font_row;
  u8 font_row2;
  u8 split_textrow;
  u8 split_fontrows;
  unsigned int yc2;
  unsigned int cs_y;
  BOOL split_screen;
#endif
  if (!stInfo.UIinited)
    return;

  EnterCriticalSection(&stInfo.drawCS);

  if (charmap_updated) {
    for (unsigned c = 0; c < 256; c++) {
      if (char_changed[c]) {
        memset(data, 0, sizeof(data));

        BOOL gfxchar = tm_info.line_graphics && ((c & 0xE0) == 0xC0);
        for (i = 0; i < (unsigned)yChar; i++) {
          data[i * 2] = vga_charmap[c * 32 + i];
          if (gfxchar) {
            data[i * 2 + 1] = (data[i * 2] << 7);
          }
        }

        SetBitmapBits(vgafont[c], 64, data);
        char_changed[c] = 0;
      }
    }

    forceUpdate = true;
    charmap_updated = 0;
  }

  for (i = 0; i < 16; i++) {
    text_pal_idx[i] =
        theVGA->get_actl_palette_idx(i); // DEV_vga_get_actl_pal_idx(i);
  }

  hdc = GetDC(stInfo.simWnd);

#if !BX_USE_WINDOWS_FONTS
  if ((tm_info.h_panning != h_panning) || (tm_info.v_panning != v_panning)) {
    forceUpdate = 1;
    h_panning = tm_info.h_panning;
    v_panning = tm_info.v_panning;
  }

  if (tm_info.line_compare != line_compare) {
    forceUpdate = 1;
    line_compare = tm_info.line_compare;
  }
#endif

  // first invalidate character at previous and new cursor location
  if ((prev_cursor_y < text_rows) && (prev_cursor_x < text_cols)) {
    curs = prev_cursor_y * tm_info.line_offset + prev_cursor_x * 2;
    old_text[curs] = ~new_text[curs];
  }

  if ((tm_info.cs_start <= tm_info.cs_end) && (tm_info.cs_start < yChar) &&
      (cursor_y < text_rows) && (cursor_x < text_cols)) {
    curs = cursor_y * tm_info.line_offset + cursor_x * 2;
    old_text[curs] = ~new_text[curs];
  } else {
    curs = 0xffff;
  }

#if !BX_USE_WINDOWS_FONTS
  rows = text_rows;
  if (v_panning)
    rows++;
  y = 0;
  cs_y = 0;
  text_base = new_text - tm_info.start_address;
  split_textrow = (line_compare + v_panning) / yChar;
  split_fontrows = ((line_compare + v_panning) % yChar) + 1;
  split_screen = 0;
  do {
    hchars = text_cols;
    if (h_panning)
      hchars++;
    if (split_screen) {
      yc = line_compare + cs_y * yChar + 1;
      font_row = 0;
      if (rows == 1) {
        cfheight = (dimension_y - line_compare - 1) % yChar;
        if (cfheight == 0)
          cfheight = yChar;
      } else {
        cfheight = yChar;
      }
    } else if (v_panning) {
      if (y == 0) {
        yc = 0;
        font_row = v_panning;
        cfheight = yChar - v_panning;
      } else {
        yc = y * yChar - v_panning;
        font_row = 0;
        if (rows == 1) {
          cfheight = v_panning;
        } else {
          cfheight = yChar;
        }
      }
    } else {
      yc = y * yChar;
      font_row = 0;
      cfheight = yChar;
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
          cfwidth = xChar - h_panning;
        } else {
          xc = x * xChar - h_panning;
          font_col = 0;
          if (hchars == 1) {
            cfwidth = h_panning;
          } else {
            cfwidth = xChar;
          }
        }
      } else {
        xc = x * xChar;
        font_col = 0;
        cfwidth = xChar;
      }

      if (forceUpdate || (old_text[0] != new_text[0]) ||
          (old_text[1] != new_text[1])) {
        cChar = new_text[0];
        cAttr = new_text[1];
        DrawBitmap(hdc, vgafont[cChar], xc, yc, cfwidth, cfheight, font_col,
                   font_row, SRCCOPY, cAttr);
        if (offset == curs) {
          if (font_row == 0) {
            yc2 = yc + tm_info.cs_start;
            font_row2 = tm_info.cs_start;
            cfheight2 = tm_info.cs_end - tm_info.cs_start + 1;
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

          cAttr = ((cAttr >> 4) & 0xF) + ((cAttr & 0xF) << 4);
          DrawBitmap(hdc, vgafont[cChar], xc, yc2, cfwidth, cfheight2, font_col,
                     font_row2, SRCCOPY, cAttr);
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
      rows = ((dimension_y - line_compare + yChar - 2) / yChar) + 1;
      split_screen = 1;
    } else {
      y++;
      cs_y++;
      new_text = new_line + tm_info.line_offset;
      old_text = old_line + tm_info.line_offset;
    }
  } while (--rows);

  h_panning = tm_info.h_panning;
#else
  rows = text_rows;
  y = 0;
  do {
    hchars = text_cols;
    yc = y * yChar;
    new_line = new_text;
    old_line = old_text;
    x = 0;
    offset = y * tm_info.line_offset;
    do {
      xc = x * xChar;
      if (forceUpdate || (old_text[0] != new_text[0]) ||
          (old_text[1] != new_text[1])) {
        cChar = new_text[0];
        cAttr = new_text[1];
        DrawChar(hdc, cChar, xc, yc, cAttr, 1, 0);
        if (offset == curs) {
          DrawChar(hdc, cChar, xc, yc, cAttr, tm_info.cs_start, tm_info.cs_end);
        }
      }

      x++;
      new_text += 2;
      old_text += 2;
      offset += 2;
    } while (--hchars);
    y++;
    new_text = new_line + tm_info.line_offset;
    old_text = old_line + tm_info.line_offset;
  } while (--rows);
#endif
  prev_cursor_x = cursor_x;
  prev_cursor_y = cursor_y;

  ReleaseDC(stInfo.simWnd, hdc);

  LeaveCriticalSection(&stInfo.drawCS);
}

// ::PALETTE_CHANGE()
//
// Allocate a color in the native GUI, for this color, and put
// it in the colormap location 'index'.
// returns: 0=no screen update needed (color map change has direct effect)

//          1=screen updated needed (redraw using current colormap)
bool bx_win32_gui_c::palette_change(unsigned index, unsigned red,
                                    unsigned green, unsigned blue) {
  if ((current_bpp == 16) && (index < 3)) {
    cmap_index[256 + index].rgbRed = red;
    cmap_index[256 + index].rgbBlue = blue;
    cmap_index[256 + index].rgbGreen = green;
    return 0;
  } else {
    cmap_index[index].rgbRed = red;
    cmap_index[index].rgbBlue = blue;
    cmap_index[index].rgbGreen = green;
  }

  return (1);
}

// ::GRAPHICS_TILE_UPDATE()
//
// Called to request that a tile of graphics be drawn to the
// screen, since info in this region has changed.
//
// tile: array of 8bit values representing a block of pixels with
//       dimension equal to the 'tilewidth' & 'tileheight' parameters to
//       ::specific_init().  Each value specifies an index into the
//       array of colors you allocated for ::palette_change()
// x0: x origin of tile
// y0: y origin of tile
//
// note: origin of tile and of window based on (0,0) being in the upper

//       left of the window.
void bx_win32_gui_c::graphics_tile_update(u8 *tile, unsigned x0, unsigned y0) {
  HDC hdc;
  HGDIOBJ oldObj;

  EnterCriticalSection(&stInfo.drawCS);
  hdc = GetDC(stInfo.simWnd);

  oldObj = SelectObject(MemoryDC, MemoryBitmap);

  StretchDIBits(MemoryDC, x0, y0, x_tilesize, y_tilesize, 0, 0, x_tilesize,
                y_tilesize, tile, bitmap_info, DIB_RGB_COLORS, SRCCOPY);

  SelectObject(MemoryDC, oldObj);

  updateUpdated(x0, y0, x0 + x_tilesize - 1, y0 + y_tilesize - 1);

  ReleaseDC(stInfo.simWnd, hdc);
  LeaveCriticalSection(&stInfo.drawCS);
}

// ::DIMENSION_UPDATE()
//
// Called when the VGA mode changes it's X,Y dimensions.
// Resize the window to this size.
//
// x: new VGA x size
// y: new VGA y size
// fheight: new VGA character height in text mode
// fwidth : new VGA character width in text mode

// bpp : bits per pixel in graphics mode
void bx_win32_gui_c::dimension_update(unsigned x, unsigned y, unsigned fheight,
                                      unsigned fwidth, unsigned bpp) {
  BxTextMode = (fheight > 0);
  if (BxTextMode) {
    text_cols = x / fwidth;
    text_rows = y / fheight;
    xChar = fwidth;
    yChar = fheight;
  }

  if (x == dimension_x && y == dimension_y && bpp == current_bpp)
    return;
  dimension_x = x;
  dimension_y = y;
  stretched_x = dimension_x;
  stretched_y = dimension_y;
  stretch_factor = 1;
  if (BxTextMode && (stretched_x < 400)) {
    stretched_x *= 2;
    stretch_factor *= 2;
  }

  bitmap_info->bmiHeader.biBitCount = bpp;
  if (bpp == 16) {
    bitmap_info->bmiHeader.biCompression = BI_BITFIELDS;

    static RGBQUAD red_mask = {0x00, 0xF8, 0x00, 0x00};
    static RGBQUAD green_mask = {0xE0, 0x07, 0x00, 0x00};
    static RGBQUAD blue_mask = {0x1F, 0x00, 0x00, 0x00};
    bitmap_info->bmiColors[256] = bitmap_info->bmiColors[0];
    bitmap_info->bmiColors[257] = bitmap_info->bmiColors[1];
    bitmap_info->bmiColors[258] = bitmap_info->bmiColors[2];
    bitmap_info->bmiColors[0] = red_mask;
    bitmap_info->bmiColors[1] = green_mask;
    bitmap_info->bmiColors[2] = blue_mask;
  } else {
    if (current_bpp == 16) {
      bitmap_info->bmiColors[0] = bitmap_info->bmiColors[256];
      bitmap_info->bmiColors[1] = bitmap_info->bmiColors[257];
      bitmap_info->bmiColors[2] = bitmap_info->bmiColors[258];
    }

    bitmap_info->bmiHeader.biCompression = BI_RGB;
    if (bpp == 15) {
      bitmap_info->bmiHeader.biBitCount = 16;
    }
  }

  current_bpp = bpp;

  resize_main_window();

  BX_INFO(("dimension update x=%d y=%d fontheight=%d fontwidth=%d bpp=%d", x, y,
           fheight, fwidth, bpp));

  host_xres = x;
  host_yres = y;
  host_bpp = bpp;
}

// ::EXIT()
//
// Called before bochs terminates, to allow for a graceful

// exit from the native GUI mechanism.
void bx_win32_gui_c::exit(void) {
  printf("# In bx_win32_gui_c::exit(void)!\n");

  // kill thread first...
  PostMessage(stInfo.mainWnd, WM_CLOSE, 0, 0);

  // Wait until it dies
  while ((stInfo.kill == 0) && (workerThreadID != 0))
    Sleep(500);

  if (!stInfo.kill)
    terminateEmul(EXIT_NORMAL);
}

void create_vga_font(void) {
  unsigned char data[64];

  // VGA font is 8 or 9 wide and up to 32 high
  for (unsigned c = 0; c < 256; c++) {
    vgafont[c] = CreateBitmap(9, 32, 1, 1, NULL);
    if (!vgafont[c])
      terminateEmul(EXIT_FONT_BITMAP_ERROR);
    memset(data, 0, sizeof(data));
    for (unsigned i = 0; i < 16; i++)
      data[i * 2] = reverse_bitorder(bx_vgafont[c].data[i]);
    SetBitmapBits(vgafont[c], 64, data);
  }
}

unsigned char reverse_bitorder(unsigned char b) {
  unsigned char ret = 0;

  for (unsigned i = 0; i < 8; i++) {
    ret |= (b & 0x01) << (7 - i);
    b >>= 1;
  }

  return (ret);
}

COLORREF GetColorRef(unsigned char attr) {
  u8 pal_idx = text_pal_idx[attr];
  return RGB(cmap_index[pal_idx].rgbRed, cmap_index[pal_idx].rgbGreen,
             cmap_index[pal_idx].rgbBlue);
}

void DrawBitmap(HDC hdc, HBITMAP hBitmap, int xStart, int yStart, int width,
                int height, int fcol, int frow, DWORD dwRop,
                unsigned char cColor) {
  BITMAP bm;
  HDC hdcMem;
  POINT ptSize;
  POINT ptOrg;
  HGDIOBJ oldObj;

  hdcMem = CreateCompatibleDC(hdc);
  SelectObject(hdcMem, hBitmap);
  SetMapMode(hdcMem, GetMapMode(hdc));

  GetObject(hBitmap, sizeof(BITMAP), (LPVOID)&bm);

  ptSize.x = width;
  ptSize.y = height;

  DPtoLP(hdc, &ptSize, 1);

  ptOrg.x = fcol;
  ptOrg.y = frow;
  DPtoLP(hdcMem, &ptOrg, 1);

  oldObj = SelectObject(MemoryDC, MemoryBitmap);

  // The highest background bit usually means blinking characters. No idea
  // how to implement that so for now it's just implemented as color.
  // Note: it is also possible to program the VGA controller to have the
  // high bit for the foreground color enable blinking characters.
  COLORREF crFore = SetTextColor(MemoryDC, GetColorRef((cColor >> 4) & 0xf));
  COLORREF crBack = SetBkColor(MemoryDC, GetColorRef(cColor & 0xf));
  BitBlt(MemoryDC, xStart, yStart, ptSize.x, ptSize.y, hdcMem, ptOrg.x, ptOrg.y,
         dwRop);
  SetBkColor(MemoryDC, crBack);
  SetTextColor(MemoryDC, crFore);

  SelectObject(MemoryDC, oldObj);

  updateUpdated(xStart, yStart, ptSize.x + xStart - 1, ptSize.y + yStart - 1);

  DeleteDC(hdcMem);
}

void updateUpdated(int x1, int y1, int x2, int y2) {
  x1 *= stretch_factor;
  x2 *= stretch_factor;
  if (!updated_area_valid) {
    updated_area.left = x1;
    updated_area.top = y1;
    updated_area.right = x2;
    updated_area.bottom = y2;
  } else {
    if (x1 < updated_area.left)
      updated_area.left = x1;
    if (y1 < updated_area.top)
      updated_area.top = y1;
    if (x2 > updated_area.right)
      updated_area.right = x2;
    if (y2 > updated_area.bottom)
      updated_area.bottom = y2;
  }

  updated_area_valid = true;
}

void bx_win32_gui_c::mouse_enabled_changed_specific(bool val) {
  if ((val != (bool)mouseCaptureMode) && !mouseToggleReq) {
    mouseToggleReq = true;
    mouseCaptureNew = val;
  }
}

void bx_win32_gui_c::get_capabilities(u16 *xres, u16 *yres, u16 *bpp) {
  if (desktop_y > 0) {
    *xres = desktop_x;
    *yres = desktop_y;
    *bpp = 32;
  } else {
    *xres = 1024;
    *yres = 768;
    *bpp = 32;
  }
}
#endif /* if BX_WITH_WIN32 */
