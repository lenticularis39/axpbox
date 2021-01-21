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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Although this is not required, the author would appreciate being
 * notified of, and receiving any modifications you may make to the
 * source code that might serve the general public.
 */

/**
 * \file
 * Contains the code for the emulated Ali M1543C IDE chipset part.
 *
 * $Id: AliM1543C_ide.cpp,v 1.34 2008/05/31 15:47:08 iamcamiel Exp $
 *
 * X-1.34       Camiel Vanderhoeven                             31-MAY-2008
 *      Changes to include parts of Poco.
 *
 * X-1.33       Camiel Vanderhoeven                             29-APR-2008
 *      CDiskController is no longer a CPCIDevice. devices that are both
 *      should multiple inherit both.
 *
 * X-1.32       Brian Wheeler                                   04-APR-2008
 *      Fixed missing variable declaration.
 *
 * X-1.31       Camiel Vanderhoeven                             02-APR-2008
 *      Fixed compiler warnings.
 *
 * X-1.30       Brian Wheeler                                   20-MAR-2008
 *   1. Improved locking by a) Removing all of the general register
 *      locking; b) Busmaster locking is still in place, but it might not
 *      be needed, this locking is pretty fine grained so nothing should
 *      time out waiting for it; c) Creating an alt_status variable which
 *      gets updated when the real status becomes stable (i.e. at the end
 *      of the execute() run, after the drq status is changed, etc),
 *      access to this variable is locked; d) Everything else is a free
 *      for all.
 *   3. Implement an optional delayed interrupt. The OSes still lose
 *      interrupts sometimes.
 *
 * X-1.29       Brian Wheeler                                   17-MAR-2008
 *      Fix some CD-ROM issues.
 *
 * X-1.28       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.27       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.26       Camiel Vanderhoeven                             13-MAR-2008
 *      Create init(), start_threads() and stop_threads() functions.
 *
 * X-1.25       Brian Wheeler                                   12-MAR-2008
 *      Better DMA support.
 *
 * X-1.24       Camiel Vanderhoeven                             11-MAR-2008
 *      Named, debuggable mutexes.
 *
 * X-1.23       Brian Wheeler                                   11-MAR-2008
 *      Even nicer, more efficient multi-threading version.
 *
 * X-1.22       Brian Wheeler                                   05-MAR-2008
 *      Nicer, more efficient multi-threading version.
 *
 * X-1.21       Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.20       Camiel Vanderhoeven                             04-MAR-2008
 *      Merged Brian wheeler's New IDE code into the standard controller.
 *
 * X-1.19.17    Brian Wheeler                                   27-FEB-2008
 *     Re-fire interrupts less often.
 *
 * X-1.19.16    Brian Wheeler                                   27-FEB-2008
 *     Improvement to last fix.
 *
 * X-1.19.15    Brian Wheeler                                   27-FEB-2008
 *     This patch fixes the vms boot problems from ide cdrom and it also
 *     allowed me to install tru64 -- albeit with timeouts:
 *   a) Clears the busmaster active bit when the bit 1 is written to the
 *      busmaster status register.
 *   b) Attempts to refire the interrupt if the controller seems to have
 *      missed it -- before the OS declares a timeout.
 *
 * X-1.19.14    Brian Wheeler                                   27-FEB-2008
 *      Avoid compiler warnings.
 *
 * X-1.19.13    Brian Wheeler                                   29-JAN-2008
 *      Avoid firing interrupts that occurred while interrupts were
 *      disabled.
 *
 * X-1.19.12    Camiel Vanderhoeven                             28-JAN-2008
 *      Avoid compiler warnings.
 *
 * X-1.19.11    Brian Wheeler                                   26-JAN-2008
 *      Don't repeat interrupt too soon.
 *
 * X-1.19.10    Camiel Vanderhoeven                             24-JAN-2008
 *      Use new CPCIDevice::do_pci_read and CPCIDevice::do_pci_write.
 *
 * X-1.19.9     Brian Wheeler                                   16-JAN-2008
 *      Less timeouts.
 *
 * X-1.19.8     Brian Wheeler                                   14-JAN-2008
 *      Less messages without debugging enabled.
 *
 * X-1.19.7     Fang Zhe                                        13-JAN-2008
 *      Big-endian support.
 *
 * X-1.19.6     Camiel Vanderhoeven                             12-JAN-2008
 *      Use disk's SCSI engine for ATAPI devices.
 *
 * X-1.19.5      Brian Wheeler                                   10-JAN-2008
 *   - Stop dividing get_lba_size() by 4 in ATAPI Get Capacity.  SRM can
 *     now boot cdrom images correctly, and OSes can use cdroms effectively...
 *     at least until they call an unimplemented SCSI command.
 *
 * X-1.19.4      Brian Wheeler                                   09-JAN-2008
 *   - During init, command_in_progress and command_cycle are cleared.
 *   - Writes to ide_control are processed correctly.  Tru64 & FreeBSD
 *     recognize disks now.
 *   - interrupt is deasserted when command register is written.
 *   - Removed a bunch of debugging sections.
 *   - Made it quieter if DEBUG_IDE wasn't defined -- users should no longer
 *     see Debug Pause messages.
 *   - busy is asserted if we're currently processing a packet command when
 *     drq is deasserted (i.e. when the read buffer is empty).  This lets us
 *     get back into the ATAPI state machine before the host can start
 *     messing  with the controller.
 *   - Removed pauses for port 0x3n7 -- its really a floppy port.
 *   - Remove pause when reset is being started.
 *   - packet dma flag is presented when get_status() is run.
 *   - ATAPI command 0x1e (media lock) implemented as no-op.
 *   - ATAPI command 0x43 (read TOC) implemented as a hack.
 *   - ATAPI read now uses get_block_size() for determining transfers.
 *   - ATAPI state machine goes from DP34 to DI immediately upon completion
 *     instead of redirecting through DP2.
 *   - Added ATA Command 0xc6 (Set Multiple).
 *   .
 *
 * X-1.19.3      Brian wheeler                                   08-JAN-2008
 *      ATAPI improved.
 *
 * X-1.19.2      Brian wheeler                                   08-JAN-2008
 *      Handle blocksize correctly for ATAPI.
 *
 * X-1.19.1     Brian wheeler                                   08-JAN-2008
 *      Complete rewrite of IDE controller.
 *
 * X-1.19       Fang Zhe                                        08-JAN-2008
 *      Endianess.
 *
 * X-1.18       Camiel Vanderhoeven                             06-JAN-2008
 *      Leave changing the blocksize to the disk itself.
 *
 * X-1.17       Camiel Vanderhoeven                             04-JAN-2008
 *      Less messages fprint'ed.
 *
 * X-1.16       Camiel Vanderhoeven                             02-JAN-2008
 *      Avoid compiler warnings.
 *
 * X-1.15       Camiel Vanderhoeven                             30-DEC-2007
 *      Print file id on initialization.
 *
 * X-1.14       Camiel Vanderhoeven                             29-DEC-2007
 *      Compileable with older compilers (VC 6.0). Avoid referencing
 *      uninitialized data.
 *
 * X-1.13       Camiel Vanderhoeven                             28-DEC-2007
 *      Throw exceptions rather than just exiting when errors occur.
 *
 * X-1.12       Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.11       Camiel Vanderhoeven                             28-DEC-2007
 *      Only delay IDE interrupts when NO_VMS is defined. (Need to fix this
 *                properly).
 *
 * X-1.10        Camiel Vanderhoeven                             20-DEC-2007
 *      More checks if disk exists.
 *
 * X-1.9         Brian wheeler                                   19-DEC-2007
 *      Added basic ATAPI support.
 *
 * X-1.8         Brian wheeler                                   17-DEC-2007
 *      Delayed IDE interrupts. (NetBSD requirement)
 *
 * X-1.7        Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.6        Camiel Vanderhoeven                             14-DEC-2007
 *      Commented out printing each IDE command.
 *
 * X-1.5        Camiel Vanderhoeven                             12-DEC-2007
 *      Use disk controller base class.
 *
 * X-1.4        Camiel Vanderhoeven                             11-DEC-2007
 *      Removed last references to ide_command[][].
 *
 * X-1.3        Camiel Vanderhoeven                             11-DEC-2007
 *      Cleanup.
 *
 * X-1.2        Camiel Vanderhoeven                             11-DEC-2007
 *      More complete IDE implementation allows NetBSD to recognize disks.
 *
 * X-1.1        Camiel Vanderhoeven                             10-DEC-2007
 *      Initial version in CVS; this part was split off from the CAliM1543C
 *      class.
 **/
#ifdef DEBUG_IDE_LOCKS
#define DEBUG_LOCKS
#endif
#include "AliM1543C_ide.hpp"
#include "StdAfx.hpp"
#include "System.hpp"

#include <cmath>

#include "gui/keymap.hpp"
#include "gui/scancodes.hpp"

#include "AliM1543C.hpp"
#include "Disk.hpp"

#define PAUSE(msg)                                                             \
  do {                                                                         \
    printf("Debug Pause: ");                                                   \
    printf(msg);                                                               \
    getc(stdin);                                                               \
  } while (0);

u32 AliM1543C_ide_cfg_data[64] = {
    /*00*/ 0x522910b9, // CFID: vendor + device
    /*04*/ 0x02800000, // CFCS: command + status
    /*08*/ 0x0101fac1, // CFRV: class + revision
    /*0c*/ 0x00000000, // CFLT: latency timer + cache line size
    /*10*/ 0x000001f1, // BAR0:
    /*14*/ 0x000003f5, // BAR1:
    /*18*/ 0x00000171, // BAR2:
    /*1c*/ 0x00000375, // BAR3:
    /*20*/ 0x0000f001, // BAR4:
    /*24*/ 0x00000000, // BAR5:
    /*28*/ 0x00000000, // CCIC: CardBus
    /*2c*/ 0x00000000, // CSID: subsystem + vendor
    /*30*/ 0x00000000, // BAR6: expansion rom base
    /*34*/ 0x00000000, // CCAP: capabilities pointer
    /*38*/ 0x00000000,
    /*3c*/ 0x040201ff, // CFIT: interrupt configuration
    0,
    0,
    /*48*/ 0x4a000000, // UDMA test
    /*4c*/ 0x1aba0000, // reserved
    0,
    /*54*/ 0x44445555, // udma setting + fifo treshold
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /*78*/ 0x00000021, // ide clock
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

u32 AliM1543C_ide_cfg_mask[64] = {
    /*00*/ 0x00000000, // CFID: vendor + device
    /*04*/ 0x00000105, // CFCS: command + status
    /*08*/ 0x00000000, // CFRV: class + revision
    /*0c*/ 0x0000ffff, // CFLT: latency timer + cache line size
    /*10*/ 0xfffffff8, // BAR0
    /*14*/ 0xfffffffc, // BAR1: CBMA
    /*18*/ 0xfffffff8, // BAR2:
    /*1c*/ 0xfffffffc, // BAR3:
    /*20*/ 0xfffffff0, // BAR4:
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

/**
 * Constructor.
 **/
CAliM1543C_ide::CAliM1543C_ide(CConfigurator *cfg, CSystem *c, int pcibus,
                               int pcidev)
    : CPCIDevice(cfg, c, pcibus, pcidev), CDiskController(2, 2) {
  if (theIDE != 0)
    FAILURE(Configuration, "More than one IDE controller");
  theIDE = this;

  // create scsi busses
  CSCSIBus *a = new CSCSIBus(cfg, c);
  CSCSIBus *b = new CSCSIBus(cfg, c);
  scsi_register(0, a, 7); // scsi id 7 by default
  scsi_register(1, b, 7); // scsi id 7 by default
}

/**
 * Initialize the IDE device.
 **/
void CAliM1543C_ide::init() {
  add_function(0, AliM1543C_ide_cfg_data, AliM1543C_ide_cfg_mask);

  add_legacy_io(PRI_COMMAND, 0x1f0, 8);
  add_legacy_io(PRI_CONTROL, 0x3f6, 2);
  add_legacy_io(SEC_COMMAND, 0x170, 8);
  add_legacy_io(SEC_CONTROL, 0x376, 2);
  add_legacy_io(PRI_BUSMASTER, 0xf000, 8);
  add_legacy_io(SEC_BUSMASTER, 0xf008, 8);

  usedma = myCfg->get_bool_value("dma", true);
  if (!usedma) {
    printf("IDE: DMA Transfers turned off.\n");
  }

  ResetPCI();

  // start controller threads
  StopThread = false;
  mtRegisters[0] = new CRWLock("ide0-registers");
  mtRegisters[1] = new CRWLock("ide1-registers");
  mtBusMaster[0] = new CRWLock("ide0-busmaster");
  mtBusMaster[1] = new CRWLock("ide1-busmaster");

  for (int i = 0; i < 2; i++) {
    semController[i] = new CSemaphore(0, 1);      // disk controller
    semControllerReady[i] = new CSemaphore(0, 1); // disk controller ready
    semBusMaster[i] = new CSemaphore(0, 1);       // bus master
    semBusMasterReady[i] = new CSemaphore(0, 1);  // bus master ready
    semControllerReady[i]->set();
    semBusMasterReady[i]->set();
    thrController[i] = 0;
  }

  printf("%%IDE-I-INIT: New IDE emulator initialized.\n");
}

void CAliM1543C_ide::start_threads() {
  char buffer[5];
  for (int i = 0; i < 2; i++) {
    if (!thrController[i]) {
      sprintf(buffer, "ide%d", i);
      thrController[i] =
          std::make_unique<std::thread>([this, i]() { this->run(i); });
      printf(" %s", buffer);
      StopThread = false;
    }
  }
}

void CAliM1543C_ide::stop_threads() {
  char buffer[5];
  StopThread = true;
  for (int i = 0; i < 2; i++) {
    if (thrController[i]) {
      sprintf(buffer, "ide%d", i);
      printf(" %s", buffer);
      semController[i]->set();
      thrController[i]->join();
      thrController[i] = nullptr;
    }
  }
}

CAliM1543C_ide::~CAliM1543C_ide() { stop_threads(); }

void CAliM1543C_ide::ResetPCI() {
  int i;

  int j;
  CPCIDevice::ResetPCI();

  for (i = 0; i < 2; i++) {
    CONTROLLER(i).bm_status = 0;
    CONTROLLER(i).selected = 0;
    for (j = 0; j < 2; j++) {
      REGISTERS(i, j).error = 0;
      COMMAND(i, j).command_in_progress = 0;
      COMMAND(i, j).command_cycle = 0;
      STATUS(i, j).busy = false;
      STATUS(i, j).drive_ready = false;
      STATUS(i, j).drq = false;
      STATUS(i, j).err = false;
      STATUS(i, j).index_pulse = false;
      STATUS(i, j).index_pulse_count = 0;
      STATUS(i, j).seek_complete = false;
      PER_DRIVE(i, j).multiple_size = 1;
      set_signature(i, j);
    }
  }
}

void CAliM1543C_ide::register_disk(class CDisk *dsk, int bus, int dev) {
  CDiskController::register_disk(dsk, bus, dev);
  if (dsk->cdrom()) {
    dsk->scsi_register(0, scsi_bus[bus], dev);
    dsk->set_atapi_mode();
  }
}

static u32 ide_magic1 = 0xB222654D;
static u32 ide_magic2 = 0xD456222C;

/**
 * Save state to a Virtual Machine State file.
 **/
int CAliM1543C_ide::SaveState(FILE *f) {
  long ss = sizeof(state);
  int res;

  if ((res = CPCIDevice::SaveState(f)))
    return res;

  fwrite(&ide_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&ide_magic2, sizeof(u32), 1, f);
  printf("%s: %d bytes saved.\n", devid_string, (int)ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CAliM1543C_ide::RestoreState(FILE *f) {
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

  if (m1 != ide_magic1) {
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

  if (m2 != ide_magic2) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  printf("%s: %d bytes restored.\n", devid_string, (int)ss);
  return 0;
}

/*
 * Region read/write redirection
 */
u32 CAliM1543C_ide::ReadMem_Legacy(int index, u32 address, int dsize) {
  int channel = 0;
  switch (index) {
  case SEC_COMMAND:
    channel = 1;

  case PRI_COMMAND:
    return ide_command_read(channel, address, dsize);

  case SEC_CONTROL:
    channel = 1;

  case PRI_CONTROL:
    return ide_control_read(channel, address);

  case SEC_BUSMASTER:
    channel = 1;

  case PRI_BUSMASTER:
    return ide_busmaster_read(channel, address, dsize);
  }

  return 0;
}

void CAliM1543C_ide::WriteMem_Legacy(int index, u32 address, int dsize,
                                     u32 data) {
  int channel = 0;
  switch (index) {
  case SEC_COMMAND:
    channel = 1;

  case PRI_COMMAND:
    ide_command_write(channel, address, dsize, data);
    break;

  case SEC_CONTROL:
    channel = 1;

  case PRI_CONTROL:
    ide_control_write(channel, address, data);
    break;

  case SEC_BUSMASTER:
    channel = 1;

  case PRI_BUSMASTER:
    ide_busmaster_write(channel, address, dsize, data);
    break;
  }
}

u32 CAliM1543C_ide::ReadMem_Bar(int func, int bar, u32 address, int dsize) {
  int channel = 0;
  switch (bar) {
  case BAR_SEC_COMMAND:
    channel = 1;

  case BAR_PRI_COMMAND:
    return ide_command_read(channel, address, dsize);

  case BAR_SEC_CONTROL:
    channel = 1;

  case BAR_PRI_CONTROL:

    // we have to offset by two because the BAR starts at 3f4 vs 3f6
    return ide_control_read(channel, address - 2);

  case BAR_BUSMASTER:
    if (address < 8)
      return ide_busmaster_read(0, address, dsize);
    else
      return ide_busmaster_read(1, address - 8, dsize);
  }

  return 0;
}

void CAliM1543C_ide::WriteMem_Bar(int func, int bar, u32 address, int dsize,
                                  u32 data) {
  int channel = 0;
  switch (bar) {
  case BAR_SEC_COMMAND:
    channel = 1;

  case BAR_PRI_COMMAND:
    ide_command_write(channel, address, dsize, data);
    return;

  case BAR_SEC_CONTROL:
    channel = 1;

  case BAR_PRI_CONTROL:
    // we have to offset by two because the BAR starts at 3f4 vs 3f6
    ide_control_write(channel, address - 2, data);
    return;

  case BAR_BUSMASTER:
    if (address < 8)
      return ide_busmaster_write(0, address, data, dsize);
    else
      return ide_busmaster_write(1, address - 8, data, dsize);
  }
}

/*
 * Register read/write handlers
 */
u32 CAliM1543C_ide::ide_command_read(int index, u32 address, int dsize) {
  u32 data = 0;
  if (!get_disk(index, 0) && !get_disk(index, 1)) {

    // no disks are present, so the data lines actually float to
    // a high state, which is logical 1.
    return 0xffffffff;
  }

  switch (address) {
  case REG_COMMAND_DATA:
    if (!SEL_STATUS(index).drq) {
#ifdef DEBUG_IDE_REG_COMMAND
      printf("Reading from data buffer when data is not ready.\n");
      ide_status(index);
      PAUSE("WTF");
#endif
      break;
    }

    data = 0;

    switch (dsize) {
    case 32:
      data = CONTROLLER(index).data[CONTROLLER(index).data_ptr++];
      data |= CONTROLLER(index).data[CONTROLLER(index).data_ptr++] << 16;
      break;

    case 16:
      data = CONTROLLER(index).data[CONTROLLER(index).data_ptr++];
    }

    if (CONTROLLER(index).data_ptr >= CONTROLLER(index).data_size) {

      // there's no more to take.
      SEL_STATUS(index).drq = false;
      if (SEL_COMMAND(index).command_in_progress) {
        SEL_STATUS(index).busy = true;
        SEL_STATUS(index).drive_ready = false;
        UPDATE_ALT_STATUS(index);
        semControllerReady[index]->wait();
        semController[index]->set(); // wake up the controller.
#if defined(DEBUG_IDE_MULTIPLE) || defined(DEBUG_IDE_PACKET)
        printf("Command still in progress, waking up controller.\n");
        printf("-- Packet Phase: %d\n", SEL_COMMAND(index).packet_phase);
        ide_status(index);
#endif
      }
    }

    if (CONTROLLER(index).data_ptr > IDE_BUFFER_SIZE) {
      printf("%%IDE-W-OVERFLOW: data pointer past end of buffer,  setting to "
             "0.\n");
      CONTROLLER(index).data_ptr = 0;
      SEL_STATUS(index).drq = false;
      UPDATE_ALT_STATUS(index);
    }
    break;

  case REG_COMMAND_ERROR:
    data = SEL_REGISTERS(index).error;
    break;

  case REG_COMMAND_SECTOR_COUNT:
    data = SEL_REGISTERS(index).sector_count;
    break;

  case REG_COMMAND_SECTOR_NO:
    data = SEL_REGISTERS(index).sector_no;
    break;

  case REG_COMMAND_CYL_LOW:
    data = SEL_REGISTERS(index).cylinder_no & 0xff;
    break;

  case REG_COMMAND_CYL_HI:
    data = (SEL_REGISTERS(index).cylinder_no >> 8) & 0xff;
    break;

  case REG_COMMAND_DRIVE:
    data = 0x80 | (SEL_REGISTERS(index).lba_mode ? 0x40 : 0x00) |
           0x20 // 512 byte sector size
           | (CONTROLLER(index).selected ? 0x10 : 0x00) |
           (SEL_REGISTERS(index).head_no & 0x0f);
    break;

  case REG_COMMAND_STATUS:

    // get the status and clear the interrupt.
    data = get_status(index);
    theAli->pic_deassert(1, 6 + index);
#ifdef DEBUG_IDE_INTERRUPT
    printf("%%IDE-I-INTERRUPT: Interrupt Acknowledged on %d.\n", index);
#endif
    break;
  }

#ifdef DEBUG_IDE_REG_COMMAND
  if (address != 0) {
    printf("%%IDE-I-REGCMD: Read from command register %d (%s) on controller "
           "%d, value: %x\n",
           address, register_names[address], index, data);
  }
#endif
  return data;
}

void CAliM1543C_ide::ide_command_write(int index, u32 address, int dsize,
                                       u32 data) {
#ifdef DEBUG_IDE_REG_COMMAND
  if (address != 0) {
    printf("%%IDE-I-REGCMD: Write to command register %d (%s) on controller "
           "%d, value: %x\n",
           address, register_names[address], index, data);
  }

  SEL_STATUS(index).debug_status_update = true;
#endif
  switch (address) {
  case REG_COMMAND_DATA:
    if (!SEL_STATUS(index).drq) {
#ifdef DEBUG_IDE_REG_COMMAND
      printf("%%IDE-I-DATA: Unrequested data written to data port: %x\n", data);
      ide_status(index);
#endif
      break;
    }

    switch (dsize) {
    case 32:
      CONTROLLER(index).data[CONTROLLER(index).data_ptr++] = data & 0xffff;
      CONTROLLER(index).data[CONTROLLER(index).data_ptr++] =
          (data >> 16) & 0xffff;
      break;

    case 16:
      CONTROLLER(index).data[CONTROLLER(index).data_ptr++] = data & 0xffff;
    }

    if (CONTROLLER(index).data_ptr >= CONTROLLER(index).data_size) {

      // we don't want any more.
      SEL_STATUS(index).drq = false;
      SEL_STATUS(index).busy = true;
      UPDATE_ALT_STATUS(index);
      semControllerReady[index]->wait();
      semController[index]->set(); // wake the controller up.
    }

    if (CONTROLLER(index).data_ptr > IDE_BUFFER_SIZE) {
      printf("%%IDE-W-OVERFLOW: data pointer overflow,  setting to 0.\n");
      CONTROLLER(index).data_ptr = 0;
      SEL_STATUS(index).drq = false;
      UPDATE_ALT_STATUS(index);
    }
    break;

  case REG_COMMAND_FEATURES:
    REGISTERS(index, 0).features = data;
    REGISTERS(index, 1).features = data;
    break;

  case REG_COMMAND_SECTOR_COUNT:
    REGISTERS(index, 0).sector_count = REGISTERS(index, 1).sector_count =
        data & 0xff;
    break;

  case REG_COMMAND_SECTOR_NO:
    REGISTERS(index, 0).sector_no = REGISTERS(index, 1).sector_no = data & 0xff;
    break;

  case REG_COMMAND_CYL_LOW:
    REGISTERS(index, 0).cylinder_no = REGISTERS(index, 1).cylinder_no =
        (REGISTERS(index, 1).cylinder_no & 0xff00) | (data & 0xff);
    break;

  case REG_COMMAND_CYL_HI:
    REGISTERS(index, 0).cylinder_no = REGISTERS(index, 1).cylinder_no =
        (REGISTERS(index, 1).cylinder_no & 0xff) | ((data << 8) & 0xff00);
    break;

  case REG_COMMAND_DRIVE:
    if (((data >> 4) & 1) != CONTROLLER(index).selected)
#ifdef DEBUG_IDE
      printf("Setting selected device on %d to: %d.  val=%02x\n", index,
             (data >> 4) & 1, data);
#endif
    CONTROLLER(index).selected = (data >> 4) & 1;
    REGISTERS(index, 0).head_no = REGISTERS(index, 1).head_no = data & 0x0f;
    REGISTERS(index, 0).lba_mode = REGISTERS(index, 1).lba_mode =
        (data >> 6) & 1;

    break;

  case REG_COMMAND_COMMAND:
    theAli->pic_deassert(1, 6 + index); // interrupt is cleared on write.
    if (!SEL_DISK(index)) {
#ifdef DEBUG_IDE
      printf("%%IDE-I-NODEV: Command to non-existing device %d.%d. cmd=%x\n",
             index, CONTROLLER(index).selected, data);
#endif
    }

    if (SEL_COMMAND(index).command_in_progress == true) {

      // we're already working, why is another command being issued?
#ifdef DEBUG_IDE
      printf("%%IDE-W-CIP: Command is already in progress.\n");
      PAUSE("dang it!");
#endif
    }

    if ((data & 0xf0) == 0x10)
      data = 0x10;

    SEL_COMMAND(index).command_in_progress = false;
    SEL_COMMAND(index).current_command = data;
#ifdef DEBUG_IDE_CMD
    printf("%%IDE-I-CMD: Command %02x issued on controller %d, disk %d.\n",
           data, index, CONTROLLER(index).selected);
#endif
    SEL_COMMAND(index).command_cycle = 0;
    SEL_STATUS(index).drq = false;
    UPDATE_ALT_STATUS(index);
    CONTROLLER(index).data_ptr = 0;

    if (data != 0x00) {
      SEL_STATUS(index).busy = true;
      UPDATE_ALT_STATUS(index);
      SEL_COMMAND(index).command_in_progress = true;
      SEL_COMMAND(index).packet_phase = PACKET_NONE;
      semControllerReady[index]->wait();
      semController[index]->set(); // wake up the controller.
    } else {

      // this is a nop, so we cancel everything that's pending and
      // pretend that this operation got done super fast!
      if (SEL_DISK(index))
        command_aborted(index, data);
    }
    break;
  }
}

u32 CAliM1543C_ide::ide_control_read(int index, u32 address) {
  u32 data = 0;
  switch (address) {
  case 0: {
    SCOPED_READ_LOCK(mtRegisters[index]);
    data = SEL_STATUS(index).alt_status;
  }
#ifdef DEBUG_IDE_REG_CONTROL
    static u32 last_data = 0;
    if (last_data != data) {
      printf("%%IDE-I-READCTRL: alternate status on IDE control %d: 0x%02x\n",
             index, data);
    }

    last_data = data;
#endif
    break;

  case 1:

    // 3x7h drive address register. (floppy?)
    data |= (CONTROLLER(index).selected == 0) ? 1 : 2;
    data |= (SEL_REGISTERS(index).head_no) << 2;
    data = (~data) & 0xff; // negate everything
#ifdef DEBUG_IDE_REG_CONTROL
    printf("%%IDE-I-READCTRL: drive address port on IDE control %d: 0x%02x\n",
           index, data);
#endif
    break;
  }

  return data;
}

/**
 * Write to the IDE controller control interface.
 **/
void CAliM1543C_ide::ide_control_write(int index, u32 address, u32 data) {
  bool prev_reset;
#ifdef DEBUG_IDE_REG_CONTROL
  printf("%%IDE-I-WRITCTRL: write port %d on IDE control %d: 0x%02x\n",
         (u32)(address), index, data);
#endif
  switch (address) {
  case 0:
    prev_reset = CONTROLLER(index).reset;
    CONTROLLER(index).reset = (data >> 2) & 1;
    CONTROLLER(index).disable_irq = (data >> 1) & 1;

    if (!prev_reset && CONTROLLER(index).reset) {
#ifdef DEBUG_IDE_REG_CONTROL
      printf("IDE reset on index %d started.\n", index);
#endif
      STATUS(index, 0).busy = true;
      STATUS(index, 0).drive_ready = false;
      STATUS(index, 0).seek_complete = true;
      STATUS(index, 0).drq = false;
      STATUS(index, 0).err = false;
      COMMAND(index, 0).current_command = 0;
      COMMAND(index, 0).command_in_progress = false;
      STATUS(index, 1).busy = true;
      STATUS(index, 1).drive_ready = false;
      STATUS(index, 1).seek_complete = true;
      STATUS(index, 1).drq = false;
      STATUS(index, 1).err = false;
      COMMAND(index, 1).current_command = 0;
      COMMAND(index, 1).command_in_progress = false;

      CONTROLLER(index).reset_in_progress = true;
      SEL_REGISTERS(index).error = 0x01; // no error
      COMMAND(index, 0).current_command = 0;
      CONTROLLER(index).disable_irq = false;
    } else if (prev_reset && !CONTROLLER(index).reset) {
#ifdef DEBUG_IDE_REG_CONTROL
      printf("IDE reset on index %d ended.\n", index);
#endif
      STATUS(index, 0).busy = false;
      STATUS(index, 0).drive_ready = true;
      STATUS(index, 1).busy = false;
      STATUS(index, 1).drive_ready = true;
      CONTROLLER(index).reset_in_progress = false;

      set_signature(index, 0);
      set_signature(index, 1);
    }
    break;

  case 1:

    // floppy?
    break;
  }
}

/**
 * Read from the IDE controller busmaster interface.
 **/
u32 CAliM1543C_ide::ide_busmaster_read(int index, u32 address, int dsize) {
  u32 data = 0;
  switch (dsize) {
  case 8:
    data = CONTROLLER(index).busmaster[address];
    break;

  case 32:
    data = *(u32 *)(&CONTROLLER(index).busmaster[address]);
    break;

  default:
    FAILURE(InvalidArgument, "16-bit read from busmaster");
    break;
  }

#ifdef DEBUG_IDE_BUSMASTER
  printf(
      "%%IDE-I-READBUSM: read port %d on IDE bus master %d: 0x%02x, %d bytes\n",
      (u32)(address), index, data, dsize / 8);
#endif
  return data;
}

/**
 * Write to the IDE controller busmaster interface.
 **/
void CAliM1543C_ide::ide_busmaster_write(int index, u32 address, u32 data,
                                         int dsize) {
#ifdef DEBUG_IDE_BUSMASTER
  if (!(dsize == 8 && (address >= 4 && address <= 7))) {
    printf("%%IDE-I-WRITBUSM: write port %d on IDE bus master %d: 0x%02x, %d "
           "bytes\n",
           (u32)(address), index, data, dsize / 8);
  }
#endif

  u32 prd_address;

  //  u32 base, control;
  switch (dsize) {
  case 32:
    ide_busmaster_write(index, address, data & 0xff, 8);
    ide_busmaster_write(index, address + 1, (data >> 8) & 0xff, 8);
    ide_busmaster_write(index, address + 2, (data >> 16) & 0xff, 8);
    ide_busmaster_write(index, address + 3, (data >> 24) & 0xff, 8);
    return;

  case 16:
    ide_busmaster_write(index, address, data & 0xff, 8);
    ide_busmaster_write(index, address + 1, (data >> 8) & 0xff, 8);
    return;
  }

  switch (address) {
  case 0: // command register
#ifdef DEBUG_IDE_BUSMASTER
    printf("%%IDE-I-BUSM: Bus master command got data: %x (%s,%s)\n", data,
           (data & 0x08 ? "write" : "read"), (data & 0x01 ? "start" : "stop"));
#endif

    // bits 7:4 & 2:1 are reserved and must return zero on read.
    CONTROLLER(index).busmaster[0] = data & 0x09;
    if (data & 0x01) {

      // set the status register
      CONTROLLER(index).busmaster[2] |= 0x01;
      semBusMasterReady[index]->wait();
      semBusMaster[index]->set(); // wake up the controller for busmastering
    } else {

      // clear the status register
      CONTROLLER(index).busmaster[2] &= 0xfe;
    }
    break;

  case 2: // status
    // bit 7 = simplex only (0=both channels are independent)
    // bit 6 = drive 1 dma capable.
    // bit 5 = drive 0 dma capable.
    // bit 4,3 = reserved
    // bit 2 = interrupt (write 1 to reset)
    // bit 1 = error (write 1 to reset)
    // bit 0 = busmaster active.
    CONTROLLER(index).busmaster[2] = data & 0x67;
    if (data & 0x04) // interrupt
      CONTROLLER(index).busmaster[2] &= ~0x04;
    if (data & 0x02) // error
      CONTROLLER(index).busmaster[2] &= ~0x02;
    if (data & 0x01) // busy
      CONTROLLER(index).busmaster[2] &= ~0x01;
    break;

  case 4: // descriptor table pointer register(s)
  case 5:
  case 6:
    CONTROLLER(index).busmaster[address] = data;
    break;

  case 7:
    CONTROLLER(index).busmaster[address] = data;
    prd_address = endian_32(*(u32 *)(&CONTROLLER(index).busmaster[4]));
#ifdef DEBUG_IDE_BUSMASTER
    printf("%%IDE-I-PRD: Virtual address: %" PRIx64 "  \n",
           endian_32(*(u32 *)(&CONTROLLER(index).busmaster[4])));
    printf("-IDE-I-PRD: Physical address: %" PRIx64 "  \n", prd_address);
    u32 base, control;
    do {
      do_pci_read(prd_address, &base, 4, 1);
      do_pci_read(prd_address + 4, &control, 4, 1);
      printf("-IDE-I-PRD: base: %x, control: %x  \n", base, control);
      prd_address += 8;
    } while (base & 0x80 == 0);
#endif
    break;

  default:
    break;
  }
}

void CAliM1543C_ide::set_signature(int index, int id) {

  // Device signature
  REGISTERS(index, id).head_no = 0;
  REGISTERS(index, id).sector_count = 1;
  REGISTERS(index, id).sector_no = 1;
  if (get_disk(index, id)) {
    if (!get_disk(index, id)->cdrom()) {
      REGISTERS(index, id).cylinder_no = 0;
      CONTROLLER(index).selected = 0; // XXX: This may not be correct.
    } else {
      REGISTERS(index, id).cylinder_no = 0xeb14;
    }
  } else {
    REGISTERS(index, id).cylinder_no = 0xffff;
  }
}

void CAliM1543C_ide::raise_interrupt(int index) {
  if (!CONTROLLER(index).disable_irq) {
#ifdef DEBUG_IDE_INTERRUPT
    printf("%%IDE-I-INTERRUPT: Interrupt raised on controller %d.\n", index);
#endif

#if !defined(IDE_YIELD_INTERRUPTS)
    {
      SCOPED_WRITE_LOCK(mtBusMaster[index]);
      CONTROLLER(index).busmaster[2] |= 0x04;
    }
    UPDATE_ALT_STATUS(index);
    theAli->pic_interrupt(1, 6 + index);
#else
    CONTROLLER(index).interrupt_pending = true;
#endif
  }
}

u8 CAliM1543C_ide::get_status(int index) {
  u8 data;
  if (!SEL_DISK(index)) {
#ifdef DEBUG_IDE_REG_COMMAND
    printf("%%IDE-I-STATUS: Read status for nonexiting device %d.%d\n", index,
           CONTROLLER(index).selected);
#endif
    return 0;
  }

  data = (SEL_STATUS(index).busy ? 0x80 : 0x00) |
         (SEL_STATUS(index).drive_ready ? 0x40 : 0x00) |
         (SEL_STATUS(index).fault ? 0x20 : 0x00) |
         (SEL_STATUS(index).seek_complete ? 0x10 : 0x00) |
         (SEL_STATUS(index).drq ? 0x08 : 0x00) |
         (SEL_STATUS(index).index_pulse ? 0x02 : 0x00) |
         (SEL_STATUS(index).err ? 0x01 : 0x00);
  SEL_STATUS(index).index_pulse_count++;
  SEL_STATUS(index).index_pulse = false;
  if (SEL_STATUS(index).index_pulse_count >= 10) {
    SEL_STATUS(index).index_pulse_count = 0;
    SEL_STATUS(index).index_pulse = true;
  }

#ifdef DEBUG_IDE_REG_COMMAND
  if ((SEL_STATUS(index).debug_last_status & 0xfd) != (data & 0xfd) ||
      SEL_STATUS(index).debug_status_update) {
    printf("%%IDE-I-STATUS: Controller %d status: %x = %s %s %s %s %s %s %s\n",
           index, data, SEL_STATUS(index).busy ? "busy" : "",
           SEL_STATUS(index).drive_ready ? "drdy" : "",
           SEL_STATUS(index).fault ? "fault" : "",
           SEL_STATUS(index).seek_complete ? "seek_complete" : "",
           SEL_STATUS(index).drq ? "drq" : "",
           SEL_STATUS(index).index_pulse ? "pulse" : "",
           SEL_STATUS(index).err ? "error" : "");
    SEL_STATUS(index).debug_status_update = false;
  }

  SEL_STATUS(index).debug_last_status = data;
#endif
  return data;
}

void CAliM1543C_ide::identify_drive(int index, bool packet) {
  char serial_number[21];
  char model_number[41];
  char rev_number[9];
  size_t i;

  // clear the block.
  for (i = 0; i < 256; i++)
    CONTROLLER(index).data[i] = 0;

  CONTROLLER(index).data_ptr = 0;
  CONTROLLER(index).data_size = 256;

  // The data here was taken from T13/1153D revision 18
  if (!packet) {

    // flags:  0x0080 = removable, 0x0040 = fixed.
    CONTROLLER(index).data[0] = SEL_DISK(index)->cdrom() ? 0x0080 : 0x0040;
  } else {

    // flags: 15-14: 10=atapi, 11=reserved; 12-8: packet set; 7:
    // removable 6-5: timing info, 4-2: command, 1-0: 00= 12 byte packet
    CONTROLLER(index).data[0] = 0x8580;
  }

  // logical cylinders
  if (SEL_DISK(index)->get_cylinders() > 16383)
    CONTROLLER(index).data[1] = 16383;
  else
    CONTROLLER(index).data[1] = (u16)(SEL_DISK(index)->get_cylinders());

  // logical heads
  CONTROLLER(index).data[3] = (u16)(SEL_DISK(index)->get_heads());

  // logical sectors per logical track
  CONTROLLER(index).data[6] = (u16)(SEL_DISK(index)->get_sectors());

  // serial number
  strcpy(serial_number, "                    ");
  i = strlen(SEL_DISK(index)->get_serial());
  i = (i > 20) ? 20 : i;
  memcpy(model_number, SEL_DISK(index)->get_serial(), i);
  for (i = 0; i < 10; i++)
    CONTROLLER(index).data[10 + i] =
        (serial_number[i * 2] << 8) | serial_number[i * 2 + 1];

  // firmware revision
  strcpy(rev_number, "        ");
  i = strlen(SEL_DISK(index)->get_rev());
  i = (i > 8) ? 8 : i;
  memcpy(model_number, SEL_DISK(index)->get_rev(), i);
  for (i = 0; i < 4; i++)
    CONTROLLER(index).data[23 + i] =
        (rev_number[i * 2] << 8) | rev_number[i * 2 + 1];

  // model number
  strcpy(model_number, "                                        ");
  i = strlen(SEL_DISK(index)->get_model());
  i = (i > 40) ? 40 : i;
  memcpy(model_number, SEL_DISK(index)->get_model(), i);
  for (i = 0; i < 20; i++)
    CONTROLLER(index).data[i + 27] =
        (model_number[i * 2] << 8) | model_number[i * 2 + 1];

  // read/write multiple (15-8 = 0x80, 7-0 = # sectors)
  CONTROLLER(index).data[47] = 0x8000 | MAX_MULTIPLE_SECTORS;

  // capabilities
  if (!packet) {
    CONTROLLER(index).data[49] = 0x0300;
  } else {
    CONTROLLER(index).data[49] = 0x0b00; // dma, iordy
  }

  // capabilities (2)
  CONTROLLER(index).data[50] = 0x4000;

  // pio data transfer number (bits 15-8)
  CONTROLLER(index).data[51] = 0x0300;

  // validity:  bit 2 = #88 valid, 1 = 64-70 valid, 0 = 54-58 valid
  CONTROLLER(index).data[53] = 7;

  // geometry
  CONTROLLER(index).data[54] = (u16)(SEL_DISK(index)->get_cylinders());
  CONTROLLER(index).data[55] = (u16)(SEL_DISK(index)->get_heads());
  CONTROLLER(index).data[56] = (u16)(SEL_DISK(index)->get_sectors());
  CONTROLLER(index).data[57] =
      (u16)(SEL_DISK(index)->get_chs_size() >> 0) & 0xFFFF;
  CONTROLLER(index).data[58] =
      (u16)(SEL_DISK(index)->get_chs_size() >> 16) & 0xFFFF;

  // multiple sector setting (valid, 1 sector per interrupt)
  if (SEL_PER_DRIVE(index).multiple_size != 0) {
    CONTROLLER(index).data[59] = 0x0100 | SEL_PER_DRIVE(index).multiple_size;
  } else {
    CONTROLLER(index).data[59] = 0x0000;
  }

  // lba capacity
  CONTROLLER(index).data[60] =
      (u16)(SEL_DISK(index)->get_lba_size() >> 0) & 0xFFFF;
  CONTROLLER(index).data[61] =
      (u16)(SEL_DISK(index)->get_lba_size() >> 16) & 0xFFFF;

  // multiword dma capability (10-8: modes selected, 2-0, modes
  // supported)
  if (usedma)
    CONTROLLER(index).data[63] =
        CONTROLLER(index).dma_mode << 8 | 0x01; // dma 0 supported
  else
    CONTROLLER(index).data[63] =
        CONTROLLER(index).dma_mode << 8 | 0x00; // dma not supported

  // pio modes supported (bit 0 = mode 3, bit 1 = mode 4)
  CONTROLLER(index).data[64] = 0x0002;

  // minimum cycle times
  CONTROLLER(index).data[65] = 480; // mode 0
  CONTROLLER(index).data[66] = 480; // mode 0
  CONTROLLER(index).data[67] = 120; // pio4
  CONTROLLER(index).data[68] = 120; // pio4
  if (packet) {

    // packet to bus release time
    CONTROLLER(index).data[71] = 120;

    // service to bus release time
    CONTROLLER(index).data[72] = 120;
  }

  // queue depth (we don't do queing)
  CONTROLLER(index).data[75] = 0;

  // ata version supported (bits/version: 1,2,3,4)
  CONTROLLER(index).data[80] = 0x001e;

  // atapi revision supported (ata/atapi-4 T13 1153D revision 17)
  CONTROLLER(index).data[81] = 0x0017;

  // command set supported (cdrom = nop,packet,removable; disk=nop)
  CONTROLLER(index).data[82] = SEL_DISK(index)->cdrom() ? 0x4014 : 0x4000;

  // command sets supported(no additional command sets)
  CONTROLLER(index).data[83] = 0x4000;
  CONTROLLER(index).data[84] = 0x4000;

  // command sets enabled.
  CONTROLLER(index).data[85] = SEL_DISK(index)->cdrom() ? 0x4014 : 0x4000;
  CONTROLLER(index).data[86] = 0x4000;
  CONTROLLER(index).data[87] = 0x4000;

  // ultra dma modes supported (10-8: modes selected, 2-0, modes
  // supported)
  CONTROLLER(index).data[88] = 0x0000;
}

void CAliM1543C_ide::command_aborted(int index, u8 command) {
  printf("ide%d.%d aborting on command 0x%02x \n", index,
         CONTROLLER(index).selected, command);
  SEL_STATUS(index).busy = false;
  SEL_STATUS(index).drive_ready = true;
  SEL_STATUS(index).err = true;
  SEL_STATUS(index).drq = false;
  SEL_REGISTERS(index).error |= 0x04; // command ABORTED
  CONTROLLER(index).data_ptr = 0;
  SEL_COMMAND(index).command_in_progress = false;
  UPDATE_ALT_STATUS(index);
  raise_interrupt(index);
}

CAliM1543C_ide *theIDE = 0;

void CAliM1543C_ide::ide_status(int index) {
  printf("IDE %d.%d: [busy: %d, drdy: %d, flt: %d, drq: %d, err: %d]\n"
         "         [c: %d, h: %d, s: %d, #: %d, f: %x, lba: %d]\n"
         "         [ptr: %d, size: %d, error: %d, cmd: %x, in progress: %d]\n"
         "         [cycle: %d, pkt phase: %d, pkt cmd: %x, dma: %d]\n"
         "         [bm-cmd: %x  bm-stat: %x]\n",
         index, CONTROLLER(index).selected, SEL_STATUS(index).busy,
         SEL_STATUS(index).drive_ready, SEL_STATUS(index).fault,
         SEL_STATUS(index).drq, SEL_STATUS(index).err,
         SEL_REGISTERS(index).cylinder_no, SEL_REGISTERS(index).head_no,
         SEL_REGISTERS(index).sector_no, SEL_REGISTERS(index).sector_count,
         SEL_REGISTERS(index).features,
         (SEL_REGISTERS(index).head_no << 24) |
             (SEL_REGISTERS(index).cylinder_no << 8) |
             SEL_REGISTERS(index).sector_no,
         CONTROLLER(index).data_ptr, CONTROLLER(index).data_size,
         SEL_REGISTERS(index).error, SEL_COMMAND(index).current_command & 0xff,
         SEL_COMMAND(index).command_in_progress,
         SEL_COMMAND(index).command_cycle, SEL_COMMAND(index).packet_phase,
         SEL_COMMAND(index).packet_command[0], SEL_COMMAND(index).packet_dma,
         CONTROLLER(index).busmaster[0], CONTROLLER(index).busmaster[2]);
}

/**
 * Check if threads are still running.
 **/
void CAliM1543C_ide::check_state() {
  if (thrController[0] && thrControllerDead[0].load())
    FAILURE(Thread, "IDE 0 thread has died");

  if (thrController[1] && thrControllerDead[1].load())
    FAILURE(Thread, "IDE 1 thread has died");
}

void CAliM1543C_ide::execute(int index) {
  if (SEL_DISK(index) == NULL && SEL_COMMAND(index).current_command != 0x90) {

    // this device doesn't exist (and its not execute device
    // diagnostic)
    // so we'll just timeout
    SEL_COMMAND(index).command_in_progress = false;
  } else {
#ifdef DEBUG_IDE_COMMAND
    printf("%%IDE-I-COMMAND: Processing command on controller %d.\n", index);
    ide_status(index);
#endif
    switch (SEL_COMMAND(index).current_command) {
    case 0x00: // nop
      SEL_REGISTERS(index).error = 0x04;
      SEL_STATUS(index).busy = false;
      SEL_STATUS(index).drive_ready = true;
      SEL_STATUS(index).fault = true;
      SEL_STATUS(index).drq = false;
      SEL_STATUS(index).err = true;
      SEL_COMMAND(index).command_in_progress = false;
      raise_interrupt(index);
      printf("got nop on %d.%d\n", index, CONTROLLER(index).selected);

      // FAILURE("This isn't possible, but you're seeing it.");
      break;

    case 0x08: // device reset
      if (SEL_DISK(index)->cdrom() || 1) {

        // the spec says that non-packet devices must not respond
        // to device reset.  However, by allowing it, Tru64
        // recognizes the device properly.
        SEL_COMMAND(index).command_in_progress = false;
        if (CONTROLLER(index).selected == 0) {
          REGISTERS(index, 0).error = 0x01; // device passed.
          REGISTERS(index, 1).error = 0x01; // slave not present or passed
        } else {
          REGISTERS(index, 1).error = 0x01; // slave passed
        }

        set_signature(index, CONTROLLER(index).selected);

        // step "k", page 216 (232)
        SEL_STATUS(index).drq = false;   // bit 3
        SEL_STATUS(index).bit_2 = false; // bit 2
        SEL_STATUS(index).err = false;   // bit 0
        if (SEL_DISK(index)->cdrom()) {
          SEL_STATUS(index).fault = false;       // bit 5
          SEL_STATUS(index).drive_ready = false; // per step "m2"
        } else {
          SEL_STATUS(index).drive_ready = true; // step "m1"
        }

        SEL_STATUS(index).busy = false;

        // some sources say there's no reset on device reset.
        // raise_interrupt(index);
      } else {
        command_aborted(index, SEL_COMMAND(index).current_command);
      }
      break;

    case 0x10: // calibrate drive
      SEL_STATUS(index).busy = false;
      SEL_STATUS(index).drive_ready = true;
      SEL_STATUS(index).seek_complete = true;
      SEL_STATUS(index).fault = false;
      SEL_STATUS(index).drq = false;
      SEL_STATUS(index).err = false;
      SEL_REGISTERS(index).cylinder_no = 0;
      SEL_COMMAND(index).command_in_progress = false;
      raise_interrupt(index);
      break;

    case 0x20: // read with retries
    case 0x21: // read without retries
      if (SEL_COMMAND(index).command_cycle == 0) {

        // fixup the 0=256 case.
        if (SEL_REGISTERS(index).sector_count == 0)
          SEL_REGISTERS(index).sector_count = 256;
      }

      if (!SEL_STATUS(index).drq) {

        // buffer is empty, so lets fill it.
        if (!SEL_REGISTERS(index).lba_mode) {
          FAILURE(NotImplemented, "Non-LBA disk read");
        } else {
          u32 lba = (SEL_REGISTERS(index).head_no << 24) |
                    (SEL_REGISTERS(index).cylinder_no << 8) |
                    SEL_REGISTERS(index).sector_no;

          SEL_DISK(index)->seek_block(lba);
          SEL_DISK(index)->read_blocks(&(CONTROLLER(index).data[0]), 1);
#if defined(ES40_BIG_ENDIAN)
          for (int i = 0; i < SEL_DISK(index)->get_block_size() / sizeof(u16);
               i++)
            CONTROLLER(index).data[i] = endian_16(CONTROLLER(index).data[i]);
#endif
          SEL_STATUS(index).busy = false;
          SEL_STATUS(index).drive_ready = true;
          SEL_STATUS(index).fault = false;
          SEL_STATUS(index).drq = true;
          SEL_STATUS(index).err = false;
          CONTROLLER(index).data_ptr = 0;
          CONTROLLER(index).data_size = 256;

          // prepare for next sector
          SEL_REGISTERS(index).sector_count--;
          if (SEL_REGISTERS(index).sector_count == 0) {
            SEL_COMMAND(index).command_in_progress = false;
            if (SEL_DISK(index)->cdrom())
              set_signature(index, CONTROLLER(index).selected); // per 9.1
          } else {

            // set the next block to read.
            // increment the lba.
            SEL_REGISTERS(index).sector_no++;
            if (SEL_REGISTERS(index).sector_no > 255) {
              SEL_REGISTERS(index).sector_no = 0;
              SEL_REGISTERS(index).cylinder_no++;
              if (SEL_REGISTERS(index).cylinder_no > 65535) {
                SEL_REGISTERS(index).cylinder_no = 0;
                SEL_REGISTERS(index).head_no++;
              }
            }
          }
        }

        raise_interrupt(index);
      }
      break;

    case 0x30: // write with retries
    case 0x31: // write without retries
      if (SEL_COMMAND(index).command_cycle == 0) {

        // this is our first time through
        if (SEL_DISK(index)->cdrom() || SEL_DISK(index)->ro()) {
          printf("%%IDE-W-RO: Write attempt to read-only disk %d.%d.\n", index,
                 CONTROLLER(index).selected);
          command_aborted(index, SEL_COMMAND(index).current_command);
        } else {
          SEL_STATUS(index).drq = true;
          SEL_STATUS(index).busy = false;
          CONTROLLER(index).data_size = 256;
          if (SEL_REGISTERS(index).sector_count == 0)
            SEL_REGISTERS(index).sector_count = 256;
        }
      } else {

        // now we should be getting data.
        if (!SEL_STATUS(index).drq) {

          // the buffer is full.  Do something with the data.
          if (!SEL_REGISTERS(index).lba_mode) {
            FAILURE(NotImplemented, "Non-LBA disk write");
          } else {
            u32 lba = (SEL_REGISTERS(index).head_no << 24) |
                      (SEL_REGISTERS(index).cylinder_no << 8) |
                      SEL_REGISTERS(index).sector_no;

#if defined(ES40_BIG_ENDIAN)
            {
              u16 data[IDE_BUFFER_SIZE];

              SEL_DISK(index)->seek_block(lba);
              for (int i = 0;
                   i < SEL_DISK(index)->get_block_size() / sizeof(u16); i++)
                data[i] = endian_16(CONTROLLER(index).data[i]);
              SEL_DISK(index)->write_blocks(&(data[0]), 1);
            }

#else
            SEL_DISK(index)->seek_block(lba);
            SEL_DISK(index)->write_blocks(&(CONTROLLER(index).data[0]), 1);
#endif
            SEL_STATUS(index).busy = false;
            SEL_STATUS(index).drive_ready = true;
            SEL_STATUS(index).fault = false;
            SEL_STATUS(index).drq = true;
            SEL_STATUS(index).err = false;
            CONTROLLER(index).data_ptr = 0;

            // prepare for next sector
            SEL_REGISTERS(index).sector_count--;
            if (SEL_REGISTERS(index).sector_count == 0) {

              // we're done
              SEL_STATUS(index).drq = false;
              SEL_COMMAND(index).command_in_progress = false;
            } else {

              // set the next block to read.
              // increment the lba.
              SEL_REGISTERS(index).sector_no++;
              if (SEL_REGISTERS(index).sector_no > 255) {
                SEL_REGISTERS(index).sector_no = 0;
                SEL_REGISTERS(index).cylinder_no++;
                if (SEL_REGISTERS(index).cylinder_no > 65535) {
                  SEL_REGISTERS(index).cylinder_no = 0;
                  SEL_REGISTERS(index).head_no++;
                }
              }
            }
          }

          raise_interrupt(index);
        }
      }
      break;

    /*
     * case 0x40, 0x41: read verify sector(s) is mandatory for
     * non-packet (no w/packet
     */
    case 0x70: // seek
      if (SEL_DISK(index)->cdrom()) {
        command_aborted(index, SEL_COMMAND(index).current_command);
      } else {
        SEL_STATUS(index).busy = false;
        SEL_STATUS(index).drive_ready = true;
        SEL_STATUS(index).seek_complete = true;
        SEL_STATUS(index).fault = false;
        SEL_STATUS(index).drq = false;
        SEL_STATUS(index).err = false;
        SEL_COMMAND(index).command_in_progress = false;
        raise_interrupt(index);
      }
      break;

    /*
     * 0x90: execute device diagnostic: mandatory
     */
    case 0x91: // initialize device parameters
      SEL_COMMAND(index).command_in_progress = false;
      if (SEL_DISK(index)->cdrom()) {
        command_aborted(index, SEL_COMMAND(index).current_command);
      } else {
#ifdef DEBUG_IDE
        printf("Original c: %d, h: %d, s: %d\n",
               SEL_DISK(index)->get_cylinders(), SEL_DISK(index)->get_heads(),
               SEL_DISK(index)->get_sectors());
        printf("Requested c: %d, h: %d, s: %d\n",
               SEL_REGISTERS(index).cylinder_no,
               SEL_REGISTERS(index).head_no + 1,
               SEL_REGISTERS(index).sector_count);
#endif
        if (SEL_DISK(index)->get_heads() ==
                (SEL_REGISTERS(index).head_no + 1) &&
            SEL_DISK(index)->get_sectors() ==
                SEL_REGISTERS(index).sector_count) {

          // use the default translation -- ok!
          SEL_STATUS(index).busy = false;
          SEL_STATUS(index).drive_ready = true;
          SEL_STATUS(index).fault = false;
          SEL_STATUS(index).drq = false;
          SEL_STATUS(index).err = false;
          raise_interrupt(index);
        } else {
#ifdef DEBUG_IDE
          PAUSE("INIT DEV PARAMS -- geometry not supported!");
#endif
          SEL_STATUS(index).busy = false;
          SEL_STATUS(index).drive_ready = true;
          SEL_STATUS(index).fault = false;
          SEL_STATUS(index).drq = false;
          SEL_STATUS(index).err = true;
          SEL_REGISTERS(index).error = 0x04; // ABORT.
          raise_interrupt(index);
        }
      }
      break;

    case 0xa0: // packet send
      /*
       * The state machine and protocol used here was actually
       * derived from ATA/ATAPI-5 (D1321R3) instead of the -4
       * documenation.  State names were taken from that document.
       */
      if (!SEL_DISK(index)->cdrom()) {
        command_aborted(index, SEL_COMMAND(index).current_command);
      } else {
        if (SEL_REGISTERS(index).features & 0x02) {

          // overlap not supported
          PAUSE("overlapping not supported");
          command_aborted(index, SEL_COMMAND(index).current_command);
        } else {
          if (SEL_COMMAND(index).packet_phase == PACKET_NONE) {

            // this must be the first time through.
            if (!scsi_arbitrate(index))
              FAILURE(IllegalState, "ATAPI SCSI bus busy");
            if (!scsi_select(index, CONTROLLER(index).selected))
              FAILURE(IllegalState, "ATAPI device not responding to selection");
            SEL_REGISTERS(index).REASON = IR_CD;
            SEL_STATUS(index).busy = false;
            SEL_STATUS(index).drq = true;
            SEL_STATUS(index).DMRD = false;
            SEL_STATUS(index).SERV = false;
            CONTROLLER(index).data_ptr = 0;
            CONTROLLER(index).data_size = 6;
            SEL_COMMAND(index).packet_dma =
                (SEL_REGISTERS(index).features & 0x01) ? true : false;
            SEL_COMMAND(index).packet_phase = PACKET_DP1;

            // we drop out of the thread and shut down the
            // controller. we will be awakened when the drq flag
            // is false, and the process will start in the state machine.
            break;
          }

          /*
           * This is the Packet I/O state machine. The gist of
           * it is this: we loop until yield==true, so we can
           * move from state to state in the same DoClock().
           * By the time we get here, we're in DP1 (Receive
           * Packet) and we're waiting for an actual packet to
           * arrive.
           */
#ifdef DEBUG_IDE_PACKET
          printf("Entering Packet state machine %d\n", index);
          ide_status(index);
#endif

          bool yield = false;
          do {
#ifdef DEBUG_IDE_PACKET
            printf("PACKET STATE: %s (%d)\n",
                   packet_states[SEL_COMMAND(index).packet_phase],
                   SEL_COMMAND(index).packet_phase);
#endif
            switch (SEL_COMMAND(index).packet_phase) {
            case PACKET_DP1: // receive packet
              // we now have a full command packet.
              if (scsi_get_phase(index) != SCSI_PHASE_COMMAND)
                FAILURE(IllegalState, "SCSI command phase expected");

              memcpy(scsi_xfer_ptr(index, 12), CONTROLLER(index).data, 12);
              memcpy(SEL_COMMAND(index).packet_command, CONTROLLER(index).data,
                     12);
              scsi_xfer_done(index);

              SEL_COMMAND(index).packet_phase = PACKET_DP2;
              SEL_COMMAND(index).packet_buffersize =
                  SEL_REGISTERS(index).cylinder_no;
              SEL_STATUS(index).busy = true;
              break;

            case PACKET_DP2: // prepare b
              SEL_STATUS(index).busy = true;
              SEL_STATUS(index).drq = false;

              if (SEL_COMMAND(index).command_in_progress) {
                switch (scsi_get_phase(index)) {
                case SCSI_PHASE_DATA_IN: {
                  size_t num_bytes = scsi_expected_xfer(index);
                  void *data_ptr = scsi_xfer_ptr(index, num_bytes);
                  memcpy(CONTROLLER(index).data, data_ptr, num_bytes);
                  scsi_xfer_done(index);
                  SEL_COMMAND(index).packet_phase = PACKET_DP34;
                  SEL_REGISTERS(index).BYTE_COUNT = (int)num_bytes;
                  CONTROLLER(index).data_size =
                      (int)num_bytes / 2; // word count.
                  CONTROLLER(index).data_ptr = 0;
                } break;

                case SCSI_PHASE_DATA_OUT:
                  FAILURE(NotImplemented,
                          "ATAPI for now does not support write operations");
                  break;

                case SCSI_PHASE_STATUS:
                  scsi_xfer_ptr(index, scsi_expected_xfer(index));
                  scsi_xfer_done(index);
                  if (scsi_get_phase(index) != SCSI_PHASE_FREE)
                    FAILURE(IllegalState, "SCSI bus free phase expected");
                  SEL_COMMAND(index).packet_phase = PACKET_DI;
                  break;

                default:
                  FAILURE(IllegalState, "Unexpected SCSI phase");
                }
              } else {

                // transition to an idle state
#if defined(DEBUG_IDE_PACKET)
                printf("Transition into idle state from DP2.\n");
#endif
                SEL_COMMAND(index).packet_phase = PACKET_DI;
              }
              break;

            case PACKET_DP34:
              if (SEL_COMMAND(index).packet_dma) {

                // send back via dma
#ifdef DEBUG_IDE_PACKET
                printf("Sending ATAPI data back via DMA.\n");
#endif

                do_dma_transfer(index, (u8 *)(&CONTROLLER(index).data[0]),
                                SEL_REGISTERS(index).BYTE_COUNT, false);
                if (scsi_get_phase(index) != SCSI_PHASE_STATUS)
                  FAILURE(IllegalState, "SCSI status phase expected");
                scsi_xfer_ptr(index, scsi_expected_xfer(index));
                scsi_xfer_done(index);
                if (scsi_get_phase(index) != SCSI_PHASE_FREE)
                  FAILURE(IllegalState, "SCSI bus free phase expected");
                SEL_STATUS(index).drq = true;
                SEL_STATUS(index).busy = false;
                SEL_COMMAND(index).packet_phase = PACKET_DI;
              } else {

                // send back via pio
#ifdef DEBUG_IDE_PACKET
                printf("Sending ATAPI data back via PIO.\n");
#endif
#if 0
                if((!SEL_STATUS(index).drq) && (CONTROLLER(index).data_ptr == 0))
                {

                  // first time through: no data
                  // transferred, and drq=0
                  SEL_STATUS(index).drq = true;
                  SEL_STATUS(index).busy = false;
                  SEL_REGISTERS(index).REASON = IR_IO;
                  raise_interrupt(index);
                  yield = true;
                }
                else
                {
                  if(!SEL_STATUS(index).drq)
                  {
                    // all of the data has been read
                    // from the buffer.
                    // for now I assume that it is
                    // everything.
                    if(scsi_get_phase(index) != SCSI_PHASE_STATUS)
                      FAILURE(IllegalState, "SCSI status phase expected");
                    scsi_xfer_ptr(index, scsi_expected_xfer(index));
                    scsi_xfer_done(index);
                    if(scsi_get_phase(index) != SCSI_PHASE_FREE)
                      FAILURE(IllegalState, "SCSI bus free phase expected");
#ifdef DEBUG_IDE_PACKET
                    printf("Finished transferring!\n");
#endif
                    SEL_COMMAND(index).packet_phase = PACKET_DI;
                    yield = false;
                  }
                }

#else

                // do the transfer
                SEL_STATUS(index).drq = true;
                SEL_STATUS(index).busy = false;
                SEL_REGISTERS(index).REASON = IR_IO;
                if (scsi_get_phase(index) != SCSI_PHASE_STATUS)
                  FAILURE(IllegalState, "SCSI status phase expected");
                scsi_xfer_ptr(index, scsi_expected_xfer(index));
                scsi_xfer_done(index);
                if (scsi_get_phase(index) != SCSI_PHASE_FREE)
                  FAILURE(IllegalState, "SCSI Bus free phase expected");
#ifdef DEBUG_IDE_PACKET
                printf("Finished Transferring\n");
#endif
                raise_interrupt(index);
                SEL_COMMAND(index).packet_phase = PACKET_DI;
                yield = true;
#endif
              }
              break;

            case PACKET_DI:

              // this is either DI0 or DI1
              SEL_REGISTERS(index).REASON = IR_CD | IR_IO;
              SEL_STATUS(index).busy = false;
              SEL_STATUS(index).drive_ready = true;
              SEL_STATUS(index).SERV = false;
              SEL_STATUS(index).CHK = false;
              SEL_STATUS(index).drq = false;
              raise_interrupt(index);
              SEL_COMMAND(index).command_in_progress = false;
              yield = true;
              break;

            default:
              FAILURE(InvalidArgument, "Unknown packet phase");
            }
          } while (!yield);
#ifdef DEBUG_IDE_PACKET
          printf("Drop out of packet state machine %d\n", index);
          ide_status(index);
#endif
        }
      }
      break;

    case 0xa1: // identify packet device
      if (SEL_DISK(index)->cdrom()) {
        identify_drive(index, true);
        SEL_STATUS(index).busy = false;
        SEL_STATUS(index).drive_ready = true;
        SEL_STATUS(index).seek_complete = true;
        SEL_STATUS(index).fault = false;
        SEL_STATUS(index).drq = true;
        SEL_STATUS(index).err = false;
        SEL_COMMAND(index).command_in_progress = false;
        raise_interrupt(index);
      } else {
        command_aborted(index, SEL_COMMAND(index).current_command);
      }
      break;

    case 0xc4: // read multiple
      if (SEL_DISK(index)->cdrom()) {
        command_aborted(index, SEL_COMMAND(index).current_command);
      } else {
        if (SEL_COMMAND(index).command_cycle == 0) {

          // fixup the 0=256 case.
          if (SEL_REGISTERS(index).sector_count == 0)
            SEL_REGISTERS(index).sector_count = 256;
          SEL_STATUS(index).drq = false;
        }

        if (!SEL_STATUS(index).drq) {

          // buffer is empty, so lets fill it.
          if (!SEL_REGISTERS(index).lba_mode) {
            FAILURE(NotImplemented, "Non-LBA disk read");
          } else {
            u32 lba = (SEL_REGISTERS(index).head_no << 24) |
                      (SEL_REGISTERS(index).cylinder_no << 8) |
                      SEL_REGISTERS(index).sector_no;

            if (SEL_REGISTERS(index).sector_count >=
                SEL_PER_DRIVE(index).multiple_size) {

              // easy, its a full block
              CONTROLLER(index).data_size =
                  256 * SEL_PER_DRIVE(index).multiple_size;
              SEL_REGISTERS(index).sector_count -=
                  SEL_PER_DRIVE(index).multiple_size;
            } else {

              // partial block.
              CONTROLLER(index).data_size =
                  256 * SEL_REGISTERS(index).sector_count;
              SEL_REGISTERS(index).sector_count = 0;
            }

#ifdef DEBUG_IDE_MULTIPLE
            printf("IDE %d.%d: Reading %d sectors, %d sectors left.\n", index,
                   CONTROLLER(index).selected,
                   CONTROLLER(index).data_size / 256,
                   SEL_REGISTERS(index).sector_count);
#endif
            SEL_DISK(index)->seek_block(lba);
            SEL_DISK(index)->read_blocks(
                &(CONTROLLER(index).data[0]),
                CONTROLLER(index).data_size /
                    256); // actual number of blocks we want.
#if defined(ES40_BIG_ENDIAN)
            for (int i = 0; i < 256; i++)
              CONTROLLER(index).data[i] = endian_16(CONTROLLER(index).data[i]);
#endif
            SEL_STATUS(index).busy = false;
            SEL_STATUS(index).drive_ready = true;
            SEL_STATUS(index).fault = false;
            SEL_STATUS(index).drq = true;
            SEL_STATUS(index).err = false;
            CONTROLLER(index).data_ptr = 0;

            // prepare for next sector
            if (SEL_REGISTERS(index).sector_count == 0) {
              SEL_COMMAND(index).command_in_progress = false;
              if (SEL_DISK(index)->cdrom())
                set_signature(index, CONTROLLER(index).selected); // per 9.1
            } else {

              // set the next block to read.
              // increment the lba.
              SEL_REGISTERS(index).sector_no +=
                  CONTROLLER(index).data_size * 256; // # sectors read.
              if (SEL_REGISTERS(index).sector_no > 255) {
                SEL_REGISTERS(index).sector_no = 0;
                SEL_REGISTERS(index).cylinder_no++;
                if (SEL_REGISTERS(index).cylinder_no > 65535) {
                  SEL_REGISTERS(index).cylinder_no = 0;
                  SEL_REGISTERS(index).head_no++;
                }
              }
            }
          }

          raise_interrupt(index);
        }
      }
      break;

    case 0xc5: // write multiple
      if (SEL_DISK(index)->cdrom()) {
        command_aborted(index, SEL_COMMAND(index).current_command);
      } else {
        if (SEL_COMMAND(index).command_cycle == 0) {

          // this is our first time through
          if (SEL_DISK(index)->ro()) {
            printf("%%IDE-W-RO: Write attempt to read-only disk %d.%d.\n",
                   index, CONTROLLER(index).selected);
            command_aborted(index, SEL_COMMAND(index).current_command);
          } else {
            SEL_STATUS(index).drq = true;
            SEL_STATUS(index).busy = false;
            if (SEL_REGISTERS(index).sector_count == 0)
              SEL_REGISTERS(index).sector_count = 256;
            if (SEL_REGISTERS(index).sector_count >=
                SEL_PER_DRIVE(index).multiple_size) {
              CONTROLLER(index).data_size =
                  256 * SEL_PER_DRIVE(index).multiple_size;
              SEL_REGISTERS(index).sector_count -=
                  SEL_PER_DRIVE(index).multiple_size;
            } else {
              CONTROLLER(index).data_size =
                  256 * SEL_REGISTERS(index).sector_count;
              SEL_REGISTERS(index).sector_count = 0;
            }
          }
        } else {

          // now we should be getting data.
          if (!SEL_STATUS(index).drq) {

            // the buffer is full.  Do something with the data.
            if (!SEL_REGISTERS(index).lba_mode) {
              FAILURE(NotImplemented, "Non-LBA disk write");
            } else {
              u32 lba = (SEL_REGISTERS(index).head_no << 24) |
                        (SEL_REGISTERS(index).cylinder_no << 8) |
                        SEL_REGISTERS(index).sector_no;

#if defined(ES40_BIG_ENDIAN)
              {
                u16 data[IDE_BUFFER_SIZE];

                SEL_DISK(index)->seek_block(lba);
                for (int i = 0; i < CONTROLLER(index).data_size; i++)
                  data[i] = endian_16(CONTROLLER(index).data[i]);
                SEL_DISK(index)->write_blocks(
                    &(data[0]), CONTROLLER(index).data_size / 256);
              }

#else
              SEL_DISK(index)->seek_block(lba);
              SEL_DISK(index)->write_blocks(&(CONTROLLER(index).data[0]),
                                            CONTROLLER(index).data_size / 256);
#endif
              SEL_STATUS(index).busy = false;
              SEL_STATUS(index).drive_ready = true;
              SEL_STATUS(index).fault = false;
              SEL_STATUS(index).drq = true;
              SEL_STATUS(index).err = false;
              CONTROLLER(index).data_ptr = 0;

              if (SEL_REGISTERS(index).sector_count == 0) {

                // we're done
                SEL_STATUS(index).drq = false;
                SEL_COMMAND(index).command_in_progress = false;
              } else {

                // prepare for next block
                if (SEL_REGISTERS(index).sector_count >=
                    SEL_PER_DRIVE(index).multiple_size) {
                  CONTROLLER(index).data_size =
                      256 * SEL_PER_DRIVE(index).multiple_size;
                  SEL_REGISTERS(index).sector_count -=
                      SEL_PER_DRIVE(index).multiple_size;
                } else {
                  CONTROLLER(index).data_size =
                      256 * SEL_REGISTERS(index).sector_count;
                  SEL_REGISTERS(index).sector_count = 0;
                }

                // set the next block to read.
                // increment the lba.
                SEL_REGISTERS(index).sector_no++;
                if (SEL_REGISTERS(index).sector_no > 255) {
                  SEL_REGISTERS(index).sector_no = 0;
                  SEL_REGISTERS(index).cylinder_no++;
                  if (SEL_REGISTERS(index).cylinder_no > 65535) {
                    SEL_REGISTERS(index).cylinder_no = 0;
                    SEL_REGISTERS(index).head_no++;
                  }
                }
              }
            }

            raise_interrupt(index);
          }
        }
      }
      break;

    case 0xc6: // set multiple mode
      if (SEL_DISK(index)->cdrom()) {
        command_aborted(index, SEL_COMMAND(index).current_command);
      } else {
        SEL_PER_DRIVE(index).multiple_size = SEL_REGISTERS(index).sector_count;
        printf("Set multiple mode: sector_count = %d\n",
               SEL_REGISTERS(index).sector_count);
        SEL_STATUS(index).busy = false;
        SEL_STATUS(index).drive_ready = true;
        SEL_STATUS(index).fault = false;
        SEL_STATUS(index).drq = false;
        SEL_STATUS(index).err = false;
        SEL_COMMAND(index).command_in_progress = false;
        raise_interrupt(index);
      }
      break;

    case 0xc8: // read dma
    case 0xc9: // read dma (old)
      if (SEL_DISK(index)->cdrom()) {
        command_aborted(index, SEL_COMMAND(index).current_command);
        SEL_COMMAND(index).command_in_progress = false;
      } else {
        if (SEL_REGISTERS(index).sector_count == 0)
          SEL_REGISTERS(index).sector_count = 256;

#ifdef DEBUG_IDE_DMA
        printf("%%IDE-I-DMA: Read %d sectors = %d bytes.\n",
               SEL_REGISTERS(index).sector_count,
               SEL_REGISTERS(index).sector_count * 512);
#endif

        u32 lba = (SEL_REGISTERS(index).head_no << 24) |
                  (SEL_REGISTERS(index).cylinder_no << 8) |
                  SEL_REGISTERS(index).sector_no;

        SEL_DISK(index)->seek_block(lba);

        SEL_DISK(index)->read_blocks(&(CONTROLLER(index).data[0]),
                                     SEL_REGISTERS(index).sector_count);

        u8 *ptr = (u8 *)(&CONTROLLER(index).data[0]);
        do_dma_transfer(index, ptr, SEL_REGISTERS(index).sector_count * 512,
                        false);
        SEL_COMMAND(index).command_in_progress = false;
        SEL_STATUS(index).drive_ready = true;
        SEL_STATUS(index).seek_complete = true;
        SEL_STATUS(index).fault = false;
        SEL_STATUS(index).drq = false;
        SEL_STATUS(index).err = false;
        SEL_STATUS(index).busy = false;
      }
      break;

    case 0xca: // write dma
    case 0xcb: // write dma (old)
      if (SEL_DISK(index)->cdrom() || SEL_DISK(index)->ro()) {
        command_aborted(index, SEL_COMMAND(index).current_command);
        SEL_COMMAND(index).command_in_progress = false;
      } else {
        if (SEL_DISK(index)->ro()) {
          printf("%%IDE-W-RO: DMA Write attempt to read-only disk %d.%d.\n",
                 index, CONTROLLER(index).selected);
          command_aborted(index, SEL_COMMAND(index).current_command);
        } else {
          if (SEL_REGISTERS(index).sector_count == 0)
            SEL_REGISTERS(index).sector_count = 256;

#ifdef DEBUG_IDE_DMA
          printf("%%IDE-I-DMA: Write %d sectors = %d bytes.\n",
                 SEL_REGISTERS(index).sector_count,
                 SEL_REGISTERS(index).sector_count * 512);
#endif

          u8 *ptr = (u8 *)(&CONTROLLER(index).data[0]);
          do_dma_transfer(index, ptr, SEL_REGISTERS(index).sector_count * 512,
                          true);
          u32 lba = (SEL_REGISTERS(index).head_no << 24) |
                    (SEL_REGISTERS(index).cylinder_no << 8) |
                    SEL_REGISTERS(index).sector_no;

          SEL_DISK(index)->seek_block(lba);
          SEL_DISK(index)->write_blocks(&(CONTROLLER(index).data[0]),
                                        SEL_REGISTERS(index).sector_count);
          SEL_COMMAND(index).command_in_progress = false;
          SEL_STATUS(index).drive_ready = true;
          SEL_STATUS(index).seek_complete = true;
          SEL_STATUS(index).fault = false;
          SEL_STATUS(index).drq = false;
          SEL_STATUS(index).err = false;
          SEL_STATUS(index).busy = false;
        }
      }
      break;

#if 0

    case 0xe5:  // check power mode
      ide_status(index);
      command_aborted(index, SEL_COMMAND(index).current_command);
      SEL_COMMAND(index).command_in_progress = false;

      // raise_interrupt(index);
      break;
#endif

    case 0xec: // identify
      if (!SEL_DISK(index)->cdrom()) {
        identify_drive(index, false);
        SEL_STATUS(index).busy = false;
        SEL_STATUS(index).drive_ready = true;
        SEL_STATUS(index).seek_complete = true;
        SEL_STATUS(index).fault = false;
        SEL_STATUS(index).drq = true;
        SEL_STATUS(index).err = false;
        SEL_COMMAND(index).command_in_progress = false;
        raise_interrupt(index);
      } else {
        set_signature(index, CONTROLLER(index).selected); // per

        //
        // 9.1
        command_aborted(index, 0xec);
      }
      break;

    case 0xef: // set features
      SEL_COMMAND(index).command_in_progress = false;
      switch (SEL_REGISTERS(index).features) {
      case 0x03: // set transfer mode
        if (SEL_REGISTERS(index).sector_count < 16) {

          // allow all PIO modes.
          SEL_STATUS(index).busy = false;
          SEL_STATUS(index).drive_ready = true;
          SEL_STATUS(index).seek_complete = true;
          SEL_STATUS(index).fault = false;
          SEL_STATUS(index).drq = false;
          SEL_STATUS(index).err = false;
          raise_interrupt(index);
          break;
        } else {

          // a DMA mode.
          switch (SEL_REGISTERS(index).sector_count) {
          case 0x20:
          case 0x21:
          case 0x22:

            // multiword dma
            CONTROLLER(index).dma_mode =
                SEL_REGISTERS(index).sector_count & 0x03;
            SEL_STATUS(index).busy = false;
            SEL_STATUS(index).drive_ready = true;
            SEL_STATUS(index).seek_complete = true;
            SEL_STATUS(index).fault = false;
            SEL_STATUS(index).drq = false;
            SEL_STATUS(index).err = false;
            raise_interrupt(index);
            break;

          case 0x40:
          case 0x41:
          case 0x42:

            // ultra dma
            command_aborted(index, SEL_COMMAND(index).current_command);
            break;
          }
          break;
        }

      default:
        printf("%%IDE-I-FEAT: Unhandled set feature subcommand %x\n",
               SEL_REGISTERS(index).features);
        command_aborted(index, SEL_COMMAND(index).current_command);
        break;
      }
      break;

    /***
     * Special cases:  commands we don't support, but return success.
     ***/
    case 0xe0: // standby now
    case 0xe1: // idle immediate
    case 0xe2: // standby
    case 0xe3: // idle
    case 0xe6: // sleep
    case 0xe7: // flush cache
    case 0xea: // flush cache ext
      SEL_STATUS(index).busy = false;
      SEL_STATUS(index).drive_ready = true;
      SEL_STATUS(index).drq = false;
      SEL_STATUS(index).err = false;
      SEL_COMMAND(index).command_in_progress = false;
      raise_interrupt(index);
      break;

    default: // unknown/unhandled ATA command
      ide_status(index);
      FAILURE_1(NotImplemented, "Unknown IDE command %x",
                SEL_COMMAND(index).current_command);
      break;
    }

#ifdef DEBUG_IDE_COMMAND
    if (SEL_COMMAND(index).command_in_progress == false) {
      printf("%%IDE-I-COMMAND: Command has completed on controller %d.\n",
             index);
      ide_status(index);
      printf("==================================================\n");
    } else {
      printf("%%IDE-I-COMMAND: controller %d is yielding to host.\n", index);
      ide_status(index);
      printf("--------------------------------------------------\n");
    }
#endif
  }

  SEL_COMMAND(index).command_cycle++;
}

int CAliM1543C_ide::do_dma_transfer(int index, u8 *buffer, u32 buffersize,
                                    bool direction) {
  u8 xfer;
  size_t xfersize = 0;
  u8 status = 0;
  u8 count = 0;
  u32 prd;
  semBusMaster[index]->wait(); // wait until the start bit is set.
  {
    SCOPED_READ_LOCK(mtBusMaster[index]);
    prd = endian_32(*(u32 *)(&CONTROLLER(index).busmaster[4]));
  }
  do {
    u32 base;
    do_pci_read(prd, &base, 4, 1);

    u16 size_16;
    do_pci_read(prd + 4, &size_16, 2, 1);

    size_t size = size_16 ? size_16 : 65536;
    do_pci_read(prd + 7, &xfer, 1, 1);

#ifdef DEBUG_IDE_DMA
    printf("-IDE-I-DMA: Transfer %d bytes to/from %lx (%x)\n", size, base,
           xfer);
#endif
    if (xfersize + size > buffersize) {
      // only copy as much data as we have from the disk.
      size = buffersize - xfersize;
      status = 2;
#ifdef DEBUG_IDE_DMA
      printf("-IDE-I-DMA: Actual transfer size: %d bytes\n", size);
#endif
    }

    // copy it to/from ram.
    if (!direction) {
      do_pci_write(base, buffer, 1, size);
      buffer += size;
    } else {
      do_pci_read(base, buffer, 1, size);
      buffer += size;
    }

    xfersize += size;
    prd += 8; // go to next entry.
    if (xfer == 0x80 && xfersize < buffersize) {

      // we still have disk data left over!
      status = 1;
    }

    if (count++ > 32) {
      FAILURE(InvalidArgument, "Too many PRD nodes?");
    }

    if (buffersize == xfersize && xfer != 0x80) {
      // we're done, but there's more prd nodes.
      status = 2;
    }
  } while (xfer != 0x80 && status == 0);

  switch (status) {
  case 0: // normal completion.
  {
    SCOPED_WRITE_LOCK(mtBusMaster[index]);
    CONTROLLER(index).busmaster[2] &= 0xfe; // clear active.
  }

    raise_interrupt(index);
    break;

  case 1: // PRD is smaller than the data we have.
  {
    SCOPED_WRITE_LOCK(mtBusMaster[index]);
    CONTROLLER(index).busmaster[2] &= 0xfe; // clear active.
  }

  // do not raise an interrupt
  break;

  case 2: // PRD is larger than the data we have.
    // leave active set.
    raise_interrupt(index);
    break;
  }

  semBusMasterReady[index]->set();
  return status;
}

/**
 * Thread entry point.
 **/
void CAliM1543C_ide::run(int index) {
  try {
    for (;;) {
      semController[index]->wait();
      if (StopThread)
        return;
      {
#ifdef DEBUG_IDE_THREADS
        printf("Thread %d: \n", index);
        ide_status(index);
#endif
        if (SEL_COMMAND(index).command_in_progress)
          execute(index);
        UPDATE_ALT_STATUS(index);

#ifdef IDE_YIELD_INTERRUPTS
        if (CONTROLLER(index).interrupt_pending) {
          {
            SCOPED_WRITE_LOCK(mtBusMaster[index]);
            CONTROLLER(index).busmaster[2] |= 0x04;
          }
          theAli->pic_interrupt(1, 6 + index);
        }
#endif
      }
      semControllerReady[index]->set();
    }
  }

  catch (CException &e) {
    printf("Exception in IDE thread: %s.\n", e.displayText().c_str());
    thrControllerDead[index].store(true);
    // Let the thread die...
  }
}
