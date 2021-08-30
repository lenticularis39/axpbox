/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * WWW    : http://es40.org
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

#include "../StdAfx.hpp"

#if defined(HAVE_SDL)
#include "../System.hpp"
#include "../VGA.hpp"
#include "gui.hpp"
#include "keymap.hpp"

#include "../Configurator.hpp"
#include "../Keyboard.hpp"

#define _MULTI_THREAD

// Define BX_PLUGGABLE in files that can be compiled into plugins.  For
// platforms that require a special tag on exported symbols, BX_PLUGGABLE
// is used to know when we are exporting symbols and when we are importing.
#define BX_PLUGGABLE

#include <SDL/SDL.h>
#include <SDL/SDL_endian.h>
#include <SDL/SDL_thread.h>
#include <stdlib.h>

#include "sdl_fonts.hpp"

/**
 * \brief GUI implementation using SDL.
 **/
class bx_sdl_gui_c : public bx_gui_c {
public:
  bx_sdl_gui_c(CConfigurator *cfg);
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

private:
  CConfigurator *myCfg;
};

// declare one instance of the gui object and call macro to insert the
// plugin code
static bx_sdl_gui_c *theGui = NULL;
IMPLEMENT_GUI_PLUGIN_CODE(sdl)
static unsigned prev_cursor_x = 0;
static unsigned prev_cursor_y = 0;
static u32 convertStringToSDLKey(const char *string);

SDL_Thread *sdl_thread;
SDL_Surface *sdl_screen;
SDL_Event sdl_event;
int sdl_grab;
unsigned res_x, res_y;
unsigned half_res_x, half_res_y;
static unsigned int text_cols = 80, text_rows = 25;
u8 h_panning = 0, v_panning = 0;
u16 line_compare = 1023;
int fontwidth = 8, fontheight = 16;
static unsigned vga_bpp = 8;
unsigned tilewidth, tileheight;
u32 palette[256];
u8 old_mousebuttons = 0, new_mousebuttons = 0;
int old_mousex = 0, new_mousex = 0;
int old_mousey = 0, new_mousey = 0;
bool just_warped = false;

bx_sdl_gui_c::bx_sdl_gui_c(CConfigurator *cfg) {
  myCfg = cfg;
  bx_keymap = new bx_keymap_c(cfg);
}

#ifdef __MORPHOS__
void bx_sdl_morphos_exit(void) {
  SDL_Quit();
  if (PowerSDLBase)
    CloseLibrary(PowerSDLBase);
}
#endif
void bx_sdl_gui_c::specific_init(unsigned x_tilesize, unsigned y_tilesize) {
  int i;

  int j;
  u32 flags;

  tilewidth = x_tilesize;
  tileheight = y_tilesize;

  for (i = 0; i < 256; i++)
    for (j = 0; j < 16; j++)
      vga_charmap[i * 32 + j] = sdl_font8x16[i][j];

#ifdef __MORPHOS__
  if (!(PowerSDLBase = OpenLibrary("powersdl.library", 0))) {
    BX_PANIC(("Unable to open SDL libraries"));
    return;
  }
#endif
  flags = SDL_INIT_VIDEO;
  if (SDL_Init(flags) < 0) {
    FAILURE(SDL, "Unable to initialize SDL libraries");
  }

#ifdef __MORPHOS__
  atexit(bx_sdl_morphos_exit);
#else
  atexit(SDL_Quit);
#endif
  sdl_screen = NULL;

  //  sdl_fullscreen_toggle = 0;
  dimension_update(640, 480);

  SDL_EnableKeyRepeat(250, 50);
  SDL_WarpMouse(half_res_x, half_res_y);

  // load keymap for sdl
  if (myCfg->get_bool_value("keyboard.use_mapping", false)) {
    bx_keymap->loadKeymap(convertStringToSDLKey);
  }

  new_gfx_api = 1;
}

void bx_sdl_gui_c::text_update(u8 *old_text, u8 *new_text,
                               unsigned long cursor_x, unsigned long cursor_y,
                               bx_vga_tminfo_t tm_info, unsigned nrows) {
  u8 *pfont_row;

  u8 *old_line;

  u8 *new_line;

  u8 *text_base;
  unsigned int cs_y;
  unsigned int i;
  unsigned int x;
  unsigned int y;
  unsigned int curs;
  unsigned int hchars;
  unsigned int offset;
  u8 fontline;
  u8 fontpixels;
  u8 fontrows;
  int rows;
  u32 fgcolor;
  u32 bgcolor;
  u32 *buf;
  u32 *buf_row;
  u32 *buf_char;
  u32 disp;
  u16 font_row;
  u16 mask;
  u8 cfstart;
  u8 cfwidth;
  u8 cfheight;
  u8 split_fontrows;
  u8 split_textrow;
  bool cursor_visible;
  bool gfxcharw9;
  bool invert;
  bool forceUpdate;
  bool split_screen;
  u32 text_palette[16];

  //  UNUSED(nrows);
  forceUpdate = 0;
  if (charmap_updated) {
    forceUpdate = 1;
    charmap_updated = 0;
  }

  for (i = 0; i < 16; i++) {
    text_palette[i] = palette[theVGA->get_actl_palette_idx(i)];
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

  disp = sdl_screen->pitch / 4;
  buf_row = (u32 *)sdl_screen->pixels;

  // first invalidate character at previous and new cursor location
  if ((prev_cursor_y < text_rows) && (prev_cursor_x < text_cols)) {
    curs = prev_cursor_y * tm_info.line_offset + prev_cursor_x * 2;
    old_text[curs] = ~new_text[curs];
  }

  cursor_visible =
      ((tm_info.cs_start <= tm_info.cs_end) && (tm_info.cs_start < fontheight));
  if ((cursor_visible) && (cursor_y < text_rows) && (cursor_x < text_cols)) {
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
  split_textrow = (line_compare + v_panning) / fontheight;
  split_fontrows = ((line_compare + v_panning) % fontheight) + 1;
  split_screen = 0;

  do {
    buf = buf_row;
    hchars = text_cols;
    if (h_panning)
      hchars++;
    cfheight = fontheight;
    cfstart = 0;
    if (split_screen) {
      if (rows == 1) {
        cfheight = (res_y - line_compare - 1) % fontheight;
        if (cfheight == 0)
          cfheight = fontheight;
      }
    } else if (v_panning) {
      if (y == 0) {
        cfheight -= v_panning;
        cfstart = v_panning;
      } else if (rows == 1) {
        cfheight = v_panning;
      }
    }

    if (!split_screen && (y == split_textrow)) {
      if ((split_fontrows - cfstart) < cfheight) {
        cfheight = split_fontrows - cfstart;
      }
    }

    new_line = new_text;
    old_line = old_text;
    x = 0;
    offset = cs_y * tm_info.line_offset;
    do {
      cfwidth = fontwidth;
      if (h_panning) {
        if (hchars > text_cols) {
          cfwidth -= h_panning;
        } else if (hchars == 1) {
          cfwidth = h_panning;
        }
      }

      // check if char needs to be updated
      if (forceUpdate || (old_text[0] != new_text[0]) ||
          (old_text[1] != new_text[1])) {

        // Get Foreground/Background pixel colors
        fgcolor = text_palette[new_text[1] & 0x0F];
        bgcolor = text_palette[(new_text[1] >> 4) & 0x0F];
        invert = ((offset == curs) && (cursor_visible));
        gfxcharw9 = ((tm_info.line_graphics) && ((new_text[0] & 0xE0) == 0xC0));

        // Display this one char
        fontrows = cfheight;
        fontline = cfstart;
        if (y > 0) {
          pfont_row = (u8 *)&vga_charmap[(new_text[0] << 5)];
        } else {
          pfont_row = (u8 *)&vga_charmap[(new_text[0] << 5) + cfstart];
        }

        buf_char = buf;
        do {
          font_row = *pfont_row++;
          if (gfxcharw9) {
            font_row = (font_row << 1) | (font_row & 0x01);
          } else {
            font_row <<= 1;
          }

          if (hchars > text_cols) {
            font_row <<= h_panning;
          }

          fontpixels = cfwidth;
          if ((invert) && (fontline >= tm_info.cs_start) &&
              (fontline <= tm_info.cs_end))
            mask = 0x100;
          else
            mask = 0x00;
          do {
            if ((font_row & 0x100) == mask)
              *buf = bgcolor;
            else
              *buf = fgcolor;
            buf++;
            font_row <<= 1;
          } while (--fontpixels);
          buf -= cfwidth;
          buf += disp;
          fontline++;
        } while (--fontrows);

        // restore output buffer ptr to start of this char
        buf = buf_char;
      }

      // move to next char location on screen
      buf += cfwidth;

      // select next char in old/new text
      new_text += 2;
      old_text += 2;
      offset += 2;
      x++;

      // process one entire horizontal row
    } while (--hchars);

    // go to next character row location
    buf_row += disp * cfheight;
    if (!split_screen && (y == split_textrow)) {
      new_text = text_base;
      forceUpdate = 1;
      cs_y = 0;
      if (tm_info.split_hpanning)
        h_panning = 0;
      rows = ((res_y - line_compare + fontheight - 2) / fontheight) + 1;
      split_screen = 1;
    } else {
      new_text = new_line + tm_info.line_offset;
      old_text = old_line + tm_info.line_offset;
      cs_y++;
      y++;
    }
  } while (--rows);
  h_panning = tm_info.h_panning;
  prev_cursor_x = cursor_x;
  prev_cursor_y = cursor_y;
}

void bx_sdl_gui_c::graphics_tile_update(u8 *snapshot, unsigned x, unsigned y) {
  u32 *buf;

  u32 disp;
  u32 *buf_row;
  int i;
  int j;

  disp = sdl_screen->pitch / 4;
  buf = (u32 *)sdl_screen->pixels + /*(headerbar_height+y)*disp +*/ x;

  i = tileheight;
  if (i + y > res_y)
    i = res_y - y;

  // FIXME
  if (i <= 0)
    return;

  switch (vga_bpp) {
  case 8: /* 8 bpp */
    do {
      buf_row = buf;
      j = tilewidth;
      do {
        *buf++ = palette[*snapshot++];
      } while (--j);
      buf = buf_row + disp;
    } while (--i);
    break;

  default:
    BX_PANIC(("%u bpp modes handled by new graphics API", vga_bpp));
    return;
  }
}

bx_svga_tileinfo_t *bx_sdl_gui_c::graphics_tile_info(bx_svga_tileinfo_t *info) {
  if (!info) {
    info = (bx_svga_tileinfo_t *)malloc(sizeof(bx_svga_tileinfo_t));
    if (!info) {
      return NULL;
    }
  }

  info->bpp = sdl_screen->format->BitsPerPixel;
  info->pitch = sdl_screen->pitch;
  info->red_shift = sdl_screen->format->Rshift + 8 - sdl_screen->format->Rloss;
  info->green_shift =
      sdl_screen->format->Gshift + 8 - sdl_screen->format->Gloss;
  info->blue_shift = sdl_screen->format->Bshift + 8 - sdl_screen->format->Bloss;
  info->red_mask = sdl_screen->format->Rmask;
  info->green_mask = sdl_screen->format->Gmask;
  info->blue_mask = sdl_screen->format->Bmask;
  info->is_indexed = (sdl_screen->format->palette != NULL);

#ifdef BX_LITTLE_ENDIAN
  info->is_little_endian = 1;
#else
  info->is_little_endian = 0;
#endif
  return info;
}

u8 *bx_sdl_gui_c::graphics_tile_get(unsigned x0, unsigned y0, unsigned *w,
                                    unsigned *h) {
  if (x0 + tilewidth > res_x)
    *w = res_x - x0;
  else
    *w = tilewidth;

  if (y0 + tileheight > res_y)
    *h = res_y - y0;
  else
    *h = tileheight;

  return (u8 *)sdl_screen->pixels + sdl_screen->pitch * y0 +
         sdl_screen->format->BytesPerPixel * x0;
}

void bx_sdl_gui_c::graphics_tile_update_in_place(unsigned x0, unsigned y0,
                                                 unsigned w, unsigned h) {}
static u32 sdl_sym_to_bx_key(SDLKey sym) {
  switch (sym) {

  //  case SDLK_UNKNOWN:              return BX_KEY_UNKNOWN;
  //  case SDLK_FIRST:                return BX_KEY_FIRST;
  case SDLK_BACKSPACE:
    return BX_KEY_BACKSPACE;

  case SDLK_TAB:
    return BX_KEY_TAB;

  //  case SDLK_CLEAR:                return BX_KEY_CLEAR;
  case SDLK_RETURN:
    return BX_KEY_ENTER;

  case SDLK_PAUSE:
    return BX_KEY_PAUSE;

  case SDLK_ESCAPE:
    return BX_KEY_ESC;

  case SDLK_SPACE:
    return BX_KEY_SPACE;

  //  case SDLK_EXCLAIM:              return BX_KEY_EXCLAIM;
  //  case SDLK_QUOTEDBL:             return BX_KEY_QUOTEDBL;
  //  case SDLK_HASH:                 return BX_KEY_HASH;
  //  case SDLK_DOLLAR:               return BX_KEY_DOLLAR;
  //  case SDLK_AMPERSAND:            return BX_KEY_AMPERSAND;
  case SDLK_QUOTE:
    return BX_KEY_SINGLE_QUOTE;

  //  case SDLK_LEFTPAREN:            return BX_KEY_LEFTPAREN;
  //  case SDLK_RIGHTPAREN:           return BX_KEY_RIGHTPAREN;
  //  case SDLK_ASTERISK:             return BX_KEY_ASTERISK;
  //  case SDLK_PLUS:                 return BX_KEY_PLUS;
  case SDLK_COMMA:
    return BX_KEY_COMMA;

  case SDLK_MINUS:
    return BX_KEY_MINUS;

  case SDLK_PERIOD:
    return BX_KEY_PERIOD;

  case SDLK_SLASH:
    return BX_KEY_SLASH;

  case SDLK_0:
    return BX_KEY_0;

  case SDLK_1:
    return BX_KEY_1;

  case SDLK_2:
    return BX_KEY_2;

  case SDLK_3:
    return BX_KEY_3;

  case SDLK_4:
    return BX_KEY_4;

  case SDLK_5:
    return BX_KEY_5;

  case SDLK_6:
    return BX_KEY_6;

  case SDLK_7:
    return BX_KEY_7;

  case SDLK_8:
    return BX_KEY_8;

  case SDLK_9:
    return BX_KEY_9;

  //  case SDLK_COLON:                return BX_KEY_COLON;
  case SDLK_SEMICOLON:
    return BX_KEY_SEMICOLON;

  //  case SDLK_LESS:                 return BX_KEY_LESS;
  case SDLK_EQUALS:
    return BX_KEY_EQUALS;

  //  case SDLK_GREATER:              return BX_KEY_GREATER;
  //  case SDLK_QUESTION:             return BX_KEY_QUESTION;
  //  case SDLK_AT:                   return BX_KEY_AT;

  /*
 Skip uppercase letters
*/
  case SDLK_LEFTBRACKET:
    return BX_KEY_LEFT_BRACKET;

  case SDLK_BACKSLASH:
    return BX_KEY_BACKSLASH;

  case SDLK_RIGHTBRACKET:
    return BX_KEY_RIGHT_BRACKET;

  //  case SDLK_CARET:                return BX_KEY_CARET;
  //  case SDLK_UNDERSCORE:           return BX_KEY_UNDERSCORE;
  case SDLK_BACKQUOTE:
    return BX_KEY_GRAVE;

  case SDLK_a:
    return BX_KEY_A;

  case SDLK_b:
    return BX_KEY_B;

  case SDLK_c:
    return BX_KEY_C;

  case SDLK_d:
    return BX_KEY_D;

  case SDLK_e:
    return BX_KEY_E;

  case SDLK_f:
    return BX_KEY_F;

  case SDLK_g:
    return BX_KEY_G;

  case SDLK_h:
    return BX_KEY_H;

  case SDLK_i:
    return BX_KEY_I;

  case SDLK_j:
    return BX_KEY_J;

  case SDLK_k:
    return BX_KEY_K;

  case SDLK_l:
    return BX_KEY_L;

  case SDLK_m:
    return BX_KEY_M;

  case SDLK_n:
    return BX_KEY_N;

  case SDLK_o:
    return BX_KEY_O;

  case SDLK_p:
    return BX_KEY_P;

  case SDLK_q:
    return BX_KEY_Q;

  case SDLK_r:
    return BX_KEY_R;

  case SDLK_s:
    return BX_KEY_S;

  case SDLK_t:
    return BX_KEY_T;

  case SDLK_u:
    return BX_KEY_U;

  case SDLK_v:
    return BX_KEY_V;

  case SDLK_w:
    return BX_KEY_W;

  case SDLK_x:
    return BX_KEY_X;

  case SDLK_y:
    return BX_KEY_Y;

  case SDLK_z:
    return BX_KEY_Z;

  case SDLK_DELETE:
    return BX_KEY_DELETE;

  /* End of ASCII mapped keysyms */

  /* Numeric keypad */
  case SDLK_KP0:
    return BX_KEY_KP_INSERT;

  case SDLK_KP1:
    return BX_KEY_KP_END;

  case SDLK_KP2:
    return BX_KEY_KP_DOWN;

  case SDLK_KP3:
    return BX_KEY_KP_PAGE_DOWN;

  case SDLK_KP4:
    return BX_KEY_KP_LEFT;

  case SDLK_KP5:
    return BX_KEY_KP_5;

  case SDLK_KP6:
    return BX_KEY_KP_RIGHT;

  case SDLK_KP7:
    return BX_KEY_KP_HOME;

  case SDLK_KP8:
    return BX_KEY_KP_UP;

  case SDLK_KP9:
    return BX_KEY_KP_PAGE_UP;

  case SDLK_KP_PERIOD:
    return BX_KEY_KP_DELETE;

  case SDLK_KP_DIVIDE:
    return BX_KEY_KP_DIVIDE;

  case SDLK_KP_MULTIPLY:
    return BX_KEY_KP_MULTIPLY;

  case SDLK_KP_MINUS:
    return BX_KEY_KP_SUBTRACT;

  case SDLK_KP_PLUS:
    return BX_KEY_KP_ADD;

  case SDLK_KP_ENTER:
    return BX_KEY_KP_ENTER;

  //  case SDLK_KP_EQUALS:            return BX_KEY_KP_EQUALS;

  /* Arrows + Home/End pad */
  case SDLK_UP:
    return BX_KEY_UP;

  case SDLK_DOWN:
    return BX_KEY_DOWN;

  case SDLK_RIGHT:
    return BX_KEY_RIGHT;

  case SDLK_LEFT:
    return BX_KEY_LEFT;

  case SDLK_INSERT:
    return BX_KEY_INSERT;

  case SDLK_HOME:
    return BX_KEY_HOME;

  case SDLK_END:
    return BX_KEY_END;

  case SDLK_PAGEUP:
    return BX_KEY_PAGE_UP;

  case SDLK_PAGEDOWN:
    return BX_KEY_PAGE_DOWN;

  /* Function keys */
  case SDLK_F1:
    return BX_KEY_F1;

  case SDLK_F2:
    return BX_KEY_F2;

  case SDLK_F3:
    return BX_KEY_F3;

  case SDLK_F4:
    return BX_KEY_F4;

  case SDLK_F5:
    return BX_KEY_F5;

  case SDLK_F6:
    return BX_KEY_F6;

  case SDLK_F7:
    return BX_KEY_F7;

  case SDLK_F8:
    return BX_KEY_F8;

  case SDLK_F9:
    return BX_KEY_F9;

  case SDLK_F10:
    return BX_KEY_F10;

  case SDLK_F11:
    return BX_KEY_F11;

  case SDLK_F12:
    return BX_KEY_F12;

  //  case SDLK_F13:                  return BX_KEY_F13;
  //  case SDLK_F14:                  return BX_KEY_F14;
  //  case SDLK_F15:                  return BX_KEY_F15;

  /* Key state modifier keys */
  case SDLK_NUMLOCK:
    return BX_KEY_NUM_LOCK;

  case SDLK_CAPSLOCK:
    return BX_KEY_CAPS_LOCK;

  case SDLK_SCROLLOCK:
    return BX_KEY_SCRL_LOCK;

  case SDLK_RSHIFT:
    return BX_KEY_SHIFT_R;

  case SDLK_LSHIFT:
    return BX_KEY_SHIFT_L;

  case SDLK_RCTRL:
    return BX_KEY_CTRL_R;

  case SDLK_LCTRL:
    return BX_KEY_CTRL_L;

  case SDLK_RALT:
    return BX_KEY_ALT_R;

  case SDLK_LALT:
    return BX_KEY_ALT_L;

  case SDLK_RMETA:
    return BX_KEY_ALT_R;

  case SDLK_LMETA:
    return BX_KEY_WIN_L;

  case SDLK_LSUPER:
    return BX_KEY_WIN_L;

  case SDLK_RSUPER:
    return BX_KEY_WIN_R;

  //  case SDLK_MODE:                 return BX_KEY_MODE;
  //  case SDLK_COMPOSE:              return BX_KEY_COMPOSE;

  /* Miscellaneous function keys */
  case SDLK_PRINT:
    return BX_KEY_PRINT;

  case SDLK_BREAK:
    return BX_KEY_PAUSE;

  case SDLK_MENU:
    return BX_KEY_MENU;
#if 0

  case SDLK_HELP:
    return BX_KEY_HELP;

  case SDLK_SYSREQ:
    return BX_KEY_SYSREQ;

  case SDLK_POWER:
    return BX_KEY_POWER;

  case SDLK_EURO:
    return BX_KEY_EURO;

  case SDLK_UNDO:
    return BX_KEY_UNDO;
#endif

  default:
    BX_ERROR(("sdl keysym %d not mapped", (int)sym));
    return BX_KEY_UNHANDLED;
  }
}

void bx_sdl_gui_c::handle_events(void) {
  u32 key_event;

  //  u8 mouse_state;
  int wheel_status;

  while (SDL_PollEvent(&sdl_event)) {
    wheel_status = 0;
    switch (sdl_event.type) {
    case SDL_VIDEOEXPOSE:
      SDL_UpdateRect(sdl_screen, 0, 0, res_x, res_y);
      break;

    case SDL_MOUSEMOTION:

    //	//fprintf (stderr, "mouse event to (%d,%d), relative (%d,%d)\n",
    //(int)(sdl_event.motion.x), (int)(sdl_event.motion.y),
    //(int)sdl_event.motion.xrel, (int)sdl_event.motion.yrel); 	if (!sdl_grab) {
    //	  //fprintf (stderr, "ignore mouse event because sdl_grab is off\n");
    //	  break;
    //	}
    //	if (just_warped
    //	    && sdl_event.motion.x == half_res_x
    //	    && sdl_event.motion.y == half_res_y) {
    //	  // This event was generated as a side effect of the WarpMouse,
    //	  // and it must be ignored.
    //	  //fprintf (stderr, "ignore mouse event because it is a side effect of
    //SDL_WarpMouse\n"); 	  just_warped = false; 	  break;
    //	}
    //	//fprintf (stderr, "processing relative mouse event\n");
    //        new_mousebuttons = ((sdl_event.motion.state &
    //        0x01)|((sdl_event.motion.state>>1)&0x02)
    //                            |((sdl_event.motion.state<<1)&0x04));
    //        DEV_mouse_motion_ext(
    //            sdl_event.motion.xrel,
    //            -sdl_event.motion.yrel,
    //            wheel_status,
    //            new_mousebuttons);
    //	old_mousebuttons = new_mousebuttons;
    //	old_mousex = (int)(sdl_event.motion.x);
    //	old_mousey = (int)(sdl_event.motion.y);
    //	//fprintf (stderr, "warping mouse to center\n");
    //	SDL_WarpMouse(half_res_x, half_res_y);
    //	just_warped = 1;
    //	break;
    //
    case SDL_MOUSEBUTTONDOWN:

    //        if( (sdl_event.button.button == SDL_BUTTON_MIDDLE)
    //            && ((SDL_GetModState() & KMOD_CTRL) > 0)
    //            && (sdl_fullscreen_toggle == 0) )
    //	{
    //	  if( sdl_grab == 0 )
    //	  {
    //	    SDL_ShowCursor(0);
    //	    SDL_WM_GrabInput(SDL_GRAB_ON);
    //	  }
    //	  else
    //	  {
    //	    SDL_ShowCursor(1);
    //	    SDL_WM_GrabInput(SDL_GRAB_OFF);
    //	  }
    //	  sdl_grab = ~sdl_grab;
    //	  toggle_mouse_enable();
    //	  break;
    //	}
    //#ifdef SDL_BUTTON_WHEELUP
    //        // get the wheel status
    //        if (sdl_event.button.button == SDL_BUTTON_WHEELUP) {
    //          wheel_status = 1;
    //        }
    //        if (sdl_event.button.button == SDL_BUTTON_WHEELDOWN) {
    //          wheel_status = -1;
    //        }
    //#endif
    case SDL_MOUSEBUTTONUP:

      //	// figure out mouse state
      //	new_mousex = (int)(sdl_event.button.x);
      //	new_mousey = (int)(sdl_event.button.y);
      //	// SDL_GetMouseState() returns the state of all buttons
      //	mouse_state = SDL_GetMouseState(NULL, NULL);
      //	new_mousebuttons =
      //	  (mouse_state & 0x01)    |
      //	  ((mouse_state>>1)&0x02) |
      //	  ((mouse_state<<1)&0x04) ;
      //	// filter out middle button if not fullscreen
      //	if( sdl_fullscreen_toggle == 0 )
      //	  new_mousebuttons &= 0x07;
      //        // send motion information
      //        DEV_mouse_motion_ext(
      //            new_mousex - old_mousex,
      //            -(new_mousey - old_mousey),
      //            wheel_status,
      //            new_mousebuttons);
      //	// mark current state to diff with next packet
      //	old_mousebuttons = new_mousebuttons;
      //	old_mousex = new_mousex;
      //	old_mousey = new_mousey;
      break;

    //
    case SDL_KEYDOWN:

      // convert sym->bochs code
      if (sdl_event.key.keysym.sym > SDLK_LAST)
        break;
      if (!myCfg->get_bool_value("keyboard.use_mapping", false))

      //    if (!SIM->get_param_bool(BXPN_KBD_USEMAPPING)->get())
      {
        key_event = sdl_sym_to_bx_key(sdl_event.key.keysym.sym);
#if defined(DEBUG_KBD)
        BX_DEBUG(("keypress scancode=%d, sym=%d, bx_key = %d",
                  sdl_event.key.keysym.scancode, sdl_event.key.keysym.sym,
                  key_event));
#endif
      } else {

        /* use mapping */
        BXKeyEntry *entry = bx_keymap->findHostKey(sdl_event.key.keysym.sym);
        if (!entry) {
          BX_ERROR(("host key %d (0x%x) not mapped!",
                    (unsigned)sdl_event.key.keysym.sym,
                    (unsigned)sdl_event.key.keysym.sym));
          break;
        }

        key_event = entry->baseKey;
      }

      if (key_event == BX_KEY_UNHANDLED)
        break;
      theKeyboard->gen_scancode(key_event);
      if ((key_event == BX_KEY_NUM_LOCK) || (key_event == BX_KEY_CAPS_LOCK)) {
        theKeyboard->gen_scancode(key_event | BX_KEY_RELEASED);
      }
      break;

    case SDL_KEYUP:

      // filter out release of Windows/Fullscreen toggle and unsupported keys
      if ((sdl_event.key.keysym.sym != SDLK_SCROLLOCK) &&
          (sdl_event.key.keysym.sym < SDLK_LAST)) {

        // convert sym->bochs code
        if (!myCfg->get_bool_value("keyboard.use_mapping", false)) {

          // if (!SIM->get_param_bool(BXPN_KBD_USEMAPPING)->get()) {
          key_event = sdl_sym_to_bx_key(sdl_event.key.keysym.sym);
        } else {

          /* use mapping */
          BXKeyEntry *entry = bx_keymap->findHostKey(sdl_event.key.keysym.sym);
          if (!entry) {
            BX_ERROR(("host key %d (0x%x) not mapped!",
                      (unsigned)sdl_event.key.keysym.sym,
                      (unsigned)sdl_event.key.keysym.sym));
            break;
          }

          key_event = entry->baseKey;
        }

        if (key_event == BX_KEY_UNHANDLED)
          break;
        if ((key_event == BX_KEY_NUM_LOCK) || (key_event == BX_KEY_CAPS_LOCK)) {
          theKeyboard->gen_scancode(key_event);
        }

        theKeyboard->gen_scancode(key_event | BX_KEY_RELEASED);
      }
      break;

    case SDL_QUIT:
      FAILURE(Graceful, "User requested shutdown");
    }
  }
}

/**
 * Flush any changes to sdl_screen to the actual window.
 **/
void bx_sdl_gui_c::flush(void) {
  SDL_UpdateRect(sdl_screen, 0, 0, res_x, res_y);
}

/**
 * Clear sdl_screen display, and flush it.
 **/
void bx_sdl_gui_c::clear_screen(void) {
  int i = res_y;

  int j;
  u32 color;
  u32 *buf;
  u32 *buf_row;
  u32 disp;

  if (!sdl_screen)
    return;

  color = SDL_MapRGB(sdl_screen->format, 0, 0, 0);
  disp = sdl_screen->pitch / 4;
  buf = (u32 *)sdl_screen->pixels;

  do {
    buf_row = buf;
    j = res_x;
    while (j--)
      *buf++ = color;
    buf = buf_row + disp;
  } while (--i);

  flush();
}

/**
 * Set palette-entry index to the desired value.
 *
 * The palette is used in text-mode and in 8bpp VGA mode.
 **/
bool bx_sdl_gui_c::palette_change(unsigned index, unsigned red, unsigned green,
                                  unsigned blue) {
  unsigned char palred = red & 0xFF;
  unsigned char palgreen = green & 0xFF;
  unsigned char palblue = blue & 0xFF;

  if (index > 255)
    return 0;

  palette[index] = SDL_MapRGB(sdl_screen->format, palred, palgreen, palblue);

  return 1;
}

void bx_sdl_gui_c::dimension_update(unsigned x, unsigned y, unsigned fheight,
                                    unsigned fwidth, unsigned bpp) {
  if ((bpp == 8) || (bpp == 15) || (bpp == 16) || (bpp == 24) || (bpp == 32)) {
    vga_bpp = bpp;
  } else {
    FAILURE_1(SDL, "%d bpp graphics mode not supported.", bpp);
  }

  if (fheight > 0) {
    fontheight = fheight;
    fontwidth = fwidth;
    text_cols = x / fontwidth;
    text_rows = y / fontheight;
  }

  if ((x == res_x) && (y == res_y))
    return;

  if (sdl_screen) {
    SDL_FreeSurface(sdl_screen);
    sdl_screen = NULL;
  }

  sdl_screen = SDL_SetVideoMode(x, y /*+headerbar_height+statusbar_height*/, 32,
                                SDL_SWSURFACE);
  if (!sdl_screen) {
    FAILURE_3(SDL, "Unable to set requested videomode: %ix%i: %s   \n", x, y,
              SDL_GetError());
  }

  res_x = x;
  res_y = y;
  half_res_x = x / 2;
  half_res_y = y / 2;
}

void bx_sdl_gui_c::mouse_enabled_changed_specific(bool val) {
  if (val == 1) {
    SDL_ShowCursor(0);
    SDL_WM_GrabInput(SDL_GRAB_ON);
  } else {
    SDL_ShowCursor(1);
    SDL_WM_GrabInput(SDL_GRAB_OFF);
  }

  sdl_grab = val;
}

void bx_sdl_gui_c::exit(void) {
  if (sdl_screen)
    SDL_FreeSurface(sdl_screen);
}

/// key mapping for SDL
typedef struct {
  const char *name;
  u32 value;
} keyTableEntry;

#define DEF_SDL_KEY(key) {#key, key},

keyTableEntry keytable[] = {
// this include provides all the entries.
#include "sdlkeys.hpp"
    // one final entry to mark the end
    {NULL, 0}};

// function to convert key names into SDLKey values.
// This first try will be horribly inefficient, but it only has
// to be done while loading a keymap.  Once the simulation starts,

// this function won't be called.
static u32 convertStringToSDLKey(const char *string) {
  keyTableEntry *ptr;
  for (ptr = &keytable[0]; ptr->name != NULL; ptr++) {
#if defined(DEBUG_SDL_KEY)
    printf("SDL: comparing string '%s' to SDL key '%s'   \n", string,
           ptr->name);
#endif
    if (!strcmp(string, ptr->name))
      return ptr->value;
  }

  return BX_KEYMAP_UNKNOWN;
}
#endif // defined(HAVE_SDL)
