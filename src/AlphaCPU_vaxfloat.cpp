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
 * Contains VAX floating point code for the Alpha CPU.
 *
 * $Id: AlphaCPU_vaxfloat.cpp,v 1.11 2008/03/14 15:30:50 iamcamiel Exp $
 *
 * X-1.9        Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.8        Camiel Vanderhoeven                             05-FEB-2008
 *      Bug description added.
 *
 * X-1.7        Camiel Vanderhoeven                             28-JAN-2008
 *      Better floating-point exception handling.
 *
 * X-1.6        Camiel Vanderhoeven                             27-JAN-2008
 *      Comments.
 *
 * X-1.5        Camiel Vanderhoeven                             27-JAN-2008
 *      Minor floating-point improvements.
 *
 * X-1.4        Camiel Vanderhoeven                             26-JAN-2008
 *      Bugfix in vax_stf.
 *
 * X-1.3        Camiel Vanderhoeven                             26-JAN-2008
 *      Made IDB compile again.
 *
 * X-1.2        Camiel Vanderhoeven                             22-JAN-2008
 *      Minor cleanup.
 *
 * X-1.1        Camiel Vanderhoeven                             21-JAN-2008
 *      File created. Contains code based upon the SIMH Alpha pre-
 *      implementation, which is Copyright (c) 2003, Robert M Supnik.
 *
 * \bug The new floating point code has some unidentified problems. The
 * OpenVMS installation routine fails with this new code. For now, the old
 * floating point code has been restored, and the new floating-point code
 * is used only when HAVE_NEW_FP has been defined. The new code should be
 * fixed, so we can take advantage of floating point exceptions.
 **/
#include "AlphaCPU.hpp"
#include "StdAfx.hpp"
#include "cpu_debug.hpp"

#define IPMAX U64(0x7FFFFFFFFFFFFFFF) /* plus MAX (int) */
#define IMMAX U64(0x8000000000000000) /* minus MAX (int) */

/* Unpacked rounding constants */
#define UF_FRND U64(0x0000008000000000) /* F round */
#define UF_DRND U64(0x0000000000000080) /* D round */
#define UF_GRND U64(0x0000000000000400) /* G round */

/***************************************************************************/

/**
 * \name VAX_fp_load_store
 * VAX floating point load and store functions
 ******************************************************************************/

//\{

/**
 * \brief Convert the VAX F-floating memory format to the VAX floating register
 * format.
 *
 * Adjust the exponent base, reorder the bytes of the fraction to compensate for
 * the VAX byte-order, and widen the exponent and fraction fields.
 *
 * \param op	32-bit VAX F-floating value in memory format.
 * \return		The value op converted to 64-bit VAX floating in
 *register format.
 **/
u64 CAlphaCPU::vax_ldf(u32 op) {
  u32 exp = F_GETEXP(op);
  if (exp != 0)
    exp = exp + G_BIAS - F_BIAS;                    /* zero? */
  u64 res = (((u64)(op & F_SIGN)) ? FPR_SIGN : 0) | /* finite non-zero */
            (((u64)exp) << FPR_V_EXP) |
            (((u64)SWAP_VAXF(op & ~(F_SIGN | F_EXP))) << F_V_FRAC);

  // printf("vax_ldf: %08x -> %016" PRIx64 ".\n", op, res);
  return res;
}

/**
 * \brief Convert the VAX G-floating memory format to the VAX floating register
 * format.
 *
 * Reorder the bytes to compensate for the VAX byte-order.
 *
 * \param op  64-bit VAX G-floating value in memory format.
 * \return    The value op converted to 64-bit VAX floating in register format.
 **/
u64 CAlphaCPU::vax_ldg(u64 op) { return SWAP_VAXG(op); /* swizzle bits */ }

/**
 * \brief Convert the VAX floating register format to the VAX F-floating memory
 * format.
 *
 * Adjust the exponent base, make the exponent and fraction fields smaller, and
 * reorder the bytes of the fraction to compensate for the VAX byte-order.
 *
 * \param op  64-bit VAX floating in register format.
 * \return    The value op converted to 32-bit VAX F-floating value in memory
 *format.
 **/
u32 CAlphaCPU::vax_stf(u64 op) {
  u32 sign = FPR_GETSIGN(op) ? F_SIGN : 0;

  // u32 exp = ((u32) (op >> (FPR_V_EXP - F_V_EXP))) & F_EXP;
  u32 exp = FPR_GETEXP(op);
  if (exp != 0)
    exp = exp + F_BIAS - G_BIAS; /* zero? */
  exp = (exp << F_V_EXP) & F_EXP;

  u32 frac = (u32)(op >> F_V_FRAC);

  u32 res = sign | exp | (SWAP_VAXF(frac) & ~(F_SIGN | F_EXP));

  // printf("vax_stf: %016" PRIx64 " -> %08x.\n", op, res);
  return res;
}

/**
 * \brief Convert the VAX floating register format to the VAX G-floating memory
 * format.
 *
 * Reorder the bytes to compensate for the VAX byte-order.
 *
 * \param op  64-bit VAX floating in register format.
 * \return    The value op converted to 64-bit VAX G-floating value in memory
 *format.
 **/
u64 CAlphaCPU::vax_stg(u64 op) { return SWAP_VAXG(op); /* swizzle bits */ }

//\}

/***************************************************************************/

/**
 * \name VAX_fp_operations
 * VAX floating point operations
 ******************************************************************************/

//\{

/**
 * \brief Compare 2 VAX floating-point values.
 *
 * \param s1  First 64-bit VAX floating in register format to be compared.
 * \param s1  Second 64-bit VAX floating in register format to be compared.
 * \param ins The instruction currently being executed. Used to properly
 *            handle exceptions.
 * \return    0 if s1==s2, 1 if s1>s2, -1 if s1<s2.
 **/
int CAlphaCPU::vax_fcmp(u64 s1, u64 s2, u32 ins) {
  UFP a;

  UFP b;

  vax_unpack(s1, &a, ins);
  vax_unpack(s2, &b, ins);
  if (s1 == s2)
    return 0; /* equal? */
  if (a.sign != b.sign)
    return (a.sign ? -1 : +1);             /* opp signs? */
  return (((s1 < s2) ^ a.sign) ? -1 : +1); /* like signs */
}

/**
 * \brief Convert 64-bit signed integer to VAX floating-point value.
 *
 * \param val 64-bit signed integer to be converted.
 * \param ins The instruction currently being executed. Used to properly
 *            handle exceptions and to determine the rounding mode.
 * \param dp  DT_F for F-floating or DT_G for G-floating.
 * \return    64-bit VAX floating in register format.
 **/
u64 CAlphaCPU::vax_cvtif(u64 val, u32 ins, u32 dp) {
  s64 num = (s64)val;
  UFP a;

  if (num == 0)
    return 0;    /* 0? return +0 */
  if (num < 0) { /* < 0? */
    a.sign = 1;  /* set sign */
    val = NEG_Q(val);
  } /* |val| */
  else
    a.sign = 0;
  a.exp = 64 + G_BIAS;           /* set exp */
  a.frac = val;                  /* set frac */
  vax_norm(&a);                  /* normalize */
  return vax_rpack(&a, ins, dp); /* round and pack */
}

/**
 * \brief Convert VAX floating-point value to 64-bit signed integer.
 *
 * Note that rounding cannot cause a carry unless the fraction has been shifted
 * right at least FP_GUARD places; in which case a carry out is impossible
 *
 * \param op  64-bit VAX floating in register format to be converted.
 * \param ins The instruction currently being executed. Used to properly
 *            handle exceptions.
 * \return    64-bit signed integer.
 **/
u64 CAlphaCPU::vax_cvtfi(u64 op, u32 ins) {
  UFP a;
  u32 rndm = I_GETFRND(ins);
  s32 ubexp;

  vax_unpack(op, &a, ins);
  ubexp = a.exp - G_BIAS; /* unbiased exp */
  if (ubexp < 0)
    return 0;                             /* zero or too small? */
  if (ubexp <= UF_V_NM) {                 /* in range? */
    a.frac = a.frac >> (UF_V_NM - ubexp); /* leave rnd bit */
    if (rndm)
      a.frac = a.frac + 1;                  /* not chopped, round */
    a.frac = a.frac >> 1;                   /* now justified */
    if ((a.frac > (a.sign ? IMMAX : IPMAX)) /* out of range? */
        && (ins & I_FTRP_V))                /* trap enabled? */
      vax_trap(TRAP_IOV, ins);
  } /* set overflow */
  else {
    if (ubexp > (UF_V_NM + 64))
      a.frac = 0; /* out of range */
    else
      a.frac = (a.frac << (ubexp - UF_V_NM - 1)) & X64_QUAD; /* no rnd bit */
    if (ins & I_FTRP_V)                                      /* trap enabled? */
      vax_trap(TRAP_IOV, ins);
  } /* set overflow */

  return (a.sign ? NEG_Q(a.frac) : a.frac);
}

/**
 * \brief Add or subtract 2 VAX floating-point values.
 *
 * \param s1  Augend or minuend in 64-bit VAX floating register format.
 * \param s2  Addend or subtrahend in 64-bit VAX floating register format.
 * \param ins The instruction currently being executed. Used to properly
 *            handle exceptions.
 * \param dp  DT_F for F-floating or DT_G for G-floating.
 * \param sub subtract if true, add if false.
 * \return    64-bit VAX floating in register format.
 **/
u64 CAlphaCPU::vax_fadd(u64 s1, u64 s2, u32 ins, u32 dp, bool sub) {
  UFP a;

  UFP b;

  UFP t;
  u32 sticky;
  s32 ediff;

  vax_unpack(s1, &a, ins);
  vax_unpack(s2, &b, ins);
  if (sub)
    b.sign = b.sign ^ 1; /* sub? invert b sign */
  if (a.exp == 0)
    a = b;        /* s1 = 0? */
  else if (b.exp) /* s2 != 0? */
  {
    if ((a.exp < b.exp) /* |s1| < |s2|? swap */
        || ((a.exp == b.exp) && (a.frac < b.frac))) {
      t = a;
      a = b;
      b = t;
    }

    ediff = a.exp - b.exp; /* exp diff */
    if (a.sign ^ b.sign)   /* eff sub? */
    {
      if (ediff > 63)
        b.frac = 1;   /* >63? retain sticky */
      else if (ediff) /* [1,63]? shift */
      {
        sticky = ((b.frac << (64 - ediff)) & X64_QUAD) ? 1 : 0; /* lost bits */
        b.frac = (b.frac >> ediff) | sticky;
      }

      a.frac = (a.frac - b.frac) & X64_QUAD; /* subtract fractions */
      vax_norm(&a);                          /* normalize */
    } else                                   /* eff add */
    {
      if (ediff > 63)
        b.frac = 0; /* >63? b disappears */
      else if (ediff)
        b.frac = b.frac >> ediff;            /* denormalize */
      a.frac = (a.frac + b.frac) & X64_QUAD; /* add frac */
      if (a.frac < b.frac)                   /* chk for carry */
      {
        a.frac = UF_NM | (a.frac >> 1); /* shift in carry */
        a.exp = a.exp + 1;              /* skip norm */
      }
    }
  } /* end else if */

  return vax_rpack(&a, ins, dp); /* round and pack */
}

/**
 * \brief Multiply 2 VAX floating-point values.
 *
 * \param s1  Multiplicand in 64-bit VAX floating register format.
 * \param s2  Multiplier in 64-bit VAX floating register format.
 * \param ins The instruction currently being executed. Used to properly
 *            handle exceptions.
 * \param dp  DT_F for F-floating or DT_G for G-floating.
 * \return    64-bit VAX floating in register format.
 **/
u64 CAlphaCPU::vax_fmul(u64 s1, u64 s2, u32 ins, u32 dp) {
  UFP a;

  UFP b;

  vax_unpack(s1, &a, ins);
  vax_unpack(s2, &b, ins);
  if ((a.exp == 0) || (b.exp == 0))
    return 0;                       /* zero argument? */
  a.sign = a.sign ^ b.sign;         /* sign of result */
  a.exp = a.exp + b.exp - G_BIAS;   /* add exponents */
  uemul64(a.frac, b.frac, &a.frac); /* mpy fractions */
  vax_norm(&a);                     /* normalize */
  return vax_rpack(&a, ins, dp);    /* round and pack */
}

/**
 * \brief Divide 2 VAX floating-point values.
 *
 * Needs to develop at least one rounding bit.  Since the first
 * divide step can fail, develop 2 more bits than the precision of
 * the fraction.
 *
 * \param s1  Dividend in 64-bit VAX floating register format.
 * \param s2  Divisor in 64-bit VAX floating register format.
 * \param ins The instruction currently being executed. Used to properly
 *            handle exceptions.
 * \param dp  DT_F for F-floating or DT_G for G-floating.
 * \return    64-bit VAX floating in register format.
 **/
u64 CAlphaCPU::vax_fdiv(u64 s1, u64 s2, u32 ins, u32 dp) {
  UFP a;

  UFP b;

  vax_unpack(s1, &a, ins);
  vax_unpack(s2, &b, ins);
  if (b.exp == 0) {          /* divr = 0? */
    vax_trap(TRAP_DZE, ins); /* dze trap */
    return 0;
  }

  if (a.exp == 0)
    return 0;                         /* divd = 0? */
  a.sign = a.sign ^ b.sign;           /* result sign */
  a.exp = a.exp - b.exp + G_BIAS + 1; /* unbiased exp */
  a.frac = a.frac >> 1;               /* allow 1 bit left */
  b.frac = b.frac >> 1;
  a.frac = ufdiv64(a.frac, b.frac, 55, NULL); /* divide */
  vax_norm(&a);                               /* normalize */
  return vax_rpack(&a, ins, dp);              /* round and pack */
}

/**
 * \brief Determine principal square root of a VAX floating-point value.
 *
 * \param op  64-bit VAX floating in register format.
 * \param ins The instruction currently being executed. Used to properly
 *            handle exceptions.
 * \param dp  DT_F for F-floating or DT_G for G-floating.
 * \return    64-bit VAX floating in register format.
 **/
u64 CAlphaCPU::vax_sqrt(u64 op, u32 ins, u32 dp) {
  UFP b;

  vax_unpack(op, &b, ins);
  if (b.exp == 0)
    return 0;                /* zero? */
  if (b.sign) {              /* minus? */
    vax_trap(TRAP_INV, ins); /* invalid operand */
    return 0;
  }

  b.exp = ((b.exp + 1 - G_BIAS) >> 1) + G_BIAS; /* result exponent */
  b.frac = fsqrt64(b.frac, b.exp);              /* result fraction */
  return vax_rpack(&b, ins, dp);                /* round and pack */
}

//\}

/***************************************************************************/

/**
 * \name VAX_fp_support
 * VAX floating point support functions
 ******************************************************************************/

//\{

/**
 * \brief Set VAX floating-point trap
 *
 * Called when a VAX floating-point operation detects an exception.
 *
 * \param mask  A bitmask in which the bits are set that correspond to
 *              the exception that occurred.
 * \param ins   The instruction currently being executed. Used to properly
 *              set some registers for the trap to be handled.
 **/
void CAlphaCPU::vax_trap(u64 mask, u32 ins) {
  ARITH_TRAP(mask | ((ins & I_FTRP_S) ? TRAP_SWC : 0), I_GETRC(ins));
}

/**
 * \brief Unpack VAX floating-point value
 *
 * Converts a VAX floating-point value to it's sign, exponent and fraction
 * components.
 *
 * \param op  64-bit VAX floating in register format.
 * \param r   Pointer to the unpacked-floating-point UFP structure
 *            where the results are to be returned.
 * \param ins The instruction currently being executed. Used to properly
 *            handle exceptions.
 **/
void CAlphaCPU::vax_unpack(u64 op, UFP *r, u32 ins) {
  r->sign = FPR_GETSIGN(op); /* get sign */
  r->exp = FPR_GETEXP(op);   /* get exponent */
  r->frac = FPR_GETFRAC(op); /* get fraction */
  if (r->exp == 0)           /* exp = 0? */
  {
    if (r->sign != 0) /* rsvd op? */
      vax_trap(TRAP_INV, ins);

    // zero
    r->frac = r->sign = 0;
    return;
  }

  r->frac = (r->frac | FPR_HB) << FPR_GUARD; /* ins hidden bit, guard */
  return;
}

/**
 * \brief Unpack VAX D-floating-point value
 *
 * Converts a VAX D-floating-point value to it's sign, exponent and fraction
 * components.
 *
 * \param op  64-bit VAX D-floating in register format.
 * \param r   Pointer to the unpacked-floating-point UFP structure
 *            where the results are to be returned.
 * \param ins The instruction currently being executed. Used to properly
 *            handle exceptions.
 **/
void CAlphaCPU::vax_unpack_d(u64 op, UFP *r, u32 ins) {
  r->sign = FDR_GETSIGN(op); /* get sign */
  r->exp = FDR_GETEXP(op);   /* get exponent */
  r->frac = FDR_GETFRAC(op); /* get fraction */
  if (r->exp == 0)           /* exp = 0? */
  {
    if (op != 0) /* rsvd op? */
      vax_trap(TRAP_INV, ins);
    r->frac = r->sign = 0;
    return;
  }

  r->exp = r->exp + G_BIAS - D_BIAS;         /* change to G bias */
  r->frac = (r->frac | FDR_HB) << FDR_GUARD; /* ins hidden bit, guard */
  return;
}

/**
 * \brief Normalize VAX floating-point value
 *
 * Normalize exponent and fraction components.
 *
 * \param r   Pointer to the unpacked-floating-point UFP structure
 *            containing the value to be normalized, and where the
 *            results are to be returned.
 **/
void CAlphaCPU::vax_norm(UFP *r) {
  s32 i;
  static u64 normmask[5] = {U64(0xc000000000000000), U64(0xf000000000000000),
                            U64(0xff00000000000000), U64(0xffff000000000000),
                            U64(0xffffffff00000000)};
  static s32 normtab[6] = {1, 2, 4, 8, 16, 32};

  r->frac = r->frac & X64_QUAD;
  if (r->frac == 0) {     /* if fraction = 0 */
    r->sign = r->exp = 0; /* result is 0 */
    return;
  }

  while ((r->frac & UF_NM) == 0) { /* normalized? */
    for (i = 0; i < 5; i++) {      /* find first 1 */
      if (r->frac & normmask[i])
        break;
    }

    r->frac = r->frac << normtab[i]; /* shift frac */
    r->exp = r->exp - normtab[i];
  } /* decr exp */

  return;
}

/**
 * \brief Round and pack VAX floating-point value
 *
 * Converts sign, exponent and fraction components to a register-format VAX
 * floating point value.
 *
 * \param r   Pointer to the unpacked-floating-point UFP structure
 *            to be packed.
 * \param ins The instruction currently being executed. Used to properly
 *            handle exceptions and to determine the rounding mode.
 * \param dp  DT_F for F-floating or DT_G for G-floating.
 * \return    64-bit VAX floating in register format.
 **/
u64 CAlphaCPU::vax_rpack(UFP *r, u32 ins, u32 dp) {
  u32 rndm = I_GETFRND(ins);
  static const u64 roundbit[2] = {UF_FRND, UF_GRND};
  static const s32 expmax[2] = {G_BIAS - F_BIAS + F_M_EXP, G_M_EXP};
  static const s32 expmin[2] = {G_BIAS - F_BIAS, 0};

  if (r->frac == 0)
    return 0;                                      /* result 0? */
  if (rndm) {                                      /* round? */
    r->frac = (r->frac + roundbit[dp]) & X64_QUAD; /* add round bit */
    if ((r->frac & UF_NM) == 0) {                  /* carry out? */
      r->frac = (r->frac >> 1) | UF_NM;            /* renormalize */
      r->exp = r->exp + 1;
    }
  }

  if (r->exp > expmax[dp]) { /* ovflo? */
    vax_trap(TRAP_OVF, ins); /* set trap */
    r->exp = expmax[dp];
  } /* return max */

  if (r->exp <= expmin[dp]) { /* underflow? */
    if (ins & I_FTRP_V)
      vax_trap(TRAP_UNF, ins); /* enabled? set trap */
    return 0;
  } /* underflow to 0 */

  return (((u64)r->sign) << FPR_V_SIGN) | (((u64)r->exp) << FPR_V_EXP) |
         ((r->frac >> FPR_GUARD) & FPR_FRAC);
}

/**
 * \brief Round and pack VAX D-floating-point value
 *
 * Converts sign, exponent and fraction components to a register-format VAX
 * D-floating-point value.
 *
 * \param r   Pointer to the unpacked-floating-point UFP structure
 *            to be packed.
 * \param ins The instruction currently being executed. Used to properly
 *            handle exceptions.
 * \return    64-bit VAX D-floating in register format.
 **/
u64 CAlphaCPU::vax_rpack_d(UFP *r, u32 ins) {
  if (r->frac == 0)
    return 0;                        /* result 0? */
  r->exp = r->exp + D_BIAS - G_BIAS; /* rebias */
  if (r->exp > FDR_M_EXP) {          /* ovflo? */
    vax_trap(TRAP_OVF, ins);         /* set trap */
    r->exp = FDR_M_EXP;
  } /* return max */

  if (r->exp <= 0) { /* underflow? */
    if (ins & I_FTRP_V)
      vax_trap(TRAP_UNF, ins); /* enabled? set trap */
    return 0;
  } /* underflow to 0 */

  return (((u64)r->sign) << FDR_V_SIGN) | (((u64)r->exp) << FDR_V_EXP) |
         ((r->frac >> FDR_GUARD) & FDR_FRAC);
}

//\}
