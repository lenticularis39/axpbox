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

#define DO_LDA state.r[REG_1] = state.r[REG_2] + DISP_16;

#define DO_LDAH state.r[REG_1] = state.r[REG_2] + (DISP_16 << 16);

#define DO_LDBU READ_VIRT(state.r[REG_2] + DISP_16, 8, state.r[REG_1]);

#define DO_LDL                                                                 \
  READ_VIRT_F(state.r[REG_2] + DISP_16, 32, state.r[REG_1], sext_u64_32);

#define DO_LDL_L                                                               \
  READ_VIRT_LOCK_F(state.r[REG_2] + DISP_16, 32, state.r[REG_1], sext_u64_32);

#define DO_LDQ READ_VIRT(state.r[REG_2] + DISP_16, 64, state.r[REG_1]);

#define DO_LDQ_L READ_VIRT_LOCK(state.r[REG_2] + DISP_16, 64, state.r[REG_1]);

#define DO_LDQ_U                                                               \
  READ_VIRT((state.r[REG_2] + DISP_16) & ~U64(0x7), 64, state.r[REG_1]);

#define DO_LDWU READ_VIRT(state.r[REG_2] + DISP_16, 16, state.r[REG_1]);

#define DO_STB WRITE_VIRT(state.r[REG_2] + DISP_16, 8, state.r[REG_1]);

#define DO_STL WRITE_VIRT(state.r[REG_2] + DISP_16, 32, state.r[REG_1]);

#define DO_STL_C                                                               \
  if (cSystem->cpu_unlock(state.iProcNum)) {                                   \
    WRITE_VIRT(state.r[REG_2] + DISP_16, 32, state.r[REG_1]);                  \
    state.r[REG_1] = 1;                                                        \
  } else                                                                       \
    state.r[REG_1] = 0;

#define DO_STQ WRITE_VIRT(state.r[REG_2] + DISP_16, 64, state.r[REG_1]);

#define DO_STQ_C                                                               \
  if (cSystem->cpu_unlock(state.iProcNum)) {                                   \
    WRITE_VIRT(state.r[REG_2] + DISP_16, 64, state.r[REG_1]);                  \
    state.r[REG_1] = 1;                                                        \
  } else                                                                       \
    state.r[REG_1] = 0;

#define DO_STQ_U                                                               \
  WRITE_VIRT((state.r[REG_2] + DISP_16) & ~U64(0x07), 64, state.r[REG_1]);

#define DO_STW WRITE_VIRT(state.r[REG_2] + DISP_16, 16, state.r[REG_1]);
