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

#if !defined(INCLUDED_STDAFX_H)
#define INCLUDED_STDAFX_H

// Include generated file with debugging flags (defines)
#include "config_debug.hpp"

#include "config.hpp"

#include "datatypes.hpp"

#ifdef _WIN32
#pragma comment( lib, "ws2_32.lib")
#endif /* _WIN32 */

#if defined(HAVE_WINDOWS_H)
#include "windows.h"
#endif

#if !defined(HAVE_STRCASECMP)
#if defined(HAVE__STRICMP)
        #define strcasecmp(a, b) _stricmp(a, b)
    #else
        #ifdef _MSC_VER
            #define strcasecmp _stricmp
        #else
            #error "Need strcasecmp"
        #endif
    #endif
#endif // !defined(HAVE_STRCASECMP)

#if !defined(HAVE_STRNCASECMP)
#if defined(HAVE__STRNICMP)
        #define strncasecmp(a, b, c) _strnicmp(a, b, c)
    #else
        #ifdef _MSC_VER
            #define strncasecmp _strnicmp
        #else
            #error "Need strncasecmp"
        #endif
    #endif
#endif // !defined(HAVE_STRNCASECMP)

#if defined(HAVE_PROCESS_H)
#include <process.h>
#endif

#if !defined(HAVE__STRDUP)
#if defined(HAVE_STRDUP)
#define _strdup(a) strdup(a)
#else
#error "Need strdup"
#endif
#endif // !defined(HAVE__STRDUP)

#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#endif

#if defined(HAVE_SIGNAL_H)
#include <signal.h>
#endif

#if defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif

#if defined(HAVE_STDLIB_H)
#include <stdlib.h>
#endif

#if defined(HAVE_STDIO_H)
#include <stdio.h>
#endif

#if defined(HAVE_STDDEF_H)
#include <stddef.h>
#endif

#if defined(HAVE_STRING_H)
#include <string.h>
#endif

#if defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#if defined(HAVE_TIME_H)
#include <time.h>
#endif

#if defined(HAVE_CTYPE_H)
#include <ctype.h>
#endif

#if !defined(HAVE_GMTIME_S)
inline void gmtime_s(struct tm *t1, time_t *t2) {
  struct tm *t3;
  t3 = gmtime(t2);
  memcpy(t1, t3, sizeof(struct tm));
}
#endif

#if !defined(HAVE_LOCALTIME_S)
#ifdef _WIN32
inline struct tm *localtime_s(struct tm *buf, time_t *timer)
#else
inline struct tm *localtime_s(time_t *timer, struct tm *buf)
#endif
{
  struct tm *tmp;
  tmp = localtime(timer);
  return (struct tm*) memcpy(buf, tmp, sizeof(struct tm));
}
#endif

#if !defined(HAVE_ISBLANK)
inline bool isblank(char c) {
  if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
    return true;
  return false;
}
#endif

inline char printable(char c) {
  if (isprint((unsigned char)c))
    return c;
  return '.';
}

#if defined(HAVE__FSEEKI64)
#define fseek_large _fseeki64
#elif defined(HAVE_FSEEKO64)
#define fseek_large fseeko64
#elif defined(HAVE_FSEEKO)
#define fseek_large fseeko
#elif defined(HAVE_FSEEK)
#define fseek_large fseek
#else
#error "Need fseek"
#endif

#if defined(HAVE__FTELLI64)
#define ftell_large _ftelli64
#define off_t_large __int64
#elif defined(HAVE_FTELLO64)
#define ftell_large ftello64
#define off_t_large off64_t
#elif defined(HAVE_FTELLO)
#define ftell_large ftello
#define off_t_large off_t
#elif defined(HAVE_FTELL)
#define ftell_large ftell
#define off_t_large off_t
#else
#error "Need ftell"
#endif

#include <atomic>
#include <memory>
#include <thread>
#include <typeinfo>

#define POCO_NO_UNWINDOWS

#include "base/Mutex.hpp"
#include "base/RWLock.hpp"
#include "base/Semaphore.hpp"
#include "base/Timestamp.hpp"

#include "es40_debug.hpp"

#include "es40_endian.hpp"

#if __cplusplus < 201402L
#ifndef _WIN32
#include "make_unique.hpp"
#endif
#endif


/* --- PRINTF_BYTE_TO_BINARY macro's --- */
#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i)    \
    (((i) & 0x80ll) ? '1' : '0'), \
    (((i) & 0x40ll) ? '1' : '0'), \
    (((i) & 0x20ll) ? '1' : '0'), \
    (((i) & 0x10ll) ? '1' : '0'), \
    (((i) & 0x08ll) ? '1' : '0'), \
    (((i) & 0x04ll) ? '1' : '0'), \
    (((i) & 0x02ll) ? '1' : '0'), \
    (((i) & 0x01ll) ? '1' : '0')

#define PRINTF_BINARY_PATTERN_INT16 \
    PRINTF_BINARY_PATTERN_INT8              PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i) \
    PRINTF_BYTE_TO_BINARY_INT8((i) >> 8),   PRINTF_BYTE_TO_BINARY_INT8(i)
#define PRINTF_BINARY_PATTERN_INT32 \
    PRINTF_BINARY_PATTERN_INT16             PRINTF_BINARY_PATTERN_INT16
#define PRINTF_BYTE_TO_BINARY_INT32(i) \
    PRINTF_BYTE_TO_BINARY_INT16((i) >> 16), PRINTF_BYTE_TO_BINARY_INT16(i)
#define PRINTF_BINARY_PATTERN_INT64    \
    PRINTF_BINARY_PATTERN_INT32             PRINTF_BINARY_PATTERN_INT32
#define PRINTF_BYTE_TO_BINARY_INT64(i) \
    PRINTF_BYTE_TO_BINARY_INT32((i) >> 32), PRINTF_BYTE_TO_BINARY_INT32(i)
/* --- end macros --- */

#endif // !defined(INCLUDED_STDAFX_H)
