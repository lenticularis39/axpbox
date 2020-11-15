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
 * Contains code macros for the processor logical instructions.
 * Based on ARM chapter 4.5.
 *
 * $Id: cpu_logical.h,v 1.6 2008/03/14 15:30:52 iamcamiel Exp $
 *
 * X-1.5        Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.4        Camiel Vanderhoeven                             11-APR-2007
 *      Moved all data that should be saved to a state file to a structure
 *      "state".
 *
 * X-1.3        Camiel Vanderhoeven                             30-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.2        Camiel Vanderhoeven                             19-FEB-2007
 *      Fixed a compiler-dependent bug (possible >> or <<by 64) in SRA
 *      opcode.
 *
 * X-1.1        Camiel Vanderhoeven                             18-FEB-2007
 *      File created. Contains code previously found in AlphaCPU.h
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#define DO_AND state.r[REG_3] = state.r[REG_1] & V_2;
#define DO_BIC state.r[REG_3] = state.r[REG_1] & ~V_2;
#define DO_BIS state.r[REG_3] = state.r[REG_1] | V_2;
#define DO_EQV state.r[REG_3] = state.r[REG_1] ^ ~V_2;
#define DO_ORNOT state.r[REG_3] = state.r[REG_1] | ~V_2;
#define DO_XOR state.r[REG_3] = state.r[REG_1] ^ V_2;

#define DO_CMOVEQ                                                              \
  if (!state.r[REG_1])                                                         \
    state.r[REG_3] = V_2;
#define DO_CMOVGE                                                              \
  if ((s64)state.r[REG_1] >= 0)                                                \
    state.r[REG_3] = V_2;
#define DO_CMOVGT                                                              \
  if ((s64)state.r[REG_1] > 0)                                                 \
    state.r[REG_3] = V_2;
#define DO_CMOVLBC                                                             \
  if (!(state.r[REG_1] & U64(0x1)))                                            \
    state.r[REG_3] = V_2;
#define DO_CMOVLBS                                                             \
  if (state.r[REG_1] & U64(0x1))                                               \
    state.r[REG_3] = V_2;
#define DO_CMOVLE                                                              \
  if ((s64)state.r[REG_1] <= 0)                                                \
    state.r[REG_3] = V_2;
#define DO_CMOVLT                                                              \
  if ((s64)state.r[REG_1] < 0)                                                 \
    state.r[REG_3] = V_2;
#define DO_CMOVNE                                                              \
  if (state.r[REG_1])                                                          \
    state.r[REG_3] = V_2;

#define DO_SLL state.r[REG_3] = state.r[REG_1] << (V_2 & 63);
#define DO_SRA                                                                 \
  state.r[REG_3] =                                                             \
      (V_2 & 63)                                                               \
          ? ((state.r[REG_1] >> (V_2 & 63)) |                                  \
             ((state.r[REG_1] >> 63) ? (X64_QUAD << (64 - (V_2 & 63))) : 0))   \
          : state.r[REG_1];
#define DO_SRL state.r[REG_3] = state.r[REG_1] >> (V_2 & 63);
