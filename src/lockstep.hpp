/* ES40 emulator.
 * Copyright (C) 2007 by Camiel Vanderhoeven
 *
 * Website: www.camicom.com
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
 * Contains telnet declarations for the lock-step code.
 *
 * X-1.2        Camiel Vanderhoeven                             30-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.1        Camiel Vanderhoeven                             28-FEB-2007
 *      Created to support lockstep debugging.
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#if !defined(INCLUDED_LOCKSTEP_H)
#define INCLUDED_LOCKSTEP_H

#include "telnet.hpp"

#if defined(IDB) && (defined(LS_MASTER) || defined(LS_SLAVE))
extern int ls_Socket;

#if defined(LS_MASTER)
extern char ls_IP[30];
#else
extern int ls_listenSocket;
#endif
void lockstep_init();
void lockstep_sync_m2s(char *s);
void lockstep_sync_s2m(char *s);
void lockstep_compare(char *s);
void lockstep_send(char *s);
void lockstep_receive(char *s, int sz);
#endif // defined(IDB) && (defined(LS_MASTER) || defined(LS_SLAVE))
#endif // !defined(INCLUDED_LOCKSTEP_H)
