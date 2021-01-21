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
 */

/**
 * \file
 * Contains the code for the emulated DMA controller.
 *
 * $Id: DMA.cpp,v 1.9 2008/04/29 09:24:52 iamcamiel Exp $
 *
 * X-1.9        Camiel Vanderhoeven                             29-APR-2008
 *      Removed double function bodies. (patch issue)
 *
 * X-1.8        Brian Wheeler                                   29-APR-2008
 *      Fixed floppy disk implementation.
 *
 * X-1.7        Brian Wheeler                                   29-APR-2008
 *      DMA now supports floppy device.
 *
 * X-1.6        Brian Wheeler                                   18-APR-2008
 *      Rewrote DMA code to make it ready for floppy support.
 *
 * X-1.5        Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.4        Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.3        Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.2        Brian Wheeler                                   26-FEB-2008
 *      Debugging statements conditionalized.
 *
 * X-1.1        Camiel Vanderhoeven                             26-FEB-2008
 *      Created. Contains code previously found in AliM1543C.cpp
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#include "DMA.hpp"
#include "AliM1543C.hpp"
#include "PCIDevice.hpp"
#include "StdAfx.hpp"
#include "System.hpp"

#define DEBUG_DMA

CDMA *theDMA = 0;

/**
 * Constructor.
 **/
CDMA::CDMA(CConfigurator *cfg, CSystem *c) : CSystemComponent(cfg, c) {
  int i;

  // DMA Setup
#define LEGACY_IO(id, port, size)                                              \
  c->RegisterMemory(this, id, U64(0x00000801fc000000) + port, size)
  LEGACY_IO(DMA0_IO_CHANNEL, 0x00, 8);
  LEGACY_IO(DMA0_IO_MAIN, 0x08, 8);
  LEGACY_IO(DMA_IO_LPAGE, 0x81, 11);
  LEGACY_IO(DMA1_IO_CHANNEL, 0xc0, 16);
  LEGACY_IO(DMA1_IO_MAIN, 0xd0, 16);
  LEGACY_IO(DMA_IO_HPAGE, 0x481, 11);
  LEGACY_IO(DMA0_IO_EXT, 0x040b, 1);
  LEGACY_IO(DMA1_IO_EXT, 0x04D6, 1);

  for (i = 0; i < 8; i++) {
    state.channel[i].c_lobyte = true;
    state.channel[i].a_lobyte = true;
  }
  state.controller[0].mask = 0xff;
  state.controller[1].mask = 0xff;

  theDMA = this;
  printf("dma: $Id: DMA.cpp,v 1.9 2008/04/29 09:24:52 iamcamiel Exp $\n");
}

/**
 * Destructor.
 **/
CDMA::~CDMA() {}
int CDMA::DoClock() { return 0; }

const char *dma_index_names[] = {
    "DMA0_IO_MAIN",    "DMA1_IO_MAIN",    "DMA_IO_LPAGE", "DMA_IO_HPAGE",
    "DMA0_IO_CHANNEL", "DMA1_IO_CHANNEL", "DMA0_IO_EXT",  "DMA1_IO_EXT"};
#define DMA_INDEX(n) dma_index_names[n - DMA_IO_BASE]

u64 CDMA::ReadMem(int index, u64 address, int dsize) {
  u64 ret;
  u8 data = 0;
  int num;
  // printf("dma: Readmem %s, %" PRIx64 ", %x\n",DMA_INDEX(index),address, dsize);
  switch (dsize) {
  case 32:
    ret = ReadMem(index, address, 8);
    ret |= ReadMem(index, address + 1, 8) << 8;
    ret |= ReadMem(index, address + 2, 8) << 16;
    ret |= ReadMem(index, address + 3, 8) << 32;
    return ret;

  case 16:
    ret = ReadMem(index, address, 8);
    ret |= ReadMem(index, address + 1, 8) << 8;
    return ret;

  case 8:

    if (index == DMA1_IO_CHANNEL || index == DMA1_IO_MAIN)
      address >>= 1;

    switch (index) {
    case DMA0_IO_CHANNEL:
    case DMA1_IO_CHANNEL:
      num = ((address & 0x0e) >> 1) + (index * 4);
      if (address & 1) {
        // word count registers
        data = (state.channel[num].count >>
                (state.channel[num].c_lobyte ? 8 : 0)) &
               0xff;
        state.channel[num].c_lobyte = !state.channel[num].c_lobyte;
      } else {
        // base address
        data = (state.channel[num].current >>
                (state.channel[num].a_lobyte ? 8 : 0)) &
               0xff;
        state.channel[num].a_lobyte = !state.channel[num].a_lobyte;
      }
      break;

    case DMA0_IO_MAIN:
    case DMA1_IO_MAIN:
      num = ((address & 0x0e) >> 1) + ((index - DMA_IO_BASE) * 4);
      printf("num: %d\n", num);
      for (int i = 0; i < 4; i++)
        data |= ((state.channel[(num * 4) + i].count ==
                  state.channel[(num * 4) + 1].current)
                     ? 1
                     : 0)
                << i;
      data |= (state.controller[num].request & 0x0f) << 4;
      break;

    default:
      FAILURE(InvalidArgument, "dma: ReadMem index out of range");
    }

#if defined(DEBUG_DMA)
    printf("dma: read %s,%" PRIx64 ": %" PRIx8 ".   \n", DMA_INDEX(index),
           address, data);
#endif
  }

  return data;
}

void CDMA::WriteMem(int index, u64 address, int dsize, u64 data) {
  int num = 0;
  int channelmap[] = {2, 3, 1, 0xff, 0xff, 0xff, 0, 0xff,
                      6, 7, 5, 0xff, 0xff, 0xff, 4};
  switch (dsize) {
  case 32:
    WriteMem(index, address + 0, 8, (data >> 0) & 0xff);
    WriteMem(index, address + 1, 8, (data >> 8) & 0xff);
    WriteMem(index, address + 2, 8, (data >> 16) & 0xff);
    WriteMem(index, address + 3, 8, (data >> 24) & 0xff);
    return;

  case 16:
    WriteMem(index, address + 0, 8, (data >> 0) & 0xff);
    WriteMem(index, address + 1, 8, (data >> 8) & 0xff);
    return;

  case 8:
    data &= 0xff;
    if (index == DMA1_IO_CHANNEL || index == DMA1_IO_MAIN)
      address >>= 1;

#if defined(DEBUG_DMA)
      // printf("dma: write %s, %02x: %02x.   \n", DMA_INDEX(index),
      // (u32)address, data);
#endif
    switch (index) {
    case DMA1_IO_CHANNEL:
      num = 1;
    case DMA0_IO_CHANNEL:
      num = ((address & 0x0e) >> 1) + (num * 4);
      if (address & 1) {
        if (state.channel[num].c_lobyte)
          state.channel[num].count = (state.channel[num].count & 0xff00) | data;
        else
          state.channel[num].count =
              (state.channel[num].count & 0xff) | (data << 8);
        state.channel[num].c_lobyte = !state.channel[num].c_lobyte;
#if defined(DEBUG_DMA)
        printf("dma channel %d count: %04x\n", num, state.channel[num].count);
#endif
      } else {
        if (state.channel[num].a_lobyte)
          state.channel[num].base = (state.channel[num].base & 0xff00) | data;
        else
          state.channel[num].base =
              (state.channel[num].base & 0xff) | (data << 8);
        state.channel[num].a_lobyte = !state.channel[num].a_lobyte;
#if defined(DEBUG_DMA)
        printf("dma channel %d base: %04x\n", num, state.channel[num].count);
#endif
      }
      break;

    case DMA1_IO_MAIN:
      num = 1;
    case DMA0_IO_MAIN:
      switch (address) {
      case 0: // command
        printf("dma: command register %d written with %" PRIx64 "\n", num,
               data);
        state.controller[num].command = data;
        break;

      case 1: // request
        set_request(num, data & 0x03, (data & 0x04) >> 2);
        break;

      case 2: // single mask
        printf("dma: mask single on %d : %" PRIx64 " %s\n", num, data & 0x03,
               data & 0x4 ? "Masked" : "Unmasked");
        state.controller[num].mask =
            (state.controller[num].mask & ~(1 << (data & 0x03))) |
            ((data & 0x04) >> 2);
        printf("     Mask status: %x\n", state.controller[num].mask);
        do_dma();
        break;

      case 3: // mode register
        printf("dma: mode register %d for channel %" PRIx64
               " written with %" PRIx64 "\n",
               num, (num * 4) + (data & 0x03), data);
        printf("    Mode: %s, Address %s, Autoinit %s, Command: %s\n",
               (data & 0x80 ? (data & 0x40 ? "Cascade" : "Block")
                            : (data & 0x40 ? "Single" : "Demand")),
               (data & 0x20 ? "Increment" : "Decrement"),
               (data & 0x10 ? "Enable" : "Disable"),
               (data & 0x08 ? (data & 0x04 ? "Illegal" : "Read")
                            : (data & 0x04 ? "Write" : "Verify")));

        state.channel[(num * 4) + (data & 0x03)].mode = data;
        break;

      case 4: // clear flipflop(s)
        printf("dma: flipflops cleared for dma %d\n", num);
        for (int i = (num * 4); i < ((num + 1) * 4); i++)
          state.channel[i].a_lobyte = state.channel[i].c_lobyte = true;
        break;

      case 5: // master reset
#if defined(DEBUG_DMA)
        printf("DMA-I-RESET: DMA %d reset.", index - DMA_IO_BASE);
#endif
        for (int i = (num * 4); i < ((num + 1) * 4); i++)
          state.channel[i].a_lobyte = state.channel[i].c_lobyte = true;
        state.controller[num].mask = 0xff;
        break;

      case 6: // master enable
        state.controller[num].mask = 0x00;
        do_dma();
        break;

      case 7: // master mask
        state.controller[num].mask = data;
        do_dma();
        break;
      }
      break;

    case DMA_IO_LPAGE:
    case DMA_IO_HPAGE:
      if (channelmap[address] == 0xff) {
        printf("dma: unknown page register %" PRIx64 "\n", address);
        return;
      }
      num = channelmap[address];
      if (index == DMA_IO_LPAGE)
        state.channel[num].pagebase =
            (state.channel[num].pagebase & 0xff00) | data;
      else
        state.channel[num].pagebase =
            (state.channel[num].pagebase & 0xff) | (data << 8);

#if defined(DEBUG_DMA)
      printf("dma channel %d pagebase: %04x\n", num,
             state.channel[num].pagebase);
#endif

      break;

    case DMA0_IO_EXT:
    case DMA1_IO_EXT:
      printf("dma: extended mode register %d written: %" PRIx64 "\n",
             index - DMA0_IO_EXT, data);
      break;

    default:
      FAILURE(InvalidArgument, "dma: WriteMem index out of range");
    }

    return;
  }
}

static u32 dma_magic1 = 0x65324387;
static u32 dma_magic2 = 0x24092875;

/**
 * Save state to a Virtual Machine State file.
 **/
int CDMA::SaveState(FILE *f) {
  long ss = sizeof(state);

  fwrite(&dma_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&dma_magic2, sizeof(u32), 1, f);
  printf("dma: %ld bytes saved.\n", ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CDMA::RestoreState(FILE *f) {
  long ss;
  u32 m1;
  u32 m2;
  size_t r;

  r = fread(&m1, sizeof(u32), 1, f);
  if (r != 1) {
    printf("dma: unexpected end of file!\n");
    return -1;
  }

  if (m1 != dma_magic1) {
    printf("dma: MAGIC 1 does not match!\n");
    return -1;
  }

  r = fread(&ss, sizeof(long), 1, f);
  if (r != 1) {
    printf("dma: unexpected end of file!\n");
    return -1;
  }

  if (ss != sizeof(state)) {
    printf("dma: STRUCT SIZE does not match!\n");
    return -1;
  }

  r = fread(&state, sizeof(state), 1, f);
  if (r != 1) {
    printf("dma: unexpected end of file!\n");
    return -1;
  }

  r = fread(&m2, sizeof(u32), 1, f);
  if (r != 1) {
    printf("dma: unexpected end of file!\n");
    return -1;
  }

  if (m2 != dma_magic2) {
    printf("dma: MAGIC 1 does not match!\n");
    return -1;
  }

  printf("dma: %ld bytes restored.\n", ss);
  return 0;
}

/**
 * Set the software request bit for a channel, and initiate DMA
 **/
void CDMA::set_request(int num, int channel, int data) {
  state.controller[num].request =
      (state.controller[num].request & ~(1 << (data & 0x03))) |
      ((data & 0x04) >> 2);
  state.channel[(num * 4) + (data & 0x03)].current = 0;
  do_dma();
}

/**
 * Perform a DMA if one is ready.
 *
 * \todo I'm not sure what would actually trigger this, so its mostly just a
 * placeholder.
 **/
void CDMA::do_dma() {
  for (int ctrlr = 0; ctrlr < 2; ctrlr++) {
    if ((state.controller[ctrlr].command & 0x04) ==
        0) // controller not disabled.
    {
      for (int chnl = 0; chnl < 4; chnl++) {
        if ((state.controller[ctrlr].mask & (1 << chnl)) ==
            0) // channel not masked
        {
          if (state.controller[ctrlr].request &
              (1 << chnl)) // channel has request
          {
            // Do it!
          }
        }
      }
    }
  }
}

/**
 * This can be called by a device to perform a DMA in one fell swoop.
 **/

void CDMA::send_data(int channel, void *data) {
  if ((state.controller[channel < 4 ? 0 : 1].command & 0x04) == 0) {
    if ((state.controller[channel < 4 ? 0 : 1].mask & (1 << channel)) == 0) {
      u64 addr =
          (state.channel[channel].pagebase << 16) + state.channel[channel].base;
      int count = get_count(channel);

      printf("DMA send_data:  %x @ %16" PRIx64 "x\n  ", count, addr);
      for (int i = 0; i < count; i++) {
        printf("%02x ", *((char *)data + i) & 0xff);
        if (i % 16 == 15)
          printf("\n  ");
      }
      printf("\n");

      // increment
      theAli->do_pci_write(addr, data, 1, count);

      // set the terminal count bit
      if (channel < 4)
        state.controller[0].status |= 1 << channel;
      else
        state.controller[1].status |= 1 << channel;
    } else {
      printf("dma: dma requested by device on channel %d, but it is masked.\n",
             channel);
    }
  } else {
    printf("dma: dma requested by device, but controller %d is disabled.\n",
           channel < 4 ? 0 : 1);
  }
}

void CDMA::recv_data(int channel, void *data) {}
