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
 * Contains floating point conversion code used by AlphaCPU.cpp.
 * We've chosen to keep the floating point values as 64-bit integers in the
 *floating point registers, and to convert them to/from the host's native
 *floating point format when required.
 *
 * $Id: es40_float.h,v 1.21 2008/03/14 15:30:52 iamcamiel Exp $
 *
 * X-1.19       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.18       Camiel Vanderhoeven                             06-FEB-2008
 *      Check for FPEN in old floating point code.
 *
 * X-1.17       Camiel Vanderhoeven                             05-FEB-2008
 *      File restored. Only use new floating-point code when HAVE_NEW_FP
 *      has been defined.
 *
 * X-1.16       Camiel Vanderhoeven                             02-JAN-2008
 *      Comments.
 *
 * X-1.15       Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.14       Camiel Vanderhoeven                             28-DEC-2007
 *      Avoid compiler warnings.
 *
 * X-1.13       Camiel Vanderhoeven                             18-DEC-2007
 *      Removed some messages.
 *
 * X-1.12       Camiel Vanderhoeven                             18-DEC-2007
 *      Conversion to/from IEEE through a union on IEEE-machined.
 *
 * X-1.11       Camiel Vanderhoeven                             10-NOV-2007
 *      Corrected IEEE conversion problem; made really sure no endless
 *      loops can occur in any of the host2xxx conversions.
 *
 * X-1.10       Camiel Vanderhoeven                             08-NOV-2007
 *      Added itof_f for ITOFF instruction (load_f without VAX-swapping).
 *
 * X-1.9        Camiel Vanderhoeven                             08-NOV-2007
 *      Restructured conversion routines. There now is a real difference
 *      between 32-bit and 64-bit floating point operations.
 *
 * X-1.8	    Camiel Vanderhoeven 07-NOV-2007 Fixed f2v to avoid endless
 *loop.
 *
 * X-1.7	    Camiel Vanderhoeven	                            07-NOV-2007
 *	Disabled some printf statements.
 *
 * X-1.6	    Eduardo Marcelo Serrat                          31-OCT-2007
 *      Fixed conversion routines.
 *
 * X-1.5        Camiel Vanderhoeven                             11-APR-2007
 *      Explicitly convert integers to double for calls to log/pow.
 *
 * X-1.4	    Brian Wheeler 30-MAR-2007 Added a couple of typecasts to
 *avoid compiler warnings
 *
 * X-1.3        Camiel Vanderhoeven                             30-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.2        Camiel Vanderhoeven                             28-MAR-2007
 *      Properly put in all 64-bit constants as X64(...) instead of 0x...
 *
 * X-1.1        Camiel Vanderhoeven                             18-MAR-2007
 *      File created to support basic floating point operations.
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#include <math.h>

//#define DEBUG_FP_CONVERSION 1
//#define DEBUG_FP_LOADSTORE 1
#define FLOAT_IS_IEEE 1

/**
 * VAX (G or F) floating point to host conversion.
 * Converts the register-form of F and G foating point values to a double.
 **/
inline double f2host(u64 val) {
  int s = (val & U64(0x8000000000000000)) ? 1 : 0;
  int e = (int)((val & U64(0x7ff0000000000000)) >> 52);
  s64 f = (val & U64(0x000fffffffffffff));
  f |= U64(0x0010000000000000);

  double res;

  if (e == 0)
    res = 0.0;
  else
    res = (s ? -1.0 : 1.0) * pow((double)2.0, e - 1024) *
          ((double)f / (double)(s64)U64(0x0020000000000000));

#if defined(DEBUG_FP_CONVERSION)
  printf("f/g->host: %016" PRIx64 " -> %f   \n", val, res);
#endif
  return res;
}

#define g2host f2host

/**
 * VAX (D) floating point to host conversion.
 * Converts the register-form of D foating point values to a double.
 **/
inline double d2host(u64 val) {
  int s = (val & U64(0x8000000000000000)) ? 1 : 0;
  int e = (int)((val & U64(0x7f80000000000000)) >> 55);
  s64 f = (val & U64(0x007fffffffffffff));
  f |= U64(0x0080000000000000);

  double res;

  if (e == 0)
    res = 0.0;
  else
    res = (s ? -1.0 : 1.0) * pow((double)2.0, e - 128) *
          ((double)f / (double)(s64)U64(0x0100000000000000));

#if defined(DEBUG_FP_CONVERSION)
  printf("d->host: %016" PRIx64 " -> %f   \n", val, res);
#endif
  return res;
}

/**
 * IEEE (S or T) floating point to host conversion.
 * Converts the register-form of S and T foating point values to a double.
 **/
inline double s2host(u64 val) {
  double res;

#if defined(FLOAT_IS_IEEE)
  if (sizeof(double) == 8) {
    union s2h_conv {
      u64 a;
      double b;
    } f_ieee;
    f_ieee.a = val;
    res = f_ieee.b;
  } else
#endif
  {
    int s = (val & U64(0x8000000000000000)) ? 1 : 0;
    int e = (int)((val & U64(0x7ff0000000000000)) >> 52);
    s64 f = (val & U64(0x000fffffffffffff));

    if (e == 2047) {
      if (f)
        res = (s ? -0.0 : 0.0) / 0.0; // NaN
      else
        res = (s ? -1.0 : 1.0) / 0.0; // +/- Inf
    } else if (e == 0) {
      if (f)
        res = (s ? -1.0 : 1.0) *
              ldexp((double)f / (double)((s64)U64(0x10000000000000)), -1022);
      else
        res = (s ? -1.0 : 1.0) * 0.0;
    } else {
      res = (s ? -1.0 : 1.0) *
            ldexp(1.0 + ((double)f / (double)((s64)U64(0x0010000000000000))),
                  e - 1023);
    }
  }

#if defined(DEBUG_FP_CONVERSION)
  printf("s/t->host: %016" PRIx64 " -> %f   \n", val, res);
#endif
  return res;
}

#define t2host s2host

/**
 * Check IEEE floating point value for NaN.
 **/
inline bool i_isnan(u64 val) {
  int e = (int)((val & U64(0x7ff0000000000000)) >> 52);
  s64 f = (val & U64(0x000fffffffffffff));

  return (e == 2047) && f;
}

/**
 * Host to VAX F floating point conversion.
 * Converts a double to the register-form of F foating point values.
 **/
inline u64 host2f(double val) {
  double fr;
  double v = val;
  int s = (v < 0.0) ? 1 : 0;
  if (s)
    v *= -1.0;

  int e = (int)(log((double)v) / log((double)2.0));
  bool exp_down = true;

  if (val == 0.0)
    return 0;

  fr = v / pow((double)2.0, e);

  while ((fr >= 1.0 && e < 127) || e < -127) {
    e++;
    exp_down = false;
    fr = v / pow((double)2.0, e);
  }

  while (((fr < 0.5 && e > -127) || e > 127) && exp_down) {
    e--;
    fr = v / pow((double)2.0, e);
  }

  e += 1024;

  u64 f = (u64)(fr * (double)U64(0x0020000000000000) + 0.5);

  f = (s ? U64(0x8000000000000000) : 0) |
      (((u64)e << 52) & U64(0x7ff0000000000000)) |
      (f & U64(0x000fffffe0000000));

#if defined(DEBUG_FP_CONVERSION)
  printf("host->f: %f -> %016" PRIx64 "   \n", val, f);
#endif
  return f;
}

/**
 * Host to VAX G floating point conversion.
 * Converts a double to the register-form of G foating point values.
 **/
inline u64 host2g(double val) {
  double fr;
  double v = val;
  int s = (v < 0.0) ? 1 : 0;
  if (s)
    v *= -1.0;

  int e = (int)(log((double)v) / log((double)2.0));
  bool exp_down = true;

  if (val == 0.0)
    return 0;

  fr = v / pow((double)2.0, e);

  while ((fr >= 1.0 && e < 1023) || e < -1023) {
    e++;
    exp_down = false;
    fr = v / pow((double)2.0, e);
  }

  while (((fr < 0.5 && e > -1023) || e > 1023) && exp_down) {
    e--;
    fr = v / pow((double)2.0, e);
  }

  e += 1024;

  u64 f = (u64)(fr * (double)U64(0x0020000000000000) + 0.5);

  f = (s ? U64(0x8000000000000000) : 0) |
      (((u64)e << 52) & U64(0x7ff0000000000000)) |
      (f & U64(0x000fffffffffffff));

#if defined(DEBUG_FP_CONVERSION)
  printf("host->g: %f -> %016" PRIx64 "   \n", val, f);
#endif
  return f;
}

/**
 * Host to VAX D floating point conversion.
 * Converts a double to the register-form of D foating point values.
 **/
inline u64 host2d(double val) {
  double fr;
  double v = val;
  int s = (v < 0.0) ? 1 : 0;
  if (s)
    v *= -1.0;

  int e = (int)(log((double)v) / log((double)2.0));
  bool exp_down = true;

  if (val == 0.0)
    return 0;

  fr = v / pow((double)2.0, e);

  while ((fr >= 1.0 && e < 127) || e < -127) {
    e++;
    exp_down = false;
    fr = v / pow((double)2.0, e);
  }

  while (((fr < 0.5 && e > -127) || e > 127) && exp_down) {
    e--;
    fr = v / pow((double)2.0, e);
  }

  e += 128;

  u64 f = (u64)(fr * (double)U64(0x0100000000000000) + 0.5);

  f = (s ? U64(0x8000000000000000) : 0) |
      (((u64)e << 55) & U64(0x7f80000000000000)) |
      (f & U64(0x007fffffffffffff));

#if defined(DEBUG_FP_CONVERSION)
  printf("host->d: %f -> %016" PRIx64 "   \n", val, f);
#endif
  return f;
}

/**
 * Map an 8-bit IEEE (S) exponent to an 11-bit IEEE (T) exponent.
 **/
inline u32 map_s(u32 val) {
  if (val == 0)
    return 0;
  else if (val == 0xff)
    return 0x7ff;
  else if (val & 0x80)
    return (val & 0x7f) | 0x400;
  else
    return (val & 0x7f) | 0x380;
}

/**
 * Host to 32-bit IEEE (S) floating point conversion.
 * Converts a double to the register-form of S foating point values.
 **/
inline u64 host2s(double val) {
  u64 f;
  int s;
  int e;
#if defined(FLOAT_IS_IEEE)
  if (sizeof(float) == 4) {
    union h2s_conv {
      u32 a;
      float b;
    } f_ieee;
    f_ieee.b = (float)val;
    s = (f_ieee.a >> 31) & 1;
    e = (f_ieee.a >> 23) & 0xff;
    f = (u64)(f_ieee.a >> 0 & 0x7fffff) << 29;
  } else
#endif
  {
    double v = val;
    s = (v < 0.0) ? 1 : 0;
    if (s)
      v *= -1.0;
    e = (int)(log((double)v) / log((double)2.0));

    double fr;
    bool exp_down = true;

    if (val == 0.0)
      return 0;

    fr = v / pow((double)2.0, e);

    while ((fr >= 2.0 && e < 127) || e < -127) {
      e++;
      exp_down = false;
      fr = v / pow((double)2.0, e);
    }

    while (((fr < 1.0 && e > -127) || e > 127) && exp_down) {
      e--;
      fr = v / pow((double)2.0, e);
    }

    e += 255;

    if (e == 0)
      fr = v / pow((double)2.0, -126);

    f = (u64)(fr * (double)U64(0x0010000000000000) + 0.5);
  }

  e = map_s(e);

  f = (s ? U64(0x800000000000000) : 0) |
      (((u64)e << 52) & U64(0x7ff0000000000000)) |
      (f & U64(0x000fffffe0000000));

#if defined(DEBUG_FP_CONVERSION)
  printf("host->s: %f -> %016" PRIx64 "   \n", val, f);
#endif
  return f;
}

/**
 * Host to 64-bit IEEE (T) floating point conversion.
 * Converts a double to the register-form of T foating point values.
 **/
inline u64 host2t(double val) {
  u64 f;
#if defined(FLOAT_IS_IEEE)
  if (sizeof(double) == 8) {
    union h2t_conv {
      u64 a;
      double b;
    } f_ieee;
    f_ieee.b = val;
    f = f_ieee.a;
  } else
#endif
  {
    double v = val;
    int s = (v < 0.0) ? 1 : 0;
    if (s)
      v *= -1.0;

    int e = (int)(log((double)v) / log((double)2.0));
    double fr;
    bool exp_down = true;

    if (val == 0.0)
      return 0;

    fr = v / pow((double)2.0, e);

    while ((fr >= 2.0 && e < 1023) || e < -1023) {
      e++;
      exp_down = false;
      fr = v / pow((double)2.0, e);
    }

    while (((fr < 1.0 && e > -1023) || e > 1023) && exp_down) {
      e--;
      fr = v / pow((double)2.0, e);
    }

    e += 1023;

    if (e == 0)
      fr = v / pow((double)2.0, -1022);

    f = (u64)(fr * (double)U64(0x0010000000000000) + 0.5);

    f = (s ? U64(0x800000000000000) : 0) |
        (((u64)e << 52) & U64(0x7ff0000000000000)) |
        (f & U64(0x000fffffffffffff));
  }

#if defined(DEBUG_FP_CONVERSION)
  printf("host->t: %f -> %016" PRIx64 "   \n", val, f);
#endif
  return f;
}

/**
 * Perform the VAX-byte ordering swap + the SEF mapping necessary to store
 * 32-bit VAX (F) floating point values to memory.
 **/
inline u32 store_f(u64 val) {
  u64 retval = (val & U64(0x00001fffe0000000)) >>
               13; /* frac.lo          : 29..44 --> 16..31 */
  retval |= (val & U64(0xc000000000000000)) >>
            48; /* exp.hi + sign    : 62..63 --> 14..15 */
  retval |= (val & U64(0x07ffe00000000000)) >>
            45; /* frac.hi + exp.lo : 45..58 -->  0..13 */

#if defined(DEBUG_FP_LOADSTORE)
  printf("f->mem: %016" PRIx64 " -> %08x   \n", val, retval);
#endif
  return (u32)retval;
}

/**
 * Perform the VAX-byte ordering swap necessary to store 64-bit VAX (G)
 * floating point values to memory.
 **/
inline u64 store_g(u64 val) {
  u64 retval = (val >> 48) & U64(0x000000000000ffff);
  retval |= (val >> 16) & U64(0x00000000ffff0000);
  retval |= (val << 48) & U64(0xffff000000000000);
  retval |= (val << 16) & U64(0x0000ffff00000000);

#if defined(DEBUG_FP_LOADSTORE)
  printf("g->mem: %016" PRIx64 " -> %016" PRIx64 "   \n", val, retval);
#endif
  return retval;
}

/**
 * Perform the VAX-byte ordering swap + the SEF mapping necessary to load
 * 32-bit VAX (F) floating point values from memory.
 **/
inline u64 load_f(u32 val) {
  u64 retval = (u64)(val & 0xffff0000)
               << 13; /* frac.lo          : 16..31 --> 29..44 */
  retval |= (u64)(val & 0x0000c000)
            << 48; /* exp.hi + sign    : 14..15 --> 62..63 */
  retval |= (u64)(val & 0x00003fff)
            << 45; /* frac.hi + exp.lo :  0..13 --> 45..58 */
  if (((val & 0x00004000) == 0) && ((val & 0x00003f80) != 0))
    retval |= U64(0x3800000000000000); /* exp.mid */

#if defined(DEBUG_FP_LOADSTORE)
  printf("mem->f: %08x -> %016" PRIx64 "   \n", val, retval);
#endif
  return retval;
}

/**
 * Perform the SEF mapping necessary to load
 * 32-bit VAX (F) floating point values from an integer register.
 **/
inline u64 itof_f(u64 val) {
  u64 retval = (val & U64(0x3fffffff))
               << 29; /* frac + exp.lo  :  0..29 --> 29..58 */
  retval |= (val & U64(0xc0000000))
            << 32; /* exp.hi + sign : 30..31 --> 62..63 */
  if (((val & U64(0x40000000)) == 0) && ((val & U64(0x3f800000)) != 0))
    retval |= U64(0x3800000000000000); /* exp.mid */

#if defined(DEBUG_FP_LOADSTORE)
  printf("reg->f: %08x -> %016" PRIx64 "   \n", val, retval);
#endif
  return retval;
}

/**
 * Perform the VAX-byte ordering swap necessary to load 64-bit VAX (G)
 * floating point values from memory.
 **/
inline u64 load_g(u64 val) {
  u64 retval = (val & U64(0x000000000000ffff)) << 48;
  retval |= (val & U64(0x00000000ffff0000)) << 16;
  retval |= (val & U64(0x0000ffff00000000)) >> 16;
  retval |= (val & U64(0xffff000000000000)) >> 48;

#if defined(DEBUG_FP_LOADSTORE)
  printf("mem->g: %016" PRIx64 " -> %016" PRIx64 "   \n", val, retval);
#endif
  return retval;
}

/**
 * Perform the the SEF mapping necessary to load 32-bit IEEE (S)
 * floating point values from memory.
 **/
inline u64 load_s(u32 val) {
  return ((val & U64(0x80000000)) << 32)          // sign
         | ((u64)map_s((val >> 23) & 0xff) << 52) // exp
         | ((val & U64(0x7fffff)) << 29);
}

/**
 * Perform the the SEF mapping necessary to store 32-bit IEEE (S)
 * floating point values to memory.
 **/
inline u32 store_s(u64 val) {
  return ((u32)(val >> 32) & 0xc0000000) | ((u32)(val >> 29) & 0x3fffffff);
}
