/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * WWW    : http://es40.org
 * E-mail : camiel@es40.org
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
 * Contains the code for the emulated Floppy Controller devices.
 *
 * $Id: FloppyController.cpp,v 1.16 2008/04/29 09:53:30 iamcamiel Exp $
 *
 * X-1.16       Camiel Vanderhoeven                             29-APR-2008
 *      Make floppy disk use CDisk images.
 *
 * X-1.15       Brian Wheeler                                   29-APR-2008
 *      Fixed floppy disk implementation.
 *
 * X-1.14       Brian Wheeler                                   29-APR-2008
 *      Floppy disk implementation.
 *
 * X-1.13       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.12       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.11       Camiel Vanderhoeven                             30-DEC-2007
 *      Print file id on initialization.
 *
 * X-1.10       Camiel Vanderhoeven                             11-DEC-2007
 *      Don't claim IO port 3f6 as this is in use by the IDE controller.
 *
 * X-1.9        Camiel Vanderhoeven                             10-DEC-2007
 *      Use configurator.
 *
 * X-1.8        Camiel Vanderhoeven                             31-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.7	Brian Wheeler					13-FEB-2007
 *	Formatting.
 *
 * X-1.6 	Camiel Vanderhoeven				12-FEB-2007
 *	Added comments.
 *
 * X-1.5        Camiel Vanderhoeven                             9-FEB-2007
 *      Added comments.
 *
 * X-1.4        Brian Wheeler                                   3-FEB-2007
 *      Formatting.
 *
 * X-1.3        Brian Wheeler                                   3-FEB-2007
 *      64-bit literals made compatible with Linux/GCC/glibc.
 *
 * X-1.2        Brian Wheeler                                   3-FEB-2007
 *      Includes are now case-correct (necessary on Linux)
 *
 * X-1.1        Camiel Vanderhoeven                             19-JAN-2007
 *      Initial version in CVS.
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#include "FloppyController.h"
#include "DMA.h"
#include "Disk.h"
#include "StdAfx.h"
#include "System.h"

/**
 * Constructor.
 **/
CFloppyController::CFloppyController(CConfigurator *cfg, CSystem *c, int id)
    : CSystemComponent(cfg, c), CDiskController(1, 2) {
  c->RegisterMemory(this, 1536, U64(0x00000801fc0003f0) - (0x80 * id), 6);
  c->RegisterMemory(this, 1537, U64(0x00000801fc0003f7) - (0x80 * id), 1);

  state.cmd_parms_ptr = 0;
  state.cmd_res_ptr = 0;
  state.status.rqm = 1;
  state.status.dio = 0;

  printf("%s: $Id: FloppyController.cpp,v 1.16 2008/04/29 09:53:30 iamcamiel "
         "Exp $\n",
         devid_string);
}

/**
 * Destructor.
 **/
CFloppyController::~CFloppyController() {}

const char *datarate_name[] = {"500 Kb/S MFM", "300 Kb/S MFM", "250 Kb/S MFM",
                               "1 Mb/S MFM"};

struct cmdinfo_t {
  u8 command;
  u8 parms;
  u8 returns;
  const char *name;
} cmdinfo[] = {
    {0, 0, 0, NULL},
    {0, 0, 0, NULL},
    {2, 9, 7, "Read Track"},
    {3, 3, 0, "Specify"},
    {4, 2, 1, "Sense Drive Status"},
    {5, 9, 7, "Write Data"},
    {6, 9, 7, "Read Data"},
    {7, 2, 0, "Recalibrate"},
    {8, 1, 2, "Sense Interrupt Status"},
    {9, 9, 7, "Write Deleted Data"},
    {10, 2, 7, "Read ID"},
    {11, 0, 0, NULL},
    {12, 9, 7, "Read Deleted"},
    {13, 6, 7, "Format Track"},
    {14, 1, 10, "DumpReg"},
    {15, 3, 0, "Seek"},
    {16, 1, 1, "Version"},
    {17, 9, 7, "Scan Equal"},
    {18, 2, 0, "Perpendicular Mode"},
    {19, 4, 0, "Configure"},
    {20, 1, 1, "Lock"},
    {21, 0, 0, NULL},
    {22, 9, 7, "Verify"},
    {23, 0, 0, NULL},
    {24, 0, 0, NULL},
    {25, 9, 7, "Scan Low or Equal"},
    {26, 0, 0, NULL},
    {27, 0, 0, NULL},
    {28, 0, 0, NULL},
    {29, 9, 7, "Scan High or Equal"},
    {30, 0, 0, NULL},
    {31, 0, 0, NULL},
};

void CFloppyController::WriteMem(int index, u64 address, int dsize, u64 data) {
  if (index == 1537)
    address += 7;

  // printf("FDC: Write port %d, value: %x\n", address, data);

  switch (address) {
  case FDC_REG_STATUS_A:
  case FDC_REG_STATUS_B:
    printf("FDC: Read only register %" PRId64 " written.\n", address);
    break;

  case FDC_REG_DOR:
    // bit 4 = drive 0 motor, bit 5 = drive 1 motor
    // bit 3 = dma enable (ps/2 reserved?)
    // bit 2 = 1: fdc enable (reset), 0: hold at reset
    // bits 1-0:  drive select 0: a, 1: b, I assume 2 & 3 are reserved.

    state.drive[0].motor = (data & 0x10) >> 4;
    state.drive[1].motor = (data & 0x20) >> 5;
    state.dma = (data & 0x08) >> 3;
    state.drive_select = data & 0x03;

    printf("FDC:  motor a: %s, motor b: %s, dma: %s, drive: %s\n",
           state.drive[0].motor ? "on" : "off",
           state.drive[1].motor ? "on" : "off", state.dma ? "on" : "off",
           state.drive_select == 0 ? "A" : "B");

    break;

  case FDC_REG_TAPE:
    printf("FDC: Tape register written with %" PRIx64 "\n", data);
    break;

  case FDC_REG_STATUS: // write = data rate selector
    // bit 7 = software reset (self clearing)
    // bit 6 = power down
    // bit 5 = reserved (0)
    // bit 4-2 = write precomp (000 = default)
    // bit 1-0 = data rate select

    state.datarate = data & 0x03;
    state.write_precomp = (data & 0x1c) >> 2;
    printf("FDC: data rate %s, precomp: %d\n", datarate_name[state.datarate],
           state.write_precomp);

    break;

  case FDC_REG_COMMAND:
    // the excitement happens here.
    if (state.status.dio) {
      printf("Unrequested data byte to command port.  Throwing away.\n");
      break;
    } else {
      state.cmd_parms[state.cmd_parms_ptr++] = data;
      int cmd = state.cmd_parms[0] & 0x1F;
      state.cmd_res_max = cmdinfo[cmd].returns;
      // printf("FDC: parm_ptr: %d, parms: %d\n", state.cmd_parms_ptr,
      // cmdinfo[cmd].parms);
      if (state.cmd_parms_ptr == cmdinfo[cmd].parms) {
        printf("FDC: command %s(", cmdinfo[cmd].name);
        for (int i = 1; i < state.cmd_parms_ptr; i++) {
          printf("%x ", state.cmd_parms[i]);
        }
        printf(")\n");

        state.cmd_res_max = cmdinfo[cmd].returns;
        state.cmd_res_ptr = 0;
        state.status.rqm = 0;
        switch (cmd) {
        case 3: // specify
          // set up some hardware parameters.  We really don't care about
          // the times (step rate time, head unload time, head load time}, but
          // we may care about the ND bit (parm byte 3, bit 1)...
          state.dma = ~(state.cmd_parms[2] & 0x01);
          break;

        case 6: // read data
          // args:
          // 0: bit 7 = MT (multitrack), 6 = MFM, 5 = SK (skip flag)
          // 1: bit 2 = HDS (head), 1 = DS1, 0 = DS0
          // 2: C = cyl
          // 3: H = head address
          // 4: R = sector
          // 5: N = sector size, 2 = 512b
          // 6: EOT = end of track 0x24 = 36 sectors (18 * 2)
          // 7: GPL = gap length
          // 8: DTL = sector size (if N = 0)
          {
            int count = theDMA->get_count(2);
            void *buffer = malloc(count + 1);
            int pos = (state.cmd_parms[2] * state.cmd_parms[6])         // cyls
                      + (state.cmd_parms[3] * (state.cmd_parms[6] / 2)) // head
                      + state.cmd_parms[4] - 1; // sector (sectors start at 1)
            SEL_FDISK->seek_byte(pos * 512);
            SEL_FDISK->read_bytes(buffer, count);

            printf("FDC: read data:  %x @ %x\n  ", count, pos * 512);
            for (int i = 0; i < count; i++) {
              printf("%02x ", *((char *)buffer + i) & 0xff);
              if (i % 16 == 15)
                printf("\n  ");
            }
            printf("\n");

            theDMA->send_data(2, buffer);

            state.cmd_parms[4]++;
            if (state.cmd_parms[4] > (state.cmd_parms[6] / 2)) {
              state.cmd_parms[4] = 1;
              state.cmd_parms[3]++;
              if (state.cmd_parms[3] > 1) {
                state.cmd_parms[3] = 0;
                state.cmd_parms[2]++;
              }
            }

            state.cmd_res[0] = (state.cmd_parms[1] & 0x03) | ST0_SE | ST0_INTR;
            state.cmd_res[1] = 0;
            state.cmd_res[2] = 0;
            state.cmd_res[3] = state.cmd_parms[2];
            state.cmd_res[4] = state.cmd_parms[3];
            state.cmd_res[5] = state.cmd_parms[4];
            state.cmd_res[6] = state.cmd_parms[5];
            SEL_DRIVE.seeking = 1;

            do_interrupt();
          }
          break;

        case 7:                  // recalibrate
          SEL_DRIVE.seeking = 3; // wait for 3 status reads to finish seek.
          SEL_DRIVE.cylinder = 0;
          do_interrupt();
          break;

        case 8: // sense interrupt status
          if (!state.interrupt)
            state.cmd_res[0] = 0x80;
          else
            state.cmd_res[0] = 0x00; // ?

          state.cmd_res[1] = SEL_DRIVE.cylinder; // present cylinder number
          break;

        case 15: // seek
          // args:
          // 0: opcode
          // 1: bit 2 = HDS (head), 1 = DS1, 0 = DS0
          // 2: NCN = new cylinder number
          SEL_DRIVE.seeking = 3; // wait 3 status reads to finish seek.
          SEL_DRIVE.cylinder = state.cmd_parms[2];
          do_interrupt();
          break;

        case 18: // perpendicular mode
          // We really don't care, somehow
          break;

        case 19: // configure
          // we're software, we don't care (I think)
          break;

        default:
          printf("Unhandled floppy command: %d = %s\n", cmd, cmdinfo[cmd].name);
          exit(1);
        }

        state.status.rqm = 1;
        if (cmdinfo[cmd].returns > 0) {
          state.status.dio = 1;
        }
        state.cmd_parms_ptr = 0;
      } else {
        // printf("FDC: command parameter byte %d = %x, expecting %d bytes for
        // %s\n", state.cmd_parms_ptr-1, data, cmdinfo[state.cmd_parms[0] &
        // 0x1f].parms, cmdinfo[state.cmd_parms[0] &0x1f].name);
      }
    }

    break;

  case FDC_REG_DIR:
    // PC/AT, PS/2
    //    bits 7-2 = reserved
    //    bit 0-1 = MFM data rate
    state.datarate = data & 0x03;
    printf("FDC: data rate %s\n", datarate_name[state.datarate]);

    break;
  }
}

u64 CFloppyController::ReadMem(int index, u64 address, int dsize) {
  u64 data = 0;

  if (index == 1537)
    address += 7;

  switch (address) {
  case FDC_REG_STATUS_A:
    // bit 7 = interrupt pending
    // bit 6 = -DRV2  (second drive installed)
    // bit 5 = step
    // bit 4 = -track0
    // bit 3 = head1 select
    // bit 2 = -index
    // bit 1 = -write protect
    // bit 0 = +direction

    break;

  case FDC_REG_STATUS_B:
    // bit 7-6 reserved (1)
    // bit 5 = drive select
    // bit 4 = write data
    // bit 3 = read data
    // bit 2 = write enable
    // bit 1 = motor 1 enable
    // bit 0 = motor 0 enable

    break;

  case FDC_REG_DOR:
  case FDC_REG_TAPE:
    printf("FDC: Write only register %" PRId64 " read.", address);
    break;

  case FDC_REG_STATUS:
    data = get_status();
    break;

  case FDC_REG_COMMAND:
    // The data comes back from here.
    data = state.cmd_res[state.cmd_res_ptr++];
    if (state.cmd_res_ptr >= state.cmd_res_max) {
      state.status.rqm = 1;
      state.status.dio = 0;
    }

    break;

  case FDC_REG_DIR:
    // PS/2 mode:
    //    bit 7 = diskette change
    //    bits 6-3 = 1
    //    bit 2 = datarate select 1
    //    bit 1 = datarate select 0
    //    bit 0 = high density select

    break;
  }

  printf("FDC: Read register %" PRId64 ", value: %" PRIx64 "\n", address, data);

  return data;
}

static u32 fdc_magic1 = 0x0fdc0fdc;
static u32 fdc_magic2 = 0xfdc0fdc0;

int CFloppyController::SaveState(FILE *f) {
  long ss = sizeof(state);

  fwrite(&fdc_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&fdc_magic2, sizeof(u32), 1, f);
  printf("fdc: %ld bytes saved.\n", ss);
  return 0;
}

int CFloppyController::RestoreState(FILE *f) {
  long ss;
  u32 m1;
  u32 m2;
  size_t r;

  r = fread(&m1, sizeof(u32), 1, f);
  if (r != 1) {
    printf("fdc: unexpected end of file!\n");
    return -1;
  }

  if (m1 != fdc_magic1) {
    printf("fdc: MAGIC 1 does not match!\n");
    return -1;
  }

  r = fread(&ss, sizeof(long), 1, f);
  if (r != 1) {
    printf("fdc: unexpected end of file!\n");
    return -1;
  }

  if (ss != sizeof(state)) {
    printf("fdc: STRUCT SIZE does not match!\n");
    return -1;
  }

  r = fread(&state, sizeof(state), 1, f);
  if (r != 1) {
    printf("fdc: unexpected end of file!\n");
    return -1;
  }

  r = fread(&m2, sizeof(u32), 1, f);
  if (r != 1) {
    printf("fdc: unexpected end of file!\n");
    return -1;
  }

  if (m2 != fdc_magic2) {
    printf("fdc: MAGIC 1 does not match!\n");
    return -1;
  }

  printf("fdc: %ld bytes restored.\n", ss);
  return 0;
}

void CFloppyController::do_interrupt() {
  // *shrug* I'll figure this out later.
  state.interrupt = true;
}

u8 CFloppyController::get_status() {
  // bit 7 = RQM data register is ready (0: no access is permitted)
  // bit 6 = 1: transfer from controller to system, 0: sys to controller
  // bit 5 = non dma mode
  // bit 4 = diskette controller is busy
  // bit 3-2 reserved
  // bit 1 = drive 1 is busy (seeking)
  // bit 0 = drive 0 is busy (seeking)

  for (int i = 0; i < 2; i++) {
    if (state.drive[i].seeking > 0)
      state.drive[i].seeking--;
    if (state.drive[i].seeking == 0)
      state.status.seeking[i] = false;
    else
      state.status.seeking[i] = true;
  }

  // we mark the controller busy if a disk is seeking or
  // if there is data waiting to be sent by the controller.
  if (state.status.seeking[0] || state.status.seeking[1] ||
      (state.status.dio && state.status.rqm))
    state.status.busy = true;
  else
    state.status.busy = false;

  printf("FDC Status: %s, %s, %s, %s, %s, %s\n",
         state.status.rqm ? "Data Register Ready" : "No Access",
         state.status.dio ? "C->S" : "S->C",
         state.status.nondma ? "No DMA" : "DMA",
         state.status.busy ? "BUSY" : "not busy",
         state.status.seeking[0] ? "Disk 1 Seeking" : "Disk 1 Idle",
         state.status.seeking[1] ? "Disk 0 Seeking" : "Disk 0 Idle");

  u8 data =
      (state.status.rqm ? 0x80 : 0x00) | (state.status.dio ? 0x40 : 0x00) |
      (state.status.nondma ? 0x20 : 0x00) | (state.status.busy ? 0x10 : 0x00) |
      (state.status.seeking[1] ? 0x02 : 0x00) |
      (state.status.seeking[0] ? 0x01 : 0x00);
  return data;
}
