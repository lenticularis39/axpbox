/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Website: https://github.com/lenticularis39/axpbox
 *
 * Forked from: ES40 emulator
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 * Copyright (C) 2007 by Camiel Vanderhoeven
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * Although this is not required, the author would appreciate being notified of,
 * and receiving any modifications you may make to the source code that might
 * serve the general public.
 */

/**
 * \file
 * Contains code macros for the processor BWX (byte and word extension)
 *instructions. Based on ARM chapter 4.6.
 *
 * $Id: cpu_bwx.h,v 1.10 2008/03/14 15:30:52 iamcamiel Exp $
 *
 * X-1.9        Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.8        Camiel Vanderhoeven                             18-JAN-2008
 *      Replaced sext_64 inlines with sext_u64_<bits> inlines for
 *      performance reasons (thanks to David Hittner for spotting this!);
 *
 * X-1.7        Camiel Vanderhoeven                             2-DEC-2007
 *      Use sext_64 inline.
 *
 * X-1.6       Camiel Vanderhoeven                             11-APR-2007
 *      Moved all data that should be saved to a state file to a structure
 *      "state".
 *
 * X-1.5        Camiel Vanderhoeven                             30-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.4        Camiel Vanderhoeven                             7-MAR-2007
 *      Bugfix in INSxH. The Wrong Thing(tm) was done when V_2 = 0. Fixes
 *      bug # 1676093.
 *
 * X-1.3        Camiel Vanderhoeven                             7-MAR-2007
 *      Bugfix in EXTxH. The Wrong Thing(tm) was done when V_2 = 0. Fixes
 *      bugs # 1667015, 1667018, 1674311, 1676079 and 1676081.
 *
 * X-1.2        Camiel Vanderhoeven                             19-FEB-2007
 *      Fixed a compiler-dependent bug (possible >> or <<by 64) in EXTxH,
 *      INSxH and MSKxH SRA opcodes.
 *
 * X-1.1        Camiel Vanderhoeven                             18-FEB-2007
 *      File created. Contains code previously found in AlphaCPU.h
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#define DO_CMPBGE                                                              \
  state.r[REG_3] =                                                             \
      (((u8)(state.r[REG_1] & 0xff) >= (u8)(V_2 & 0xff)) ? 1 : 0) |            \
      (((u8)((state.r[REG_1] >> 8) & 0xff) >= (u8)((V_2 >> 8) & 0xff)) ? 2     \
                                                                       : 0) |  \
      (((u8)((state.r[REG_1] >> 16) & 0xff) >= (u8)((V_2 >> 16) & 0xff))       \
           ? 4                                                                 \
           : 0) |                                                              \
      (((u8)((state.r[REG_1] >> 24) & 0xff) >= (u8)((V_2 >> 24) & 0xff))       \
           ? 8                                                                 \
           : 0) |                                                              \
      (((u8)((state.r[REG_1] >> 32) & 0xff) >= (u8)((V_2 >> 32) & 0xff))       \
           ? 16                                                                \
           : 0) |                                                              \
      (((u8)((state.r[REG_1] >> 40) & 0xff) >= (u8)((V_2 >> 40) & 0xff))       \
           ? 32                                                                \
           : 0) |                                                              \
      (((u8)((state.r[REG_1] >> 48) & 0xff) >= (u8)((V_2 >> 48) & 0xff))       \
           ? 64                                                                \
           : 0) |                                                              \
      (((u8)((state.r[REG_1] >> 56) & 0xff) >= (u8)((V_2 >> 56) & 0xff)) ? 128 \
                                                                         : 0);

#define DO_EXTBL                                                               \
  state.r[REG_3] = (state.r[REG_1] >> ((V_2 & 7) * 8)) & X64_BYTE;
#define DO_EXTWL                                                               \
  state.r[REG_3] = (state.r[REG_1] >> ((V_2 & 7) * 8)) & X64_WORD;
#define DO_EXTLL                                                               \
  state.r[REG_3] = (state.r[REG_1] >> ((V_2 & 7) * 8)) & X64_LONG;
#define DO_EXTQL state.r[REG_3] = (state.r[REG_1] >> ((V_2 & 7) * 8));
#define DO_EXTWH                                                               \
  state.r[REG_3] = (state.r[REG_1] << ((64 - ((V_2 & 7) * 8)) & 63)) & X64_WORD;
#define DO_EXTLH                                                               \
  state.r[REG_3] = (state.r[REG_1] << ((64 - ((V_2 & 7) * 8)) & 63)) & X64_LONG;
#define DO_EXTQH                                                               \
  state.r[REG_3] = (state.r[REG_1] << ((64 - ((V_2 & 7) * 8)) & 63)) & X64_QUAD;

#define DO_INSBL                                                               \
  state.r[REG_3] = (state.r[REG_1] & X64_BYTE) << ((V_2 & 7) * 8);
#define DO_INSWL                                                               \
  state.r[REG_3] = (state.r[REG_1] & X64_WORD) << ((V_2 & 7) * 8);
#define DO_INSLL                                                               \
  state.r[REG_3] = (state.r[REG_1] & X64_LONG) << ((V_2 & 7) * 8);
#define DO_INSQL state.r[REG_3] = (state.r[REG_1]) << ((V_2 & 7) * 8);
#define DO_INSWH                                                               \
  state.r[REG_3] =                                                             \
      (V_2 & 7)                                                                \
          ? ((state.r[REG_1] & X64_WORD) >> ((64 - ((V_2 & 7) * 8)) & 63))     \
          : 0;
#define DO_INSLH                                                               \
  state.r[REG_3] =                                                             \
      (V_2 & 7)                                                                \
          ? ((state.r[REG_1] & X64_LONG) >> ((64 - ((V_2 & 7) * 8)) & 63))     \
          : 0;
#define DO_INSQH                                                               \
  state.r[REG_3] =                                                             \
      (V_2 & 7)                                                                \
          ? ((state.r[REG_1] & X64_QUAD) >> ((64 - ((V_2 & 7) * 8)) & 63))     \
          : 0;

#define DO_MSKBL                                                               \
  state.r[REG_3] = state.r[REG_1] & ~(X64_BYTE << ((V_2 & 7) * 8));
#define DO_MSKWL                                                               \
  state.r[REG_3] = state.r[REG_1] & ~(X64_WORD << ((V_2 & 7) * 8));
#define DO_MSKLL                                                               \
  state.r[REG_3] = state.r[REG_1] & ~(X64_LONG << ((V_2 & 7) * 8));
#define DO_MSKQL                                                               \
  state.r[REG_3] = state.r[REG_1] & ~(X64_QUAD << ((V_2 & 7) * 8));
#define DO_MSKWH                                                               \
  state.r[REG_3] =                                                             \
      (V_2 & 7)                                                                \
          ? (state.r[REG_1] & ~(X64_WORD >> ((64 - ((V_2 & 7) * 8)) & 63)))    \
          : state.r[REG_1];
#define DO_MSKLH                                                               \
  state.r[REG_3] =                                                             \
      (V_2 & 7)                                                                \
          ? (state.r[REG_1] & ~(X64_LONG >> ((64 - ((V_2 & 7) * 8)) & 63)))    \
          : state.r[REG_1];
#define DO_MSKQH                                                               \
  state.r[REG_3] =                                                             \
      (V_2 & 7)                                                                \
          ? (state.r[REG_1] & ~(X64_QUAD >> ((64 - ((V_2 & 7) * 8)) & 63)))    \
          : state.r[REG_1];

#define DO_SEXTB state.r[REG_3] = sext_u64_8(V_2);
#define DO_SEXTW state.r[REG_3] = sext_u64_16(V_2);

#define DO_ZAP                                                                 \
  state.r[REG_3] =                                                             \
      state.r[REG_1] &                                                         \
      (((V_2 & 1) ? 0 : U64(0xff)) | ((V_2 & 2) ? 0 : U64(0xff00)) |           \
       ((V_2 & 4) ? 0 : U64(0xff0000)) | ((V_2 & 8) ? 0 : U64(0xff000000)) |   \
       ((V_2 & 16) ? 0 : U64(0xff00000000)) |                                  \
       ((V_2 & 32) ? 0 : U64(0xff0000000000)) |                                \
       ((V_2 & 64) ? 0 : U64(0xff000000000000)) |                              \
       ((V_2 & 128) ? 0 : U64(0xff00000000000000)));

#define DO_ZAPNOT                                                              \
  state.r[REG_3] =                                                             \
      state.r[REG_1] &                                                         \
      (((V_2 & 1) ? U64(0xff) : 0) | ((V_2 & 2) ? U64(0xff00) : 0) |           \
       ((V_2 & 4) ? U64(0xff0000) : 0) | ((V_2 & 8) ? U64(0xff000000) : 0) |   \
       ((V_2 & 16) ? U64(0xff00000000) : 0) |                                  \
       ((V_2 & 32) ? U64(0xff0000000000) : 0) |                                \
       ((V_2 & 64) ? U64(0xff000000000000) : 0) |                              \
       ((V_2 & 128) ? U64(0xff00000000000000) : 0));
