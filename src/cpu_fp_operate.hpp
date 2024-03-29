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

/* copy sign */
#define DO_CPYS                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] =                                                            \
      (state.f[FREG_1] & FPR_SIGN) | (state.f[FREG_2] & ~FPR_SIGN);

#define DO_CPYSN                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = ((state.f[FREG_1] & FPR_SIGN) ^ FPR_SIGN) |                \
                    (state.f[FREG_2] & ~FPR_SIGN);

#define DO_CPYSE                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = (state.f[FREG_1] & (FPR_SIGN | FPR_EXP)) |                 \
                    (state.f[FREG_2] & ~(FPR_SIGN | FPR_EXP));

/* conditional move */
#define DO_FCMOVEQ                                                             \
  FPSTART;                                                                     \
  if ((state.f[FREG_1] & ~FPR_SIGN) == 0)                                      \
    state.f[FREG_3] = state.f[FREG_2];

#define DO_FCMOVGE                                                             \
  FPSTART;                                                                     \
  if (state.f[FREG_1] <= FPR_SIGN)                                             \
    state.f[FREG_3] = state.f[FREG_2];

#define DO_FCMOVGT                                                             \
  FPSTART;                                                                     \
  if (!FPR_GETSIGN(state.f[FREG_1]) && (state.f[FREG_1] != 0))                 \
    state.f[FREG_3] = state.f[FREG_2];

#define DO_FCMOVLE                                                             \
  FPSTART;                                                                     \
  if (FPR_GETSIGN(state.f[FREG_1]) || (state.f[FREG_1] == 0))                  \
    state.f[FREG_3] = state.f[FREG_2];

#define DO_FCMOVLT                                                             \
  FPSTART;                                                                     \
  if (state.f[FREG_1] > FPR_SIGN)                                              \
    state.f[FREG_3] = state.f[FREG_2];

#define DO_FCMOVNE                                                             \
  FPSTART;                                                                     \
  if ((state.f[FREG_1] & ~FPR_SIGN) != 0)                                      \
    state.f[FREG_3] = state.f[FREG_2];

/* floating-point control register */
#define DO_MF_FPCR                                                             \
  FPSTART;                                                                     \
  state.f[FREG_1] = state.fpcr;

#define DO_MT_FPCR                                                             \
  FPSTART;                                                                     \
  state.fpcr = state.f[FREG_1] & U64(0x7fff800000000000);                      \
  if (state.fpcr & U64(0x03f0000000000000))                                    \
    state.fpcr |= U64(0x8000000000000000); /* SUM */

/* add */
#define DO_ADDG                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = vax_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_G, 0);

#define DO_ADDF                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = vax_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_F, 0);

#define DO_ADDT                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_T, 0);

#define DO_ADDS                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_S, 0);

/* subtract */
#define DO_SUBG                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = vax_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_G, 1);

#define DO_SUBF                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = vax_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_F, 1);

#define DO_SUBT                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_T, 1);

#define DO_SUBS                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_S, 1);

/* comparison */
#define DO_CMPGEQ                                                              \
  FPSTART;                                                                     \
  state.f[FREG_3] =                                                            \
      (vax_fcmp(state.f[FREG_1], state.f[FREG_2], ins) == 0) ? FP_TRUE : 0;

#define DO_CMPGLE                                                              \
  FPSTART;                                                                     \
  state.f[FREG_3] =                                                            \
      (vax_fcmp(state.f[FREG_1], state.f[FREG_2], ins) <= 0) ? FP_TRUE : 0;

#define DO_CMPGLT                                                              \
  FPSTART;                                                                     \
  state.f[FREG_3] =                                                            \
      (vax_fcmp(state.f[FREG_1], state.f[FREG_2], ins) < 0) ? FP_TRUE : 0;

#define DO_CMPTEQ                                                              \
  FPSTART;                                                                     \
  state.f[FREG_3] = (ieee_fcmp(state.f[FREG_1], state.f[FREG_2], ins, 0) == 0) \
                        ? FP_TRUE                                              \
                        : 0;

#define DO_CMPTLE                                                              \
  FPSTART;                                                                     \
  state.f[FREG_3] = (ieee_fcmp(state.f[FREG_1], state.f[FREG_2], ins, 1) <= 0) \
                        ? FP_TRUE                                              \
                        : 0;

#define DO_CMPTLT                                                              \
  FPSTART;                                                                     \
  state.f[FREG_3] =                                                            \
      (ieee_fcmp(state.f[FREG_1], state.f[FREG_2], ins, 1) < 0) ? FP_TRUE : 0;

#define DO_CMPTUN                                                              \
  FPSTART;                                                                     \
  state.f[FREG_3] = ((ieee_unpack(state.f[FREG_1], &ufp1, ins) == UFT_NAN) ||  \
                     (ieee_unpack(state.f[FREG_2], &ufp2, ins) == UFT_NAN))    \
                        ? FP_TRUE                                              \
                        : 0;

/* format conversions */
#define DO_CVTQL                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = ((state.f[FREG_2] & 0xC0000000) << 32) |                   \
                    ((state.f[FREG_2] & 0x3FFFFFFF) << 29);                    \
  if (FPR_GETSIGN(state.f[FREG_2])                                             \
          ? (state.f[FREG_2] < U64(0xFFFFFFFF80000000))                        \
          : (state.f[FREG_2] > U64(0x000000007FFFFFFF))) {                     \
    if (ins & I_FTRP_V)                                                        \
      vax_trap(TRAP_IOV, ins);                                                 \
  }

#define DO_CVTLQ                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = sext_u64_32(((state.f[FREG_2] >> 32) & 0xC0000000) |       \
                                ((state.f[FREG_2] >> 29) & 0x3FFFFFFF));

#define DO_CVTGQ                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = vax_cvtfi(state.f[FREG_2], ins);

#define DO_CVTQG                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = vax_cvtif(state.f[FREG_2], ins, DT_G);

#define DO_CVTQF                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = vax_cvtif(state.f[FREG_2], ins, DT_F);

#define DO_CVTTQ                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_cvtfi(state.f[FREG_2], ins);

#define DO_CVTQT                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_cvtif(state.f[FREG_2], ins, DT_T);

#define DO_CVTQS                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_cvtif(state.f[FREG_2], ins, DT_S);

#define DO_CVTGD                                                               \
  FPSTART;                                                                     \
  vax_unpack(state.f[FREG_2], &ufp2, ins);                                     \
  state.f[FREG_3] = vax_rpack_d(&ufp2, ins);

#define DO_CVTDG                                                               \
  FPSTART;                                                                     \
  vax_unpack_d(state.f[FREG_2], &ufp2, ins);                                   \
  state.f[FREG_3] = vax_rpack(&ufp2, ins, DT_G);

#define DO_CVTGF                                                               \
  FPSTART;                                                                     \
  vax_unpack(state.f[FREG_2], &ufp2, ins);                                     \
  state.f[FREG_3] = vax_rpack(&ufp2, ins, DT_F);

#define DO_CVTST                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_cvtst(state.f[FREG_2], ins);

#define DO_CVTTS                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_cvtts(state.f[FREG_2], ins);

/* float <-> integer register moves */
#define DO_FTOIS                                                               \
  FPSTART;                                                                     \
  state.r[REG_3] = ieee_sts(state.f[FREG_1]);

#define DO_FTOIT                                                               \
  FPSTART;                                                                     \
  state.r[REG_3] = state.f[FREG_1];

#define DO_ITOFT                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = state.r[REG_1];

#define DO_ITOFS                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_lds((u32)state.r[REG_1]);

#define DO_ITOFF                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = vax_ldf(SWAP_VAXF((u32)state.r[REG_1]));

/* Multiply */
#define DO_MULG                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = vax_fmul(state.f[FREG_1], state.f[FREG_2], ins, DT_G);

#define DO_MULF                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = vax_fmul(state.f[FREG_1], state.f[FREG_2], ins, DT_F);

#define DO_MULT                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_fmul(state.f[FREG_1], state.f[FREG_2], ins, DT_T);

#define DO_MULS                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_fmul(state.f[FREG_1], state.f[FREG_2], ins, DT_S);

/* Divide */
#define DO_DIVG                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = vax_fdiv(state.f[FREG_1], state.f[FREG_2], ins, DT_G);

#define DO_DIVF                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = vax_fdiv(state.f[FREG_1], state.f[FREG_2], ins, DT_F);

#define DO_DIVT                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_fdiv(state.f[FREG_1], state.f[FREG_2], ins, DT_T);

#define DO_DIVS                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_fdiv(state.f[FREG_1], state.f[FREG_2], ins, DT_S);

/* Square-root */
#define DO_SQRTG                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = vax_sqrt(state.f[FREG_2], ins, DT_G);

#define DO_SQRTF                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = vax_sqrt(state.f[FREG_2], ins, DT_F);

#define DO_SQRTT                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_sqrt(state.f[FREG_2], ins, DT_T);

#define DO_SQRTS                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = ieee_sqrt(state.f[FREG_2], ins, DT_S);

#else
#define DO_CPYS                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = (state.f[FREG_1] & U64(0x8000000000000000)) |              \
                    (state.f[FREG_2] & U64(0x7fffffffffffffff));

#define DO_CPYSN                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] =                                                            \
      (state.f[FREG_1] & U64(0x8000000000000000) ^ U64(0x8000000000000000)) |  \
      (state.f[FREG_2] & U64(0x7fffffffffffffff));

#define DO_CPYSE                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = (state.f[FREG_1] & U64(0xfff0000000000000)) |              \
                    (state.f[FREG_2] & U64(0x000fffffffffffff));

#define DO_CVTQL                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = ((state.f[FREG_2] & U64(0x00000000c0000000)) << 32) |      \
                    ((state.f[FREG_2] & U64(0x000000003fffffff)) << 29);

#define DO_CVTLQ                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] =                                                            \
      sext_u64_32(((state.f[FREG_2] >> 32) & U64(0x00000000c0000000)) |        \
                  ((state.f[FREG_2] >> 29) & U64(0x000000003fffffff)));

#define DO_FCMOVEQ                                                             \
  FPSTART;                                                                     \
  if (state.f[FREG_1] == U64(0x0000000000000000) ||                            \
      state.f[FREG_1] == U64(0x8000000000000000))                              \
    state.f[FREG_3] = state.f[FREG_2];
#define DO_FCMOVGE                                                             \
  FPSTART;                                                                     \
  if (!(state.f[FREG_1] & U64(0x8000000000000000)) ||                          \
      state.f[FREG_1] == U64(0x8000000000000000))                              \
    state.f[FREG_3] = state.f[FREG_2];
#define DO_FCMOVGT                                                             \
  FPSTART;                                                                     \
  if (!(state.f[FREG_1] & U64(0x8000000000000000)) &&                          \
      state.f[FREG_1] != U64(0x0000000000000000))                              \
    state.f[FREG_3] = state.f[FREG_2];
#define DO_FCMOVLE                                                             \
  FPSTART;                                                                     \
  if ((state.f[FREG_1] & U64(0x8000000000000000)) ||                           \
      state.f[FREG_1] == U64(0x0000000000000000))                              \
    state.f[FREG_3] = state.f[FREG_2];
#define DO_FCMOVLT                                                             \
  FPSTART;                                                                     \
  if ((state.f[FREG_1] & U64(0x8000000000000000)) &&                           \
      state.f[FREG_1] != U64(0x8000000000000000))                              \
    state.f[FREG_3] = state.f[FREG_2];
#define DO_FCMOVNE                                                             \
  FPSTART;                                                                     \
  if (state.f[FREG_1] != U64(0x0000000000000000) &&                            \
      state.f[FREG_1] != U64(0x8000000000000000))                              \
    state.f[FREG_3] = state.f[FREG_2];

#define DO_MF_FPCR                                                             \
  FPSTART;                                                                     \
  state.f[FREG_1] = state.fpcr;
#define DO_MT_FPCR                                                             \
  FPSTART;                                                                     \
  state.fpcr = state.f[FREG_1];

#define DO_ADDG                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2g(g2host(state.f[FREG_1]) + g2host(state.f[FREG_2]));
#define DO_ADDF                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2f(f2host(state.f[FREG_1]) + f2host(state.f[FREG_2]));
#define DO_ADDT                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2t(t2host(state.f[FREG_1]) + t2host(state.f[FREG_2]));
#define DO_ADDS                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2s(s2host(state.f[FREG_1]) + s2host(state.f[FREG_2]));

#define DO_SUBG                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2g(g2host(state.f[FREG_1]) - g2host(state.f[FREG_2]));
#define DO_SUBF                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2f(f2host(state.f[FREG_1]) - f2host(state.f[FREG_2]));
#define DO_SUBT                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2t(t2host(state.f[FREG_1]) - t2host(state.f[FREG_2]));
#define DO_SUBS                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2s(s2host(state.f[FREG_1]) - s2host(state.f[FREG_2]));

#define DO_CMPGEQ                                                              \
  FPSTART;                                                                     \
  state.f[FREG_3] = (g2host(state.f[FREG_1]) == g2host(state.f[FREG_2]))       \
                        ? U64(0x4000000000000000)                              \
                        : 0;
#define DO_CMPGLE                                                              \
  FPSTART;                                                                     \
  state.f[FREG_3] = (g2host(state.f[FREG_1]) <= g2host(state.f[FREG_2]))       \
                        ? U64(0x4000000000000000)                              \
                        : 0;
#define DO_CMPGLT                                                              \
  FPSTART;                                                                     \
  state.f[FREG_3] = (g2host(state.f[FREG_1]) < g2host(state.f[FREG_2]))        \
                        ? U64(0x4000000000000000)                              \
                        : 0;

#define DO_CMPTEQ                                                              \
  FPSTART;                                                                     \
  state.f[FREG_3] = (t2host(state.f[FREG_1]) == t2host(state.f[FREG_2]))       \
                        ? U64(0x4000000000000000)                              \
                        : 0;
#define DO_CMPTLE                                                              \
  FPSTART;                                                                     \
  state.f[FREG_3] = (t2host(state.f[FREG_1]) <= t2host(state.f[FREG_2]))       \
                        ? U64(0x4000000000000000)                              \
                        : 0;
#define DO_CMPTLT                                                              \
  FPSTART;                                                                     \
  state.f[FREG_3] = (t2host(state.f[FREG_1]) < t2host(state.f[FREG_2]))        \
                        ? U64(0x4000000000000000)                              \
                        : 0;
#define DO_CMPTUN                                                              \
  FPSTART;                                                                     \
  state.f[FREG_3] = (i_isnan(state.f[FREG_1]) || i_isnan(state.f[FREG_2]))     \
                        ? U64(0x4000000000000000)                              \
                        : 0;

#define DO_CVTGQ                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = (u64)((s64)g2host(state.f[FREG_2]));
#define DO_CVTQG                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2g((double)((s64)state.f[FREG_2]));
#define DO_CVTQF                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2f((double)((s64)state.f[FREG_2]));

#define DO_CVTTQ                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = (u64)((s64)t2host(state.f[FREG_2]));
#define DO_CVTQT                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2t((double)((s64)state.f[FREG_2]));
#define DO_CVTQS                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2s((double)((s64)state.f[FREG_2]));
#define DO_CVTGD                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2d(g2host(state.f[FREG_2]));
#define DO_CVTDG                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2g(d2host(state.f[FREG_2]));
#define DO_CVTGF                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2f(g2host(state.f[FREG_2]));

#define DO_FTOIS                                                               \
  FPSTART;                                                                     \
  temp_64 = state.f[FREG_1];                                                   \
  state.r[REG_3] =                                                             \
      (temp_64 & U64(0x000000003fffffff)) |                                    \
      ((temp_64 & U64(0xc000000000000000)) >> 32) |                            \
      (((temp_64 & U64(0x8000000000000000)) >> 31) * U64(0xffffffff));

#define DO_FTOIT                                                               \
  FPSTART;                                                                     \
  state.r[REG_3] = state.f[FREG_1];
#define DO_ITOFT                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = state.r[REG_1];
#define DO_ITOFS                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = load_s((u32)state.r[REG_1]);
#define DO_ITOFF                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = itof_f(state.r[REG_1]);

#define DO_MULG                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2g(g2host(state.f[FREG_1]) * g2host(state.f[FREG_2]));
#define DO_MULF                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2f(f2host(state.f[FREG_1]) * f2host(state.f[FREG_2]));
#define DO_MULT                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2t(t2host(state.f[FREG_1]) * t2host(state.f[FREG_2]));
#define DO_MULS                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2s(s2host(state.f[FREG_1]) * s2host(state.f[FREG_2]));

#define DO_DIVG                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2g(g2host(state.f[FREG_1]) / g2host(state.f[FREG_2]));
#define DO_DIVF                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2f(f2host(state.f[FREG_1]) / f2host(state.f[FREG_2]));
#define DO_DIVT                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2t(t2host(state.f[FREG_1]) / t2host(state.f[FREG_2]));
#define DO_DIVS                                                                \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2s(s2host(state.f[FREG_1]) / s2host(state.f[FREG_2]));

#define DO_SQRTG                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2g(sqrt(g2host(state.f[FREG_2])));
#define DO_SQRTF                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2f(sqrt(f2host(state.f[FREG_2])));
#define DO_SQRTT                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2t(sqrt(t2host(state.f[FREG_2])));
#define DO_SQRTS                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2s(sqrt(s2host(state.f[FREG_2])));

#define DO_CVTST                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2t(s2host(state.f[FREG_2]));
#define DO_CVTTS                                                               \
  FPSTART;                                                                     \
  state.f[FREG_3] = host2s(t2host(state.f[FREG_2]));
#endif
