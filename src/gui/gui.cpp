/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Website: https://github.com/lenticularis39/axpbox
 *
 * Forked from: ES40 emulator
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 * Copyright (C) 2007 by Camiel Vanderhoeven
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

//#define DEBUG_LOCKS
//#define NO_LOCK_TIMEOUTS

#include "../StdAfx.hpp"

#include <signal.h>

#include "gui.hpp"

bx_gui_c *bx_gui = NULL;

#define BX_KEY_UNKNOWN 0x7fffffff
#define N_USER_KEYS 36

typedef struct {
  const char *key;
  u32 symbol;
} user_key_t;

static user_key_t user_keys[N_USER_KEYS] = {
    {"f1", BX_KEY_F1},           {"f2", BX_KEY_F2},
    {"f3", BX_KEY_F3},           {"f4", BX_KEY_F4},
    {"f5", BX_KEY_F5},           {"f6", BX_KEY_F6},
    {"f7", BX_KEY_F7},           {"f8", BX_KEY_F8},
    {"f9", BX_KEY_F9},           {"f10", BX_KEY_F10},
    {"f11", BX_KEY_F11},         {"f12", BX_KEY_F12},
    {"alt", BX_KEY_ALT_L},       {"bksl", BX_KEY_BACKSLASH},
    {"bksp", BX_KEY_BACKSPACE},  {"ctrl", BX_KEY_CTRL_L},
    {"del", BX_KEY_DELETE},      {"down", BX_KEY_DOWN},
    {"end", BX_KEY_END},         {"enter", BX_KEY_ENTER},
    {"esc", BX_KEY_ESC},         {"home", BX_KEY_HOME},
    {"ins", BX_KEY_INSERT},      {"left", BX_KEY_LEFT},
    {"menu", BX_KEY_MENU},       {"minus", BX_KEY_MINUS},
    {"pgdwn", BX_KEY_PAGE_DOWN}, {"pgup", BX_KEY_PAGE_UP},
    {"plus", BX_KEY_KP_ADD},     {"right", BX_KEY_RIGHT},
    {"shift", BX_KEY_SHIFT_L},   {"space", BX_KEY_SPACE},
    {"tab", BX_KEY_TAB},         {"up", BX_KEY_UP},
    {"win", BX_KEY_WIN_L},       {"print", BX_KEY_PRINT}};

bx_gui_c::bx_gui_c(void) {
  framebuffer = NULL;
  guiMutex = new CMutex("gui-lock");
}

bx_gui_c::~bx_gui_c() {
  if (framebuffer != NULL) {
    delete[] framebuffer;
  }
}

void bx_gui_c::init(unsigned tilewidth, unsigned tileheight) {
  new_gfx_api = 0;
  host_xres = 640;
  host_yres = 480;
  host_bpp = 8;

  specific_init(tilewidth, tileheight);

  charmap_updated = 0;

  if (!new_gfx_api && (framebuffer == NULL)) {
    framebuffer = new u8[BX_MAX_XRES * BX_MAX_YRES * 4];
  }
}

void bx_gui_c::cleanup(void) {}
u32 get_user_key(char *key) {
  int i = 0;

  while (i < N_USER_KEYS) {
    if (!strcmp(key, user_keys[i].key))
      return user_keys[i].symbol;
    i++;
  }

  return BX_KEY_UNKNOWN;
}

void bx_gui_c::mouse_enabled_changed(bool val) {

  // This is only called when SIM->get_init_done is 1.  Note that VAL
  // is the new value of mouse_enabled, which may not match the old
  // value which is still in SIM->get_param_bool(BXPN_MOUSE_ENABLED)->get().
  bx_gui->mouse_enabled_changed_specific(val);
}

void bx_gui_c::init_signal_handlers() {
#if BX_GUI_SIGHANDLER
  if (bx_gui_sighandler) {
    u32 mask = bx_gui->get_sighandler_mask();
    for (u32 sig = 0; sig < 32; sig++) {
      if (mask & (1 << sig))
        signal(sig, bx_signal_handler);
    }
  }
#endif
}

void bx_gui_c::set_text_charmap(u8 *fbuffer) {
  memcpy(&bx_gui->vga_charmap, fbuffer, 0x2000);
  for (unsigned i = 0; i < 256; i++)
    bx_gui->char_changed[i] = 1;
  bx_gui->charmap_updated = 1;
}

void bx_gui_c::set_text_charbyte(u16 address, u8 data) {
  bx_gui->vga_charmap[address] = data;
  bx_gui->char_changed[address >> 5] = 1;
  bx_gui->charmap_updated = 1;
}

void bx_gui_c::beep_on(float frequency) {
  BX_INFO(("GUI Beep ON (frequency=%.2f)", frequency));
}

void bx_gui_c::beep_off() { BX_INFO(("GUI Beep OFF")); }

void bx_gui_c::get_capabilities(u16 *xres, u16 *yres, u16 *bpp) {
  *xres = 1024;
  *yres = 768;
  *bpp = 32;
}

bx_svga_tileinfo_t *bx_gui_c::graphics_tile_info(bx_svga_tileinfo_t *info) {
  if (!info) {
    info = (bx_svga_tileinfo_t *)malloc(sizeof(bx_svga_tileinfo_t));
    if (!info) {
      return NULL;
    }
  }

  host_pitch = host_xres * ((host_bpp + 1) >> 3);

  info->bpp = host_bpp;
  info->pitch = host_pitch;
  switch (info->bpp) {
  case 15:
    info->red_shift = 15;
    info->green_shift = 10;
    info->blue_shift = 5;
    info->red_mask = 0x7c00;
    info->green_mask = 0x03e0;
    info->blue_mask = 0x001f;
    break;

  case 16:
    info->red_shift = 16;
    info->green_shift = 11;
    info->blue_shift = 5;
    info->red_mask = 0xf800;
    info->green_mask = 0x07e0;
    info->blue_mask = 0x001f;
    break;

  case 24:
  case 32:
    info->red_shift = 24;
    info->green_shift = 16;
    info->blue_shift = 8;
    info->red_mask = 0xff0000;
    info->green_mask = 0x00ff00;
    info->blue_mask = 0x0000ff;
    break;
  }

  info->is_indexed = (host_bpp == 8);
#ifdef BX_LITTLE_ENDIAN
  info->is_little_endian = 1;
#else
  info->is_little_endian = 0;
#endif
  return info;
}

u8 *bx_gui_c::graphics_tile_get(unsigned x0, unsigned y0, unsigned *w,
                                unsigned *h) {
  if (x0 + X_TILESIZE > host_xres) {
    *w = host_xres - x0;
  } else {
    *w = X_TILESIZE;
  }

  if (y0 + Y_TILESIZE > host_yres) {
    *h = host_yres - y0;
  } else {
    *h = Y_TILESIZE;
  }

  return (u8 *)framebuffer + y0 * host_pitch + x0 * ((host_bpp + 1) >> 3);
}

void bx_gui_c::graphics_tile_update_in_place(unsigned x0, unsigned y0,
                                             unsigned w, unsigned h) {
  u8 tile[X_TILESIZE * Y_TILESIZE * 4];
  u8 *tile_ptr;
  u8 *fb_ptr;
  u16 xc;
  u16 yc;
  u16 fb_pitch;
  u16 tile_pitch;
  u8 r;
  u8 diffx;
  u8 diffy;

  diffx = (x0 % X_TILESIZE);
  diffy = (y0 % Y_TILESIZE);
  if (diffx > 0) {
    x0 -= diffx;
    w += diffx;
  }

  if (diffy > 0) {
    y0 -= diffy;
    h += diffy;
  }

  fb_pitch = host_pitch;
  tile_pitch = X_TILESIZE * ((host_bpp + 1) >> 3);
  for (yc = y0; yc < (y0 + h); yc += Y_TILESIZE) {
    for (xc = x0; xc < (x0 + w); xc += X_TILESIZE) {
      fb_ptr = framebuffer + (yc * fb_pitch + xc * ((host_bpp + 1) >> 3));
      tile_ptr = &tile[0];
      for (r = 0; r < h; r++) {
        memcpy(tile_ptr, fb_ptr, tile_pitch);
        fb_ptr += fb_pitch;
        tile_ptr += tile_pitch;
      }

      graphics_tile_update(tile, xc, yc);
    }
  }
}

void bx_gui_c::lock() { MUTEX_LOCK(guiMutex); }

void bx_gui_c::unlock() { MUTEX_UNLOCK(guiMutex); }
