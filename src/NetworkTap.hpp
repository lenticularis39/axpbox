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

#if !defined(INCLUDED_NETWORK_TAP_H)
#define INCLUDED_NETWORK_TAP_H

#include "NetworkBackend.hpp"

#if defined(__linux__)

class CNetworkTap : public CNetworkBackend {
public:
  CNetworkTap();
  virtual ~CNetworkTap();

  virtual bool init(const char *devid_string, CConfigurator *cfg);
  virtual int send(const u8 *data, int len);
  virtual int receive(const u8 **data, int *len);
  virtual void set_filter(u8 mac_list[][6], int num_macs, bool promiscuous);
  virtual void close();

private:
  int tap_fd;
  char ifname[16];
  bool created_by_us;
  u8 rx_buf[1518]; // max ethernet frame

  bool tap_create(const char *devid_string, const char *name);
  bool tap_attach(const char *devid_string, const char *name);
  bool tap_setup_bridge(const char *devid_string, const char *bridge,
                        const char *uplink);
  bool tap_bring_up(const char *devid_string);
  bool bring_up_interface(const char *devid_string, const char *iface);
  bool set_interface_addr(const char *devid_string, const char *iface,
                          const char *cidr);
  bool bridge_create(const char *devid_string, const char *bridge);
  bool bridge_add_interface(const char *devid_string, const char *bridge,
                            const char *iface);
};

#endif // __linux__

#endif // !defined(INCLUDED_NETWORK_TAP_H)
