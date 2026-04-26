/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Website: https://github.com/lenticularis39/axpbox
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
 */

#if !defined(INCLUDED_NETWORK_PCAP_H)
#define INCLUDED_NETWORK_PCAP_H

#include "NetworkBackend.hpp"

#if defined(WIN32)
#define HAVE_REMOTE
#endif
#include <pcap.h>

class CNetworkPcap : public CNetworkBackend {
public:
  CNetworkPcap();
  virtual ~CNetworkPcap();

  virtual bool init(const char *devid_string, CConfigurator *cfg);
  virtual int send(const u8 *data, int len);
  virtual int receive(const u8 **data, int *len);
  virtual void set_filter(u8 mac_list[][6], int num_macs, bool promiscuous);
  virtual void close();

private:
  pcap_t *fp;
  struct bpf_program fcode;
  bool opened;
};

#endif // !defined(INCLUDED_NETWORK_PCAP_H)
