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
 *
 * Parts of this file based upon GXemul, which is Copyright (C) 2004-2007
 * Anders Gavare.  All rights reserved.
 */

#pragma once
#include "base/Exception.hpp"
#include <cstdarg>

#if !defined(INCLUDED_DEBUG_H)
#define INCLUDED_DEBUG_H
#endif

#define FAILURE(cls, error_msg, ...)                                           \
  {                                                                             \
    char what_msg[8000];                                             \
    int ret = snprintf(what_msg, sizeof(what_msg), error_msg, ##__VA_ARGS__);    \
    if (ret < 0 || ret >= sizeof(what_msg)) {                                   \
        throw CRuntimeException("Error constructing error message");           \
    }                                                                             \
    char where_msg[8000];                                            \
    ret = snprintf(where_msg, sizeof(where_msg), "%s, line %i", __FILE__, __LINE__); \
    if (ret < 0 || ret >= sizeof(where_msg)) {                                  \
        throw CRuntimeException("Error constructing error message");           \
    }                                                                           \
    throw C##cls##Exception(what_msg, where_msg);                               \
  }

#define FAILURE_1(cls, error_msg, a)                                           \
  FAILURE(cls, error_msg, a)

#define FAILURE_2(cls, error_msg, a, b)                                        \
  FAILURE(cls, error_msg, a, b)

#define FAILURE_3(cls, error_msg, a, b, c)                                     \
  FAILURE(cls, error_msg, a, b, c)

#define FAILURE_4(cls, error_msg, a, b, c, d)                                  \
  FAILURE(cls, error_msg, a, b, c, d)

#define FAILURE_5(cls, error_msg, a, b, c, d, e)                               \
  FAILURE(cls, error_msg, a, b, c, d, e)

#define FAILURE_6(cls, error_msg, a, b, c, d, e, f)                            \
  FAILURE(cls, error_msg, a, b, c, d, e, f)


#define CHECK_ALLOCATION(ptr)                                                  \
  {                                                                            \
    if ((ptr) == NULL)                                                         \
      FAILURE(OutOfMemory, "Out of memory");                                   \
  }

#define CHECK_REALLOCATION(dst, src, type)                                     \
  {                                                                            \
    type *rea_x;                                                               \
    rea_x = (type *)src;                                                       \
    if ((rea_x) == NULL) {                                                     \
      FAILURE(OutOfMemory, "Out of memory");                                   \
    } else {                                                                   \
      dst = rea_x;                                                             \
    }                                                                          \
  }

