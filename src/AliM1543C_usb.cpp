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
 * Contains the code for the emulated Ali M1543C IDE chipset part.
 *
 * X-1.5        Brian Wheeler                                   27-FEB-2008
 *      Avoid compiler warnings.
 *
 * X-1.4        Brian wheeler                                   18-FEB-2008
 *      Implemented HCI register space.
 *
 * X-1.3        Camiel Vanderhoeven                             30-DEC-2007
 *      Print file id on initialization.
 *
 * X-1.2        Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.1        Camiel Vanderhoeven                             10-DEC-2007
 *      Initial version in CVS; this part was split off from the CAliM1543C
 *      class.
 **/
#include "AliM1543C_usb.h"
#include "StdAfx.h"
#include "System.h"

u32 usb_cfg_data[64] = {
    /*00*/ 0x523710b9, // CFID: vendor + device
    /*04*/ 0x02800000, // CFCS: command + status
    /*08*/ 0x0c031003, // CFRV: class + revision
    /*0c*/ 0x00000000, // CFLT: latency timer + cache line size
    /*10*/ 0x00000000, // BAR0:
    /*14*/ 0x00000000, // BAR1:
    /*18*/ 0x00000000, // BAR2:
    /*1c*/ 0x00000000, // BAR3:
    /*20*/ 0x00000000, // BAR4:
    /*24*/ 0x00000000, // BAR5:
    /*28*/ 0x00000000, // CCIC: CardBus
    /*2c*/ 0x00000000, // CSID: subsystem + vendor
    /*30*/ 0x00000000, // BAR6: expansion rom base
    /*34*/ 0x00000000, // CCAP: capabilities pointer
    /*38*/ 0x00000000,
    /*3c*/ 0x500001ff, // CFIT: interrupt configuration
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

u32 usb_cfg_mask[64] = {
    /*00*/ 0x00000000, // CFID: vendor + device
    /*04*/ 0x00000157, // CFCS: command + status
    /*08*/ 0x00000000, // CFRV: class + revision
    /*0c*/ 0x0000ffff, // CFLT: latency timer + cache line size
    /*10*/ 0xfffff000, // BAR0
    /*14*/ 0x00000000, // BAR1:
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
    /*40*/ 0x04100000, // TM - test mode register
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

/**
 * Constructor.
 **/
CAliM1543C_usb::CAliM1543C_usb(CConfigurator *cfg, CSystem *c, int pcibus,
                               int pcidev)
    : CPCIDevice(cfg, c, pcibus, pcidev) {
  add_function(0, usb_cfg_data, usb_cfg_mask);

  ResetPCI();

  state.usb_data[0x34 / 4] = 0x2edf;
  state.usb_data[0x48 / 4] = 0x01000003;

  printf(
      "%s: $Id: AliM1543C_usb.cpp,v 1.6 2008/03/14 15:30:50 iamcamiel Exp $\n",
      devid_string);
}

CAliM1543C_usb::~CAliM1543C_usb() {}
u32 CAliM1543C_usb::ReadMem_Bar(int func, int bar, u32 address, int dsize) {
  u32 data = 0;
  switch (bar) {
  case 0:
    data = usb_hci_read(address, dsize);
    break;
  default:
    printf("%%USB-W-READBAR: Bad BAR %d selected.\n", bar);
  }

  return data;
}

void CAliM1543C_usb::WriteMem_Bar(int func, int bar, u32 address, int dsize,
                                  u32 data) {
  switch (bar) {
  case 0:
    usb_hci_write(address, dsize, data);
    break;
  default:
    printf("%%USB-W-WRITEBAR: Bad BAR %d selected.\n", bar);
  }

  return;
}

u64 CAliM1543C_usb::usb_hci_read(u64 address, int dsize) {
  u64 data = 0;
  if (dsize != 32)
    printf("%%USB-W-HCIREAD: Non dword read, returning 32 bits anyway.\n");
  switch (address) {
  case 0: // HcRevision
    data = 0x00000110;
    break;

  case 4:     // HcControl
  case 8:     // HcCommandStatus
  case 0x0c:  // HcInterruptStatus
  case 0x10:  // HcInterrupt Enable
  case 0x14:  // HcInterruptDisable
  case 0x18:  // HcHCCA (datasheet says 0x17, but that's wrong)
  case 0x1c:  // HcPeriodCurrentED
  case 0x20:  // HcControlHeadED
  case 0x24:  // HcControlCurrentED
  case 0x28:  // HcBulkHeadED
  case 0x2c:  // HcBulkCurrentED
  case 0x30:  // HcDoneHead
  case 0x34:  // HcFmInterval
  case 0x38:  // HcFrameRemaining
  case 0x3c:  // HcFmNumber
  case 0x40:  // HcPeriodicStart
  case 0x44:  // HcLSThreshold
  case 0x48:  // HcRhDescriptorA
  case 0x4c:  // HcRhDescriptorB
  case 0x50:  // HcRhStatus
  case 0x54:  // HcRhPortStatus1
  case 0x58:  // HcRhPortStatus1
  case 0x5c:  // HcRhPortStatus1
  case 0x100: // HceControlRegister
  case 0x104: // HceInputRegister
  case 0x108: // HceOutputRegister
  case 0x10c: // HceStatusRegister
    data = state.usb_data[address / 4];
    break;

  default:
    printf("%%USB-W-HCIREAD: Reading from unknown address %x.  Ignoring.\n",
           (int)address);
  }

  return data;
}

void CAliM1543C_usb::usb_hci_write(u64 address, int dsize, u64 data) {
  if (dsize != 32)
    printf("%%USB-W-HCIWRITE: Non dword write, writing 32 bits anyway.\n");
  switch (address) {
  case 4:     // HcControl
  case 8:     // HcCommandStatus
  case 0x0c:  // HcInterruptStatus
  case 0x10:  // HcInterrupt Enable
  case 0x14:  // HcInterruptDisable
  case 0x18:  // HcHCCA (datasheet says 0x17, but that's wrong)
  case 0x1c:  // HcPeriodCurrentED
  case 0x20:  // HcControlHeadED
  case 0x24:  // HcControlCurrentED
  case 0x28:  // HcBulkHeadED
  case 0x2c:  // HcBulkCurrentED
  case 0x30:  // HcDoneHead
  case 0x34:  // HcFmInterval
  case 0x38:  // HcFrameRemaining
  case 0x3c:  // HcFmNumber
  case 0x40:  // HcPeriodicStart
  case 0x44:  // HcLSThreshold
  case 0x48:  // HcRhDescriptorA
  case 0x4c:  // HcRhDescriptorB
  case 0x50:  // HcRhStatus
  case 0x54:  // HcRhPortStatus1
  case 0x58:  // HcRhPortStatus1
  case 0x5c:  // HcRhPortStatus1
  case 0x100: // HceControlRegister
  case 0x104: // HceInputRegister
  case 0x108: // HceOutputRegister
  case 0x10c: // HceStatusRegister
    state.usb_data[address / 4] = data;
    break;

  default:
    printf("%%USB-W-HCIWRITE: Writing to unknown address %x.  Ignoring.\n",
           (int)address);
  }
}

static u32 usb_magic1 = 0x9000432B;
static u32 usb_magic2 = 0xB2340009;

/**
 * Save state to a Virtual Machine State file.
 **/
int CAliM1543C_usb::SaveState(FILE *f) {
  long ss = sizeof(state);
  int res;

  if ((res = CPCIDevice::SaveState(f)))
    return res;

  fwrite(&usb_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&usb_magic2, sizeof(u32), 1, f);
  printf("%s: %d bytes saved.\n", devid_string, (int)ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CAliM1543C_usb::RestoreState(FILE *f) {
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

  if (m1 != usb_magic1) {
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

  if (m2 != usb_magic2) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  printf("%s: %d bytes restored.\n", devid_string, (int)ss);
  return 0;
}
