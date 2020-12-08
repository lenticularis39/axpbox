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

/**
 * \file
 * Definitions for VGA cards.
 *
 * \todo Split this out for the different VGA cards.
 *
 * $Id: vga.h,v 1.5 2008/03/14 15:31:29 iamcamiel Exp $
 *
 * X-1.3        Camiel Vanderhoeven                             02-JAN-2008
 *      Comments.
 *
 * X-1.2        Camiel Vanderhoeven                             7-DEC-2007
 *      Code cleanup.
 *
 * X-1.1        Camiel Vanderhoeven                             6-DEC-2007
 *      Initial version for ES40 emulator.
 *
 **/
#ifndef BX_IODEV_VGA_H
#define BX_IODEV_VGA_H

// Make colour
#define MAKE_COLOUR                                                            \
  (red, red_shiftfrom, red_shiftto, red_mask, green, green_shiftfrom,          \
   green_shiftto, green_mask, blue, blue_shiftfrom, blue_shiftto,              \
   blue_mask)(((((red_shiftto) > (red_shiftfrom))                              \
                    ? (red) << ((red_shiftto) - (red_shiftfrom))               \
                    : (red) >> ((red_shiftfrom) - (red_shiftto))) &            \
               (red_mask)) |                                                   \
              ((((green_shiftto) > (green_shiftfrom))                          \
                    ? (green) << ((green_shiftto) - (green_shiftfrom))         \
                    : (green) >> ((green_shiftfrom) - (green_shiftto))) &      \
               (green_mask)) |                                                 \
              ((((blue_shiftto) > (blue_shiftfrom))                            \
                    ? (blue) << ((blue_shiftto) - (blue_shiftfrom))            \
                    : (blue) >> ((blue_shiftfrom) - (blue_shiftto))) &         \
               (blue_mask)))
#if BX_SUPPORT_VBE
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_MB 8
#define VBE_DISPI_4BPP_PLANE_SHIFT 21

#define VBE_DISPI_BANK_ADDRESS 0xA0000
#define VBE_DISPI_BANK_SIZE_KB 64

#define VBE_DISPI_MAX_XRES 1600
#define VBE_DISPI_MAX_YRES 1200
#define VBE_DISPI_MAX_BPP 32

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA 0x01CF

#define VBE_DISPI_IOPORT_INDEX_OLD 0xFF80
#define VBE_DISPI_IOPORT_DATA_OLD 0xFF81

#define VBE_DISPI_INDEX_ID 0x0
#define VBE_DISPI_INDEX_XRES 0x1
#define VBE_DISPI_INDEX_YRES 0x2
#define VBE_DISPI_INDEX_BPP 0x3
#define VBE_DISPI_INDEX_ENABLE 0x4
#define VBE_DISPI_INDEX_BANK 0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH 0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 0x7
#define VBE_DISPI_INDEX_X_OFFSET 0x8
#define VBE_DISPI_INDEX_Y_OFFSET 0x9

#define VBE_DISPI_ID0 0xB0C0
#define VBE_DISPI_ID1 0xB0C1
#define VBE_DISPI_ID2 0xB0C2
#define VBE_DISPI_ID3 0xB0C3
#define VBE_DISPI_ID4 0xB0C4

#define VBE_DISPI_BPP_4 0x04
#define VBE_DISPI_BPP_8 0x08
#define VBE_DISPI_BPP_15 0x0F
#define VBE_DISPI_BPP_16 0x10
#define VBE_DISPI_BPP_24 0x18
#define VBE_DISPI_BPP_32 0x20

#define VBE_DISPI_DISABLED 0x00
#define VBE_DISPI_ENABLED 0x01
#define VBE_DISPI_GETCAPS 0x02
#define VBE_DISPI_8BIT_DAC 0x20
#define VBE_DISPI_LFB_ENABLED 0x40
#define VBE_DISPI_NOCLEARMEM 0x80

#define VBE_DISPI_LFB_PHYSICAL_ADDRESS 0xE0000000

#define VBE_DISPI_TOTAL_VIDEO_MEMORY_KB (VBE_DISPI_TOTAL_VIDEO_MEMORY_MB * 1024)
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_BYTES                                     \
  (VBE_DISPI_TOTAL_VIDEO_MEMORY_KB * 1024)
#define BX_MAX_XRES VBE_DISPI_MAX_XRES
#define BX_MAX_YRES VBE_DISPI_MAX_YRES

#elif BX_SUPPORT_CLGD54XX
#define BX_MAX_XRES 1280
#define BX_MAX_YRES 1024

#else
#define BX_MAX_XRES 800
#define BX_MAX_YRES 600
#endif // BX_SUPPORT_VBE
#define X_TILESIZE 16
#define Y_TILESIZE 24
#define BX_NUM_X_TILES (BX_MAX_XRES / X_TILESIZE)
#define BX_NUM_Y_TILES (BX_MAX_YRES / Y_TILESIZE)

// Support varying number of rows of text.  This used to
// be limited to only 25 lines.
#define BX_MAX_TEXT_LINES 100

//  struct {
//    u16  vbe_cur_dispi;
//    u16  vbe_xres;
//    u16  vbe_yres;
//    u16  vbe_bpp;
//    u16  vbe_max_xres;
//    u16  vbe_max_yres;
//    u16  vbe_max_bpp;
//    u16  vbe_bank;
//    bool vbe_enabled;
//    u16  vbe_curindex;
//    u32  vbe_visible_screen_size; /**< in bytes */
//    u16  vbe_offset_x;		 /**< Virtual screen x start (in pixels)
//    */ u16  vbe_offset_y;		 /**< Virtual screen y start (in pixels)
//    */ u16  vbe_virtual_xres; u16  vbe_virtual_yres; u32  vbe_virtual_start;
//    /**< For dealing with bpp>8, this is where the virtual screen starts. */
//    u8   vbe_bpp_multiplier;  /**< We have to save this b/c sometimes we need
//    to recalculate stuff with it. */ bool vbe_lfb_enabled; bool
//    vbe_get_capabilities; bool vbe_8bit_dac;
//  } s;  // state information
#endif
