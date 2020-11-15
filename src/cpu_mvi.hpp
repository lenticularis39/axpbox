/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * Website: http://sourceforge.net/projects/es40
 * E-mail : camiel@camicom.com
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
 * Contains code macros for the processor MVI (multimedia) instructions.
 *
 * $Id: cpu_mvi.h,v 1.5 2008/03/14 15:30:52 iamcamiel Exp $
 *
 * X-1.4        Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.3        Camiel Vanderhoeven                             11-APR-2007
 *      Moved all data that should be saved to a state file to a structure
 *      "state".
 *
 * X-1.2        Camiel Vanderhoeven                             30-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.1        Camiel Vanderhoeven                             18-FEB-2007
 *      File created. Contains code previously found in AlphaCPU.h
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#define DO_MINUB8                                                              \
  temp_64 = 0;                                                                 \
  temp_64_1 = state.r[REG_1];                                                  \
  temp_64_2 = V_2;                                                             \
  for (i = 0; i < 64; i += 8) {                                                \
    if ((u8)((temp_64_1 >> i) & X64_BYTE) > (u8)((temp_64_2 >> i) & X64_BYTE)) \
      temp_64 |= (((temp_64_2 >> i) & X64_BYTE) << i);                         \
    else                                                                       \
      temp_64 |= (((temp_64_1 >> i) & X64_BYTE) << i);                         \
  }                                                                            \
  state.r[REG_3] = temp_64;

#define DO_MINSB8                                                              \
  temp_64 = 0;                                                                 \
  temp_64_1 = state.r[REG_1];                                                  \
  temp_64_2 = V_2;                                                             \
  for (i = 0; i < 64; i += 8) {                                                \
    if ((s8)((temp_64_1 >> i) & X64_BYTE) > (s8)((temp_64_2 >> i) & X64_BYTE)) \
      temp_64 |= (((temp_64_2 >> i) & X64_BYTE) << i);                         \
    else                                                                       \
      temp_64 |= (((temp_64_1 >> i) & X64_BYTE) << i);                         \
  }                                                                            \
  state.r[REG_3] = temp_64;

#define DO_MINUW4                                                              \
  temp_64 = 0;                                                                 \
  temp_64_1 = state.r[REG_1];                                                  \
  temp_64_2 = V_2;                                                             \
  for (i = 0; i < 64; i += 16) {                                               \
    if ((u16)((temp_64_1 >> i) & X64_WORD) >                                   \
        (u16)((temp_64_2 >> i) & X64_WORD))                                    \
      temp_64 |= (((temp_64_2 >> i) & X64_WORD) << i);                         \
    else                                                                       \
      temp_64 |= (((temp_64_1 >> i) & X64_WORD) << i);                         \
  }                                                                            \
  state.r[REG_3] = temp_64;

#define DO_MINSW4                                                              \
  temp_64 = 0;                                                                 \
  temp_64_1 = state.r[REG_1];                                                  \
  temp_64_2 = V_2;                                                             \
  for (i = 0; i < 64; i += 16) {                                               \
    if ((s16)((temp_64_1 >> i) & X64_WORD) >                                   \
        (s16)((temp_64_2 >> i) & X64_WORD))                                    \
      temp_64 |= (((temp_64_2 >> i) & X64_WORD) << i);                         \
    else                                                                       \
      temp_64 |= (((temp_64_1 >> i) & X64_WORD) << i);                         \
  }                                                                            \
  state.r[REG_3] = temp_64;

#define DO_MAXUB8                                                              \
  temp_64 = 0;                                                                 \
  temp_64_1 = state.r[REG_1];                                                  \
  temp_64_2 = V_2;                                                             \
  for (i = 0; i < 64; i += 8) {                                                \
    if ((u8)((temp_64_1 >> i) & X64_BYTE) > (u8)((temp_64_2 >> i) & X64_BYTE)) \
      temp_64 |= (((temp_64_1 >> i) & X64_BYTE) << i);                         \
    else                                                                       \
      temp_64 |= (((temp_64_2 >> i) & X64_BYTE) << i);                         \
  }                                                                            \
  state.r[REG_3] = temp_64;

#define DO_MAXSB8                                                              \
  temp_64 = 0;                                                                 \
  temp_64_1 = state.r[REG_1];                                                  \
  temp_64_2 = V_2;                                                             \
  for (i = 0; i < 64; i += 8) {                                                \
    if ((s8)((temp_64_1 >> i) & X64_BYTE) > (s8)((temp_64_2 >> i) & X64_BYTE)) \
      temp_64 |= (((temp_64_1 >> i) & X64_BYTE) << i);                         \
    else                                                                       \
      temp_64 |= (((temp_64_2 >> i) & X64_BYTE) << i);                         \
  }                                                                            \
  state.r[REG_3] = temp_64;

#define DO_MAXUW4                                                              \
  temp_64 = 0;                                                                 \
  temp_64_1 = state.r[REG_1];                                                  \
  temp_64_2 = V_2;                                                             \
  for (i = 0; i < 64; i += 16) {                                               \
    if ((u16)((temp_64_1 >> i) & X64_WORD) >                                   \
        (u16)((temp_64_2 >> i) & X64_WORD))                                    \
      temp_64 |= (((temp_64_1 >> i) & X64_WORD) << i);                         \
    else                                                                       \
      temp_64 |= (((temp_64_2 >> i) & X64_WORD) << i);                         \
  }                                                                            \
  state.r[REG_3] = temp_64;

#define DO_MAXSW4                                                              \
  temp_64 = 0;                                                                 \
  temp_64_1 = state.r[REG_1];                                                  \
  temp_64_2 = V_2;                                                             \
  for (i = 0; i < 64; i += 16) {                                               \
    if ((s16)((temp_64_1 >> i) & X64_WORD) >                                   \
        (s16)((temp_64_2 >> i) & X64_WORD))                                    \
      temp_64 |= (((temp_64_1 >> i) & X64_WORD) << i);                         \
    else                                                                       \
      temp_64 |= (((temp_64_2 >> i) & X64_WORD) << i);                         \
  }                                                                            \
  state.r[REG_3] = temp_64;

#define DO_PERR                                                                \
  temp_64 = 0;                                                                 \
  temp_64_1 = state.r[REG_1];                                                  \
  temp_64_2 = V_2;                                                             \
  for (i = 0; i < 64; i += 8) {                                                \
    if ((s8)((temp_64_1 >> i) & X64_BYTE) > (s8)((temp_64_2 >> i) & X64_BYTE)) \
      temp_64 |= ((u64)((s8)((temp_64_1 >> i) & X64_BYTE) -                    \
                        (s8)((temp_64_2 >> i) & X64_BYTE))                     \
                  << i);                                                       \
    else                                                                       \
      temp_64 |= ((u64)((s8)((temp_64_2 >> i) & X64_BYTE) -                    \
                        (s8)((temp_64_1 >> i) & X64_BYTE))                     \
                  << i);                                                       \
  }                                                                            \
  state.r[REG_3] = temp_64;

#define DO_PKLB                                                                \
  temp_64_2 = V_2;                                                             \
  state.r[REG_3] = (temp_64_2 & U64(0x00000000000000ff)) |                     \
                   ((temp_64_2 & U64(0x000000ff00000000)) >> 24);

#define DO_PKWB                                                                \
  temp_64_2 = V_2;                                                             \
  state.r[REG_3] = (temp_64_2 & U64(0x00000000000000ff)) |                     \
                   ((temp_64_2 & U64(0x0000000000ff0000)) >> 8) |              \
                   ((temp_64_2 & U64(0x000000ff00000000)) >> 16) |             \
                   ((temp_64_2 & U64(0x00ff000000000000)) >> 24);

#define DO_UNPKBL                                                              \
  temp_64_2 = V_2;                                                             \
  state.r[REG_3] =                                                             \
      (temp_64_2 & U64(0x000000ff)) | ((temp_64_2 & U64(0x0000ff00)) << 24);

#define DO_UNPKBW                                                              \
  temp_64_2 = V_2;                                                             \
  state.r[REG_3] = (temp_64_2 & U64(0x000000ff)) |                             \
                   ((temp_64_2 & U64(0x0000ff00)) << 8) |                      \
                   ((temp_64_2 & U64(0x00ff0000)) << 16) |                     \
                   ((temp_64_2 & U64(0xff000000)) << 24);
