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

#if defined(HAVE_NEW_FP)
#define DO_LDF                                                                 \
  FPSTART;                                                                     \
  if (FREG_1 != 31) {                                                          \
    READ_VIRT_F(state.r[REG_2] + DISP_16, 32, state.f[FREG_1], vax_ldf);       \
  }

#define DO_LDG                                                                 \
  FPSTART;                                                                     \
  if (FREG_1 != 31) {                                                          \
    READ_VIRT_F(state.r[REG_2] + DISP_16, 64, state.f[FREG_1], vax_ldg);       \
  }

#define DO_LDS                                                                 \
  FPSTART;                                                                     \
  if (FREG_1 != 31) {                                                          \
    READ_VIRT_F(state.r[REG_2] + DISP_16, 32, state.f[FREG_1], ieee_lds);      \
  }

#define DO_LDT                                                                 \
  FPSTART;                                                                     \
  if (FREG_1 != 31) {                                                          \
    READ_VIRT(state.r[REG_2] + DISP_16, 64, state.f[FREG_1]);                  \
  }

#define DO_STF                                                                 \
  FPSTART;                                                                     \
  WRITE_VIRT(state.r[REG_2] + DISP_16, 32, vax_stf(state.f[FREG_1]));

#define DO_STG                                                                 \
  FPSTART;                                                                     \
  WRITE_VIRT(state.r[REG_2] + DISP_16, 64, vax_stg(state.f[FREG_1]));

#define DO_STS                                                                 \
  FPSTART;                                                                     \
  WRITE_VIRT(state.r[REG_2] + DISP_16, 32, ieee_sts(state.f[FREG_1]));

#define DO_STT                                                                 \
  FPSTART;                                                                     \
  WRITE_VIRT(state.r[REG_2] + DISP_16, 64, state.f[FREG_1]);

#else
#define DO_LDF                                                                 \
  FPSTART;                                                                     \
  if (FREG_1 != 31) {                                                          \
    READ_VIRT_F(state.r[REG_2] + DISP_16, 32, state.f[FREG_1], load_f);        \
  }

#define DO_LDG                                                                 \
  FPSTART;                                                                     \
  if (FREG_1 != 31) {                                                          \
    READ_VIRT_F(state.r[REG_2] + DISP_16, 64, state.f[FREG_1], load_g);        \
  }

#define DO_LDS                                                                 \
  FPSTART;                                                                     \
  if (FREG_1 != 31) {                                                          \
    READ_VIRT_F(state.r[REG_2] + DISP_16, 32, state.f[FREG_1], load_s);        \
  }

#define DO_LDT                                                                 \
  FPSTART;                                                                     \
  if (FREG_1 != 31) {                                                          \
    READ_VIRT(state.r[REG_2] + DISP_16, 64, state.f[FREG_1]);                  \
  }

#define DO_STF                                                                 \
  FPSTART;                                                                     \
  WRITE_VIRT(state.r[REG_2] + DISP_16, 32, store_f(state.f[FREG_1]));

#define DO_STG                                                                 \
  FPSTART;                                                                     \
  WRITE_VIRT(state.r[REG_2] + DISP_16, 64, store_g(state.f[FREG_1]));

#define DO_STS                                                                 \
  FPSTART;                                                                     \
  WRITE_VIRT(state.r[REG_2] + DISP_16, 32, store_s(state.f[FREG_1]));

#define DO_STT                                                                 \
  FPSTART;                                                                     \
  WRITE_VIRT(state.r[REG_2] + DISP_16, 64, state.f[FREG_1]);
#endif
