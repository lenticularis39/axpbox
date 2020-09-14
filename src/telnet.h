/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * WWW    : http://sourceforge.net/projects/es40
 * E-mail : camiel@camicom.com
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
 * Contains TCP/IP declarations used by the serial port emulator and lock-step
 * code. Different OS'es need different header files included, and some OS'es
 * miss certain functions or macro's we need. In this file, we try to take
 * away most of these differences.
 *
 * $Id: telnet.h,v 1.12 2008/03/26 19:25:40 iamcamiel Exp $
 *
 * X-1.12       Camiel Vanderhoeven                             26-MAR-2008
 *      Use config.h information.
 *
 * X-1.11       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.10       Alex                                            20-FEB-2008
 *      GNU compiler support on Windows.
 *
 * X-1.9        Camiel Vanderhoeven                             04-JAN-2008
 *      Comments.
 *
 * X-1.8        Fang Zhe                                        04-JAN-2008
 *      Include sys/socket.h on Apple OS X.
 *
 * X-1.7        Camiel Vanderhoeven                             02-JAN-2008
 *      Comments.
 *
 * X-1.6        Brian Wheeler                                   1-DEC-2007
 *      Corrected an unsigned/signed issue in inet_aton.
 *
 * X-1.5        Camiel Vanderhoeven                             15-NOV-2007
 *      Replace winsock.h by winsock2.h.
 *
 * X-1.4        Camiel Vanderhoeven                             15-NOV-2007
 *      Added some includes for Linux.
 *
 * X-1.3        Camiel Vanderhoeven                             14-NOV-2007
 *      Added inet_aton.
 *
 * X-1.2        Camiel Vanderhoeven                             30-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.1        Camiel Vanderhoeven                             28-FEB-2007
 *      File created. Code was previously found in Serial.cpp and Serial.h
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#if !defined(INCLUDED_TELNET_H)
#define INCLUDED_TELNET_H

#if defined(HAVE_WINSOCK2_H)
#include <winsock2.h>
#endif

#if defined(HAVE_WS2TCPIP_H)
#include <ws2tcpip.h>
#endif

#if defined(HAVE_SYS_SOCKET_H)
#include <sys/socket.h>
#endif

#if defined(HAVE_SOCKET_H)
#include <socket.h>
#endif

#if defined(HAVE_IN_H)
#include <in.h>
#endif

#if defined(HAVE_INET_H)
#include <inet.h>
#endif

#if defined(HAVE_ARPA_INET_H)
#include <arpa/inet.h>
#endif

#if defined(HAVE_ARPA_TELNET_H)
#include <arpa/telnet.h>
#endif

#if defined(HAVE_NETINET_IN_H)
#include <netinet/in.h>
#endif

#if defined(HAVE_SYS_SELECT_H)
#include <sys/select.h>
#endif

#if defined(HAVE_ERRNO_H)
#include <errno.h>
#endif

#if defined(HAVE_FCNTL_H)
#include <fcntl.h>
#endif

#if defined(HAVE_SIGNAL_H)
#include <signal.h>
#endif

#if defined(_WIN32) && !defined(__GNUWIN32__)
typedef size_t ssize_t;
typedef int socklen_t;
#endif // _WIN32

#if defined(__VMS)
#define INVALID_SOCKET -1
typedef unsigned int socklen_t;
#endif // __VMS

#if defined(_WIN32) || defined(__VMS)
#define IAC 255   /* interpret as command: */
#define DONT 254  /* you are not to use option */
#define DO 253    /* please, you use option */
#define WONT 252  /* I won't use option */
#define WILL 251  /* I will use option */
#define SB 250    /* interpret as subnegotiation */
#define GA 249    /* you may reverse the line */
#define EL 248    /* erase the current line */
#define EC 247    /* erase the current character */
#define AYT 246   /* are you there */
#define AO 245    /* abort output--but let prog finish */
#define IP 244    /* interrupt process--permanently */
#define BREAK 243 /* break */
#define DM 242    /* data mark--for connect. cleaning */
#define NOP 241   /* nop */
#define SE 240    /* end sub negotiation */
#define EOR 239   /* end of record (transparent mode) */
#define ABORT 238 /* Abort process */
#define SUSP 237  /* Suspend process */
#define xEOF 236  /* End of file: EOF is already used... */

#define SYNCH 242       /* for telfunc calls */
#define TELOPT_ECHO 1   /* echo */
#define TELOPT_SGA 3    /* suppress go ahead */
#define TELOPT_NAWS 31  /* window size */
#define TELOPT_LFLOW 33 /* remote flow control */

#else // defined(_WIN32) || defined(__VMS)
#define INVALID_SOCKET 1
#endif // defined (_WIN32) || defined(__VMS)

/* inet_aton -- Emulate BSD inet_aton via inet_addr.
 *
 * Useful on systems that don't have inet_aton, such as Solaris,
 * to let your code use the better inet_aton interface and use autoconf
 * and AC_REPLACE_FUNCS([inet_aton]).
 *
 * Copyright (C) 2003 Matthias Andree <matthias.andree@gmx.de>
 */
#if !defined(HAVE_INET_ATON)
inline int inet_aton(const char *name, struct in_addr *addr) {
  unsigned long a = inet_addr(name);
  addr->s_addr = a;
  return a != (unsigned int)-1;
}
#endif
#endif // !defined(INCLUDED_TELNET_H)
