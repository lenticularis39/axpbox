/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Copyright (C) 2021 Dietmar M. Zettl
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

/**
 * \file
 * Contains the code for the emulated DEC 21143 NIC device.
 *
 * $Id: DEC21143.cpp,v 1.36 2008/05/31 15:47:09 iamcamiel Exp $
 *
 * X-1.36       Camiel Vanderhoeven                             31-MAY-2008
 *      Changes to include parts of Poco.
 *
 * X-1.35       Camiel Vanderhoeven                             26-MAR-2008
 *      Fix compiler warnings.
 *
 * X-1.34       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.33       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.32       Camiel Vanderhoeven                             13-MAR-2008
 *      Create init(), start_threads() and stop_threads() functions.
 *
 * X-1.31       Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.30       Camiel Vanderhoeven                             02-MAR-2008
 *      Natural way to specify large numeric values ("10M") in the config file.
 *
 * X-1.29       Brian Wheeler                                   29-FEB-2008
 *      Compute SROM checksum. Tru64 needs this.
 *
 * X-1.28       David Hittner                                   26-FEB-2008
 *      Major rewrite. Real internal loopback support, ring queue for
 *      incoming packets, and various other improvements.
 *
 * X-1.27       Camiel Vanderhoeven                             24-JAN-2008
 *      Use new CPCIDevice::do_pci_read and CPCIDevice::do_pci_write.
 *
 * X-1.26       Fang Zhe                                        08-JAN-2008
 *      Avoid compiler warning.
 *
 * X-1.25       Fausto Saporito                                 05-JAN-2008
 *      Fixed typo (\m instead of \n).
 *
 * X-1.24       David Hittner                                   04-JAN-2008
 *      MAC address configurable.
 *
 * X-1.23       Camiel Vanderhoeven                             02-JAN-2008
 *      Ignore OPMODE_OM (loopback mode) bits.
 *
 * X-1.22       Camiel Vanderhoeven                             02-JAN-2008
 *      Replaced USE_NETWORK with HAVE_PCAP.
 *
 * X-1.21       Camiel Vanderhoeven                             30-DEC-2007
 *      Print file id on initialization.
 *
 * X-1.20       Camiel Vanderhoeven                             29-DEC-2007
 *      Compileable with older compilers (VC 6.0). Avoid referencing
 *      uninitialized data. Fixed memory-leak.
 *
 * X-1.19       Camiel Vanderhoeven                             28-DEC-2007
 *      Throw exceptions rather than just exiting when errors occur.
 *
 * X-1.18       Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.17       Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.16       Brian Wheeler                                   10-DEC-2007
 *      Added pthread.h
 *
 * X-1.15       Camiel Vanderhoeven                             10-DEC-2007
 *      Use configurator.
 *
 * X-1.14       Camiel Vanderhoeven                             6-DEC-2007
 *      Identifies itself as DE-500BA.
 *
 * X-1.13       Camiel Vanderhoeven                             2-DEC-2007
 *      Receive network data in a separate thread.
 *
 * X-1.12       Camiel Vanderhoeven                             1-DEC-2007
 *      Moved inclusion of StdAfx.h outside conditional block; necessary
 *      for using precompiled headers in Visual C++.
 *
 * X-1.11       Camiel Vanderhoeven                             17-NOV-2007
 *      File should end in a newline.
 *
 * X-1.10       Camiel Vanderhoeven                             17-NOV-2007
 *      Use the standard pcap functions (not the extended windows ones), we
 *      want to be compatible.
 *
 * X-1.9        Camiel Vanderhoeven                             17-NOV-2007
 *      Corrected a small "oops" error in getting the DECnet address.
 *
 * X-1.8        Camiel Vanderhoeven                             17-NOV-2007
 *      Get the adapter and DECnet address to use from the configuration
 *      file.
 *
 * X-1.7        Camiel Vanderhoeven                             17-NOV-2007
 *      Changed the MAC address into the DigitalE-range.
 *
 * X-1.6        Camiel Vanderhoeven                             16-NOV-2007
 *      Change the packet filter less often (only when required).
 *
 * X-1.5        Camiel Vanderhoeven                             16-NOV-2007
 *      Removed some debug messages, and corrected readout of CSR 12.
 *
 * X-1.4        Camiel Vanderhoeven                             16-NOV-2007
 *      BPF filter used for perfect filtering; more correct behaviour of
 *      registers.
 *
 * X-1.3        Camiel Vanderhoeven                             15-NOV-2007
 *      Use pcap for network access.
 *
 * X-1.2        Camiel Vanderhoeven                             14-NOV-2007
 *      Removed some debug messages.
 *
 * X-1.1        Camiel Vanderhoeven                             14-NOV-2007
 *      Initial version for ES40 emulator.
 **/
#include "StdAfx.hpp"

#if defined(HAVE_PCAP)
#include "DEC21143.hpp"
#include "System.hpp"

#if defined(DEBUG_NIC)
#define DEBUG_NIC_FILTER
#define DEBUG_NIC_SROM
#endif

/*  Internal states during MII data stream decode:  */
#define MII_STATE_RESET 0
#define MII_STATE_START_WAIT 1
#define MII_STATE_READ_OP 2
#define MII_STATE_READ_PHYADDR_REGADDR 3
#define MII_STATE_A 4
#define MII_STATE_D 5
#define MII_STATE_IDLE 6

/**
 * Thread entry point.
 **/
void CDEC21143::run() {
  try {
    for (;;) {
      if (StopThread)
        return;
      receive_process();

      bool asserted;

      if ((state.reg[CSR_OPMODE / 8] & OPMODE_ST))
        while (dec21143_tx())
          ;

      /*  Normal and Abnormal interrupt summary:  */
      state.reg[CSR_STATUS / 8] &= ~(STATUS_NIS | STATUS_AIS);
      if (state.reg[CSR_STATUS / 8] & state.reg[CSR_INTEN / 8] & 0x00004845)
        state.reg[CSR_STATUS / 8] |= STATUS_NIS;
      if (state.reg[CSR_STATUS / 8] & state.reg[CSR_INTEN / 8] & 0x0c0037ba)
        state.reg[CSR_STATUS / 8] |= STATUS_AIS;

      asserted = (state.reg[CSR_STATUS / 8] & state.reg[CSR_INTEN / 8] &
                  0x0c01ffff) != 0;

      if (asserted != state.irq_was_asserted) {
        if (do_pci_interrupt(0, asserted))
          state.irq_was_asserted = asserted;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }

  catch (CException &e) {
    printf("Exception in NIC thread: %s.\n", e.displayText().c_str());
    myThreadDead.store(true);
    // Let the thread die...
  }
}

u32 dec21143_cfg_data[64] = {
    /*00*/ 0x00191011, // CFID: vendor + device
    /*04*/ 0x02800000, // CFCS: command + status
    /*08*/ 0x02000030, // CFRV: class + revision   //dth:was 41
    /*0c*/ 0x00000000, // CFLT: latency timer + cache line size
    /*10*/ 0x00000001, // BAR0: CBIO
    /*14*/ 0x00000000, // BAR1: CBMA
    /*18*/ 0x00000000, // BAR2:
    /*1c*/ 0x00000000, // BAR3:
    /*20*/ 0x00000000, // BAR4:
    /*24*/ 0x00000000, // BAR5:
    /*28*/ 0x00000000, // CCIC: CardBus
    /*2c*/ 0x500b1011, // CSID: subsystem + vendor
    /*30*/ 0x00000000, // BAR6: expansion rom base
    /*34*/ 0x00000000, // CCAP: capabilities pointer
    /*38*/ 0x00000000,
    /*3c*/ 0x281401ff, // CFIT: interrupt configuration
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0};

u32 dec21143_cfg_mask[64] = {
    /*00*/ 0x00000000, // CFID: vendor + device
    /*04*/ 0x0000ffff, // CFCS: command + status
    /*08*/ 0x00000000, // CFRV: class + revision
    /*0c*/ 0x0000ffff, // CFLT: latency timer + cache line size
    /*10*/ 0xffffff00, // BAR0
    /*14*/ 0xffffff00, // BAR1: CBMA
    /*18*/ 0x00000000, // BAR2:
    /*1c*/ 0x00000000, // BAR3:
    /*20*/ 0x00000000, // BAR4:
    /*24*/ 0x00000000, // BAR5:
    /*28*/ 0x00000000, // CCIC: CardBus
    /*2c*/ 0x00000000, // CSID: subsystem + vendor
    /*30*/ 0x00000000, // BAR6: expansion rom base
    /*34*/ 0x00000000, // CCAP: capabilities pointer
    /*38*/ 0x00000000,
    /*3c*/ 0x000000ff, // CFIT: interrupt configuration
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0};

int CDEC21143::nic_num = 0;

/**
 * Constructor.
 **/
CDEC21143::CDEC21143(CConfigurator *confg, CSystem *c, int pcibus, int pcidev)
    : CPCIDevice(confg, c, pcibus, pcidev) {}

/**
 * Initialize the network device.
 **/
void CDEC21143::init() {
  pcap_if_t *alldevs;

  pcap_if_t *d;
  u_int inum;
  u_int i = 0;
  char errbuf[PCAP_ERRBUF_SIZE];
  char *cfg;

  add_function(0, dec21143_cfg_data, dec21143_cfg_mask);

  cfg = myCfg->get_text_value("adapter");
  if (!cfg) {
    printf("\n%s: Choose a network adapter to connect to:\n", devid_string);
    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
      FAILURE_1(Runtime, "Error in pcap_findalldevs_ex: %s", errbuf);
    }

    /* Print the list */
    for (d = alldevs; d; d = d->next) {
      printf("%d. %s\n    ", ++i, d->name);
      if (d->description)
        printf(" (%s)\n", d->description);
      else
        printf(" (No description available)\n");
    }

    if (i == 0)
      FAILURE(Runtime, "No network interfaces found");

    if (i == 1)
      inum = 1;
    else {
      inum = 0;
      while (inum < 1 || inum > i) {
        printf("%%NIC-Q-NICNO: Enter the interface number (1-%d):", i);
        (void)!scanf("%d", &inum);
      }
    }

    /* Jump to the selected adapter */
    for (d = alldevs, i = 0; i < inum - 1; d = d->next, i++)
      ;

    cfg = d->name;
  }

#if defined(WIN32)

  // Opening with pcap_open on Windows allows specification of
  // PCAP_OPENFLAG_NOCAPTURE_LOCAL, which stops the pcap device from seeing it's
  // own transmitted packets.
  //
  // This is important because:
  //    1. Real ethernet cards don't reflect packets except while in loopback
  //    mode(s).
  //    2. Reflecting all packets increases inbound packet processing and host
  //    load.
  //    3. DECNET Phase IV will think a reflected packet is from another node
  //            that has the same DECNET Phase IV address (AA-xx-xx-xx-xx-xx),
  //            and will panic on startup and abort.
  //    4. Libpcap doesn't reflect packets, and we want winpcap/libpcap
  //    processing to be identical.
  // Loopback packets are handled via direct entry in the receive queue.
  if ((fp = pcap_open(cfg, 65536 /*snaplen: capture entire packets */,
                      PCAP_OPENFLAG_PROMISCUOUS |
                          PCAP_OPENFLAG_NOCAPTURE_LOCAL /*promiscuous */,
                      10 /*read timeout: 10ms. */, 0 /* auth structure */,
                      errbuf)) == NULL) // connect to pcap...
#else
  if ((fp = pcap_open_live(cfg, 65536 /*snaplen: capture entire packets */,
                           1 /*promiscuous */, 1 /*read timeout: 1ms. */,
                           errbuf)) == nullptr) // connect to pcap...
#endif
    FAILURE_2(Runtime, "Error opening adapter %s:\n %s", cfg, errbuf);

  if (pcap_setnonblock(fp, 1, errbuf) == PCAP_ERROR)
    FAILURE_2(Runtime, "Error setting adapter %s non-blocking:\n %s", cfg,
              errbuf);

  // set default mac = Digital ethernet prefix: 08-00-2B + hexified "ES40" + nic
  // number
  state.mac[0] = 0x08;
  state.mac[1] = 0x00;
  state.mac[2] = 0x2B;
  state.mac[3] = 0xE5;
  state.mac[4] = 0x40;
  state.mac[5] = nic_num++;

  // set assigned mac
  cfg = myCfg->get_text_value("mac");
  if (cfg) {
    const char *mac_chars = "0123456789abcdefABCDEF-.:";
    const char *hex_chars = "0123456789abcdefABCDEF";
    const char *hex_scanf = "%hx";
    bool mac_replaced = false;
    if ((strlen(cfg) == 17) && (strspn(cfg, mac_chars) == 17)) {
      char newmac[18];
      strcpy(newmac, cfg);
      newmac[2] = newmac[5] = newmac[8] = newmac[11] = newmac[14] = 0;
      if ((strspn(&newmac[0], hex_chars) == 2) &&
          (strspn(&newmac[3], hex_chars) == 2) &&
          (strspn(&newmac[6], hex_chars) == 2) &&
          (strspn(&newmac[9], hex_chars) == 2) &&
          (strspn(&newmac[12], hex_chars) == 2) &&
          (strspn(&newmac[15], hex_chars) == 2)) {
        short unsigned int num;
        sscanf(&newmac[0], hex_scanf, &num);
        state.mac[0] = num & 0xff;
        sscanf(&newmac[3], hex_scanf, &num);
        state.mac[1] = num & 0xff;
        sscanf(&newmac[6], hex_scanf, &num);
        state.mac[2] = num & 0xff;
        sscanf(&newmac[9], hex_scanf, &num);
        state.mac[3] = num & 0xff;
        sscanf(&newmac[12], hex_scanf, &num);
        state.mac[4] = num & 0xff;
        sscanf(&newmac[15], hex_scanf, &num);
        state.mac[5] = num & 0xff;
        mac_replaced = true;
      }
    }

    if (mac_replaced) {
      printf("%s: MAC set to %s\n", devid_string, cfg);
    } else {
      FAILURE_1(Configuration,
                "MAC address (%s) should have xx-xx-xx-xx-xx-xx format", cfg);
    }
  } else {
    char mac[18];
    sprintf(mac, "%02X-%02X-%02X-%02X-%02X-%02X", state.mac[0], state.mac[1],
            state.mac[2], state.mac[3], state.mac[4], state.mac[5]);
    printf("%s: MAC defaulted to %s\n", devid_string, mac);
  }

  rx_queue = new CPacketQueue("rx_queue",
                              (int)myCfg->get_num_value("queue", false, 100));
  calc_crc = myCfg->get_bool_value("crc", false);

  state.rx.cur_buf = NULL;
  state.tx.cur_buf = (unsigned char *)malloc(1514);
  state.irq_was_asserted = false;
  state.tx.idling = 0;

  ResetPCI();

  printf("%s: $Id: DEC21143.cpp,v 1.36 2008/05/31 15:47:09 iamcamiel Exp $\n",
         devid_string);
}

void CDEC21143::start_threads() {
  if (!myThread) {
    printf(" nic");
    StopThread = false;
    myThread = std::make_unique<std::thread>([this](){ this->run(); });
  }
}

void CDEC21143::stop_threads() {
  StopThread = true;
  if (myThread) {
    printf(" nic");
    myThread->join();
    myThread = nullptr;
  }
}

/**
 * Destructor.
 **/
CDEC21143::~CDEC21143() {
  stop_threads();

  pcap_close(fp);
  delete rx_queue;
}

u32 CDEC21143::ReadMem_Bar(int func, int bar, u32 address, int dsize) {
  switch (bar) {
  case 0: // CBIO
  case 1: // CBMA
    return nic_read(address, dsize);
  }

  printf("21143: ReadMem_Bar: unsupported bar %d\n", bar);
  return 0;
}

void CDEC21143::WriteMem_Bar(int func, int bar, u32 address, int dsize,
                             u32 data) {
  switch (bar) {
  case 0: // CBIO
  case 1: // CBMA
    nic_write(address, dsize, (u32)endian_bits(data, dsize));
    return;
  }

  printf("21143: WriteMem_Bar: unsupported bar %d\n", bar);
}

/**
 * Check if threads are still running.
 **/
void CDEC21143::check_state() {
  if (myThreadDead.load())
    FAILURE(Thread, "NIC thread has died");
}

void CDEC21143::receive_process() {
  struct pcap_pkthdr *packet_header;
  const u_char *packet_data = NULL;

  // if receive process active
  if (state.reg[CSR_OPMODE / 8] & OPMODE_SR) {

    // get packets from host nic if not in internal loopback mode
    if (!(state.reg[CSR_OPMODE / 8] & OPMODE_OM_INTLOOP)) {
      while (pcap_next_ex(fp, &packet_header, &packet_data) > 0) {
        rx_queue->add_tail(packet_data, packet_header->caplen, calc_crc, true);
        state.reg[CSR_SIASTAT / 8] |= SIASTAT_TRA; // set 10bT activity
      }
    }

    // process a receive descriptor until we run out of
    // descriptors or packets to process
    while (dec21143_rx())
      ;
  }
}

/**
 * Read from the NIC registers.
 **/
u32 CDEC21143::nic_read(u32 address, int dsize) {
  u32 data = 0;

  int regnr = (int)(address >> 3);

  if ((address & 7) == 0 && regnr < 32) {
    data = state.reg[regnr];
  } else
    printf("dec21143: WARNING! unaligned access (0x%x) \n", (int)address);
#if defined(DEBUG_NIC)
  printf("21143: nic_read - CSR(%d), value: %08x\n", regnr, data);
#endif
  return data;
}

/**
 * Write to the NIC registers.
 **/
void CDEC21143::nic_write(u32 address, int dsize, u32 data) {
  uint32_t oldreg = 0;

  int regnr = (int)(address >> 3);

#if defined(DEBUG_NIC)
  printf("21143: nic_write - CSR(%d), value: %08x\n", regnr, data);
#endif

  // printf("21143: rx_queue->name= %s\n", rx_queue->name);
  if ((address & 7) == 0 && regnr < 32) {
    oldreg = state.reg[regnr];
    switch (regnr) {
    case CSR_STATUS / 8: /*  Zero-on-write  */
      state.reg[regnr] &= ~((u32)data & 0x0c01ffff);
      break;

    case CSR_MISSED / 8: /*  Read only  */
      break;

    default:
      state.reg[regnr] = (u32)data;
    }
  } else
    printf("[ dec21143: WARNING! unaligned access (0x%x) ]\n", (int)address);

  switch (address) {
  case CSR_BUSMODE: /*  csr0  */
    if (data & BUSMODE_SWR) {
      ResetNIC();
      data &= ~BUSMODE_SWR;
    }

    // calculate descriptor skip length in bytes
    state.descr_skip = ((data & BUSMODE_DSL) >> 2) * 4;
    break;

  case CSR_TXPOLL: /*  csr1  */
    /* CaVa interpretation... */
    state.reg[CSR_STATUS / 8] &= ~STATUS_TU;
    state.tx.suspend = false;
    state.tx.idling = state.tx.idling_threshold;

    //        DoClock();
    break;

  case CSR_RXPOLL: /*  csr2  */
    //        DoClock();
    break;

  case CSR_RXLIST: /*  csr3  */
    /* debug("[ dec21143: setting RXLIST to 0x%x ]\n", (int)data); */
    if (data & 0x3)
      printf("[ dec21143: WARNING! RXLIST not aligned? (0x%x) ]\n", data);
    data &= ~0x3;
    state.rx.cur_addr = data;
    break;

  case CSR_TXLIST: /*  csr4  */
    /* debug("[ dec21143: setting TXLIST to 0x%x ]\n", (int)data); */
    if (data & 0x3)
      printf("[ dec21143: WARNING! TXLIST not aligned? (0x%x) ]\n", data);
    data &= ~0x3;
    state.tx.cur_addr = data;
    break;

  case CSR_STATUS: /*  csr5  */
  case CSR_INTEN:  /*  csr7  */
    /*  Recalculate interrupt assertion.  */

    //              DoClock();
    break;

  case CSR_OPMODE: /*  csr6:  */
    if (data & 0x02000000) {

      /*  A must-be-one bit.  */
      data &= ~0x02000000;
    }

    if (data & OPMODE_ST) {

      // data &= ~OPMODE_ST;
    } else {

      /*  Turned off TX? Then idle:  */

      //      state.reg[CSR_STATUS/8] |= STATUS_TPS;
    }

    if (data & OPMODE_SR) {

      // data &= ~OPMODE_SR;
    } else {

      /*  Turned off RX? Then go to stopped state:  */

      // state.reg[CSR_STATUS/8] &= ~STATUS_RS;
    }

    // Did Start/Stop Transmission change state ?
    if ((data ^ oldreg) & OPMODE_ST) {
      if (data & OPMODE_ST) { // ST went high
        set_tx_state(STATUS_TS_SUSPENDED);
      } else { // ST went low
        set_tx_state(STATUS_TS_STOPPED);
      }
    }

    // Did Start/Stop Receive change state ?
    if ((data ^ oldreg) & OPMODE_SR) {
      if (data & OPMODE_SR) { // SR went high
        set_rx_state(STATUS_RS_WAIT);
      } else { // SR went low
        set_tx_state(STATUS_RS_STOPPED);
      }
    }

    data &= ~(OPMODE_HBD | OPMODE_SCR | OPMODE_PCS | OPMODE_PS | OPMODE_SF |
              OPMODE_TTM | OPMODE_FD | OPMODE_TR | OPMODE_OM);

    //              if (data & OPMODE_PNIC_IT) {
    //                      data &= ~OPMODE_PNIC_IT;
    //                  state.tx.idling = state.tx.idling_threshold;
    //              }
    //              if (data != 0) {
    //                      printf("[ dec21143: UNIMPLEMENTED OPMODE bits:
    //                      0x%08x ]\n", (int)data);
    //              }
    //              DoClock();
    break;

  case CSR_MISSED: /*  csr8  */
    break;

  case CSR_MIIROM: /*  csr9  */
    if (data & MIIROM_MDC)
      mii_access(oldreg, (u32)data);
    else
      srom_access(oldreg, (u32)data);
    break;

  case CSR_SIASTAT: /*  csr12  */
    if (((data & SIASTAT_ANS) == SIASTAT_ANS_START) &&
        (state.reg[CSR_SIATXRX / 8] & SIATXRX_ANE)) {

      // autonegotiation restart... completes immediately in our emulated
      // environment.
      state.reg[CSR_SIASTAT / 8] &= ~SIASTAT_ANS;
      state.reg[CSR_SIASTAT / 8] |=
          (SIASTAT_ANS_FLPGOOD | SIASTAT_LPN | SIASTAT_LPC);
      state.reg[CSR_STATUS / 8] |= STATUS_LNPANC;
      state.reg[CSR_SIATXRX / 8] &= ~(SIATXRX_TH | SIATXRX_THX | SIATXRX_T4);
      state.reg[CSR_SIATXRX / 8] |= SIATXRX_TXF;

      //          DoClock();
    } else {
      state.reg[CSR_SIASTAT / 8] = oldreg;
    }
    break;

  case CSR_SIATXRX: /*  csr14  */
    break;

  case CSR_SIACONN: /*  csr13  */
    if ((data & SIACONN_SRL) && (state.reg[CSR_SIATXRX / 8] & SIATXRX_ANE)) {

      // SIA started with autonegotiation... completes immediately in our
      // emulated environment.
      state.reg[CSR_SIASTAT / 8] &= ~SIASTAT_ANS;
      state.reg[CSR_SIASTAT / 8] |=
          (SIASTAT_ANS_FLPGOOD | SIASTAT_LPN | SIASTAT_LPC);
      state.reg[CSR_STATUS / 8] |= STATUS_LNPANC;
      state.reg[CSR_SIATXRX / 8] &= ~(SIATXRX_TH | SIATXRX_THX | SIATXRX_T4);
      state.reg[CSR_SIATXRX / 8] |= SIATXRX_TXF;

      //          DoClock();
    }
    break;

  case CSR_SIAGEN: /*  csr15  */
    break;

  default:
    printf("[ dec21143: write to unimplemented 0x%02x: 0x%02x ]\n",
           (int)address, (int)data);
  }
}

void CDEC21143::set_tx_state(int tx_state) {
  state.reg[CSR_STATUS / 8] &= ~STATUS_TS;
  state.reg[CSR_STATUS / 8] |= (tx_state & STATUS_TS);
}

void CDEC21143::set_rx_state(int rx_state) {
  state.reg[CSR_STATUS / 8] &= ~STATUS_RS;
  state.reg[CSR_STATUS / 8] |= (rx_state & STATUS_RS);
}

/**
 *  This function handles accesses to the MII. Data streams seem to be of the
 *  following format:
 *
 *      vv---- starting delimiter
 *  ... 01 xx yyyyy zzzzz a[a] dddddddddddddddd
 *         ^---- I am starting with mii_bit = 0 here
 *
 *  where x = opcode (10 = read, 01 = write)
 *        y = PHY address
 *        z = register address
 *        a = on Reads: ACK bit (returned, should be 0)
 *            on Writes: _TWO_ dummy bits (10)
 *        d = 16 bits of data (MSB first)
 **/
void CDEC21143::mii_access(uint32_t oldreg, uint32_t idata) {
  int obit;

  int ibit = 0;
  uint16_t tmp;

  /*  Only care about data during clock cycles:  */
  if (!(idata & MIIROM_MDC))
    return;

  if (idata & MIIROM_MDC && oldreg & MIIROM_MDC)
    return;

  /*  printf("[ mii_access(): 0x%08x ]\n", (int)idata);  */
  if (idata & MIIROM_BR) {
    printf("[ mii_access(): MIIROM_BR: TODO ]\n");
    return;
  }

  obit = idata & MIIROM_MDO ? 1 : 0;

  if (state.mii.state >= MII_STATE_START_WAIT &&
      state.mii.state <= MII_STATE_READ_PHYADDR_REGADDR &&
      idata & MIIROM_MIIDIR)
    printf("[ mii_access(): bad dir? ]\n");

  switch (state.mii.state) {
  case MII_STATE_RESET:

    /*  Wait for a starting delimiter (0 followed by 1).  */
    if (obit)
      return;
    if (idata & MIIROM_MIIDIR)
      return;

    /*  printf("[ mii_access(): got a 0 delimiter ]\n");  */
    state.mii.state = MII_STATE_START_WAIT;
    state.mii.opcode = 0;
    state.mii.phyaddr = 0;
    state.mii.regaddr = 0;
    break;

  case MII_STATE_START_WAIT:

    /*  Wait for a starting delimiter (0 followed by 1).  */
    if (!obit)
      return;
    if (idata & MIIROM_MIIDIR) {
      state.mii.state = MII_STATE_RESET;
      return;
    }

    /*  printf("[ mii_access(): got a 1 delimiter ]\n");  */
    state.mii.state = MII_STATE_READ_OP;
    state.mii.bit = 0;
    break;

  case MII_STATE_READ_OP:
    if (state.mii.bit == 0) {
      state.mii.opcode = obit << 1;

      /*  printf("[ mii_access(): got first opcode bit (%i) ]\n", obit);  */
    } else {
      state.mii.opcode |= obit;

      /*  printf("[ mii_access(): got opcode = %i ]\n", state.mii.opcode);  */
      state.mii.state = MII_STATE_READ_PHYADDR_REGADDR;
    }

    state.mii.bit++;
    break;

  case MII_STATE_READ_PHYADDR_REGADDR:

    /*  printf("[ mii_access(): got phy/reg addr bit nr %i (%i)"
       " ]\n", state.mii.bit - 2, obit);  */
    if (state.mii.bit <= 6)
      state.mii.phyaddr |= obit << (6 - state.mii.bit);
    else
      state.mii.regaddr |= obit << (11 - state.mii.bit);
    state.mii.bit++;
    if (state.mii.bit >= 12) {

      /*  printf("[ mii_access(): phyaddr=0x%x regaddr=0x"
         "%x ]\n", state.mii.phyaddr, state.mii.regaddr);  */
      state.mii.state = MII_STATE_A;
    }
    break;

  case MII_STATE_A:
    switch (state.mii.opcode) {
    case MII_COMMAND_WRITE:
      if (state.mii.bit >= 13)
        state.mii.state = MII_STATE_D;
      break;

    case MII_COMMAND_READ:
      ibit = 0;
      state.mii.state = MII_STATE_D;
      break;

    default:
      printf("[ mii_access(): UNIMPLEMENTED MII opcode %i (probably just a bug "
             "in GXemul's MII data stream handling) ]\n",
             state.mii.opcode);
      state.mii.state = MII_STATE_RESET;
    }

    state.mii.bit++;
    break;

  case MII_STATE_D:
    switch (state.mii.opcode) {
    case MII_COMMAND_WRITE:
      if (idata & MIIROM_MIIDIR)
        printf("[ mii_access(): write: bad dir? ]\n");
      obit = obit ? (0x8000 >> (state.mii.bit - 14)) : 0;
      tmp = state.mii.phy_reg[(state.mii.phyaddr << 5) + state.mii.regaddr] |
            obit;
      if (state.mii.bit >= 29) {
        state.mii.state = MII_STATE_IDLE;

        //                              debug("[ mii_access(): WRITE to
        //                              phyaddr=0x%x regaddr=0x%x: 0x%04x ]\n",
        //                              state.mii.phyaddr, state.mii.regaddr,
        //                              tmp);
      }
      break;

    case MII_COMMAND_READ:
      if (!(idata & MIIROM_MIIDIR))
        break;
      tmp = state.mii.phy_reg[(state.mii.phyaddr << 5) + state.mii.regaddr];

      //                      if (state.mii.bit == 13)
      //                              debug("[ mii_access(): READ phyaddr=0x%x
      //                              regaddr=0x%x: 0x%04x ]\n",
      //                              state.mii.phyaddr, state.mii.regaddr,
      //                              tmp);
      ibit = tmp & (0x8000 >> (state.mii.bit - 13));
      if (state.mii.bit >= 28)
        state.mii.state = MII_STATE_IDLE;
      break;
    }

    state.mii.bit++;
    break;

  case MII_STATE_IDLE:
    state.mii.bit++;
    if (state.mii.bit >= 31)
      state.mii.state = MII_STATE_RESET;
    break;
  }

  state.reg[CSR_MIIROM / 8] &= ~MIIROM_MDI;
  if (ibit)
    state.reg[CSR_MIIROM / 8] |= MIIROM_MDI;
}

/**
 *  This function handles reads from the Ethernet Address ROM. This is not a
 *  100% correct implementation, as it was reverse-engineered from OpenBSD
 *  sources; it seems to work with OpenBSD, NetBSD, and Linux, though.
 *
 *  Each transfer (if I understood this correctly) is of the following format:
 *
 *	1xx yyyyyy zzzzzzzzzzzzzzzz
 *
 *  where 1xx    = operation (6 means a Read),
 *        yyyyyy = ROM address
 *        zz...z = data
 *
 *  y and z are _both_ read and written to at the same time; this enables the
 *  operating system to sense the number of bits in y (when reading, all y bits
 *  are 1 except the last one).
 **/
void CDEC21143::srom_access(uint32_t oldreg, uint32_t idata) {
  int obit;

  int ibit;

  /*  debug("CSR9 WRITE! 0x%08x\n", (int)idata);  */

  /*  New selection? Then reset internal state.  */
  if (idata & MIIROM_SR && !(oldreg & MIIROM_SR)) {
    state.srom.curbit = 0;
    state.srom.opcode = 0;
    state.srom.opcode_has_started = 0;
    state.srom.addr = 0;
  }

  /*  Only care about data during clock cycles:  */
  if (!(idata & MIIROM_SROMSK))
    return;

  obit = 0;
  ibit = idata & MIIROM_SROMDI ? 1 : 0;

  /*  debug("CLOCK CYCLE! (bit %i): ", state.srom.curbit);  */

  /*
   *  Linux sends more zeroes before starting the actual opcode, than
   *  OpenBSD and NetBSD. Hopefully this is correct. (I'm just guessing
   *  that all opcodes should start with a 1, perhaps that's not really
   *  the case.)
   */
  if (!ibit && !state.srom.opcode_has_started)
    return;

  if (state.srom.curbit < 3) {
    state.srom.opcode_has_started = 1;
    state.srom.opcode <<= 1;
    state.srom.opcode |= ibit;

    /*  debug("opcode input '%i'\n", ibit);  */
  } else {
    switch (state.srom.opcode) {
    case TULIP_SROM_OPC_READ:
      if (state.srom.curbit < 6 + 3) {
        obit = state.srom.curbit < 6 + 2;
        state.srom.addr <<= 1;
        state.srom.addr |= ibit;
      } else {
        uint16_t romword = state.srom.data[state.srom.addr * 2] +
                           (state.srom.data[state.srom.addr * 2 + 1] << 8);

        //                              if (state.srom.curbit == 6 + 3)
        //                                      debug("[ dec21143: ROM read from
        //                                      offset 0x%03x: 0x%04x ]\n",
        //                                      state.srom.addr, romword);
        obit = romword & (0x8000 >> (state.srom.curbit - 6 - 3)) ? 1 : 0;
#if defined(DEBUG_NIC_SROM)
        printf("%%NIC-I-SROMREAD: Read %04x from %04x\n", romword,
               state.srom.addr);
#endif
      }
      break;

    default:
      printf("[ dec21243: unimplemented SROM/EEPROM opcode %i ]\n",
             state.srom.opcode);
    }

    state.reg[CSR_MIIROM / 8] &= ~MIIROM_SROMDO;
    if (obit)
      state.reg[CSR_MIIROM / 8] |= MIIROM_SROMDO;

    /*  debug("input '%i', output '%i'\n", ibit, obit);  */
  }

  state.srom.curbit++;

  /*
   *  Done opcode + addr + data? Then restart. (At least NetBSD does
   *  sequential reads without turning selection off and then on.)
   */
  if (state.srom.curbit >= 3 + 6 + 16) {
    state.srom.curbit = 0;
    state.srom.opcode = 0;
    state.srom.opcode_has_started = 0;
    state.srom.addr = 0;
  }
}

/*
bool CDEC21143:acquire_rx_descriptor(u32 status_true, u32 status_false) {
        // get current receive descriptor
    do_pci_read(state.rx.cur_addr, descr, 4, 4);

        if (rdes0 & TDSTAT_OWN) {	// 21143 owns descriptor
                state.reg[CSR_STATUS/8] &= ~STATUS_RU;	// clear buffers
unavailable if (state.reg[CSR_OPMODE/8] & OPMODE_SR) {	// if receive start
                        state.reg[CSR_STATUS/8] = (state.reg[CSR_STATUS/8] &
~STATUS_RS) | STATUS_RS_WAIT; } else {
                }
                return true;		// indicate nothing was processed
        } else {
                state.reg[CSR_STATUS/8] = (state.reg[CSR_STATUS/8] & ~(STATUS_RS
| STATUS_RU)) | STATUS_R return true;
        }
}
*/

/**
 *  Receive a packet. (If there is no current packet, then check for newly
 *  arrived ones. If the current packet couldn't be fully transfered the
 *  last time, then continue on that packet.)
 **/
int CDEC21143::dec21143_rx() {
  static u32 descr[4];
  static u32 &rdes0 = descr[0];
  static u32 &rdes1 = descr[1];
  static u32 &rdes2 = descr[2];
  static u32 &rdes3 = descr[3];

  u32 bufaddr;
  int bufsize;
  int buf1_size;
  int buf2_size;
  int to_xfer;

  /*  Is current packet finished? Then check for new ones.  */
  if (state.rx.current.used >= state.rx.current.len) {

    /*  Nothing available? Then abort.  */
    if (rx_queue->count() == 0)
      return 0; // indicate nothing was processed

    //      printf("pcap recv: %d bytes (%d captured) for
    //      %02x:%02x:%02x:%02x:%02x:%02x  \n",packet_header->len,
    //      packet_header->caplen,packet_data[0],packet_data[1],packet_data[2],packet_data[3],packet_data[4],packet_data[5]);
    // get next packet from receive queue
    rx_queue->get_head(state.rx.current);

    /*  Append a 4 byte CRC:  */

    // state.rx.cur_buf_len += 4;
    // CHECK_REALLOCATION(state.rx.cur_buf, realloc(state.rx.cur_buf,
    // state.rx.cur_buf_len), unsigned char);

    /*  Get the next packet into our buffer:  */

    // memcpy(state.rx.cur_buf, packet_data, state.rx.cur_buf_len);

    /*  Well... the CRC is just zeros, for now.  */

    // memset(state.rx.cur_buf + state.rx.cur_buf_len - 4, 0, 4);
    // state.rx.cur_offset = 0;
  }

  // read current descriptor
  do_pci_read(state.rx.cur_addr, descr, 4, 4);

  // rdes0 = descr[0] + (descr[1]<<8) + (descr[2]<<16) + (descr[3]<<24);
  // rdes1 = descr[4] + (descr[5]<<8) + (descr[6]<<16) + (descr[7]<<24);
  // rdes2 = descr[8] + (descr[9]<<8) + (descr[10]<<16) + (descr[11]<<24);
  // rdes3 = descr[12] + (descr[13]<<8) + (descr[14]<<16) + (descr[15]<<24);

  /*  Only use descriptors owned by the 21143:  */
  if (!(rdes0 & TDSTAT_OWN)) {

    // set recive buffers unavailable and receive state to suspended
    state.reg[CSR_STATUS / 8] = (state.reg[CSR_STATUS / 8] & ~STATUS_RS) |
                                STATUS_RU | STATUS_RS_SUSPENDED;
    return 0; // indicate nothing was processed
  }

  buf1_size = rdes1 & TDCTL_SIZE1;
  buf2_size = (rdes1 & TDCTL_SIZE2) >> TDCTL_SIZE2_SHIFT;
  bufaddr = buf1_size ? rdes2 : rdes3;
  bufsize = buf1_size ? buf1_size : buf2_size;

  // state.reg[CSR_STATUS/8] &= ~STATUS_RS; // dth: wrong, this is receive state
  // stopped
  //  printf("{ dec21143_rx: base = 0x%08x }\n", (int)addr);
  //      debug("{ RX (%llx): 0x%08x 0x%08x 0x%x 0x%x: buf %i bytes at 0x%x
  //      }\n",
  //          (long long)addr, rdes0, rdes1, rdes2, rdes3, bufsize,
  //          (int)bufaddr);
  // Turn off all status bits, and give up ownership
  rdes0 = 0x00000000;

  //  Is this the first buffer of the frame?
  if (state.rx.current.used == 0)
    rdes0 |= TDSTAT_Rx_FS;

  // use buffer 1, if length is non-zero
  if (buf1_size > 0) {
    to_xfer = state.rx.current.len - state.rx.current.used;
    if (to_xfer > buf1_size)
      to_xfer = buf1_size;

    // DMA bytes from the packet into buffer 1
    do_pci_write(rdes2, &state.rx.current.frame[state.rx.current.used], 1,
                 to_xfer);

    // update used
    state.rx.current.used += to_xfer;
  }

  // use buffer 2, if length is non-zero and not a chain buffer
  if ((buf2_size > 0) && (!(rdes1 & TDCTL_CH))) {
    to_xfer = state.rx.current.len - state.rx.current.used;
    if (to_xfer > buf2_size)
      to_xfer = buf2_size;

    // DMA bytes from the packet into buffer 2
    do_pci_write(rdes3, state.rx.current.frame + state.rx.current.used, 1,
                 to_xfer);

    // update used
    state.rx.current.used += to_xfer;
  }

  //  Frame completed?
  if (state.rx.current.used >= state.rx.current.len) {

    //        debug("frame complete.\n");
    rdes0 |= TDSTAT_Rx_LS;

    /*  Set the frame length:  */
    rdes0 |= (state.rx.current.len << 16) & TDSTAT_Rx_FL;

    /*  Frame too long? (1518 is max ethernet frame length)  */
    if (state.rx.current.len > 1518)
      rdes0 |= TDSTAT_Rx_TL;

    // set receive interrupt and receive state to waiting-for-packet
    state.reg[CSR_STATUS / 8] =
        (state.reg[CSR_STATUS / 8] & ~STATUS_RS) | STATUS_RI | STATUS_RS_WAIT;
  }

  // Writeback rdes0, others are read-only
  state.reg[CSR_STATUS / 8] =
      (state.reg[CSR_STATUS / 8] & ~STATUS_RS) | STATUS_RS_CLOSE;
  do_pci_write(state.rx.cur_addr, descr, 4, 1);

  // move to next descriptor
  if (rdes1 & TDCTL_ER) // end-of-ring, return to base
    state.rx.cur_addr = state.reg[CSR_RXLIST / 8];
  else {
    if (rdes1 & TDCTL_CH) // explicit chain, use chain address
      state.rx.cur_addr = rdes3;
    else
      // implicit chain
      state.rx.cur_addr += sizeof(descr) + state.descr_skip;
  }

  return 1; // indicate processing has occurred
}

/**
 *  Transmit a packet, if the guest OS has marked a descriptor as containing
 *  data to transmit.
 **/
int CDEC21143::dec21143_tx() {
  u32 addr = state.tx.cur_addr;

  u32 bufaddr;
  unsigned char descr[16];
  u32 tdes0;
  u32 tdes1;
  u32 tdes2;
  u32 tdes3;
  int bufsize;
  int buf1_size;
  int buf2_size;

  if (state.tx.suspend)
    return 0;

  do_pci_read(addr, descr, 1, 16);

  tdes0 = descr[0] + (descr[1] << 8) + (descr[2] << 16) + (descr[3] << 24);
  tdes1 = descr[4] + (descr[5] << 8) + (descr[6] << 16) + (descr[7] << 24);
  tdes2 = descr[8] + (descr[9] << 8) + (descr[10] << 16) + (descr[11] << 24);
  tdes3 = descr[12] + (descr[13] << 8) + (descr[14] << 16) + (descr[15] << 24);

  /*  printf("{ dec21143_tx: base=0x%08x, tdes0=0x%08x }\n", (int)addr,
   * (int)tdes0);  */

  /*  Only process packets owned by the 21143:  */
  if (!(tdes0 & TDSTAT_OWN)) {
    if (state.tx.idling > state.tx.idling_threshold) {
      state.reg[CSR_STATUS / 8] |= STATUS_TU;
      state.tx.suspend = true;
      state.tx.idling = 0;
    } else
      state.tx.idling++;
    return 0;
  }

  buf1_size = tdes1 & TDCTL_SIZE1;
  buf2_size = (tdes1 & TDCTL_SIZE2) >> TDCTL_SIZE2_SHIFT;
  bufaddr = buf1_size ? tdes2 : tdes3;
  bufsize = buf1_size ? buf1_size : buf2_size;

  // state.reg[CSR_STATUS/8] &= ~STATUS_TS;
  if (tdes1 & TDCTL_ER) // end-of-ring, return to base
    state.tx.cur_addr = state.reg[CSR_TXLIST / 8];
  else {
    if (tdes1 & TDCTL_CH) // explicit chain, use chain address
      state.tx.cur_addr = tdes3;
    else
      // implicit chain
      state.tx.cur_addr += (4 * sizeof(uint32_t)) + state.descr_skip;
  }

  /*
     printf("{ TX (%llx): 0x%08x 0x%08x 0x%x 0x%x: buf %i bytes at 0x%x }\n",
     (long long)addr, tdes0, tdes1, tdes2, tdes3, bufsize, (int)bufaddr);
   */

  /*  Assume no error:  */
  tdes0 &= ~(TDSTAT_Tx_UF | TDSTAT_Tx_EC | TDSTAT_Tx_LC | TDSTAT_Tx_NC |
             TDSTAT_Tx_LO | TDSTAT_Tx_TO | TDSTAT_ES);

  if (tdes1 & TDCTL_Tx_SET) {

    /*
     *  Setup Packet.
     *
     *  TODO. For now, just ignore it, and pretend it worked.
     */

    //              printf("{ TX: setup packet }\n");
    if (bufsize != 192)
      printf("[ dec21143: setup packet len = %i, should be 192! ]\n",
             (int)bufsize);
    do_pci_read(bufaddr, state.setup_filter, 1, 192);
    SetupFilter();

    if (tdes1 & TDCTL_Tx_IC)
      state.reg[CSR_STATUS / 8] |= STATUS_TI;

    /*  New descriptor values, according to the docs:  */
    tdes0 = 0x7fffffff;
    tdes1 = 0xffffffff;
    tdes2 = 0xffffffff;
    tdes3 = 0xffffffff;
  } else {

    /*
     *  Data Packet.
     */

    //              printf("{ TX: data packet: ");
    if (tdes1 & TDCTL_Tx_FS) {

      /*  First segment. Let's allocate a new buffer:  */

      /*  printf("new frame }\n");  */

      // CHECK_ALLOCATION(state.tx.cur_buf = (unsigned char *)malloc(bufsize));
      state.tx.cur_buf_len = 0;
    } else {

      /*  Not first segment. Increase the length of the current buffer:  */

      /*  printf("continuing last frame }\n");  */
      if (state.tx.cur_buf == NULL)
        printf("[ dec21143: WARNING! tx: middle segment, but no first "
               "segment?! ]\n");

      // CHECK_REALLOCATION(state.tx.cur_buf, realloc(state.tx.cur_buf,
      // state.tx.cur_buf_len + bufsize), unsigned char);
    }

    /*  "DMA" data from emulated physical memory into the buf:  */
    do_pci_read(bufaddr, state.tx.cur_buf + state.tx.cur_buf_len, 1, bufsize);

    state.tx.cur_buf_len += bufsize;

/* only partial frames were written to the pcap filter, because the second
 * buffer was not considered when collecting the ethernet frames in dec21143_tx.
 * It can happen that both buffers to which tdes2 and tdes3 point contain data.
 * When this happens the data of both buffers have to be combined to get a valid
 * ethernet frame and hence IP packet. The patch simply checks if buf2_size is
 * greater 0 and if that's true append the data from the buffer pointed to by
 * tdes3 to the current frame.
 */
    if ((buf2_size > 0) && (!(tdes1 & TDCTL_CH))) {
      do_pci_read(tdes3, state.tx.cur_buf + state.tx.cur_buf_len, 1, buf2_size);
      state.tx.cur_buf_len += buf2_size;
    }

    /*  Last segment? Then actually transmit it:  */
    if (tdes1 & TDCTL_Tx_LS) {

      /*  printf("{ TX: data frame complete. }\n");  */

      // if not in internal loopback mode, transmit packet to wire
      if (!(state.reg[CSR_OPMODE / 8] & OPMODE_OM_INTLOOP)) {

        // printf("pcap send: %d bytes   \n", state.tx.cur_buf_len);
        if (pcap_sendpacket(fp, state.tx.cur_buf, state.tx.cur_buf_len))
          printf("Error sending the packet: %s\n", pcap_geterr(fp));
      }

      // if in internal or external loopback mode, add packet to read queue
      if (state.reg[CSR_OPMODE / 8] & OPMODE_OM) {
        bool crc = !(tdes1 & TDCTL_Tx_AC);

        // printf("21143: %s packet, AC: %d\n", (state.reg[CSR_OPMODE / 8] &
        // OPMODE_OM_INTLOOP) ? "IL" : "EL", !crc); printf("21143: TX enabled:
        // %d, RX enabled: %d\n", state.reg[CSR_OPMODE /8] & OPMODE_ST,
        // state.reg[CSR_OPMODE /8] & OPMODE_SR); printf("21143: tx(), len=%d,
        // addr=%08x, mode=%s\n", state.tx.cur_buf_len, (int) state.tx.cur_buf,
        // (state.reg[CSR_OPMODE / 8] & OPMODE_OM_INTLOOP) ? "IL" : "EL");
        // printf("21143: tx(), data=|");
        // unsigned char* aptr = state.tx.cur_buf;
        // for(int i=0; i<state.tx.cur_buf_len; i++) {
        //      printf("%02x-",*aptr++);
        //}
        // printf("|\n");
        rx_queue->add_tail(state.tx.cur_buf, state.tx.cur_buf_len, calc_crc,
                           crc);
      }

      // free(state.tx.cur_buf);
      // state.tx.cur_buf = NULL;
      state.tx.cur_buf_len = 0;

      /*  Interrupt, if Tx_IC is set:  */
      if (tdes1 & TDCTL_Tx_IC)
        state.reg[CSR_STATUS / 8] |= STATUS_TI;
    }

    /*  We are done with this segment.  */
    tdes0 &= ~TDSTAT_OWN;
  }

  /*  Error summary:  */
  if (tdes0 & (TDSTAT_Tx_UF | TDSTAT_Tx_EC | TDSTAT_Tx_LC | TDSTAT_Tx_NC |
               TDSTAT_Tx_LO | TDSTAT_Tx_TO))
    tdes0 |= TDSTAT_ES;

  /*  Descriptor writeback:  */
  descr[0] = (u8)tdes0;
  descr[1] = (u8)(tdes0 >> 8);
  descr[2] = (u8)(tdes0 >> 16);
  descr[3] = (u8)(tdes0 >> 24);
  descr[4] = (u8)tdes1;
  descr[5] = (u8)(tdes1 >> 8);
  descr[6] = (u8)(tdes1 >> 16);
  descr[7] = (u8)(tdes1 >> 24);
  descr[8] = (u8)tdes2;
  descr[9] = (u8)(tdes2 >> 8);
  descr[10] = (u8)(tdes2 >> 16);
  descr[11] = (u8)(tdes2 >> 24);
  descr[12] = (u8)tdes3;
  descr[13] = (u8)(tdes3 >> 8);
  descr[14] = (u8)(tdes3 >> 16);
  descr[15] = (u8)(tdes3 >> 24);

  do_pci_write(addr, descr, 1, 16);

  return 1;
}

void CDEC21143::SetupFilter() {
  u8 mac[16][6];
  char mac_txt[16][20];
  char filter[1000];
  int i;
  int j;
  int numUnique;
  int unique[16];
  bool u;
#if defined(DEBUG_NIC_FILTER)
  printf("Building a filter...\n");
#endif
  for (i = 0; i < 16; i++) {
    mac[i][0] = state.setup_filter[i * 12];
    mac[i][1] = state.setup_filter[i * 12 + 1];
    mac[i][2] = state.setup_filter[i * 12 + 4];
    mac[i][3] = state.setup_filter[i * 12 + 5];
    mac[i][4] = state.setup_filter[i * 12 + 8];
    mac[i][5] = state.setup_filter[i * 12 + 9];
    sprintf(mac_txt[i], "%02x:%02x:%02x:%02x:%02x:%02x", mac[i][0], mac[i][1],
            mac[i][2], mac[i][3], mac[i][4], mac[i][5]);
#if defined(DEBUG_NIC_FILTER)
    printf("MAC[%d] = %s. \n", i, mac_txt[i]);
#endif
  }

#if defined(DEBUG_NIC_FILTER)
  printf("Filter mode: ");
  if (state.reg[CSR_OPMODE / 8] & OPMODE_PR)
    printf("promiscuous.\n");
  else {
    if (state.reg[CSR_OPMODE / 8] & OPMODE_IF)
      printf("inverse ");
    if (state.reg[CSR_OPMODE / 8] & OPMODE_HP) {
      printf("hash ");
      if (state.reg[CSR_OPMODE / 8] & OPMODE_HO)
        printf("only ");
    } else
      printf("perfect ");
    printf("filtering.\n");
  }
#endif
  numUnique = 0;
  for (i = 0; i < 16; i++) {
    u = true;
    for (j = 0; j < numUnique; j++) {
      if (mac[i][0] == mac[unique[j]][0] && mac[i][1] == mac[unique[j]][1] &&
          mac[i][2] == mac[unique[j]][2] && mac[i][3] == mac[unique[j]][3] &&
          mac[i][4] == mac[unique[j]][4] && mac[i][5] == mac[unique[j]][5]) {
        u = false;
        break;
      }
    }

    if (u) {
      unique[numUnique] = i;
      numUnique++;
    }
  }

#if defined(DEBUG_NIC_FILTER)
  for (i = 0; i < numUnique; i++)
    printf("Unique MAC[%d] = %s. \n", i, mac_txt[unique[i]]);
#endif
  filter[0] = '\0';

  // strcat(filter,"ether broadcast");
  // There must be at least one unique item; at least the mac of the card
  strcat(filter, "ether dst ");
  strcat(filter, mac_txt[unique[0]]);
  for (i = 1; i < numUnique; i++) {
    strcat(filter, " or ether dst ");
    strcat(filter, mac_txt[unique[i]]);
  }

#if defined(DEBUG_NIC_FILTER)
  printf("FILTER = %s.   \n", filter);
#endif
  if (pcap_compile(fp, &fcode, filter, 1, 0xffffffff) < 0)
    FAILURE_1(Logic, "Unable to compile the packet filter (%s)", filter);

  if (pcap_setfilter(fp, &fcode) < 0)
    FAILURE(Runtime, "Error setting the filter.");
}

/**
 * Reset the network interface internals to their condition immediately
 * after power-up, including the PCI configuration.
 **/
void CDEC21143::ResetPCI() {
  CPCIDevice::ResetPCI();

  ResetNIC();
}

/**
 * Reset the network interface internals to their condition immediately
 * after power-up. This does not affect the PCI configuration.
 *
 * Code for computing the SROM checksum is taken from [T64].
 **/
void CDEC21143::ResetNIC() {
  int leaf;

  if (state.rx.cur_buf != NULL)
    free(state.rx.cur_buf);

  //  if (state.tx.cur_buf != NULL)
  //        free(state.tx.cur_buf);
  state.rx.cur_buf = /*state.tx.cur_buf = */ NULL;

  memset(state.reg, 0, sizeof(uint32_t) * 32);
  memset(state.srom.data, 0, sizeof(state.srom.data));
  memset(state.mii.phy_reg, 0, sizeof(state.mii.phy_reg));

  /*  Register values at reset, according to the manual:  */
  state.reg[CSR_BUSMODE / 8] = 0xfe000000; /*  csr0   */
  state.reg[CSR_MIIROM / 8] = 0xfff483ff;  /*  csr9   */
  state.reg[CSR_SIACONN / 8] = 0xffff0000; /*  csr13  */
  state.reg[CSR_SIATXRX / 8] = 0xffffffff; /*  csr14  */
  state.reg[CSR_SIAGEN / 8] = 0x8ff00000;  /*  csr15  */

  state.tx.idling_threshold = 10;
  state.rx.cur_addr = state.tx.cur_addr = 0;

  /*  Version (= 1) and Chip count (= 1):  */
  state.srom.data[TULIP_ROM_SROM_FORMAT_VERION] = 1;
  state.srom.data[TULIP_ROM_CHIP_COUNT] = 1;

  /*  Set the MAC address:  */
  memcpy(state.srom.data + TULIP_ROM_IEEE_NETWORK_ADDRESS, state.mac, 6);

  leaf = 30;
  state.srom.data[TULIP_ROM_CHIPn_DEVICE_NUMBER(0)] = 0;
  state.srom.data[TULIP_ROM_CHIPn_INFO_LEAF_OFFSET(0)] = leaf & 255;
  state.srom.data[TULIP_ROM_CHIPn_INFO_LEAF_OFFSET(0) + 1] = leaf >> 8;

  state.srom.data[leaf + TULIP_ROM_IL_SELECT_CONN_TYPE] = 0; /*  Not used?  */
  state.srom.data[leaf + TULIP_ROM_IL_MEDIA_COUNT] = 2;
  leaf += TULIP_ROM_IL_MEDIAn_BLOCK_BASE;

  state.srom.data[leaf] = 7; /*  descriptor length  */
  state.srom.data[leaf + 1] = TULIP_ROM_MB_21142_SIA;
  state.srom.data[leaf + 2] = TULIP_ROM_MB_MEDIA_100TX;

  /*  here comes 4 bytes of GPIO control/data settings  */
  leaf += state.srom.data[leaf];

  state.srom.data[leaf] = 15; /*  descriptor length  */
  state.srom.data[leaf + 1] = TULIP_ROM_MB_21142_MII;
  state.srom.data[leaf + 2] = 0; /*  PHY nr  */
  state.srom.data[leaf + 3] = 0; /*  len of select sequence  */
  state.srom.data[leaf + 4] = 0; /*  len of reset sequence  */

  /*  5,6, 7,8, 9,10, 11,12, 13,14 = unused by GXemul  */
  leaf += state.srom.data[leaf];

  /*  MII PHY initial state:  */
  state.mii.state = MII_STATE_RESET;

  /*  PHY #0:  */
  state.mii.phy_reg[MII_BMSR] =
      BMSR_100TXFDX | BMSR_10TFDX | BMSR_ACOMP | BMSR_ANEG | BMSR_LINK;

  state.tx.suspend = false;

  // compute the CRC for the SROM data.  This code is from the
  // tu sample driver from HP in if_tu.c, which was apparently
  // derived from the 21140 Hardware Specification
  unsigned int POLY = 0x04c11db6;
  unsigned int FlippedCrc = 0;
  unsigned int Crc = 0xffffffff;
  unsigned char i;
  unsigned char CurrentByte;
  unsigned char Bit;
  unsigned char Msb;
  unsigned char chksm_1;
  unsigned char chksm_2;

  for (i = 0; i < 126; i++) {
    CurrentByte = state.srom.data[i];

    for (Bit = 0; Bit < 8; Bit++) {
      Msb = (Crc >> 31) & 1;
      Crc <<= 1;

      if (Msb ^ (CurrentByte & 1)) {
        Crc ^= POLY;
        Crc |= 1;
      }

      CurrentByte >>= 1;
    }
  }

  for (i = 0; i < 32; i++) {
    FlippedCrc <<= 1;
    Bit = Crc & 1;
    Crc >>= 1;
    FlippedCrc += Bit;
  }

  Crc = FlippedCrc ^ 0xffffffff;

  chksm_1 = Crc & 0xff;
  chksm_2 = Crc >> 8;

  state.srom.data[126] = chksm_1;
  state.srom.data[127] = chksm_2;
#if defined(DEBUG_NIC_SROM)
  printf("%%NIC-I-CKSUM: SROM checksum bytes are %02x, %02x\n",
         state.srom.data[126], state.srom.data[127]);
#endif
}

static u32 nic_magic1 = 0xDEC21143;
static u32 nic_magic2 = 0x21143DEC;

/**
 * Save state to a Virtual Machine State file.
 **/
int CDEC21143::SaveState(FILE *f) {
  long ss = sizeof(state);
  int res;

  if ((res = CPCIDevice::SaveState(f)))
    return res;

  fwrite(&nic_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&nic_magic2, sizeof(u32), 1, f);
  printf("%s: %ld bytes saved.\n", devid_string, ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CDEC21143::RestoreState(FILE *f) {
  long ss;
  u32 m1;
  u32 m2;
  int res;
  size_t r;

  if ((res = CPCIDevice::RestoreState(f)))
    return res;

  r = fread(&m1, sizeof(u32), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (m1 != nic_magic1) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  r = fread(&ss, sizeof(long), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (ss != sizeof(state)) {
    printf("%s: STRUCT SIZE does not match!\n", devid_string);
    return -1;
  }

  r = fread(&state, sizeof(state), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  r = fread(&m2, sizeof(u32), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (m2 != nic_magic2) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  printf("%s: %ld bytes restored.\n", devid_string, ss);
  return 0;
}
#endif // defined(HAVE_PCAP)
