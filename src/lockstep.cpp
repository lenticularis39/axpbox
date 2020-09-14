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
 * Contains the code for the lockstep debugging mechanism.
 *
 * X-1.4        Camiel Vanderhoeven                             18-APR-2007
 *      Faster lockstep mechanism (send info 50 cpu cycles at a time)
 *
 * X-1.3        Camiel Vanderhoeven                             31-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.2        Camiel Vanderhoeven                             28-FEB-2007
 *      Fixed a silly bug (forgot to declare nAddressSize)
 *
 * X-1.1        Camiel Vanderhoeven                             28-FEB-2007
 *      Created to support lockstep mechanism.
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#include "StdAfx.h"

#include "lockstep.h"

#if defined(IDB) && (defined(LS_MASTER) || defined(LS_SLAVE))
int ls_Socket;

#if defined(LS_MASTER)
char ls_IP[30];
#else
int ls_listenSocket;
#endif
void lockstep_init() {
  struct sockaddr_in Address;

#if defined(_WIN32)

  // Windows Sockets only work after calling WSAStartup.
  WSADATA wsa;
  WSAStartup(0x0101, &wsa);
#endif // defined (_WIN32)
#if defined(LS_MASTER)
  int result = -1;
  printf("Please enter the IP address of the lockstep slave to connect to: ");
  scanf("%s", ls_IP);

  ls_Socket = socket(AF_INET, SOCK_STREAM, 0);

  Address.sin_family = AF_INET;
  Address.sin_port = htons(21260);
  Address.sin_addr.s_addr = inet_addr(ls_IP);

  printf("%%LST-I-WAIT: Waiting to initiate lockstep connection to %s.\n",
         ls_IP);

  while (result == -1)
    result = connect(ls_Socket, (struct sockaddr *)&Address,
                     sizeof(struct sockaddr));

#else // defined(LS_MASTER)
  socklen_t nAddressSize = sizeof(struct sockaddr_in);

  ls_listenSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (ls_listenSocket == INVALID_SOCKET)
    printf("%%LST-F-NOSOCK Could not open lockstep socket to listen on!\n");

  Address.sin_addr.s_addr = INADDR_ANY;
  Address.sin_port = htons(21260);
  Address.sin_family = AF_INET;

  int optval = 1;
  setsockopt(ls_listenSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&optval,
             sizeof(optval));
  bind(ls_listenSocket, (struct sockaddr *)&Address, sizeof(Address));
  listen(ls_listenSocket, 1);

  printf("%%LST-I-WAIT: Waiting for lockstep connection on port %d.\n", 21260);

  //  Wait until we have a connection
  ls_Socket = INVALID_SOCKET;
  while (ls_Socket == INVALID_SOCKET)
    ls_Socket =
        accept(ls_listenSocket, (struct sockaddr *)&Address, &nAddressSize);
#endif
  printf("%%LST-I-INIT: Lock-step connection initialized.\n");
}

void lockstep_sync_m2s(char *s) {
#if defined(LS_MASTER)
  send(ls_Socket, s, strlen(s) + 1, 0);

#else
  fd_set readset;
  unsigned char buffer[1000];
  ssize_t size;
  struct timeval tv;

  buffer[0] = 0;

  while (strcmp((char *)buffer, s)) {
    FD_ZERO(&readset);
    FD_SET(ls_Socket, &readset);
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    while (select(ls_Socket + 1, &readset, NULL, NULL, &tv) <= 0)
      ;
    size = recv(ls_Socket, (char *)buffer, 999, 0);
    buffer[size + 1] = 0; // force null termination.
  }
#endif
}

void lockstep_sync_s2m(char *s) {
#if defined(LS_SLAVE)
  send(ls_Socket, s, strlen(s) + 1, 0);

#else
  fd_set readset;
  unsigned char buffer[1000];
  ssize_t size;
  struct timeval tv;

  buffer[0] = 0;

  while (strcmp((char *)buffer, s)) {
    FD_ZERO(&readset);
    FD_SET(ls_Socket, &readset);
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    while (select(ls_Socket + 1, &readset, NULL, NULL, &tv) <= 0)
      ;
    size = recv(ls_Socket, (char *)buffer, 999, 0);
    buffer[size + 1] = 0; // force null termination.
  }
#endif
}

char cmpbuffer[10000];

void lockstep_compare(char *s) {
#if defined(LS_SLAVE)
  send(ls_Socket, s, strlen(s) + 1, 0);

#else
  fd_set readset;
  ssize_t size;
  struct timeval tv;
  char *b1;
  char *b2;
  char *n1;
  char *n2;

  FD_ZERO(&readset);
  FD_SET(ls_Socket, &readset);
  tv.tv_sec = 30;
  tv.tv_usec = 0;
  while (select(ls_Socket + 1, &readset, NULL, NULL, &tv) <= 0)
    ;
  size = recv(ls_Socket, cmpbuffer, 99999, 0);
  cmpbuffer[size + 1] = 0; // force null termination.

  //  printf("Comparing <%s> AND <%s>\n",s,buffer);
  b1 = s;
  b2 = cmpbuffer;

  while (b1 && b2) {
    n1 = strchr(b1, '\n');
    n2 = strchr(b2, '\n');
    if (n1)
      *n1++ = '\0';
    if (n2)
      *n2++ = '\0';
    if (strcmp(b1, b2)) {
      printf(
          "*************** LOCKSTEP: DIFFERENCE ENCOUNTERED ***************\n");
      printf(" local system: %s\n", b1);
      printf("remote system: %s\n", b2);
      printf(
          "***************     PRESS ENTER TO CONTINUE      ***************\n");
      getc(stdin);
    }

    b1 = n1;
    b2 = n2;
  }
#endif
}

void lockstep_send(char *s) {

  //  printf("<send %s>",s);
  fd_set readset;
  ssize_t size;
  struct timeval tv;

  char sConf[100] = "";
  while (strcmp(sConf, "ACK")) {
    send(ls_Socket, s, strlen(s) + 1, 0);
    FD_ZERO(&readset);
    FD_SET(ls_Socket, &readset);
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (select(ls_Socket + 1, &readset, NULL, NULL, &tv) > 0) {
      size = recv(ls_Socket, sConf, 99, 0);
      sConf[size + 1] = 0; // force null termination.
    }
  }
}

void lockstep_receive(char *s, int sz) {
  fd_set readset;
  ssize_t size = 0;
  struct timeval tv;
  s[0] = 'R';

  while (!size) {
    FD_ZERO(&readset);
    FD_SET(ls_Socket, &readset);
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    if (select(ls_Socket + 1, &readset, NULL, NULL, &tv) > 0) {
      size = recv(ls_Socket, s, sz - 1, 0);
      s[size + 1] = 0; // force null termination.
    }
  }

  send(ls_Socket, "ACK", 4, 0);
}
#endif // defined(IDB) && (defined(LS_MASTER) || defined(LS_SLAVE))
