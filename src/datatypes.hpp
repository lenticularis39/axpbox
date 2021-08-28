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
 * Contains the datatype definitions for use with Microsoft Visual C++ and
 *Linux.
 *
 * $Id: datatypes.h,v 1.20 2008/03/26 19:25:40 iamcamiel Exp $
 *
 * X-1.20       Camiel Vanderhoeven                             26-MAR-2008
 *      Use config.h information.
 *
 * X-1.19       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.18       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.17       Alex                                            20-FEB-2008
 *      GNU compiler support on Windows.
 *
 * X-1.16       Camiel Vanderhoeven                             01-FEB-2008
 *      Avoid unnecessary shift-operations to calculate constant values.
 *
 * X-1.15       Camiel Vanderhoeven                             18-JAN-2008
 *      Replaced sext_64 inlines with sext_u64_<bits> inlines for
 *      performance reasons (thanks to David Hittner for spotting this!)
 *
 * X-1.14       Camiel Vanderhoeven                             14-DEC-2007
 *      Added sext_32.
 *
 * X-1.13       Camiel Vanderhoeven                             2-DEC-2007
 *      Added various bitmask operations as inline functions.
 *
 * X-1.12       Camiel Vanderhoeven                             1-DEC-2007
 *      Use __int8, __int16 and __int32 on windows, instead of char, short,
 *      long. This makes us compatible with SDL.
 *
 * X-1.11       Camiel Vanderhoeven                             15-NOV-2007
 *      Use typedefs in stead of #define's
 *
 * X-1.10       Camiel Vanderhoeven                             14-NOV-2007
 *      Added BSD-style definitions.
 *
 * X-1.9        Camiel Vanderhoeven                             30-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.8        Camiel Vanderhoeven                             1-MAR-2007
 *      Used typedef's instead of #define's (conflict with Solaris' libc)
 *
 * X-1.7        Camiel Vanderhoeven                             27-FEB-2007
 *      OpenVMS support added.
 *
 * X-1.6        Camiel Vanderhoeven                             16-FEB-2007
 *      Added definition of "LL" for printf and scanf statements.
 *
 * X-1.5        Camiel Vanderhoeven                             12-FEB-2007
 *      Added comments.
 *
 * X-1.4        Camiel Vanderhoeven                             7-FEB-2007
 *      Added comments.
 *
 * X-1.3        Brian Wheeler                                   3-FEB-2007
 *      Formatting.
 *
 * X-1.2        Brian Wheeler                                   3-FEB-2007
 *      Linux support added.
 *
 * X-1.1        Camiel Vanderhoeven                             19-JAN-2007
 *      Initial version in CVS.
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#if !defined(INCLUDED_DATATYPES_H)
#define INCLUDED_DATATYPES_H

#if defined(HAVE_STDINT_H)
#include <cstdint>
#endif

#if defined(HAVE_INTTYPES_H)
#include <cinttypes>
#endif

#if defined(_WIN32) && !defined(__GNUWIN32__)

#define U64(a) a##ui64

#else // defined(_WIN32)

#define U64(a)  UINT64_C(a)

#endif // defined(_WIN32)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef u8 u_int8_t;
typedef u16 u_int16_t;
typedef u32 u_int32_t;
typedef u64 u_int64_t;

#define HAVE_U_INT8_T 1
#define HAVE_INT8_T 1
#define HAVE_U_INT16_T 1
#define HAVE_INT16_T 1
#define HAVE_U_INT32_T 1
#define HAVE_INT32_T 1
#define HAVE_U_INT64_T 1
#define HAVE_INT64_T 1

/**
 * Sign-extend an 8-bit value to 64 bits.
 **/
inline u64 sext_u64_8(u64 a) {
  return (((a)&U64(0x0000000000000080)) ? ((a) | U64(0xffffffffffffff00))
                                        : ((a)&U64(0x00000000000000ff)));
}

/**
 * Sign-extend a 12-bit value to 64 bits.
 **/
inline u64 sext_u64_12(u64 a) {
  return (((a)&U64(0x0000000000000800)) ? ((a) | U64(0xfffffffffffff000))
                                        : ((a)&U64(0x0000000000000fff)));
}

/**
 * Sign-extend a 13-bit value to 64 bits.
 **/
inline u64 sext_u64_13(u64 a) {
  return (((a)&U64(0x0000000000001000)) ? ((a) | U64(0xffffffffffffe000))
                                        : ((a)&U64(0x0000000000001fff)));
}

/**
 * Sign-extend an 16-bit value to 64 bits.
 **/
inline u64 sext_u64_16(u64 a) {
  return (((a)&U64(0x0000000000008000)) ? ((a) | U64(0xffffffffffff0000))
                                        : ((a)&U64(0x000000000000ffff)));
}

/**
 * Sign-extend a 21-bit value to 64 bits.
 **/
inline u64 sext_u64_21(u64 a) {
  return (((a)&U64(0x0000000000100000)) ? ((a) | U64(0xffffffffffe00000))
                                        : ((a)&U64(0x00000000001fffff)));
}

/**
 * Sign-extend a 32-bit value to 64 bits.
 **/
inline u64 sext_u64_32(u64 a) {
  return (((a)&U64(0x0000000080000000)) ? ((a) | U64(0xffffffff00000000))
                                        : ((a)&U64(0x00000000ffffffff)));
}

/**
 * Sign-extend a 48-bit value to 64 bits.
 **/
inline u64 sext_u64_48(u64 a) {
  return (((a)&U64(0x0000800000000000)) ? ((a) | U64(0xffff000000000000))
                                        : ((a)&U64(0x0000ffffffffffff)));
}

inline bool test_bit_64(u64 x, int bit) {
  return (x & (U64(0x1) << bit)) ? true : false;
}

/**
 * Sign-extend a 24-bit value to 32 bits.
 **/
inline u32 sext_u32_24(u32 a) {
  return (((a)&0x00800000) ? ((a) | 0xff000000) : ((a)&0x00ffffff));
}
#endif // INCLUDED_DATATYPES_H
