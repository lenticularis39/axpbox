/* ES40 emulator.
 * Copyright (C) 2007-2008 by Camiel Vanderhoeven
 *
 * Website: http://www.es40.org
 * E-mail : camiel@es40.org
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
 * Standard includes.
 * Include file for standard system include files,
 * or project specific include files that are used frequently, but
 * are changed infrequently.
 *
 * $Id: StdAfx.h,v 1.35 2008/05/31 15:47:13 iamcamiel Exp $
 *
 * X-1.35       Camiel Vanderhoeven                             31-MAY-2008
 *      Changes to include parts of Poco.
 *
 * X-1.34       Camiel Vanderhoeven                             31-MAR-2008
 *      Moved Poco-includes to StdAfx.h, include config_vms.h on VMS.
 *
 * X-1.33       Camiel Vanderhoeven                             26-MAR-2008
 *      Use config.h information.
 *
 * X-1.32       Camiel Vanderhoeven                             21-MAR-2008
 *      Added inclusion of config_debug.h.
 *
 * X-1.31       Camiel Vanderhoeven                             14-MAR-2008
 *      Added inclusion of typeinfo.
 *
 * X-1.30       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.29       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.28       Camiel Vanderhoeven                             11-MAR-2008
 *      Named, debuggable mutexes.
 *
 * X-1.27       Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.26       Alex                                            20-FEB-2008
 *      GNU compiler support on Windows.
 *
 * X-1.25       Camiel Vanderhoeven                             19-JAN-2008
 *      Run CPU in a separate thread if CPU_THREADS is defined.
 *      NOTA BENE: This is very experimental, and has several problems.
 *
 * X-1.24       Fang Zhe                                        05-JAN-2008
 *      Do 64-bit file I/O properly for FreeBSD and OS X.
 *
 * X-1.23       Camiel Vanderhoeven                             04-JAN-2008
 *      Put in definitions to handle 64-bit file I/O OS-independently.
 *
 * X-1.22       Fang Zhe                                        03-JAN-2008
 *      Put es40_endian.h after es40_debug.h as it uses the FAILURE macro.
 *
 * X-1.20       Fang Zhe                                        03-JAN-2008
 *      Help compilation on Mac OS X and FreeBSD.
 *
 * X-1.19       Camiel Vanderhoeven                             02-JAN-2008
 *      Cleanup.
 *
 * X-1.18       Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.17       Camiel Vanderhoeven                             1-DEC-2007
 *      Include stdlib.h by default. We don't do MFC, so replace afx.h
 *      with windows.h
 *
 * X-1.16       Camiel Vanderhoeven                             16-NOV-2007
 *      Removed winsock.
 *
 * X-1.15       Camiel Vanderhoeven                             14-NOV-2007
 *      Added es40_debug.h
 *
 * X-1.14       Camiel Vanderhoeven                             30-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.13       Brian Wheeler                                   8-MAR-2007
 *      endian.h renamed to es40_endian.h
 *
 * X-1.12       Camiel Vanderhoeven                             1-MAR-2007
 *      Included endian.h to support the Solaris/SPARC port.
 *
 * X-1.11       Camiel Vanderhoeven                             28-FEB-2007
 *      Included datatypes.h
 *
 *
 * X-1.10       Camiel Vanderhoeven                             16-FEB-2007
 *   a) Changed header guards
 *   b) Re-design of #if-#else-#endif constructions to make more sense.
 *   c) Added strcasecmp and strncasecmp definitions for Win32.
 *   d) Added _strdup for non-Win32.
 *
 * X-1.9        Camiel Vanderhoeven                             12-FEB-2007
 *      Added comments.
 *
 * X-1.8        Camiel Vanderhoeven                             9-FEB-2007
 *      Added comments.
 *
 * X-1.7        Camiel Vanderhoeven                             7-FEB-2007
 *      Added isblank for DEC CXX (Linux/Alpha).
 *
 * X-1.6        Camiel Vanderhoeven                             3-FEB-2007
 *      Added sleep_ms.
 *
 * X-1.5        Camiel Vanderhoeven                             3-FEB-2007
 *      Added printable.
 *
 * X-1.4        Camiel Vanderhoeven                             3-FEB-2007
 *      Added is_blank for Win32.
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
#if !defined(INCLUDED_STDAFX_H)
#define INCLUDED_STDAFX_H

// Include generated file with debugging flags (defines)
#include "config_debug.hpp"

#include "config.hpp"

#include "datatypes.hpp"

#if defined(HAVE_WINDOWS_H)
#include <windows.h>
#endif

#if !defined(HAVE_STRCASECMP)
#if defined(HAVE__STRICMP)
#define strcasecmp(a, b) _stricmp(a, b)
#else
#error "Need strcasecmp"
#endif
#endif // !defined(HAVE_STRCASECMP)

#if !defined(HAVE_STRNCASECMP)
#if defined(HAVE__STRNICMP)
#define strncasecmp(a, b, c) _strnicmp(a, b, c)
#else
#error "Need strncasecmp"
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

// Different OS'es define different functions to access 64-bit files
#if defined(HAVE_FOPEN64)
#define fopen_large fopen64
#elif defined(HAVE_FOPEN)
#define fopen_large fopen
#else
#error "Need fopen"
#endif

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
#include "make_unique.hpp"
#endif

#endif // !defined(INCLUDED_STDAFX_H)
