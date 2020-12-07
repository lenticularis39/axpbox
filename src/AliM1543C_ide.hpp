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
 * Contains the definitions for the emulated Ali M1543C IDE chipset part.
 *
 * $Id: AliM1543C_ide.h,v 1.24 2008/05/31 15:47:08 iamcamiel Exp $
 *
 * X-1.24       Camiel Vanderhoeven                             31-MAY-2008
 *      Changes to include parts of Poco.
 *
 * X-1.23       Camiel Vanderhoeven                             29-APR-2008
 *      CDiskController is no longer a CPCIDevice. devices that are both
 *      should multiple inherit both.
 *
 * X-1.22       Camiel Vanderhoeven                             26-MAR-2008
 *      Fix compiler warnings.
 *
 * X-1.21       Camiel Vanderhoeven                             24-MAR-2008
 *      Comments.
 *
 * X-1.20       Brian Wheeler                                   20-MAR-2008
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
 * X-1.19       Brian Wheeler                                   17-MAR-2008
 *      Fix some CD-ROM issues.
 *
 * X-1.18       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.17       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.16       Camiel Vanderhoeven                             13-MAR-2008
 *      Create init(), start_threads() and stop_threads() functions.
 *
 * X-1.15       Brian Wheeler                                   12-MAR-2008
 *      Better DMA support.
 *
 * X-1.16       Camiel Vanderhoeven                             11-MAR-2008
 *      Named, debuggable mutexes.
 *
 * X-1.15       Brian Wheeler                                   11-MAR-2008
 *      Even nicer, more efficient multi-threading version.
 *
 * X-1.14       Brian Wheeler                                   05-MAR-2008
 *      Nicer, more efficient multi-threading version.
 *
 * X-1.13       Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.12       Camiel Vanderhoeven                             04-MAR-2008
 *      Merged Brian wheeler's New IDE code into the standard controller.
 *
 * X-1.11.4     Brian Wheeler                                   27-FEB-2008
 *      Attempts to refire the interrupt if the controller seems to have
 *      missed it -- before the OS declares a timeout.
 *
 * X-1.11.3     Camiel Vanderhoeven                             12-JAN-2008
 *      Use disk's SCSI engine for ATAPI devices.
 *
 * X-1.11.2      Brian wheeler                                  08-JAN-2008
 *      ATAPI improved.
 *
 * X-1.11.1      Brian wheeler                                  08-JAN-2008
 *      Complete rewrite of IDE controller.
 *
 * X-1.11       Camiel Vanderhoeven                             06-JAN-2008
 *      Leave changing the blocksize to the disk itself.
 *
 * X-1.10       Camiel Vanderhoeven                             02-JAN-2008
 *      Comments.
 *
 * X-1.9        Camiel Vanderhoeven                             28-DEC-2007
 *      Only delay IDE interrupts when NO_VMS is defined. (Need to fix this
 *		properly).
 *
 * X-1.8         Brian wheeler                                  19-DEC-2007
 *      Added basic ATAPI support.
 *
 * X-1.7         Brian wheeler                                  17-DEC-2007
 *      Delayed IDE interrupts. (NetBSD requirement)
 *
 * X-1.6        Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.5        Camiel Vanderhoeven                             12-DEC-2007
 *      Use disk controller base class.
 *
 * X-1.4        Camiel Vanderhoeven                             11-DEC-2007
 *      Cleanup.
 *
 * X-1.3        Camiel Vanderhoeven                             11-DEC-2007
 *      More complete IDE implementation allows NetBSD to recognize disks.
 *
 * X-1.2        Brian Wheeler                                   10-DEC-2007
 *      Fixed include case.
 *
 * X-1.1        Camiel Vanderhoeven                             10-DEC-2007
 *      Initial version in CVS; this part was split off from the CAliM1543C
 *      class.
 **/
#if !defined(INCLUDED_ALIM1543C_IDE_H_)
#define INCLUDED_ALIM1543C_IDE_H_

// If DEBUG_IDE is defined, define all IDE debugging flags.
#ifdef DEBUG_IDE
#define DEBUG_IDE_BUSMASTER
#define DEBUG_IDE_COMMAND
#define DEBUG_IDE_DMA
#define DEBUG_IDE_INTERRUPT
#define DEBUG_IDE_REG_COMMAND
#define DEBUG_IDE_REG_CONTROL
#define DEBUG_IDE_PACKET
#define DEBUG_IDE_MULTIPLE
#endif

#include "Configurator.hpp"
#include "DiskController.hpp"
#include "PCIDevice.hpp"
#include "SCSIBus.hpp"
#include "SCSIDevice.hpp"

#define MAX_MULTIPLE_SECTORS 128

/**
 * \brief Emulated IDE part of ALi M1543C multi-function device.
 *
 * Documentation consulted:
 *  - Ali M1543C B1 South Bridge Version 1.20
 *(http://mds.gotdns.com/sensors/docs/ali/1543dScb1-120.pdf)
 *  - AT Attachment with Packet Interface - 5 (ATA/ATAPI-5)
 *(http://www.t13.org/Documents/UploadedDocuments/project/d1321r3-ATA-ATAPI-5.pdf)
 *  - Programming Interface for Bus Master IDE COntroller
 *(http://suif.stanford.edu/%7Ecsapuntz/specs/idems100.ps)
 *  - T13-1153Dr18  ATA/ATAPI-4
 *  - Mt. Fuji Commands for Multimedia Devices Version 7 INF-8090i v7
 *  .
 **/
class CAliM1543C_ide : public CPCIDevice,
                       public CDiskController,
                       public CSCSIDevice {
public:
  CAliM1543C_ide(CConfigurator *cfg, class CSystem *c, int pcibus, int pcidev);
  virtual ~CAliM1543C_ide();
  virtual void register_disk(class CDisk *dsk, int bus, int dev);

  virtual void WriteMem_Legacy(int index, u32 address, int dsize, u32 data);
  virtual u32 ReadMem_Legacy(int index, u32 address, int dsize);

  virtual void WriteMem_Bar(int func, int bar, u32 address, int dsize,
                            u32 data);
  virtual u32 ReadMem_Bar(int func, int bar, u32 address, int dsize);

  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);

  virtual void check_state();
  virtual void ResetPCI();

  void run(int index);
  virtual void init();
  virtual void start_threads();
  virtual void stop_threads();

private:
  // IDE controller
  u32 ide_command_read(int channel, u32 address, int dsize);
  void ide_command_write(int channel, u32 address, int dsize, u32 data);
  u32 ide_control_read(int channel, u32 address);
  void ide_control_write(int channel, u32 address, u32 data);
  u32 ide_busmaster_read(int channel, u32 address, int dsize);
  void ide_busmaster_write(int channel, u32 address, u32 data, int dsize);
  int do_dma_transfer(int index, u8 *buffer, u32 size, bool direction);

  void raise_interrupt(int channel);
  void set_signature(int channel, int id);
  u8 get_status(int index);
  void command_aborted(int index, u8 command);
  void identify_drive(int index, bool packet);
  void ide_status(int index);

  void execute(int index);

  std::unique_ptr<std::thread> thrController[2];
  std::atomic_bool thrControllerDead[2] = {{false}, {false}};
  CSemaphore *semController[2];      // controller start/stop
  CSemaphore *semControllerReady[2]; // controller ready
  CSemaphore *semBusMaster[2];       // bus master start/stop
  CSemaphore *semBusMasterReady[2];  // bus master ready
  CRWLock *mtRegisters[2];           // main registers
  CRWLock *mtBusMaster[2];           // busmaster registers
  bool StopThread;

  bool usedma;

  // The state structure contains all elements that need to be saved to the
  // statefile.
  struct SAliM1543C_ideState {
    struct SDriveState {
      struct {
        bool busy;
        bool drive_ready;
        bool fault;
        bool seek_complete;
        bool drq;
        bool bit_2;
        bool index_pulse;
        bool err;
        int index_pulse_count;

        // debugging
        u8 debug_last_status;
        bool debug_status_update;

        u8 alt_status; // this is the latched status.
      } status;

      struct {
        bool lba_mode;
        int features;
        int error;
        int sector_count;
        int sector_no;
        int cylinder_no;
        int head_no;
        int command;
      } registers;

      struct {
        bool command_in_progress;
        int current_command;
        int command_cycle;
        bool packet_dma;
        int packet_phase;
        u8 packet_command[12];
        int packet_buffersize;
        u8 packet_sense;
        u8 packet_asc;
        u8 packet_ascq;
      } command;

      u8 multiple_size;
    };

    struct SControllerState {

      // the attached devices
      struct SDriveState drive[2];

      // control data.
      bool disable_irq;
      bool reset;

      // internal state
      bool reset_in_progress;
      int selected;

      // dma stuff
      u8 busmaster[8];
      u8 dma_mode;
      u8 bm_status;

      // pio stuff
#define IDE_BUFFER_SIZE 65536 // 64K words = 128K = 256 sectors @ 512 bytes
      u16 data[IDE_BUFFER_SIZE];
      int data_ptr;
      int data_size;

      bool interrupt_pending;
    } controller[2];
  } state;
};

/// Status for selected drive on controller a
#define SEL_STATUS(a)                                                          \
  state.controller[a].drive[state.controller[a].selected].status

/// Command for selected drive on controller a
#define SEL_COMMAND(a)                                                         \
  state.controller[a].drive[state.controller[a].selected].command

/// Registers for selected drive on controller a
#define SEL_REGISTERS(a)                                                       \
  state.controller[a].drive[state.controller[a].selected].registers

/// Selected drive on controller a
#define SEL_DISK(a) get_disk(a, state.controller[a].selected)

/// Per-drive data for selected drive on controller a
#define SEL_PER_DRIVE(a) state.controller[a].drive[state.controller[a].selected]

// Status for drive b on controller a
#define STATUS(a, b) state.controller[a].drive[b].status

// Command for drive b on controller a
#define COMMAND(a, b) state.controller[a].drive[b].command

// Registers for drive b on controller a
#define REGISTERS(a, b) state.controller[a].drive[b].registers

// Per-drive data for drive b on controller a
#define PER_DRIVE(a, b) state.controller[a].drive[b]

// Data for controller a
#define CONTROLLER(a) state.controller[a]

// Update alt-status for controller a with locking
#define UPDATE_ALT_STATUS(a)                                                   \
  {                                                                            \
    SCOPED_WRITE_LOCK(mtRegisters[a]);                                         \
    SEL_STATUS(a).alt_status = get_status(a);                                  \
  }

/* memory region ids */
#define PRI_COMMAND 1
#define PRI_CONTROL 2
#define SEC_COMMAND 3
#define SEC_CONTROL 4
#define PRI_BUSMASTER 5
#define SEC_BUSMASTER 6

/* bar IDs */
#define BAR_PRI_COMMAND 0
#define BAR_PRI_CONTROL 1
#define BAR_SEC_COMMAND 2
#define BAR_SEC_CONTROL 3
#define BAR_BUSMASTER 4

/* device registers */
#define REG_COMMAND_DATA 0
#define REG_COMMAND_ERROR 1
#define REG_COMMAND_FEATURES 1
#define REG_COMMAND_SECTOR_COUNT 2
#define REG_COMMAND_SECTOR_NO 3
#define REG_COMMAND_CYL_LOW 4
#define REG_COMMAND_CYL_HI 5
#define REG_COMMAND_DRIVE 6
#define REG_COMMAND_STATUS 7
#define REG_COMMAND_COMMAND 7

static const char *register_names[] = {
    "DATA",      "ERROR/FEATURES",       "SECTOR_COUNT/PKT REASON",
    "SECTOR_NO", "CYL_LOW/PKT BYTE LOW", "CYL_HI/PKT BYTE HI",
    "DRIVE",     "STATUS/COMMAND",
};

/* misc constants */

/* Packet Protocol Aliases */
#define DMRD fault
#define SERV seek_complete
#define CHK err
#define BYTE_COUNT cylinder_no
#define REASON sector_count
#define IR_CD 0x01
#define IR_IO 0x02
#define IR_REL 0x04

/* Packet protocol states */
static const char *packet_states[] = {
    "DP0: Prepare A",         "DP1: Receive Packet",
    "DP2: Prepare B",         "DP3/4: Ready INITRQ/Transfer Data",
    "DIx: Device Interrupt ",
};

#define PACKET_NONE 0
#define PACKET_DP0 0
#define PACKET_DP1 1
#define PACKET_DP2 2
#define PACKET_DP34 3
#define PACKET_DI 4

/* SCSI SENSE Constants */
#define SENSE_NONE 0x00
#define SENSE_RECOVERED_ERROR 0x01
#define SENSE_NOT_READY 0x02
#define SENSE_MEDIUM_ERROR 0x03
#define SENSE_HARDWARE_ERROR 0x04
#define SENSE_ILLEGAL_REQUEST 0x05
#define SENSE_UNIT_ATTENTION 0x06
#define SENSE_DATA_PROTECT 0x07
#define SENSE_BLANK_CHECK 0x08
#define SENSE_ABORT_COMMAND 0x0b
#define SENSE_MISCOMPARE 0x0e

extern CAliM1543C_ide *theIDE;

#endif
