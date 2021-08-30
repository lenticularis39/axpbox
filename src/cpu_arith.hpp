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

/* comparison */
#define DO_CMPEQ RCV = (RAV == RBV) ? 1 : 0;
#define DO_CMPLT RCV = ((s64)RAV < (s64)RBV) ? 1 : 0;
#define DO_CMPLE RCV = ((s64)RAV <= (s64)RBV) ? 1 : 0;

/* addition */
#define DO_ADDQ RCV = RAV + RBV;
#define DO_S4ADDQ RCV = (RAV * 4) + RBV;
#define DO_S8ADDQ RCV = (RAV * 8) + RBV;

#define DO_ADDQ_V                                                              \
  {                                                                            \
    u64 rav = RAV;                                                             \
    u64 rbv = RBV;                                                             \
    RCV = rav + rbv;                                                           \
                                                                               \
    /* test for integer overflow */                                            \
    if (((~rav ^ rbv) & (rav ^ RCV)) & Q_SIGN) {                               \
      ARITH_TRAP_I(TRAP_IOV, RC);                                              \
      printf("ADDQ_V %016" PRIu64 "x + %016" PRIu64 "x = %016" PRIu64          \
             "x + TRAP.\n",                                                    \
             rav, rbv, RCV);                                                   \
    }                                                                          \
  }

#define DO_ADDL RCV = sext_u64_32(RAV + RBV);
#define DO_S4ADDL RCV = sext_u64_32((RAV * 4) + RBV);
#define DO_S8ADDL RCV = sext_u64_32((RAV * 8) + RBV);

#define DO_ADDL_V                                                              \
  {                                                                            \
    u64 rav = RAV;                                                             \
    u64 rbv = RBV;                                                             \
    RCV = sext_u64_32(rav + rbv);                                              \
                                                                               \
    /* test for integer overflow */                                            \
    if (((~rav ^ rbv) & (rav ^ RCV)) & L_SIGN) {                               \
      ARITH_TRAP_I(TRAP_IOV, RC);                                              \
      printf("ADDL_V %016" PRIu64 "x + %016" PRIu64 "x = %016" PRIu64          \
             "x + TRAP.\n",                                                    \
             rav, rbv, RCV);                                                   \
    }                                                                          \
  }

#define DO_CTLZ                                                                \
  temp_64 = 0;                                                                 \
  temp_64_2 = RBV;                                                             \
  for (i = 63; i >= 0; i--)                                                    \
    if ((temp_64_2 >> i) & 1)                                                  \
      break;                                                                   \
    else                                                                       \
      temp_64++;                                                               \
  RCV = temp_64;

#define DO_CTPOP                                                               \
  temp_64 = 0;                                                                 \
  temp_64_2 = RBV;                                                             \
  for (i = 0; i < 64; i++)                                                     \
    if ((temp_64_2 >> i) & 1)                                                  \
      temp_64++;                                                               \
  RCV = temp_64;

#define DO_CTTZ                                                                \
  temp_64 = 0;                                                                 \
  temp_64_2 = RBV;                                                             \
  for (i = 0; i < 64; i++)                                                     \
    if ((temp_64_2 >> i) & 1)                                                  \
      break;                                                                   \
    else                                                                       \
      temp_64++;                                                               \
  RCV = temp_64;

#define DO_CMPULT RCV = ((u64)RAV < (u64)RBV) ? 1 : 0;
#define DO_CMPULE RCV = ((u64)RAV <= (u64)RBV) ? 1 : 0;

/* multiplication */
#define DO_MULL RCV = sext_u64_32(sext_u64_32(RAV) * sext_u64_32(RBV));

#define DO_MULL_V                                                              \
  {                                                                            \
    u64 rav = RAV;                                                             \
    u64 rbv = RBV;                                                             \
    u64 sr = sext_u64_32(rav) * sext_u64_32(rbv);                              \
    RCV = sext_u64_32(sr);                                                     \
    if ((RCV ^ sr) & U64(0xffffffff00000000)) {                                \
      ARITH_TRAP_I(TRAP_IOV, RC);                                              \
      printf("MULL_V %016" PRIu64 "x * %016" PRIu64 "x = %016" PRIu64          \
             "x + TRAP.\n",                                                    \
             rav, rbv, RCV);                                                   \
    }                                                                          \
  }

#define DO_MULQ RCV = RAV * RBV;

#define DO_MULQ_V                                                              \
  {                                                                            \
    u64 rav = RAV;                                                             \
    u64 rbv = RBV;                                                             \
    u64 t64;                                                                   \
    RCV = uemul64(rav, rbv, &t64);                                             \
    if (Q_GETSIGN(rav))                                                        \
      t64 -= rbv;                                                              \
    if (Q_GETSIGN(rbv))                                                        \
      t64 -= rav;                                                              \
    if (Q_GETSIGN(RCV) ? (t64 != X64_QUAD) : (t64 != 0)) {                     \
      ARITH_TRAP_I(TRAP_IOV, RC);                                              \
      printf("MULQ_V %016" PRIu64 "x * %016" PRIu64 "x = %016" PRIu64          \
             "x + TRAP.\n",                                                    \
             rav, rbv, RCV);                                                   \
    }                                                                          \
  }

#define DO_UMULH uemul64(RAV, RBV, &RCV);

/* subtraction */
#define DO_SUBQ RCV = RAV - RBV;
#define DO_S4SUBQ RCV = (RAV * 4) - RBV;
#define DO_S8SUBQ RCV = (RAV * 8) - RBV;

#define DO_SUBQ_V                                                              \
  {                                                                            \
    u64 rav = RAV;                                                             \
    u64 rbv = RBV;                                                             \
    RCV = rav - rbv;                                                           \
                                                                               \
    /* test for integer overflow */                                            \
    if (((rav ^ rbv) & (rav ^ RCV)) & Q_SIGN) {                                \
      ARITH_TRAP_I(TRAP_IOV, RC);                                              \
      printf("SUBQ_V %016" PRIu64 "x - %016" PRIu64 "x = %016" PRIu64          \
             "x + TRAP.\n",                                                    \
             rav, rbv, RCV);                                                   \
    }                                                                          \
  }

#define DO_SUBL RCV = sext_u64_32(RAV - RBV);
#define DO_S4SUBL RCV = sext_u64_32((RAV * 4) - RBV);
#define DO_S8SUBL RCV = sext_u64_32((RAV * 8) - RBV);

#define DO_SUBL_V                                                              \
  {                                                                            \
    u64 rav = RAV;                                                             \
    u64 rbv = RBV;                                                             \
    RCV = sext_u64_32(rav - rbv);                                              \
                                                                               \
    /* test for integer overflow */                                            \
    if (((rav ^ rbv) & (rav ^ RCV)) & L_SIGN) {                                \
      ARITH_TRAP_I(TRAP_IOV, RC);                                              \
      printf("SUBL_V %016" PRIu64 "x - %016" PRIu64 "x = %016" PRIu64          \
             "x + TRAP.\n",                                                    \
             rav, rbv, RCV);                                                   \
    }                                                                          \
  }
