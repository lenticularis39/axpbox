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

#include "StdAfx.hpp"

#if defined(__linux__)

#include "NetworkTap.hpp"
#include "Configurator.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if_tun.h>

// For bridge ioctl
#include <linux/sockios.h>

// Bridge ioctls may not be defined everywhere
#ifndef SIOCBRADDIF
#define SIOCBRADDIF 0x89a2
#endif
#ifndef SIOCBRADDBR
#define SIOCBRADDBR 0x89a0
#endif

CNetworkTap::CNetworkTap() : tap_fd(-1), created_by_us(false) {
  memset(ifname, 0, sizeof(ifname));
}

CNetworkTap::~CNetworkTap() { close(); }

bool CNetworkTap::tap_create(const char *devid_string, const char *name) {
  struct ifreq ifr;

  tap_fd = open("/dev/net/tun", O_RDWR);
  if (tap_fd < 0) {
    printf("%s: Cannot open /dev/net/tun: %s\n", devid_string, strerror(errno));
    printf("%s: Make sure the tun module is loaded (modprobe tun)\n",
           devid_string);
    return false;
  }

  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

  if (name && name[0]) {
    strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);
  }
  // else: leave empty, kernel will assign tapN

  if (ioctl(tap_fd, TUNSETIFF, &ifr) < 0) {
    printf("%s: TUNSETIFF failed: %s\n", devid_string, strerror(errno));
    if (errno == EPERM) {
      printf("%s: Creating a TAP device requires CAP_NET_ADMIN or root.\n",
             devid_string);
      printf("%s: Either run as root, use 'setcap cap_net_admin+ep axpbox',\n",
             devid_string);
      printf("%s: or pre-create the TAP interface:\n", devid_string);
      printf("%s:   ip tuntap add %s mode tap user $USER\n", devid_string,
             name ? name : "tap0");
    }
    ::close(tap_fd);
    tap_fd = -1;
    return false;
  }

  strncpy(ifname, ifr.ifr_name, IFNAMSIZ - 1);
  created_by_us = true;
  printf("%s: Created TAP device %s\n", devid_string, ifname);
  return true;
}

bool CNetworkTap::tap_attach(const char *devid_string, const char *name) {
  struct ifreq ifr;

  tap_fd = open("/dev/net/tun", O_RDWR);
  if (tap_fd < 0) {
    printf("%s: Cannot open /dev/net/tun: %s\n", devid_string, strerror(errno));
    return false;
  }

  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);

  if (ioctl(tap_fd, TUNSETIFF, &ifr) < 0) {
    printf("%s: Failed to attach to TAP device %s: %s\n", devid_string, name,
           strerror(errno));
    ::close(tap_fd);
    tap_fd = -1;
    return false;
  }

  strncpy(ifname, ifr.ifr_name, IFNAMSIZ - 1);
  created_by_us = false;
  printf("%s: Attached to existing TAP device %s\n", devid_string, ifname);
  return true;
}

bool CNetworkTap::bring_up_interface(const char *devid_string,
                                     const char *iface) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    printf("%s: Cannot create socket for interface control: %s\n",
           devid_string, strerror(errno));
    return false;
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

  if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
    printf("%s: Cannot get interface flags for %s: %s\n", devid_string, iface,
           strerror(errno));
    ::close(sock);
    return false;
  }

  ifr.ifr_flags |= IFF_UP;
  if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
    printf("%s: Cannot bring up interface %s: %s\n", devid_string, iface,
           strerror(errno));
    if (errno == EPERM) {
      printf("%s: Bringing up the interface requires CAP_NET_ADMIN or root.\n",
             devid_string);
    }
    ::close(sock);
    return false;
  }

  ::close(sock);
  printf("%s: Interface %s is up\n", devid_string, iface);
  return true;
}

bool CNetworkTap::set_interface_addr(const char *devid_string,
                                     const char *iface, const char *cidr) {
  // Parse "x.x.x.x/prefix" format
  char ip_buf[64];
  strncpy(ip_buf, cidr, sizeof(ip_buf) - 1);
  ip_buf[sizeof(ip_buf) - 1] = '\0';

  char *slash = strchr(ip_buf, '/');
  int prefix_len = 24; // default
  if (slash) {
    *slash = '\0';
    prefix_len = atoi(slash + 1);
  }

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    printf("%s: Cannot create socket for address config: %s\n", devid_string,
           strerror(errno));
    return false;
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

  // Set IP address
  struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
  addr->sin_family = AF_INET;
  if (inet_pton(AF_INET, ip_buf, &addr->sin_addr) != 1) {
    printf("%s: Invalid IP address: %s\n", devid_string, ip_buf);
    ::close(sock);
    return false;
  }

  if (ioctl(sock, SIOCSIFADDR, &ifr) < 0) {
    printf("%s: Cannot set IP address on %s: %s\n", devid_string, iface,
           strerror(errno));
    ::close(sock);
    return false;
  }

  // Set netmask from prefix length
  uint32_t mask = prefix_len == 0 ? 0 : htonl(~((1u << (32 - prefix_len)) - 1));
  struct sockaddr_in *netmask = (struct sockaddr_in *)&ifr.ifr_netmask;
  netmask->sin_family = AF_INET;
  netmask->sin_addr.s_addr = mask;

  if (ioctl(sock, SIOCSIFNETMASK, &ifr) < 0) {
    printf("%s: Cannot set netmask on %s: %s\n", devid_string, iface,
           strerror(errno));
    ::close(sock);
    return false;
  }

  ::close(sock);
  printf("%s: Set %s address to %s/%d\n", devid_string, iface, ip_buf,
         prefix_len);
  return true;
}

bool CNetworkTap::tap_bring_up(const char *devid_string) {
  return bring_up_interface(devid_string, ifname);
}

bool CNetworkTap::bridge_create(const char *devid_string, const char *bridge) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    printf("%s: Cannot create socket for bridge control: %s\n", devid_string,
           strerror(errno));
    return false;
  }

  // Check if bridge already exists
  if (if_nametoindex(bridge) != 0) {
    printf("%s: Bridge %s already exists\n", devid_string, bridge);
    ::close(sock);
    return true;
  }

  // SIOCBRADDBR takes the bridge name directly in ifr_name
  if (ioctl(sock, SIOCBRADDBR, bridge) < 0) {
    if (errno == EEXIST) {
      printf("%s: Bridge %s already exists\n", devid_string, bridge);
    } else {
      printf("%s: Cannot create bridge %s: %s\n", devid_string, bridge,
             strerror(errno));
      if (errno == EPERM) {
        printf("%s: Bridge creation requires CAP_NET_ADMIN or root.\n",
               devid_string);
      }
      ::close(sock);
      return false;
    }
  } else {
    printf("%s: Created bridge %s\n", devid_string, bridge);
  }

  ::close(sock);
  return true;
}

bool CNetworkTap::bridge_add_interface(const char *devid_string,
                                       const char *bridge, const char *iface) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    printf("%s: Cannot create socket for bridge control: %s\n", devid_string,
           strerror(errno));
    return false;
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, bridge, IFNAMSIZ - 1);
  ifr.ifr_ifindex = if_nametoindex(iface);

  if (ifr.ifr_ifindex == 0) {
    printf("%s: Cannot find interface index for %s\n", devid_string, iface);
    ::close(sock);
    return false;
  }

  if (ioctl(sock, SIOCBRADDIF, &ifr) < 0) {
    if (errno == EBUSY) {
      printf("%s: Interface %s already in bridge %s\n", devid_string, iface,
             bridge);
    } else {
      printf("%s: Cannot add %s to bridge %s: %s\n", devid_string, iface,
             bridge, strerror(errno));
      if (errno == EPERM) {
        printf("%s: Bridge control requires CAP_NET_ADMIN or root.\n",
               devid_string);
      }
      ::close(sock);
      return false;
    }
  } else {
    printf("%s: Added %s to bridge %s\n", devid_string, iface, bridge);
  }

  ::close(sock);
  return true;
}

bool CNetworkTap::tap_setup_bridge(const char *devid_string,
                                   const char *bridge, const char *uplink) {
  // Create bridge if it doesn't exist
  if (!bridge_create(devid_string, bridge))
    return false;

  // Add the TAP interface to the bridge
  if (!bridge_add_interface(devid_string, bridge, ifname))
    return false;

  // Add the uplink (physical) interface to the bridge if specified
  if (uplink && uplink[0]) {
    if (!bridge_add_interface(devid_string, bridge, uplink))
      return false;
  }

  // Bring up the bridge
  if (!bring_up_interface(devid_string, bridge))
    return false;

  return true;
}

bool CNetworkTap::init(const char *devid_string, CConfigurator *cfg) {
  char *adapter = cfg->get_text_value("adapter");
  bool tap_create_flag = cfg->get_bool_value("tap_create", true);
  char *bridge = cfg->get_text_value("bridge");
  char *uplink = cfg->get_text_value("uplink");

  const char *tap_name = adapter ? adapter : "tap0";

  if (tap_create_flag) {
    // First try to attach to an existing interface
    if (!tap_attach(devid_string, tap_name)) {
      // If that fails, try to create it
      printf("%s: Trying to create TAP device %s...\n", devid_string, tap_name);
      if (!tap_create(devid_string, tap_name)) {
        return false;
      }
    }
  } else {
    // Only try to attach, don't create
    if (!tap_attach(devid_string, tap_name)) {
      printf("%s: TAP device %s does not exist and tap_create is false.\n",
             devid_string, tap_name);
      printf("%s: Create it with: ip tuntap add %s mode tap user $USER\n",
             devid_string, tap_name);
      return false;
    }
  }

  // Set non-blocking
  int flags = fcntl(tap_fd, F_GETFL, 0);
  if (flags < 0 || fcntl(tap_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    printf("%s: Cannot set TAP fd non-blocking: %s\n", devid_string,
           strerror(errno));
    close();
    return false;
  }

  // Assign host-side IP to TAP if configured
  char *host_ip = cfg->get_text_value("host_ip");
  if (host_ip) {
    set_interface_addr(devid_string, ifname, host_ip);
  }

  // Always bring up the interface
  tap_bring_up(devid_string);

  // Set up bridge if configured
  if (bridge) {
    tap_setup_bridge(devid_string, bridge, uplink);
  }

  return true;
}

int CNetworkTap::send(const u8 *data, int len) {
  if (tap_fd < 0)
    return -1;
  ssize_t n = write(tap_fd, data, len);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return 0; // treat as success, packet dropped
    printf("TAP: Error sending packet: %s\n", strerror(errno));
    return -1;
  }
  return 0;
}

int CNetworkTap::receive(const u8 **data, int *len) {
  if (tap_fd < 0)
    return -1;
  ssize_t n = read(tap_fd, rx_buf, sizeof(rx_buf));
  if (n > 0) {
    *data = rx_buf;
    *len = (int)n;
    return 1;
  }
  if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
    return 0; // no packet available
  if (n == 0)
    return 0;
  return -1; // error
}

void CNetworkTap::set_filter(u8 mac_list[][6], int num_macs,
                              bool promiscuous) {
  // TAP devices deliver only frames destined for this interface,
  // so hardware-level filtering is not needed. The guest OS driver
  // handles MAC filtering via the emulated NIC's filter setup.
  (void)mac_list;
  (void)num_macs;
  (void)promiscuous;
}

void CNetworkTap::close() {
  if (tap_fd >= 0) {
    ::close(tap_fd);
    tap_fd = -1;
  }
}

#endif // __linux__
