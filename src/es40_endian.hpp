/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Website: https://github.com/lenticularis39/axpbox
 *
 * Forked from: ES40 emulator
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 * Copyright (C) 2007 by Camiel Vanderhoeven
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

#if !defined(INCLUDED_ENDIAN_H)
#define INCLUDED_ENDIAN_H

#if !defined(ES40_LITTLE_ENDIAN) && !defined(ES40_BIG_ENDIAN)
#if defined(_WIN32) || defined(__VMS)
#define ES40_LITTLE_ENDIAN
#else // defined (_WIN32) || defined(__VMS)
#include <sys/param.h>

#if !defined(__BYTE_ORDER) && defined(BYTE_ORDER)
#define __BYTE_ORDER BYTE_ORDER
#if !defined(__BIG_ENDIAN) && defined(BIG_ENDIAN)
#define __BIG_ENDIAN BIG_ENDIAN
#endif
#if !defined(__LITTLE_ENDIAN) && defined(LITTLE_ENDIAN)
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#endif
#endif
#if (defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN)) || defined(sparc)
#define ES40_BIG_ENDIAN
#else // assume little endian
#define ES40_LITTLE_ENDIAN
#endif
#endif // defined (_WIN32) || defined(__VMS)
#endif // !defined(ES40_LITTLE_ENDIAN) && !defined(ES40_BIG_ENDIAN)
#define swap_64(x)                                                             \
  ((((x)&U64(0x00000000000000ff)) << 56) |                                     \
   (((x)&U64(0x000000000000ff00)) << 40) |                                     \
   (((x)&U64(0x0000000000ff0000)) << 24) |                                     \
   (((x)&U64(0x00000000ff000000)) << 8) |                                      \
   (((x)&U64(0x000000ff00000000)) >> 8) |                                      \
   (((x)&U64(0x0000ff0000000000)) >> 24) |                                     \
   (((x)&U64(0x00ff000000000000)) >> 40) |                                     \
   (((x)&U64(0xff00000000000000)) >> 56))
#define swap_32(x)                                                             \
  ((((x)&0x000000ff) << 24) | (((x)&0x0000ff00) << 8) |                        \
   (((x)&0x00ff0000) >> 8) | (((x)&0xff000000) >> 24))
#define swap_16(x) ((((x)&0x00ff) << 8) | (((x)&0xff00) >> 8))
#define swap_8(x) ((x)&0xff)
#if defined(ES40_BIG_ENDIAN)
#define endian_64(x) swap_64(x)
#define endian_32(x) swap_32(x)
#define endian_16(x) swap_16(x)
#define endian_8(x) swap_8(x)
#else // defined(ES40_BIG_ENDIAN)
#define endian_64(x) (x)
#define endian_32(x) ((x)&0xffffffff)
#define endian_16(x) ((x)&0xffff)
#define endian_8(x) ((x)&0xff)
#endif // defined(ES40_BIG_ENDIAN)
inline u64 endian_bits(u64 x, int numbits) {
  switch (numbits) {
  case 64:
    return endian_64(x);
  case 32:
    return endian_32(x);
  case 16:
    return endian_16(x);
  case 8:
    return endian_8(x);
  default:
    FAILURE(InvalidArgument, "Weird numbits in endian_bits");
  }
}
#endif // INCLUDED_ENDIAN_H
