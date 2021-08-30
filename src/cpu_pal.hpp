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

#define DO_HW_MFPR                                                             \
  if ((function & 0xc0) == 0x40) { /* PCTX */                                  \
    state.r[REG_1] = ((u64)state.asn << 39) | ((u64)state.astrr << 9) |        \
                     ((u64)state.aster << 5) |                                 \
                     (state.fpen ? U64(0x1) << 2 : 0) |                        \
                     (state.ppcen ? U64(0x1) << 1 : 0);                        \
  } else {                                                                     \
    switch (function) {                                                        \
    case 0x05: /* PMPC     */                                                  \
      state.r[REG_1] = state.pmpc;                                             \
      break;                                                                   \
                                                                               \
    case 0x06: /* EXC_ADDR */                                                  \
      state.r[REG_1] = state.exc_addr;                                         \
      break;                                                                   \
                                                                               \
    case 0x07: /* IVA_FORM */                                                  \
      state.r[REG_1] = va_form(state.exc_addr, true);                          \
      break;                                                                   \
                                                                               \
    case 0x08: /* IER_CM   */                                                  \
    case 0x09: /* CM       */                                                  \
    case 0x0a: /* IER      */                                                  \
    case 0x0b: /* IER_CM   */                                                  \
      state.r[REG_1] = (((u64)state.eien) << 33) | (((u64)state.slen) << 32) | \
                       (((u64)state.cren) << 31) | (((u64)state.pcen) << 29) | \
                       (((u64)state.sien) << 13) |                             \
                       (((u64)state.asten) << 13) | (((u64)state.cm) << 3);    \
      break;                                                                   \
                                                                               \
    case 0x0c: /* SIRR */                                                      \
      state.r[REG_1] = ((u64)state.sir) << 13;                                 \
      break;                                                                   \
                                                                               \
    case 0x0d: /* ISUM */                                                      \
      state.r[REG_1] =                                                         \
          (((u64)(state.eir & state.eien)) << 33) |                            \
          (((u64)(state.slr & state.slen)) << 32) |                            \
          (((u64)(state.crr & state.cren)) << 31) |                            \
          (((u64)(state.pcr & state.pcen)) << 29) |                            \
          (((u64)(state.sir & state.sien)) << 13) |                            \
          (((u64)(((U64(0x1) << (state.cm + 1)) - 1) & state.aster &           \
                  state.astrr & (state.asten * 0x3)))                          \
           << 3) |                                                             \
          (((u64)(((U64(0x1) << (state.cm + 1)) - 1) & state.aster &           \
                  state.astrr & (state.asten * 0xc)))                          \
           << 7);                                                              \
      break;                                                                   \
                                                                               \
    case 0x0f: /* EXC_SUM */                                                   \
      state.r[REG_1] = state.exc_sum;                                          \
      break;                                                                   \
                                                                               \
    case 0x10: /* PAL_BASE */                                                  \
      state.r[REG_1] = state.pal_base;                                         \
      break;                                                                   \
                                                                               \
    case 0x11: /* i_ctl */                                                     \
      state.r[REG_1] =                                                         \
          state.i_ctl_other | (((u64)CPU_CHIP_ID) << 24) |                     \
          (u64)state.i_ctl_vptb | (((u64)state.i_ctl_va_mode) << 15) |         \
          (state.hwe ? U64(0x1) << 12 : 0) | (state.sde ? U64(0x1) << 7 : 0) | \
          (((u64)state.i_ctl_spe) << 3);                                       \
      break;                                                                   \
                                                                               \
    case 0x14: /* PCTR_CTL */                                                  \
      state.r[REG_1] = state.pctr_ctl;                                         \
      break;                                                                   \
                                                                               \
    case 0x16: /* I_STAT */                                                    \
      state.r[REG_1] = state.i_stat;                                           \
      break;                                                                   \
                                                                               \
    case 0x27: /* MM_STAT */                                                   \
      state.r[REG_1] = state.mm_stat;                                          \
      break;                                                                   \
                                                                               \
    case 0x2a: /* DC_STAT */                                                   \
      state.r[REG_1] = state.dc_stat;                                          \
      break;                                                                   \
                                                                               \
    case 0x2b: /* C_DATA */                                                    \
      state.r[REG_1] = 0;                                                      \
      break;                                                                   \
                                                                               \
    case 0xc0: /* CC */                                                        \
      state.r[REG_1] =                                                         \
          (((u64)state.cc_offset) << 32) | (state.cc & U64(0xffffffff));       \
      break;                                                                   \
                                                                               \
    case 0xc2: /* VA */                                                        \
      state.r[REG_1] = state.fault_va;                                         \
      break;                                                                   \
                                                                               \
    case 0xc3: /* VA_FORM */                                                   \
      state.r[REG_1] = va_form(state.fault_va, false);                         \
      break;                                                                   \
                                                                               \
    default:                                                                   \
      UNKNOWN2;                                                                \
    }                                                                          \
  }

#define DO_HW_MTPR                                                             \
  if ((function & 0xc0) == 0x40) {                                             \
    if (function & 1)                                                          \
      state.asn = (int)(state.r[REG_2] >> 39) & 0xff;                          \
    if (function & 2) {                                                        \
      state.aster = (int)(state.r[REG_2] >> 5) & 0xf;                          \
      state.check_int = true;                                                  \
    }                                                                          \
    if (function & 4) {                                                        \
      state.astrr = (int)(state.r[REG_2] >> 9) & 0xf;                          \
      state.check_int = true;                                                  \
    }                                                                          \
    if (function & 8)                                                          \
      state.ppcen = (int)(state.r[REG_2] >> 1) & 1;                            \
    if (function & 16)                                                         \
      state.fpen = (int)(state.r[REG_2] >> 2) & 1;                             \
  } else {                                                                     \
    switch (function) {                                                        \
    case 0x00: /* ITB_TAG */                                                   \
      state.last_tb_virt = state.r[REG_2];                                     \
      break;                                                                   \
                                                                               \
    case 0x01: /* ITB_PTE */                                                   \
      add_tb_i(state.last_tb_virt, state.r[REG_2]);                            \
      break;                                                                   \
                                                                               \
    case 0x02: /* ITB_IAP */                                                   \
      tbiap(ACCESS_EXEC);                                                      \
      break;                                                                   \
                                                                               \
    case 0x03: /* ITB_IA */                                                    \
      tbia(ACCESS_EXEC);                                                       \
      break;                                                                   \
                                                                               \
    case 0x04: /* ITB_IS */                                                    \
      tbis(state.r[REG_2], ACCESS_EXEC);                                       \
      break;                                                                   \
                                                                               \
    case 0x09: /* CM */                                                        \
      state.cm = (int)(state.r[REG_2] >> 3) & 3;                               \
      state.check_int = true;                                                  \
      break;                                                                   \
                                                                               \
    case 0x0b: /* IER_CM */                                                    \
      state.cm = (int)(state.r[REG_2] >> 3) & 3;                               \
      state.check_int = true;                                                  \
                                                                               \
    case 0x0a: /* IER */                                                       \
      state.asten = (int)(state.r[REG_2] >> 13) & 1;                           \
      state.sien = (int)(state.r[REG_2] >> 13) & 0xfffe;                       \
      state.pcen = (int)(state.r[REG_2] >> 29) & 3;                            \
      state.cren = (int)(state.r[REG_2] >> 31) & 1;                            \
      state.slen = (int)(state.r[REG_2] >> 32) & 1;                            \
      state.eien = (int)(state.r[REG_2] >> 33) & 0x3f;                         \
      state.check_int = true;                                                  \
      break;                                                                   \
                                                                               \
    case 0x0c: /* SIRR */                                                      \
      state.sir = (int)(state.r[REG_2] >> 13) & 0xfffe;                        \
      state.check_int = true;                                                  \
      break;                                                                   \
                                                                               \
    case 0x0e: /* HW_INT_CLR */                                                \
      state.pcr &= ~((state.r[REG_2] >> 29) & U64(0x3));                       \
      state.crr &= ~((state.r[REG_2] >> 31) & U64(0x1));                       \
      state.slr &= ~((state.r[REG_2] >> 32) & U64(0x1));                       \
      break;                                                                   \
                                                                               \
    case 0x10: /* PAL_BASE */                                                  \
      set_PAL_BASE(state.r[REG_2] & U64(0x00000fffffff8000));                  \
      break;                                                                   \
                                                                               \
    case 0x11: /* i_ctl */                                                     \
      state.i_ctl_other = state.r[REG_2] & U64(0x00000000007e2f67);            \
      state.i_ctl_vptb =                                                       \
          sext_u64_48(state.r[REG_2] & U64(0x0000ffffc0000000));               \
      state.i_ctl_spe = (int)(state.r[REG_2] >> 3) & 3;                        \
      state.sde = (state.r[REG_2] >> 7) & 1;                                   \
      state.hwe = (state.r[REG_2] >> 12) & 1;                                  \
      state.i_ctl_va_mode = (int)(state.r[REG_2] >> 15) & 3;                   \
      break;                                                                   \
                                                                               \
    case 0x12: /* ic_flush_asm */                                              \
      flush_icache_asm();                                                      \
      break;                                                                   \
                                                                               \
    case 0x13: /* IC_FLUSH */                                                  \
      flush_icache();                                                          \
      break;                                                                   \
                                                                               \
    case 0x14: /* PCTR_CTL */                                                  \
      state.pctr_ctl = state.r[REG_2] & U64(0xffffffffffffffdf);               \
      break;                                                                   \
                                                                               \
    case 0x15: /* CLR_MAP */                                                   \
    case 0x17: /* SLEEP   */                                                   \
    case 0x27: /* MM_STAT */                                                   \
    case 0x2b: /* C_DATA  */                                                   \
    case 0x2c: /* C_SHIFT */                                                   \
    case 0x2d: /* M_FIX */                                                     \
      break;                                                                   \
                                                                               \
    case 0x16:                         /* I_STAT */                            \
      state.i_stat &= ~state.r[REG_2]; /* W1C */                               \
      break;                                                                   \
                                                                               \
    case 0x20: /* DTB_TAG0 */                                                  \
      state.last_tb_virt = state.r[REG_2];                                     \
      break;                                                                   \
                                                                               \
    case 0x21: /* DTB_PTE0 */                                                  \
      add_tb_d(state.last_tb_virt, state.r[REG_2]);                            \
      break;                                                                   \
                                                                               \
    case 0x24: /* DTB_IS0 */                                                   \
      tbis(state.r[REG_2], ACCESS_READ);                                       \
      break;                                                                   \
                                                                               \
    case 0x25: /* DTB_ASN0 */                                                  \
      state.asn0 = (int)(state.r[REG_2] >> 56);                                \
      break;                                                                   \
                                                                               \
    case 0x26: /* DTB_ALTMODE */                                               \
      state.alt_cm = (int)(state.r[REG_2] & 3);                                \
      break;                                                                   \
                                                                               \
    case 0x28: /* M_CTL */                                                     \
      state.smc = (int)(state.r[REG_2] >> 4) & 3;                              \
      state.m_ctl_spe = (int)(state.r[REG_2] >> 1) & 7;                        \
      break;                                                                   \
                                                                               \
    case 0x29: /* DC_CTL */                                                    \
      state.dc_ctl = state.r[REG_2];                                           \
      break;                                                                   \
                                                                               \
    case 0x2a: /* DC_STAT */                                                   \
      state.dc_stat &= ~state.r[REG_2];                                        \
      break;                                                                   \
                                                                               \
    case 0xa0: /* DTB_TAG1 */                                                  \
      state.last_tb_virt = state.r[REG_2];                                     \
      break;                                                                   \
                                                                               \
    case 0xa1: /* DTB_PTE1 */                                                  \
      add_tb_d(state.last_tb_virt, state.r[REG_2]);                            \
      break;                                                                   \
                                                                               \
    case 0xa2: /* DTB_IAP */                                                   \
      tbiap(ACCESS_READ);                                                      \
      break;                                                                   \
                                                                               \
    case 0xa3: /* DTB_IA */                                                    \
      tbia(ACCESS_READ);                                                       \
      break;                                                                   \
                                                                               \
    case 0xa4: /* DTB_IS1 */                                                   \
      tbis(state.r[REG_2], ACCESS_READ);                                       \
      break;                                                                   \
                                                                               \
    case 0xa5: /* DTB_ASN1 */                                                  \
      state.asn1 = (int)(state.r[REG_2] >> 56);                                \
      break;                                                                   \
                                                                               \
    case 0xc0: /* CC */                                                        \
      state.cc_offset = (u32)(state.r[REG_2] >> 32);                           \
      break;                                                                   \
                                                                               \
    case 0xc1: /* CC_CTL */                                                    \
      state.cc_ena = (state.r[REG_2] >> 32) & 1;                               \
      state.cc = (u32)(state.r[REG_2] & U64(0xfffffff0));                      \
      break;                                                                   \
                                                                               \
    case 0xc4: /* VA_CTL */                                                    \
      state.va_ctl_vptb =                                                      \
          sext_u64_48(state.r[REG_2] & U64(0x0000ffffc0000000));               \
      state.va_ctl_va_mode = (int)(state.r[REG_2] >> 1) & 3;                   \
      break;                                                                   \
                                                                               \
    default:                                                                   \
      UNKNOWN2;                                                                \
    }                                                                          \
  }

#define DO_HW_RET set_pc(state.r[REG_2])
#define DO_HW_LDL                                                              \
  switch (function) {                                                          \
  case 0: /* longword physical */                                              \
    phys_address = state.r[REG_2] + DISP_12;                                   \
    state.r[REG_1] = READ_PHYS_NT(32);                                         \
    break;                                                                     \
                                                                               \
  case 2: /* longword physical locked */                                       \
    phys_address = state.r[REG_2] + DISP_12;                                   \
    cSystem->cpu_lock(state.iProcNum, phys_address);                           \
    state.r[REG_1] = READ_PHYS_NT(32);                                         \
    break;                                                                     \
                                                                               \
  case 4: /* longword virtual vpte                 chk   alt    vpte */        \
    DATA_PHYS_NT(state.r[REG_2] + DISP_12, ACCESS_READ | NO_CHECK | VPTE);     \
    state.r[REG_1] = READ_PHYS_NT(32);                                         \
    break;                                                                     \
                                                                               \
  case 8: /* longword virtual */                                               \
    DATA_PHYS_NT(state.r[REG_2] + DISP_12, ACCESS_READ | NO_CHECK);            \
    state.r[REG_1] = READ_PHYS_NT(32);                                         \
    break;                                                                     \
                                                                               \
  case 10: /* longword virtual check */                                        \
    DATA_PHYS_NT(state.r[REG_2] + DISP_12, ACCESS_READ);                       \
    state.r[REG_1] = READ_PHYS_NT(32);                                         \
    break;                                                                     \
                                                                               \
  case 12: /* longword virtual alt */                                          \
    DATA_PHYS_NT(state.r[REG_2] + DISP_12, ACCESS_READ | NO_CHECK | ALT);      \
    state.r[REG_1] = READ_PHYS_NT(32);                                         \
    break;                                                                     \
                                                                               \
  case 14: /* longword virtual alt check */                                    \
    DATA_PHYS_NT(state.r[REG_2] + DISP_12, ACCESS_READ | ALT);                 \
    state.r[REG_1] = READ_PHYS_NT(32);                                         \
    break;                                                                     \
                                                                               \
  default:                                                                     \
    UNKNOWN2;                                                                  \
  }

#define DO_HW_LDQ                                                              \
  switch (function) {                                                          \
  case 1: /* quadword physical */                                              \
    phys_address = state.r[REG_2] + DISP_12;                                   \
    state.r[REG_1] = READ_PHYS_NT(64);                                         \
    break;                                                                     \
                                                                               \
  case 3: /* quadword physical locked */                                       \
    phys_address = state.r[REG_2] + DISP_12;                                   \
    cSystem->cpu_lock(state.iProcNum, phys_address);                           \
    state.r[REG_1] = READ_PHYS_NT(64);                                         \
    break;                                                                     \
                                                                               \
  case 5: /* quadword virtual vpte                 chk   alt    vpte */        \
    DATA_PHYS_NT(state.r[REG_2] + DISP_12, ACCESS_READ | NO_CHECK | VPTE);     \
    state.r[REG_1] = READ_PHYS_NT(64);                                         \
    break;                                                                     \
                                                                               \
  case 9: /* quadword virtual */                                               \
    DATA_PHYS_NT(state.r[REG_2] + DISP_12, ACCESS_READ | NO_CHECK);            \
    state.r[REG_1] = READ_PHYS_NT(64);                                         \
    break;                                                                     \
                                                                               \
  case 11: /* quadword virtual check */                                        \
    DATA_PHYS_NT(state.r[REG_2] + DISP_12, ACCESS_READ);                       \
    state.r[REG_1] = READ_PHYS_NT(64);                                         \
    break;                                                                     \
                                                                               \
  case 13: /* quadword virtual alt */                                          \
    DATA_PHYS_NT(state.r[REG_2] + DISP_12, ACCESS_READ | NO_CHECK | ALT);      \
    state.r[REG_1] = READ_PHYS_NT(64);                                         \
    break;                                                                     \
                                                                               \
  case 15: /* quadword virtual alt check */                                    \
    DATA_PHYS_NT(state.r[REG_2] + DISP_12, ACCESS_READ | ALT);                 \
    state.r[REG_1] = READ_PHYS_NT(64);                                         \
    break;                                                                     \
                                                                               \
  default:                                                                     \
    UNKNOWN2;                                                                  \
  }

#define DO_HW_STL                                                              \
  switch (function) {                                                          \
  case 0: /* longword physical */                                              \
    phys_address = state.r[REG_2] + DISP_12;                                   \
    WRITE_PHYS_NT(state.r[REG_1], 32);                                         \
    break;                                                                     \
                                                                               \
  case 2: /* longword physical conditional */                                  \
    if (cSystem->cpu_unlock(state.iProcNum)) {                                 \
      phys_address = state.r[REG_2] + DISP_12;                                 \
      WRITE_PHYS_NT(state.r[REG_1], 32);                                       \
      state.r[REG_1] = 1;                                                      \
    } else                                                                     \
      state.r[REG_1] = 0;                                                      \
    break;                                                                     \
                                                                               \
  case 4: /* longword virtual                      chk   alt    vpte */        \
    DATA_PHYS_NT(state.r[REG_2] + DISP_12, ACCESS_READ | NO_CHECK);            \
    WRITE_PHYS_NT(state.r[REG_1], 32);                                         \
    break;                                                                     \
                                                                               \
  case 12: /* longword virtual alt */                                          \
    DATA_PHYS_NT(state.r[REG_2] + DISP_12, ACCESS_READ | NO_CHECK | ALT);      \
    WRITE_PHYS_NT(state.r[REG_1], 32);                                         \
    break;                                                                     \
                                                                               \
  default:                                                                     \
    UNKNOWN2;                                                                  \
  }

#define DO_HW_STQ                                                              \
  switch (function) {                                                          \
  case 1: /* quadword physical */                                              \
    phys_address = state.r[REG_2] + DISP_12;                                   \
    WRITE_PHYS_NT(state.r[REG_1], 64);                                         \
    break;                                                                     \
                                                                               \
  case 3: /* quadword physical conditional */                                  \
    if (cSystem->cpu_unlock(state.iProcNum)) {                                 \
      phys_address = state.r[REG_2] + DISP_12;                                 \
      WRITE_PHYS_NT(state.r[REG_1], 64);                                       \
      state.r[REG_1] = 1;                                                      \
    } else                                                                     \
      state.r[REG_1] = 0;                                                      \
    break;                                                                     \
                                                                               \
  case 5: /* quadword virtual                      chk    alt    vpte */       \
    DATA_PHYS_NT(state.r[REG_2] + DISP_12, ACCESS_READ | NO_CHECK);            \
    WRITE_PHYS_NT(state.r[REG_1], 64);                                         \
    break;                                                                     \
                                                                               \
  case 13: /* quadword virtual alt */                                          \
    DATA_PHYS_NT(state.r[REG_2] + DISP_12, ACCESS_READ | NO_CHECK | ALT);      \
    WRITE_PHYS_NT(state.r[REG_1], 64);                                         \
    break;                                                                     \
                                                                               \
  default:                                                                     \
    UNKNOWN2;                                                                  \
  }
