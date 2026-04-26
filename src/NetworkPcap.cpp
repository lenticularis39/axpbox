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

#if defined(HAVE_PCAP)
#include "NetworkPcap.hpp"
#include "Configurator.hpp"

CNetworkPcap::CNetworkPcap() : fp(nullptr), opened(false) {
  memset(&fcode, 0, sizeof(fcode));
}

CNetworkPcap::~CNetworkPcap() { close(); }

bool CNetworkPcap::init(const char *devid_string, CConfigurator *cfg) {
  pcap_if_t *alldevs;
  pcap_if_t *d;
  u_int inum;
  u_int i = 0;
  char errbuf[PCAP_ERRBUF_SIZE];

  char *adapter = cfg->get_text_value("adapter");
  if (!adapter) {
    printf("\n%s: Choose a network adapter to connect to:\n", devid_string);
    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
      printf("%s: Error in pcap_findalldevs: %s\n", devid_string, errbuf);
      return false;
    }

    for (d = alldevs; d; d = d->next) {
      printf("%d. %s\n    ", ++i, d->name);
      if (d->description)
        printf(" (%s)\n", d->description);
      else
        printf(" (No description available)\n");
    }

    if (i == 0) {
      printf("%s: No network interfaces found\n", devid_string);
      return false;
    }

    if (i == 1)
      inum = 1;
    else {
      inum = 0;
      while (inum < 1 || inum > i) {
        printf("%%NIC-Q-NICNO: Enter the interface number (1-%d):", i);
        (void)!scanf("%d", &inum);
      }
    }

    for (d = alldevs, i = 0; i < inum - 1; d = d->next, i++)
      ;

    adapter = d->name;
  }

#if defined(WIN32)
  if ((fp = pcap_open(adapter, 65536,
                      PCAP_OPENFLAG_PROMISCUOUS | PCAP_OPENFLAG_NOCAPTURE_LOCAL,
                      10, 0, errbuf)) == NULL)
#else
  if ((fp = pcap_open_live(adapter, 65536, 1, 1, errbuf)) == nullptr)
#endif
  {
    printf("%s: Error opening adapter %s: %s\n", devid_string, adapter, errbuf);
    return false;
  }

  if (pcap_setnonblock(fp, 1, errbuf) == PCAP_ERROR) {
    printf("%s: Error setting adapter %s non-blocking: %s\n", devid_string,
           adapter, errbuf);
    pcap_close(fp);
    fp = nullptr;
    return false;
  }

  opened = true;
  printf("%s: Using pcap adapter %s\n", devid_string, adapter);
  return true;
}

int CNetworkPcap::send(const u8 *data, int len) {
  if (!fp)
    return -1;
  if (pcap_sendpacket(fp, data, len)) {
    printf("Error sending the packet: %s\n", pcap_geterr(fp));
    return -1;
  }
  return 0;
}

int CNetworkPcap::receive(const u8 **data, int *len) {
  if (!fp)
    return -1;
  struct pcap_pkthdr *packet_header;
  const u_char *packet_data = NULL;
  int res = pcap_next_ex(fp, &packet_header, &packet_data);
  if (res > 0) {
    *data = packet_data;
    *len = packet_header->caplen;
    return 1;
  }
  if (res == 0)
    return 0; // timeout, no packet
  return -1;  // error
}

void CNetworkPcap::set_filter(u8 mac_list[][6], int num_macs,
                               bool promiscuous) {
  if (!fp)
    return;

  if (promiscuous) {
    // No filter needed in promiscuous mode - pcap already opened promiscuous
    return;
  }

  char mac_txt[16][20];
  char filter[1000];
  int numUnique = 0;
  int unique[16];

  int count = num_macs > 16 ? 16 : num_macs;

  for (int i = 0; i < count; i++) {
    sprintf(mac_txt[i], "%02x:%02x:%02x:%02x:%02x:%02x", mac_list[i][0],
            mac_list[i][1], mac_list[i][2], mac_list[i][3], mac_list[i][4],
            mac_list[i][5]);
  }

  // Deduplicate
  for (int i = 0; i < count; i++) {
    bool u = true;
    for (int j = 0; j < numUnique; j++) {
      if (memcmp(mac_list[i], mac_list[unique[j]], 6) == 0) {
        u = false;
        break;
      }
    }
    if (u) {
      unique[numUnique] = i;
      numUnique++;
    }
  }

  filter[0] = '\0';
  strcat(filter, "ether dst ");
  strcat(filter, mac_txt[unique[0]]);
  for (int i = 1; i < numUnique; i++) {
    strcat(filter, " or ether dst ");
    strcat(filter, mac_txt[unique[i]]);
  }

  if (pcap_compile(fp, &fcode, filter, 1, 0xffffffff) < 0) {
    printf("Unable to compile the packet filter (%s)\n", filter);
    return;
  }

  if (pcap_setfilter(fp, &fcode) < 0) {
    printf("Error setting the filter.\n");
  }
}

void CNetworkPcap::close() {
  if (fp) {
    pcap_close(fp);
    fp = nullptr;
  }
  opened = false;
}

#endif // HAVE_PCAP
