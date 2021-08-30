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

#if !defined(INCLUDED_ETHERNET_H)
#define INCLUDED_ETHERNET_H

#include "StdAfx.hpp"

#define ETH_MAX_PACKET_RAW 1514
#define ETH_MAX_PACKET_CRC 1518

struct eth_frame { // ethernet (wire) frame
  u8 src[6];       // source address
  u8 dst[6];       // destination address
  u8 protocol[2];  // protocol
  u8 data[1500];   // data: variable 46-1500 bytes
  u8 crc_fill[4];  // space for max packet crc
};

struct eth_packet {             // ethernet packet
  int len;                      // size of packet
  int used;                     // bytes used (consumed)
  u8 frame[ETH_MAX_PACKET_CRC]; // ethernet frame
};

/**
 * \brief Packet Queue for Ethernet packets.
 **/
class CPacketQueue { // Ethernet Packet Queue

  // private:
public:
  const char *name;    // queue name
  int max;             // maximum items allowed in queue
  int head;            // first item in queue
  int tail;            // last item in queue
  int cnt;             // current item count
  int highwater;       // highwater mark (statistics)
  int dropped;         // packets dropped because queue was full
  eth_packet *packets; // packet array; dynamically allocated
public:
  inline int count() { return cnt; }

  // get current count
  inline int lost() { return dropped; }

  // get number of lost packets
  void flush(); // empties packet queue
  bool add_tail(const u8 *packet_data, int packet_len, bool calc_crc,
                bool need_crc);            // adds pcap packet to queue
  bool get_head(eth_packet &packet);       // get packet at head
  CPacketQueue(const char *name, int max); // constructor
  ~CPacketQueue();                         // destructor
};
#endif // !defined(INCLUDED_ETHERNET_H)
