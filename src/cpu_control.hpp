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
 * Contains code macros for the processor control instructions.
 * Based on ARM chapter 4.3
 *
 * $Id: cpu_control.h,v 1.7 2008/03/14 15:30:52 iamcamiel Exp $
 *
 * X-1.6        Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.5        Camiel Vanderhoeven                             30-JAN-2008
 *      Always use set_pc or add_pc to change the program counter.
 *
 * X-1.4        Camiel Vanderhoeven                             30-JAN-2008
 *      Remember number of instructions left in current memory page, so
 *      that the translation-buffer doens't need to be consulted on every
 *      instruction fetch when the Icache is disabled.
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
 **/
#define DO_BEQ                                                                 \
  if (!state.r[REG_1])                                                         \
    add_pc(DISP_21 * 4);

#define DO_BGE                                                                 \
  if ((s64)state.r[REG_1] >= 0)                                                \
    add_pc(DISP_21 * 4);

#define DO_BGT                                                                 \
  if ((s64)state.r[REG_1] > 0)                                                 \
    add_pc(DISP_21 * 4);

#define DO_BLBC                                                                \
  if (!(state.r[REG_1] & 1))                                                   \
    add_pc(DISP_21 * 4);

#define DO_BLBS                                                                \
  if (state.r[REG_1] & 1)                                                      \
    add_pc(DISP_21 * 4);

#define DO_BLE                                                                 \
  if ((s64)state.r[REG_1] <= 0)                                                \
    add_pc(DISP_21 * 4);

#define DO_BLT                                                                 \
  if ((s64)state.r[REG_1] < 0)                                                 \
    add_pc(DISP_21 * 4);

#define DO_BNE                                                                 \
  if (state.r[REG_1])                                                          \
    add_pc(DISP_21 * 4);

#define DO_BR                                                                  \
  {                                                                            \
    state.r[REG_1] = state.pc & ~U64(0x3);                                     \
    add_pc(DISP_21 * 4);                                                       \
  }

#define DO_BSR DO_BR

#define DO_JMP                                                                 \
  {                                                                            \
    temp_64 = state.r[REG_2] & ~U64(0x3);                                      \
    state.r[REG_1] = state.pc & ~U64(0x3);                                     \
    set_pc(temp_64 | (state.pc & 3));                                          \
  }

// JSR, RET and JSR_COROUTINE is really JMP, just with different prediction
// bits.
