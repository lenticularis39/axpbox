/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * WWW    : http://www.es40.org
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
 * Contains the code for the emulated Symbios SCSI controller.
 *
 * $Id: Sym53C810.cpp,v 1.14 2008/05/31 15:47:13 iamcamiel Exp $
 *
 * X-1.14       Camiel Vanderhoeven                             31-MAY-2008
 *      Changes to include parts of Poco.
 *
 * X-1.13       Camiel Vanderhoeven                             29-APR-2008
 *      CDiskController is no longer a CPCIDevice. devices that are both
 *      should multiple inherit both.
 *
 * X-1.12       Camiel Vanderhoeven                             25-MAR-2008
 *      Separate functions for different instructions, comments.
 *
 * X-1.11       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.10       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.9        Camiel Vanderhoeven                             13-MAR-2008
 *      Create init(), start_threads() and stop_threads() functions.
 *
 * X-1.8        Camiel Vanderhoeven                             11-MAR-2008
 *      Named, debuggable mutexes.
 *
 * X-1.7        Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.5        Brian Wheeler                                   27-FEB-2008
 *      Avoid compiler warnings.
 *
 * X-1.4        Camiel Vanderhoeven                             18-FEB-2008
 *      Debugging info on xfer size made conditional.
 *
 * X-1.3        Camiel Vanderhoeven                             17-FEB-2008
 *      Debugging info.
 *
 * X-1.2        Camiel Vanderhoeven                             16-FEB-2008
 *      Unique names for PCI config arrays.
 *
 * X-1.1        Camiel Vanderhoeven                             16-FEB-2008
 *      Created as a spinoff from 53C895 controller, as we couldn't get
 *      that chip to work properly with the OpenVMS driver.
 **/
#if defined(DEBUG_SYM)
#define DEBUG_SYM_REGS
#define DEBUG_SYM_SCRIPTS
#endif
#include "Sym53C810.h"
#include "Disk.h"
#include "SCSIBus.h"
#include "StdAfx.h"
#include "System.h"

/// Register 00: SCNTL0: SCSI Control 0
#define R_SCNTL0 0x00
#define R_SCNTL0_ARB1 0x80
#define R_SCNTL0_ARB0 0x40
#define R_SCNTL0_START 0x20
#define R_SCNTL0_WATN 0x10
#define R_SCNTL0_EPC 0x08
#define R_SCNTL0_AAP 0x02
#define R_SCNTL0_TRG 0x01
#define SCNTL0_MASK 0xFB

/// Register 01: SCNTL1: SCSI Control 1
#define R_SCNTL1 0x01
#define R_SCNTL1_CON 0x10
#define R_SCNTL1_RST 0x08
#define R_SCNTL1_IARB 0x02

/// Register 02: SCNTL2: SCSI Control 2
#define R_SCNTL2 0x02
#define R_SCNTL2_SDU 0x80
#define SCNTL2_MASK 0x80

/// Register 03: SCNTL3: SCSI Control 3
#define R_SCNTL3 0x03
#define SCNTL3_MASK 0x77

/// Register 04: SCID: SCSI Chip ID
#define R_SCID 0x04
#define R_SCID_ID 0x07
#define SCID_MASK 0x67

/// Register 05: SXFER: SCSI Transfer
#define R_SXFER 0x05

/// Register 06: SDID: SCSI Destination ID
#define R_SDID 0x06
#define R_SDID_ID 0x07
#define SDID_MASK 0x07

/// Register 07: GPREG: General Purpose
#define R_GPREG 0x07
#define GPREG_MASK 0x03

/// Register 08: SFBR: SCSI First Byte REceived
#define R_SFBR 0x08

/// Register 09: SOCL: SCSI Output Control Latch
#define R_SOCL 0x09
#define R_SOCL_ACK 0x40
#define R_SOCL_ATN 0x20

/// Register 0A: SSID: SCSI Selector ID
#define R_SSID 0x0A
#define R_SSID_VAL 0x80
#define R_SSID_ID 0x07

/// Register 0B: SBCL: SCSI Bus Control Lines
#define R_SBCL 0x0B
#define R_SBCL_REQ 0x80
#define R_SBCL_ACK 0x40
#define R_SBCL_BSY 0x20
#define R_SBCL_SEL 0x10
#define R_SBCL_ATN 0x08
#define R_SBCL_MSG 0x04
#define R_SBCL_CD 0x02
#define R_SBCL_IO 0x01
#define R_SBCL_PHASE 0x07

/// Register 0C: DSTAT: DMA Status
#define R_DSTAT 0x0C
#define R_DSTAT_DFE 0x80
#define R_DSTAT_MDPE 0x40
#define R_DSTAT_BF 0x20
#define R_DSTAT_ABRT 0x10
#define R_DSTAT_SSI 0x08
#define R_DSTAT_SIR 0x04
#define R_DSTAT_IID 0x01
#define DSTAT_RC 0x7D
#define DSTAT_FATAL 0x7D

/// Register 0D: SSTAT0: SCSI Status 0
#define R_SSTAT0 0x0D
#define R_SSTAT0_RST 0x02
#define R_SSTAT0_SDP0 0x01

/// Register 0E: SSTAT1: SCSI Status 1
#define R_SSTAT1 0x0E
#define R_SSTAT1_SDP1 0x01

/// Register 0F: SSTAT2: SCSI Status 2
#define R_SSTAT2 0x0F
#define R_SSTAT2_LDSC 0x02

/// Register 10..13: DSA: Data Structure Address
#define R_DSA 0x10

/// Register 14: ISTAT: Interrupt Status
#define R_ISTAT 0x14
#define R_ISTAT_ABRT 0x80
#define R_ISTAT_SRST 0x40
#define R_ISTAT_SIGP 0x20
#define R_ISTAT_SEM 0x10
#define R_ISTAT_CON 0x08
#define R_ISTAT_INTF 0x04
#define R_ISTAT_SIP 0x02
#define R_ISTAT_DIP 0x01
#define ISTAT_MASK 0xF0
#define ISTAT_W1C 0x04

/// Register 18: CTEST0: Chip Test 0
#define R_CTEST0 0x18

/// Register 19: CTEST1: Chip Test 1
#define R_CTEST1 0x19
#define R_CTEST1_FMT 0xF0
#define R_CTEST1_FFL 0x0F

/// Register 1A: CTEST2: Chip Test 2
#define R_CTEST2 0x1A
#define R_CTEST2_DDIR 0x80
#define R_CTEST2_SIGP 0x40
#define R_CTEST2_CIO 0x20
#define R_CTEST2_CM 0x10
#define R_CTEST2_TEOP 0x04
#define R_CTEST2_DREQ 0x02
#define R_CTEST2_DACK 0x01

/// Register 1B: CTEST3: Chip Test 3
#define R_CTEST3 0x1B
#define R_CTEST3_REV 0xf0
#define R_CTEST3_FLF 0x08
#define R_CTEST3_CLF 0x04
#define R_CTEST3_FM 0x02
#define CTEST3_MASK 0x0B

/// Register 1C..1F: TEMP: Temporary
#define R_TEMP 0x1C

/// Register 20: DFIFO: DMA FIFO
#define R_DFIFO 0x20

/// Register 21: CTEST4: Chip Test 4
#define R_CTEST4 0x21

/// Register 22: CTEST5: Chip Test 5
#define R_CTEST5 0x22
#define R_CTEST5_ADCK 0x80
#define R_CTEST5_BBCK 0x40
#define CTEST5_MASK 0x18

/// Register 24..26: DBC: DMA Byte Counter
#define R_DBC 0x24

/// Register 27: DCMD: DMA Command
#define R_DCMD 0x27

/// Register 28..2B: DNAD: DMA Next Address
#define R_DNAD 0x28

/// Register 2C..2F: DSP: DMA SCRIPTS Pointer
#define R_DSP 0x2C

/// Register 30..33: DSPS: DMA SCRIPTS Pointer Save
#define R_DSPS 0x30

/// Register 34..37: SCRATCHA: Scratch Register A
#define R_SCRATCHA 0x34

/// Register 38: DMODE: DMA Mode
#define R_DMODE 0x38
#define R_DMODE_MAN 0x01

/// Register 39: DIEN: DMA Interrupt Enable
#define R_DIEN 0x39
#define DIEN_MASK 0x7D

/// Register 3A: SBR: Scratch Byte Register
#define R_SBR 0x3A

/// Register 3B: DCNTL: DMA Control
#define R_DCNTL 0x3B
#define R_DCNTL_SSM 0x10
#define R_DCNTL_STD 0x04
#define R_DCNTL_IRQD 0x02
#define R_DCNTL_COM 0x01
#define DCNTL_MASK 0xFB

/// Register 3C..37: ADDER: Adder Sum Output
#define R_ADDER 0x3C

/// Register 40: SIEN0: SCSI Interrupt Enable 0
#define R_SIEN0 0x40
#define SIEN0_MASK 0xFF

/// Register 41: SIEN1: SCSI Interrupt Enable 1
#define R_SIEN1 0x41
#define SIEN1_MASK 0x07

/// Register 42: SIST0: SCSI Interrupt Status 0
#define R_SIST0 0x42
#define R_SIST0_MA 0x80
#define R_SIST0_CMP 0x40
#define R_SIST0_SEL 0x20
#define R_SIST0_RSL 0x10
#define R_SIST0_SGE 0x08
#define R_SIST0_UDC 0x04
#define R_SIST0_RST 0x02
#define R_SIST0_PAR 0x01
#define SIST0_RC 0xFF
#define SIST0_FATAL 0x8F

/// Register 43: SIST1: SCSI Interrupt Status 1
#define R_SIST1 0x43
#define R_SIST1_STO 0x04
#define R_SIST1_GEN 0x02
#define R_SIST1_HTH 0x01
#define SIST1_RC 0x07
#define SIST1_FATAL 0x04

/// Register 46: MACNTL: Memory Access Control
#define R_MACNTL 0x46
#define MACNTL_MASK 0x0F

/// Register 47: GPCNTL: General Purpose Pin Control
#define R_GPCNTL 0x47

/// Register 48: STIME0: SCSI Timer 0
#define R_STIME0 0x48

/// Register 49: STIME1: SCSI Timer 1
#define R_STIME1 0x49
#define R_STIME1_GEN 0x0F
#define STIME1_MASK 0x0F

/// Register 4A: RESPID: SCSI Response ID
#define R_RESPID 0x4A

/// Register 4C: STEST0: SCSI Test 0
#define R_STEST0 0x4C

/// Register 4D: STEST1: SCSI Test 1
#define R_STEST1 0x4D
#define STEST1_MASK 0xC0

/// Register 4E: STEST2: SCSI Test 2
#define R_STEST2 0x4E
#define R_STEST2_SCE 0x80
#define R_STEST2_ROF 0x40
#define R_STEST2_SLB 0x10
#define R_STEST2_SZM 0x08
#define R_STEST2_EXT 0x02
#define R_STEST2_LOW 0x01
#define STEST2_MASK 0x9B

/// Register 4F: STEST3: SCSI Test 3
#define R_STEST3 0x4F
#define R_STEST3_TE 0x80
#define R_STEST3_STR 0x40
#define R_STEST3_HSC 0x20
#define R_STEST3_DSI 0x10
#define R_STEST3_TTM 0x04
#define R_STEST3_CSF 0x02
#define R_STEST3_STW 0x01
#define STEST3_MASK 0xF7

/// Register 58: SBDL: SCSI Bus Data Lines
#define R_SBDL 0x58

/// Registers 5C..5F: SCRATCHB: Scratch Register B
#define R_SCRATCHB 0x5C

/// Acces an 8-byte register
#define R8(a) state.regs.reg8[R_##a]

/// Acces a 16-byte register
#define R16(a) state.regs.reg16[R_##a / 2]

/// Access a 32-byte register
#define R32(a) state.regs.reg32[R_##a / 4]

/**
 * Test bit in register
 *
 * \param a is the name of the register
 * \param b is the name of the bit
 **/
#define TB_R8(a, b) ((R8(a) & R_##a##_##b) == R_##a##_##b)

/**
 * Set bit in register.
 *
 * \param a is the name of the register
 * \param b is the name of the bit
 * \param c is the value for the bit
 **/
#define SB_R8(a, b, c) R8(a) = (R8(a) & ~R_##a##_##b) | (c ? R_##a##_##b : 0)

/**
 * Write to a register, using a mask
 *
 * \param a is the name of the register
 * \param b is the value to write.
 *
 * Only those bits that are set to 1 in <regname>_MASK will be changed.
 **/
#define WRM_R8(a, b) R8(a) = (R8(a) & ~a##_MASK) | ((b)&a##_MASK)

/**
 * Write to a register, using a mask, and using write-1-to-clear bits
 *
 * \param a is the name of the register
 * \param b is the value to write.
 *
 * Only those bits that are set to 1 in <regname>_MASK will be changed.
 * In addition, bits that are set to 1 in <regname>_W1C will be cleared
 * in the register if they are set to 1 in the value.
 **/
#define WRMW1C_R8(a, b)                                                        \
  R8(a) =                                                                      \
      (R8(a) & ~a##_MASK & ~a##_W1C) | ((b)&a##_MASK) | (R8(a) & ~(b)&a##_W1C)

/**
 * Raise an interrupt
 *
 * \param a is the name of the interrupt register
 * \param b is the name of the bit to set
 **/
#define RAISE(a, b) set_interrupt(R_##a, R_##a##_##b)

/**
 * Clear read-to-clear-bits
 *
 * \param a is the name of the register
 *
 * Clear bits that are set to 1 in <regname>_RC
 **/
#define RDCLR_R8(a) R8(a) &= ~a##_RC

/**
 * Get the SCSI destination ID from the SDID register
 **/
#define GET_DEST() (R8(SDID) & R_SCID_ID)

/**
 * Set the SCSI destination ID in the SDID register
 **/
#define SET_DEST(a) R8(SDID) = (a)&R_SCID_ID

/**
 * Get the value of the DBC register (24-bits)
 **/
#define GET_DBC() (R32(DBC) & 0x00ffffff)

/**
 * Set the value of the DBC register (24-bits)
 **/
#define SET_DBC(a) R32(DBC) = (R32(DBC) & 0xff000000) | ((a)&0x00ffffff)

/**
 * PCI Configuration Data Block
 **/
static u32 osym_cfg_data[64] = {
    /*00*/ 0x00011000, // CFID: vendor + device
    /*04*/ 0x02000001, // CFCS: command + status
    /*08*/ 0x01000001, // CFRV: class + revision
    /*0c*/ 0x00000000, // CFLT: latency timer + cache line size
    /*10*/ 0x00000001, // BAR0: IO Space
    /*14*/ 0x00000000, // BAR1: Memory space
    /*18*/ 0x00000000, // BAR2: RAM space
    /*1c*/ 0x00000000, // BAR3:
    /*20*/ 0x00000000, // BAR4:
    /*24*/ 0x00000000, // BAR5:
    /*28*/ 0x00000000, // CCIC: CardBus
    /*2c*/ 0x00000000, // CSID: subsystem + vendor
    /*30*/ 0x00000000, // BAR6: expansion rom base
    /*34*/ 0x00000000, // CCAP: capabilities pointer
    /*38*/ 0x00000000,
    /*3c*/ 0x401101ff, // CFIT: interrupt configuration
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
 * PCI Configuration Mask Block
 **/
static u32 osym_cfg_mask[64] = {
    /*00*/ 0x00000000, // CFID: vendor + device
    /*04*/ 0x00000157, // CFCS: command + status
    /*08*/ 0x00000000, // CFRV: class + revision
    /*0c*/ 0x0000ffff, // CFLT: latency timer + cache line size
    /*10*/ 0xffffff00, // BAR0: IO space (256 bytes)
    /*14*/ 0xffffff00, // BAR1: Memory space (256 bytes)
    /*18*/ 0x00000000, // BAR2: RAM space (4KB)
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

/**
 * Thread entry point.
 *
 * Repeat:
 *   - Waiting until the semaphore is set
 *   - Executing SCRIPTS code until execution ends.
 *   .
 **/
void CSym53C810::run() {
  try {
    for (;;) {
      mySemaphore.wait();
      if (StopThread)
        return;
      while (state.executing) {
        MUTEX_LOCK(myRegLock);
        execute();
        MUTEX_UNLOCK(myRegLock);
      }
    }
  }

  catch (CException &e) {
    printf("Exception in SYM thread: %s.\n", e.displayText().c_str());

    // Let the thread die...
  }
}

/**
 * Constructor.
 *
 * Set up the SCSI bus, and defer the rest of initialization to
 * CSym53C895::init.
 **/
CSym53C810::CSym53C810(CConfigurator *cfg, CSystem *c, int pcibus, int pcidev)
    : CPCIDevice(cfg, c, pcibus, pcidev), CDiskController(1, 7),
      mySemaphore(0, 1) {

  // create scsi bus
  CSCSIBus *a = new CSCSIBus(cfg, c);
  scsi_register(0, a, 7); // scsi id 7 by default
}

/**
 * Initialize the Symbios device.
 *
 * Reset PCI structures, reset the chipset, and set up locks.
 **/
void CSym53C810::init() {
  add_function(0, osym_cfg_data, osym_cfg_mask);

  ResetPCI();

  chip_reset();

  myRegLock = new CMutex("sym-reg");

  myThread = 0;

  printf("%s: $Id: Sym53C810.cpp,v 1.14 2008/05/31 15:47:13 iamcamiel Exp $\n",
         devid_string);
}

/**
 * Create the thread, and start executing it.
 **/
void CSym53C810::start_threads() {
  if (!myThread) {
    myThread = new CThread("sym");
    printf(" %s", myThread->getName().c_str());
    StopThread = false;
    myThread->start(*this);
    if (state.executing)
      mySemaphore.set();
  }
}

/**
 * Stop and destroy the thread.
 **/
void CSym53C810::stop_threads() {
  StopThread = true;
  if (myThread) {
    printf(" %s", myThread->getName().c_str());
    mySemaphore.set();
    myThread->join();
    delete myThread;
    myThread = 0;
  }
}

/**
 * Destructor.
 *
 * Kill thread if still running, and destroy the SCSI bus.
 **/
CSym53C810::~CSym53C810() {
  stop_threads();
  delete scsi_bus[0];
}

/**
 * Reset the chipset.
 *
 * Initialize all registers to their default values.
 **/
void CSym53C810::chip_reset() {
  state.executing = false;
  state.wait_reselect = false;
  state.irq_asserted = false;
  state.gen_timer = 0;
  memset(state.regs.reg32, 0, sizeof(state.regs.reg32));
  R8(SCNTL0) = R_SCNTL0_ARB1 | R_SCNTL0_ARB0; // 810
  R8(DSTAT) = R_DSTAT_DFE;                    // DMA FIFO empty // 810

  //  R8(SSTAT2) = R_SSTAT2_LDSC; // 810
  R8(CTEST1) = R_CTEST1_FMT;  // 810
  R8(CTEST2) = R_CTEST2_DACK; // 810
  R8(CTEST3) =
      (u8)(pci_state.config_data[0][2] << 4) & R_CTEST3_REV; // Chip rev.
  R8(MACNTL) = 0x40;                                         // 810 type ID
  R8(GPCNTL) = 0x0F;                                         // 810
  R8(STEST0) = 0x03;                                         // 810
}

/**
 * Register a disk
 *
 * Attach the disk to the SCSI bus.
 **/
void CSym53C810::register_disk(class CDisk *dsk, int bus, int dev) {
  CDiskController::register_disk(dsk, bus, dev);
  dsk->scsi_register(0, scsi_bus[0], dev);
}

static u32 sym_magic1 = 0x53C810CC;
static u32 sym_magic2 = 0xCC53C810;

/**
 * Save state to a Virtual Machine State file.
 **/
int CSym53C810::SaveState(FILE *f) {
  long ss = sizeof(state);
  int res;

  if (res = CPCIDevice::SaveState(f))
    return res;

  fwrite(&sym_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&sym_magic2, sizeof(u32), 1, f);
  printf("%s: %d bytes saved.\n", devid_string, (int)ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CSym53C810::RestoreState(FILE *f) {
  long ss;
  u32 m1;
  u32 m2;
  int res;
  size_t r;

  if (res = CPCIDevice::RestoreState(f))
    return res;

  r = fread(&m1, sizeof(u32), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (m1 != sym_magic1) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  fread(&ss, sizeof(long), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (ss != sizeof(state)) {
    printf("%s: STRUCT SIZE does not match!\n", devid_string);
    return -1;
  }

  fread(&state, sizeof(state), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  r = fread(&m2, sizeof(u32), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (m2 != sym_magic2) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  printf("%s: %d bytes restored.\n", devid_string, (int)ss);
  return 0;
}

/**
 * write data to one of the PCI BAR (relocatable) address ranges.
 **/
void CSym53C810::WriteMem_Bar(int func, int bar, u32 address, int dsize,
                              u32 data) {
  void *p;

  switch (bar) {
  case 0:
  case 1:
    address &= 0x7f;
    switch (dsize) {
    case 8:
      MUTEX_LOCK(myRegLock);
#if defined(DEBUG_SYM_REGS)
      printf("SYM: Write to register %02x: %02x.   \n", address, data);
#endif
      if (address >= R_SCRATCHB) {
        state.regs.reg8[address] = (u8)data;
        MUTEX_UNLOCK(myRegLock);
        break;
      }

      switch (address) {

      // SIMPLE CASES: JUST WRITE
      case R_SXFER:        // 05
      case R_DSA:          // 10
      case R_DSA + 1:      // 11
      case R_DSA + 2:      // 12
      case R_DSA + 3:      // 13
      case R_CTEST0:       // 18
      case R_TEMP:         // 1C
      case R_TEMP + 1:     // 1D
      case R_TEMP + 2:     // 1E
      case R_TEMP + 3:     // 1F
      case R_DSP:          // 2C
      case R_DSP + 1:      // 2D
      case R_DSP + 2:      // 2E
      case R_DSPS:         // 30
      case R_DSPS + 1:     // 31
      case R_DSPS + 2:     // 32
      case R_DSPS + 3:     // 33
      case R_SCRATCHA:     // 34
      case R_SCRATCHA + 1: // 35
      case R_SCRATCHA + 2: // 36
      case R_SCRATCHA + 3: // 37
      case R_DMODE:        // 38
      case R_SBR:          // 3A     // 810
      case R_GPCNTL:       // 47
      case R_STIME0:       // 48
      case R_RESPID:       // 4A
      case R_STEST0:       // 4C
        state.regs.reg8[address] = (u8)data;
        break;

      case R_SCNTL0: // 00
        // side effects: start arbitration bit
        write_b_scntl0((u8)data);
        break;

      case R_SCNTL1: // 01
        // side effects: start immediate arbitration bit
        write_b_scntl1((u8)data);
        break;

      case R_SCNTL2: // 02
        WRM_R8(SCNTL2, (u8)data);
        break;

      case R_SCNTL3: // 03
        // side effects: clearing EWS
        WRM_R8(SCNTL3, (u8)data);
        break;

      case R_SCID: // 04
        WRM_R8(SCID, (u8)data);
        break;

      case R_SDID: // 06
        WRM_R8(SDID, (u8)data);
        break;

      case R_GPREG: // 07
        WRM_R8(GPREG, (u8)data);
        break;

      case R_ISTAT: // 14
        write_b_istat((u8)data);
        break;

      case R_CTEST3: // 1B
        write_b_ctest3((u8)data);
        break;

      case R_CTEST4: // 21
        write_b_ctest4((u8)data);
        break;

      case R_CTEST5: // 22
        write_b_ctest5((u8)data);
        break;

      case R_DSP + 3: // 2F
        state.regs.reg8[address] = (u8)data;
        post_dsp_write();
        break;

      case R_DIEN: // 39
        WRM_R8(DIEN, (u8)data);
        eval_interrupts();
        break;

      case R_DCNTL: // 3B
        write_b_dcntl((u8)data);
        break;

      case R_SIEN0: // 40
        R8(SIEN0) = (u8)data;
        eval_interrupts();
        break;

      case R_SIEN1: // 41
        WRM_R8(SIEN1, (u8)data);
        eval_interrupts();
        break;

      case R_MACNTL: // 46     // 810
        WRM_R8(MACNTL, (u8)data);
        break;

      case R_STIME1: // 49
        WRM_R8(STIME1, (u8)data);
        state.gen_timer = (R8(STIME1) & R_STIME1_GEN) * 30;
        break;

      case R_STEST1: // 4D
        WRM_R8(STEST1, (u8)data);
        break;

      case R_STEST2: // 4E
        write_b_stest2((u8)data);
        break;

      case R_STEST3: // 4F
        write_b_stest3((u8)data);
        break;

      case R_DSTAT:  // 0C
      case R_SSTAT0: // 0D
      case R_SSTAT1: // 0E
      case R_SSTAT2: // 0F
      case R_CTEST2: // 1A
        // printf("SYM: Write to read-only register at %02x. FreeBSD driver
        // cache test.\n", address);
        break;

      case 0x4b: // ??? Linux wants this
        // printf("SYM: Write to non-existing register at %02x. Linux generic
        // driver.\n", address);
        break;

      default:
        FAILURE_2(NotImplemented,
                  "SYM: Write to unknown register at %02x with %08x.\n",
                  address, data);
      }

      MUTEX_UNLOCK(myRegLock);
      break;

    case 16:
      WriteMem_Bar(0, 1, address + 0, 8, (data >> 0) & 0xff);
      WriteMem_Bar(0, 1, address + 1, 8, (data >> 8) & 0xff);
      break;

    case 32:
      WriteMem_Bar(0, 1, address + 0, 8, (data >> 0) & 0xff);
      WriteMem_Bar(0, 1, address + 1, 8, (data >> 8) & 0xff);
      WriteMem_Bar(0, 1, address + 2, 8, (data >> 16) & 0xff);
      WriteMem_Bar(0, 1, address + 3, 8, (data >> 24) & 0xff);
      break;
    }
    break;

  case 2:
    p = (u8 *)state.ram + address;
    switch (dsize) {
    case 8:
      *((u8 *)p) = (u8)data;
      break;
    case 16:
      *((u16 *)p) = (u16)data;
      break;
    case 32:
      *((u32 *)p) = (u32)data;
      break;
    }
    break;
  }
}

/**
 * Read data from one of the PCI BAR (relocatable) address ranges.
 **/
u32 CSym53C810::ReadMem_Bar(int func, int bar, u32 address, int dsize) {
  u32 data = 0;
  void *p;

  switch (bar) {
  case 0:
  case 1:
    address &= 0x7f;
    switch (dsize) {
    case 8:
      MUTEX_LOCK(myRegLock);
      if (address >= R_SCRATCHB) {
        data = state.regs.reg8[address];
        MUTEX_UNLOCK(myRegLock);
        break;
      }

      switch (address) {
      case R_SCNTL0:       // 00
      case R_SCNTL1:       // 01
      case R_SCNTL2:       // 02
      case R_SCNTL3:       // 03
      case R_SCID:         // 04
      case R_SXFER:        // 05
      case R_SDID:         // 06
      case R_GPREG:        // 07
      case R_SFBR:         // 08
      case R_SSID:         // 0A
      case R_SBCL:         // 0B
      case R_SSTAT0:       // 0D
      case R_SSTAT1:       // 0E
      case R_SSTAT2:       // 0F
      case R_DSA:          // 10
      case R_DSA + 1:      // 11
      case R_DSA + 2:      // 12
      case R_DSA + 3:      // 13
      case R_ISTAT:        // 14
      case R_CTEST0:       // 18
      case R_CTEST1:       // 19
      case R_CTEST3:       // 1B
      case R_TEMP:         // 1C
      case R_TEMP + 1:     // 1D
      case R_TEMP + 2:     // 1E
      case R_TEMP + 3:     // 1F
      case R_CTEST4:       // 21
      case R_CTEST5:       // 22
      case R_DBC:          // 24  // 810
      case R_DBC + 1:      // 25  // 810
      case R_DBC + 2:      // 26  // 810
      case R_DCMD:         // 27  // 810
      case R_DNAD:         // 28  // 810
      case R_DNAD + 1:     // 29  // 810
      case R_DNAD + 2:     // 2A  // 810
      case R_DNAD + 3:     // 2B  // 810
      case R_DSP:          // 2C
      case R_DSP + 1:      // 2D
      case R_DSP + 2:      // 2E
      case R_DSP + 3:      // 2F
      case R_DSPS:         // 30
      case R_DSPS + 1:     // 31
      case R_DSPS + 2:     // 32
      case R_DSPS + 3:     // 33
      case R_SCRATCHA:     // 34
      case R_SCRATCHA + 1: // 35
      case R_SCRATCHA + 2: // 36
      case R_SCRATCHA + 3: // 37
      case R_DMODE:        // 38
      case R_DIEN:         // 39
      case R_SBR:          // 3A     // 810
      case R_DCNTL:        // 3B
      case R_SIEN0:        // 40
      case R_SIEN1:        // 41
      case R_MACNTL:       // 46     // 810
      case R_GPCNTL:       // 47
      case R_STIME0:       // 48
      case R_STIME1:       // 49
      case R_RESPID:       // 4A
      case R_STEST0:       // 4C
      case R_STEST1:       // 4D
      case R_STEST2:       // 4E
      case R_STEST3:       // 4F
      case R_SBDL:         // 58
        data = state.regs.reg8[address];
        break;

      case R_DSTAT: // 0C
        data = read_b_dstat();
        break;

      case R_CTEST2: // 1A
        data = read_b_ctest2();
        break;

      case R_DFIFO:            // 20
        data = R8(DBC) & 0x7f; // 810 - fake the DFIFO count
        break;

      case R_SIST0: // 42
      case R_SIST1: // 43
        data = read_b_sist(address - R_SIST0);
        break;

      case 0x17: // ??? Linux wants this.
      case 0x4b: // ??? Linux wants this
      case 0x52: // ??? Linux wants this.
      case 0x59: // ??? Linux wants this.
        // printf("SYM: Read from non-existing register at %02x. Linux generic
        // driver.\n", address);
        data = 0;
        break;

      default:
        FAILURE_2(NotImplemented,
                  "SYM: Attempt to read from unknown register at %02x\n", dsize,
                  address);
      }

      MUTEX_UNLOCK(myRegLock);
#if defined(DEBUG_SYM_REGS)
      printf("SYM: Read frm register %02x: %02x.   \n", address, data);
#endif
      break;

    case 16:
      data = (ReadMem_Bar(0, 1, address + 0, 8) << 0) & 0x00ff;
      data |= (ReadMem_Bar(0, 1, address + 1, 8) << 8) & 0xff00;
      break;

    case 32:
      data = (ReadMem_Bar(0, 1, address + 0, 8) << 0) & 0x000000ff;
      data |= (ReadMem_Bar(0, 1, address + 1, 8) << 8) & 0x0000ff00;
      data |= (ReadMem_Bar(0, 1, address + 2, 8) << 16) & 0x00ff0000;
      data |= (ReadMem_Bar(0, 1, address + 3, 8) << 24) & 0xff000000;
      break;
    }
    break;

  case 2:
    p = (u8 *)state.ram + address;
    switch (dsize) {
    case 8:
      return *((u8 *)p);
    case 16:
      return *((u16 *)p);
    case 32:
      return *((u32 *)p);
    }
    break;
  }

  return data;
}

/**
 * Override PCI Configuration Space read action.
 *
 * Lower 80 bytes are normal, upper 80 bytes reflect into the
 * register space.
 **/
u32 CSym53C810::config_read_custom(int func, u32 address, int dsize, u32 data) {
  if (address >= 0x80)
    return ReadMem_Bar(func, 1, address - 0x80, dsize);
  else
    return data;
}

/**
 * Override PCI Configuration Space write action.
 *
 * Lower 80 bytes are normal, upper 80 bytes reflect into the
 * register space.
 **/
void CSym53C810::config_write_custom(int func, u32 address, int dsize,
                                     u32 old_data, u32 new_data, u32 data) {
  if (address >= 0x80)
    WriteMem_Bar(func, 1, address - 0x80, dsize, data);
}

/**
 * Write a byte to the SCSI Control 0 register.
 *
 * This is a normal masked write operation; implemented as a separate
 * function, because there are some bits in here (START and TRG) that
 * we should do something with if a driver sets these, but that we
 * don't implement.
 *
 * START: When this bit is set, the controller should start the
 * arbitration seqence indicated by the arbitration mode bits. Used
 * only in low-level mode. UNIMPLEMENTED.
 *
 * TRG: When this bit is set, the controller is a target device.
 * UNIMPLEMENTED.
 **/
void CSym53C810::write_b_scntl0(u8 value) {
  bool old_start = TB_R8(SCNTL0, START);

  WRM_R8(SCNTL0, value);

  if (TB_R8(SCNTL0, START) && !old_start)
    FAILURE(NotImplemented,
            "SYM: Don't know how to start arbitration sequence");

  if (TB_R8(SCNTL0, TRG))
    FAILURE(NotImplemented, "SYM: Don't know how to operate in target mode");
}

/**
 * Write a byte to the SCSI Control 1 register.
 *
 * This is implemented as a separate function, because there are
 * quite a few side-effects that occur when writing to this register
 *
 * CON (Connected): This bit is automatically set any time the
 * controller is connected to the SCSI bus. The CPU can force a
 * connection or disconnection by setting or clearing this bit.
 * UNIMPLEMENTED.
 *
 * RST: Asserts the SCSI RST/ signal. Has the "side-effect" of
 * resetting the SCSI bus. This effects a couple of other registers.
 *
 * \todo: Implement real reset of the SCSI bus.
 **/
void CSym53C810::write_b_scntl1(u8 value) {
  bool old_iarb = TB_R8(SCNTL1, IARB);
  bool old_con = TB_R8(SCNTL1, CON);
  bool old_rst = TB_R8(SCNTL1, RST);

  R8(SCNTL1) = value;

  //  if (TB_R8(SCNTL1,CON) != old_con)
  //    printf("SYM: Don't know how to forcibly connect or disconnect\n");
  if (TB_R8(SCNTL1, RST) != old_rst) {
    SB_R8(SSTAT0, SDP0, false);
    SB_R8(SSTAT1, SDP1, false);
    R16(SBDL) = 0;
    R8(SBCL) = 0;

    SB_R8(SSTAT0, RST, !old_rst);

    //    printf("SYM: %s SCSI bus reset.\n",old_rst?"end":"start");
    if (!old_rst)
      RAISE(SIST0, RST);
  }
}

/**
 * Write a byte to the SCSI Interrupt Status.
 *
 * This is implemented as a separate function, because there are
 * quite a few side-effects that occur when writing to this register
 *
 * ABRT (Abort Operation): Aborts the currently executing SCRIP, and
 * generate an interrupt.
 *
 * SRST (Software Reset): Resets the SCSI chipset.
 *
 * SIGP (Signal Process): Aborts a Wait for (Re)Selection instruction
 * by jumping to the alternate address immediately.
 *
 * Since interrupt state is affected, call eval_interrupts.
 **/
void CSym53C810::write_b_istat(u8 value) {
  bool old_srst = TB_R8(ISTAT, SRST);
  bool old_sem = TB_R8(ISTAT, SEM);
  bool old_sigp = TB_R8(ISTAT, SIGP);

  WRMW1C_R8(ISTAT, value);

  if (TB_R8(ISTAT, ABRT)) {

    //    printf("SYM: Aborting on request.\n");
    RAISE(DSTAT, ABRT);
  }

  if (TB_R8(ISTAT, SRST) && !old_srst) {

    //    printf("SYM: Resetting on request.\n");
    chip_reset();
  }

  //  if (TB_R8(ISTAT,SEM) != old_sem)
  //    printf("SYM: SEM %s.\n",old_sem?"reset":"set");
  //  if (TB_R8(ISTAT,SIGP) != old_sigp)
  //    printf("SYM: SIGP %s.\n",old_sigp?"reset":"set");
  if (TB_R8(ISTAT, SIGP)) {
    if (state.wait_reselect) {

      //      printf("SYM: SIGP while wait_reselect. Jumping...\n");
      R32(DSP) = state.wait_jump;
      state.wait_reselect = false;
      state.executing = true;
      mySemaphore.set();
    }
  }

  eval_interrupts();
}

/**
 * Reads a byte from the Chip Test 2 register.
 *
 * This is implemented as a separate function, because:
 *   - The SIGP flag read by this register comes from the ISTAT
 *     register.
 *   - The CIO (configured as I/O) and CM (configured as memory)
 *     flags are determined from PCI Configuration space.
 *   - Reading this register has the side effect of clearing the
 *     SIGP flag.
 *   .
 **/
u8 CSym53C810::read_b_ctest2() {
  SB_R8(CTEST2, CIO, pci_state.config_data[0][4] != 0);
  SB_R8(CTEST2, CM, pci_state.config_data[0][5] != 0);
  SB_R8(CTEST2, SIGP, TB_R8(ISTAT, SIGP));
  SB_R8(ISTAT, SIGP, false);

  //  printf("SYM: SIGP cleared by CTEST2 read.\n");
  return R8(CTEST2);
}

/**
 * Write a byte to the Chip Test 3 register.
 *
 * This is implemented as a separate function, because there are
 * some unimplemented bits that probably should have a function if
 * a driver ever decides to use these.
 *
 * FM (Fetch Pin Mode): When set, this bit causes the FETCH/ pin to
 * deassert during indirect and table indirect read operations.
 * FETCH/ will only be active during the op codde portion of an
 * instruction fetch. This allows SCRIPTS to be stored in a PROM
 * while data tables are stored in RAM. UNIMPLEMENTED.
 **/
void CSym53C810::write_b_ctest3(u8 value) {
  WRM_R8(CTEST3, value);

  // if ((value>>3) & 1)
  //  printf("SYM: Don't know how to flush DMA FIFO\n");
  // if ((value>>2) & 1)
  //  printf("SYM: Don't know how to clear DMA FIFO\n");
  if ((value >> 1) & 1)
    FAILURE(NotImplemented, "SYM: Don't know how to handle FM mode");
}

/**
 * Write a byte to the Chip Test 4 register.
 *
 * This is implemented as a separate function, because there are
 * some unimplemented bits that probably should have a function if
 * a driver ever decides to use these.
 *
 * SRTM: Shadow Register Test Mode. Access shadow copies of TEMP and
 * DSA. Used for manufacturing diagnostics only. UNIMPLEMENTED.
 **/
void CSym53C810::write_b_ctest4(u8 value) {
  R8(CTEST4) = value;

  if ((value >> 4) & 1)
    FAILURE(NotImplemented, "SYM: Don't know how to handle SRTM mode");
}

/**
 * Write a byte to the Chip Test 5 register.
 *
 * This is implemented as a separate function, because there are
 * some unimplemented bits that probably should have a function if
 * a driver ever decides to use these.
 *
 * ADCK (Clock Address Incrementor): Setting this bit increments the
 * DNAD register. The DNAD register is incremented based on the DNAD
 * contents and the current DBC value. This bit automatically clears
 * itself after incrementing the  DNAD register. UNIMPLEMENTED.
 *
 * BBCK (Clock Byte Counter): Setting this bit decrements the byte
 * count contained in the 24-bit DBC register. It is decremented
 * based on the DBC contents and the current DNAD value. This bit
 * automatically clears itself after decrementing the DBC register.
 * UNIMPLEMENTED.
 **/
void CSym53C810::write_b_ctest5(u8 value) {
  WRM_R8(CTEST5, value);

  if ((value >> 7) & 1)
    FAILURE(NotImplemented,
            "SYM: Don't know how to do Clock Address increment");

  if ((value >> 6) & 1)
    FAILURE(NotImplemented,
            "SYM: Don't know how to do Clock Byte Counter decrement");
}

/**
 * Read a byte from the DSTAT register.
 *
 * This is implemented as a separate function, because it requires
 * interrupt re-evaluation.
 **/
u8 CSym53C810::read_b_dstat() {
  u8 retval = R8(DSTAT);

  RDCLR_R8(DSTAT);

  // printf("Read DSTAT --> eval int\n");
  eval_interrupts();

  // printf("Read DSTAT <-- eval int; retval: %02x; dstat:
  // %02x.\n",retval,R8(DSTAT));
  return retval;
}

/**
 * Read a byte from the SIST0 or SIST1 register.
 *
 * This is implemented as a separate function, because it requires
 * interrupt re-evaluation.
 **/
u8 CSym53C810::read_b_sist(int id) {
  u8 retval = state.regs.reg8[R_SIST0 + id];

  if (id)
    RDCLR_R8(SIST1);
  else
    RDCLR_R8(SIST0);

  eval_interrupts();

  return retval;
}

/**
 * Write a byte to the DMA Control register.
 *
 * This is implemented as a separate function, because there are
 * some side-effects.
 *
 * STD (Start DMA Operation): Start executing SCSI SCRIPT. Needs to
 * wake the thread.
 *
 * IRQD (IRQ Disable): disables the IRQ pin. Requires interrupt
 * re-evaluation.
 **/
void CSym53C810::write_b_dcntl(u8 value) {
  WRM_R8(DCNTL, value);

  // start operation
  if (value & R_DCNTL_STD) {
    state.executing = true;
    mySemaphore.set();
  }

  // IRQD bit...
  eval_interrupts();
}

/**
 * Write a byte to the SCSI Test 2 register.
 *
 * This is implemented as a separate function, because there are
 * some unimplemented bits that probably should have a function if
 * a driver ever decides to use these.
 *
 * LOW (Low-level-mode). Switches the SCSI controller to low-level
 * mode operation. No SCRIPTS processor, but raw manipulation of
 * SCSI registers. Yuck. UNIMPLEMENTED.
 **/
void CSym53C810::write_b_stest2(u8 value) {
  WRM_R8(STEST2, value);

  //  if (value & R_STEST2_ROF)
  //    printf("SYM: Don't know how to reset SCSI offset!\n");
  if (TB_R8(STEST2, LOW))
    FAILURE(NotImplemented, "SYM: I don't like LOW level mode");
}

/**
 * Write a byte to the SCSI Test 3 register.
 *
 * This is implemented as a separate function, because there are
 * some unimplemented bits that probably should have a function if
 * a driver ever decides to use these.
 **/
void CSym53C810::write_b_stest3(u8 value) {
  WRM_R8(STEST3, value);

  //  if (value & R_STEST3_CSF)
  //    printf("SYM: Don't know how to clear the SCSI fifo.\n");
}

/**
 * Called after the DMA Scripts Pointer register has been written.
 *
 * Start executing SCSI SCRIPT. Needs to wake the thread.
 **/
void CSym53C810::post_dsp_write() {
  if (!TB_R8(DMODE, MAN)) {
    state.executing = true;
    mySemaphore.set();

    // printf("SYM: Execution started @ %08x.\n",R32(DSP));
  }
}

/**
 * Check if threads are still running.
 **/
void CSym53C810::check_state() {
  if (myThread && !myThread->isRunning())
    FAILURE(Thread, "SYM thread has died");

  if (state.gen_timer) {
    state.gen_timer--;
    if (!state.gen_timer) {
      state.gen_timer = (R8(STIME1) & R_STIME1_GEN) * 30;
      RAISE(SIST1, GEN);
      return;
    }
  }

  /**

  if (state.wait_reselect && PT.disconnected)
  {
    state.executing = true;
    state.wait_reselect = false;
    PT.disconnected = false;
    //PT.disconnect_priv = false;
    //PT.will_disconnect = false;
    PT.reselected = true;
    state.phase = 7; // msg in //PT.disconnect_phase;
    R8(SSID) = GET_DEST() | R_SSID_VAL; // valid scsi selector id
    if (TB_R8(DCNTL,COM))
      R8(SFBR) = GET_DEST();
    // don't expect a disconnect.
    SB_R8(SCNTL2,SDU,true);
    //RAISE(SIST0,RSL);
    return 0;
  }

 **/
  if (state.disconnected) {
    if (!TB_R8(SCNTL2, SDU)) {

      // disconnect expected
      // printf("SYM: Disconnect expected. stopping disconnect timer at
      // %d.\n",state.disconnected);
      state.disconnected = 0;
      return;
    }

    state.disconnected--;
    if (!state.disconnected) {

      // printf("SYM: Disconnect unexpected. raising interrupt!\n");
      // printf(">");
      // getchar();
      RAISE(SIST0, UDC);
      return;
    }
  }
}

/**
 * Check SCSI Bus Phase.
 *
 * Returns -1 on timeout or similar, 0 on different phase, and 1 on same phase
 **/

int CSym53C810::check_phase(int chk_phase) {
  int real_phase = scsi_get_phase(0);

  if (real_phase == SCSI_PHASE_ARBITRATION) {
#if defined(DEBUG_SYM_SCRIPTS)
    printf("Phase check... selection time-out!\n");
#endif
    RAISE(SIST1, STO); // select time-out
    scsi_free(0);
    state.select_timeout = false;
    return -1;
  }

  if (real_phase == SCSI_PHASE_FREE && state.disconnected) {
#if defined(DEBUG_SYM_SCRIPTS)
    printf("Phase check... disconnected!\n");
#endif
    state.disconnected = 1;
    R32(DSP) -= 8;
    return -1;
  }

  if (real_phase == chk_phase)
    return 1;
  else
    return 0;
}

/**
 * Execute one SCRIPTS Block Move instruction
 *
 * The Block Move instruction moves data between system memory and the
 * SCSI Bus. This is a two DWORD instruction. The instruction format in
 * the DCMD register is as follows:
 *
 * \code
 * +---+-+-+-+-----+
 * |7 6|5|4|3|2 1 0| DCMD Register
 * +---+-+-+-+-----+
 *   |  | | |   +- 0..2: SCSI Phase (I/O, C/D and MSG/ signals):
 *   |  | | |            The data transfer only occurs if these bits
 *   |  | | |            match the actual SCSI bus phase.
 *   |  | | +- 3: Op Code: IGNORED
 *   |  | +- 4: Table Indirect Adressing:
 *   |  |         0: The DSPS register contains the address of the data,
 *   |  |            and the DBC register contains the number of bytes to
 *   |  |            transfer.
 *   |  |         1: The DSPS register contains a 24-bit signed offset
 *   |  |            that is added to the DSA register to get a pointer
 *   |  |            to a data structure that contains the address and
 *   |  |            byte count. This structure looks as follows:
 *   |  |                           +----+------------+
 *   |  |               DSA+DSPS:   | 00 | Byte Count |
 *   |  |                           +----+------------+
 *   |  |               DSA+DSPS+4: |  Data Address   |
 *   |  |                           +-----------------+
 *   |  +- 5: Indirect Addressing:
 *   |          0: The DSPS register contains the address of the data
 *   |          1: The DSPS register contains the address of a 32-bit
 *   |             pointer to the data.
 *   +- 6..7: Instruction Type: 00 = Block Move
 * \endcode
 **/
void CSym53C810::execute_bm_op() {
  bool indirect = (R8(DCMD) >> 5) & 1;
  bool table_indirect = (R8(DCMD) >> 4) & 1;
  int opcode = (R8(DCMD) >> 3) & 1;
  int scsi_phase = (R8(DCMD) >> 0) & 7;

#if defined(DEBUG_SYM_SCRIPTS)
  printf("SYM: INS = Block Move (i %d, t %d, opc %d, phase %d\n", indirect,
         table_indirect, opcode, scsi_phase);
#endif
  // Compare phase
  if (check_phase(scsi_phase) > 0) {
#if defined(DEBUG_SYM_SCRIPTS)
    printf("SYM: Ready for transfer.\n");
#endif

    u32 start;
    u32 count;

    if (table_indirect) {
      u32 add = R32(DSA) + sext_u32_24(R32(DSPS));

      add &= ~0x03; // 810
#if defined(DEBUG_SYM_SCRIPTS)
      printf("SYM: Reading table at DSA(%08x)+DSPS(%08x) = %08x.\n", R32(DSA),
             R32(DSPS), add);
#endif
      do_pci_read(add, &count, 4, 1);
      count &= 0x00ffffff;
      do_pci_read(add + 4, &start, 4, 1);
    } else if (indirect) {
      FAILURE(NotImplemented, "SYM: Unsupported: indirect addressing");
    } else {
      start = R32(DSPS);
      count = GET_DBC();
    }

#if defined(DEBUG_SYM_SCRIPTS)
    printf("SYM: %08x: MOVE Start/count %x, %x\n", R32(DSP) - 8, start, count);
#endif
    R32(DNAD) = start;
    SET_DBC(count); // page 5-32
    if (count == 0) {

      // printf("SYM: Count equals zero!\n");
      RAISE(DSTAT, IID); // page 5-32
      return;
    }

    if ((size_t)count > scsi_expected_xfer(0)) {
#if defined(DEBUG_SYM_SCRIPTS)
      printf("SYM: xfer %d bytes, max %d expected, in phase %d.\n", count,
             scsi_expected_xfer(0), scsi_phase);
#endif
      count = (u32)scsi_expected_xfer(0);
    }

    u8 *scsi_data_ptr = (u8 *)scsi_xfer_ptr(0, count);
    u8 *org_sdata_ptr = scsi_data_ptr;

    switch (scsi_phase) {
    case SCSI_PHASE_COMMAND:
    case SCSI_PHASE_DATA_OUT:
    case SCSI_PHASE_MSG_OUT:
      do_pci_read(R32(DNAD), scsi_data_ptr, 1, count);
      R32(DNAD) += count;
      break;

    case SCSI_PHASE_STATUS:
    case SCSI_PHASE_DATA_IN:
    case SCSI_PHASE_MSG_IN:
      do_pci_write(R32(DNAD), scsi_data_ptr, 1, count);
      R32(DNAD) += count;
      break;
    }

    R8(SFBR) = *org_sdata_ptr;
    scsi_xfer_done(0);
    return;
  }
}

/* Execute one SCRIPTS I/O instruction
 *
 * The I/O instructions perform common SCSI hardware sequences, like
 * Selection and reselection. The instruction format in
 * the DCMD and DBC register is as follows:
 *
 * \code
 * +---+-----+-+-+-+
 * |7 6|5 4 3|2|1|0| DCMD Register
 * +---+-----+-+-+-+
 *   |    |   | | +- 0: Select with ATN/:
 *   |    |   | |       Valid only for Select instruction. Assert ATN/ during
 *   |    |   | |       selection
 *   |    |   | +- 1: Table Indirect Mode:
 *   |    |   |         0: All information is taken from the instruction,
 *   |    |   |            and the contents of the SCNTL3 and SXFER registers.
 *   |    |   |         1: The DSPS register contains a 24-bit signed offset
 *   |    |   |            that is added to the DSA register to get a pointer
 *   |    |   |            to a data structure that contains the destination
 *   |    |   |            ID, SCNTL3 bits, and SXFER bits. This structure
 *   |    |   |            is 32 bits long and looks as follows:
 *   |    |   |                        +--------+--------+--------+--------+
 *   |    |   |            DSA+DSPS:   | SCNTL3 | ID     | SXFER  |        |
 *   |    |   |                        +--------+--------+--------+--------+
 *   |    |   +- 2: Relative Addrressing:
 *   |    |           0: The value in the DNAD register is an absolute address.
 *   |    |           1: The value in the DNAD register is a 24-bit signed
 *   |    |              displacement from the current DSP address.
 *   |    +- 3..5: Op Code
 *   +- 6..7 Instruction Type: 01 = I/O
 *
 * +-------+-------++--------+--+-+-++-+-+---+-+-----+
 * |       |19   16||        |10|9| || |6|   |3|     | DBC Register
 * +-------+-------++--------+--+-+-++-+-+---+-+-----+
 *             |              |  |      |     +- 3: Set/Clear ATN
 *             |              |  |      +- 6: Set/Clear ACK
 *             |              |  +- 9: Set/Clear Target Mode
 *             |              +- 10: Set/Clear Carry
 *             +- 16..19: Destination ID
 * \endcode
 *
 * The Opcode determines the actual instruction:
 * \code
 * +-----+-----------------+-------------+
 * | OPC | Initiator mode  | Target Mode |
 * +-----+-----------------+-------------+
 * | 000 | Select          | Reselect    |
 * | 001 | Wait Disconnect | Disconnect  |
 * | 010 | Wait Reselect   | Wait Select |
 * | 011 | Set             | Set         |
 * | 100 | Clear           | Clear       |
 * +-----+-----------------+-------------+
 * \endcode
 *
 * Select:
 *   - Arbitrate for the SCSI bus until arbitration is won.
 *   - If arbitration is won, try to select the destination ID
 *   - If the controller is selected or reselected before winning
 *     arbitration, jump to the address in the DNAD register.
 *   .
 *
 * Wait Disconnect:
 *   - Wait for the target to disconnect from the SCSI bus.
 *   .
 *
 * Wait Reselect:
 *   - If the controller is reselected, go to the next instruction.
 *   - If the controller is selected before being reselected, or if
 *     the CPU sets the SIGP flag, jump to the address in the DNAD
 *     register
 *   .
 *
 * Set/Clear:
 *   - Set or Clear the flags whose Set/Clear bits are set in the
 *     instruction.
 *   .
 **/
void CSym53C810::execute_io_op() {
  int opcode = (R8(DCMD) >> 3) & 7;
  bool relative = (R8(DCMD) >> 2) & 1;
  bool table_indirect = (R8(DCMD) >> 1) & 1;
  bool atn = (R8(DCMD) >> 0) & 1;
  int destination = (GET_DBC() >> 16) & 0x0f;
  bool sc_carry = (GET_DBC() >> 10) & 1;
  bool sc_target = (GET_DBC() >> 9) & 1;
  bool sc_ack = (GET_DBC() >> 6) & 1;
  bool sc_atn = (GET_DBC() >> 3) & 1;

  R32(DNAD) = R32(DSPS);

  u32 dest_addr = R32(DNAD);

  if (relative)
    dest_addr = R32(DSP) + sext_u32_24(R32(DNAD));

#if defined(DEBUG_SYM_SCRIPTS)
  printf("SYM: INS = I/O (opc %d, r %d, t %d, a %d, dest %d, sc %d%d%d%d\n",
         opcode, relative, table_indirect, atn, destination, sc_carry,
         sc_target, sc_ack, sc_atn);
#endif
  if (table_indirect) {
    u32 io_addr = R32(DSA) + sext_u32_24(GET_DBC());
    io_addr &= ~3; // 810
#if defined(DEBUG_SYM_SCRIPTS)
    printf("SYM: Reading table at DSA(%08x)+DBC(%08x) = %08x.\n", R32(DSA),
           sext_u32_24(GET_DBC()), io_addr);
#endif

    u32 io_struc;
    do_pci_read(io_addr, &io_struc, 4, 1);
    destination = (io_struc >> 16) & 0x0f;
#if defined(DEBUG_SYM_SCRIPTS)
    printf("SYM: table indirect. io_struct = %08x, new dest = %d.\n", io_struc,
           destination);
#endif
  }

  switch (opcode) {
  case 0:
#if defined(DEBUG_SYM_SCRIPTS)
    printf("SYM: %08x: SELECT %d.\n", R32(DSP) - 8, destination);
#endif
    SET_DEST(destination);
    if (!scsi_arbitrate(0)) {

      // scsi bus busy, try again next clock...
      printf("scsi bus busy...\n");
      R32(DSP) -= 8;
      return;
    }

    state.select_timeout = !scsi_select(0, destination);
    if (!state.select_timeout)  // select ok
      SB_R8(SCNTL2, SDU, true); // don't expect a disconnect
    return;

  case 1:
#if defined(DEBUG_SYM_SCRIPTS)
    printf("SYM: %08x: WAIT DISCONNECT\n", R32(DSP) - 8);
#endif

    // maybe we need to do more??
    scsi_free(0);
    return;

  case 2:
#if defined(DEBUG_SYM_SCRIPTS)
    printf("SYM: %08x: WAIT RESELECT\n", R32(DSP) - 8);
#endif
    if (TB_R8(ISTAT, SIGP)) {
#if defined(DEBUG_SYM_SCRIPTS)
      printf("SYM: SIGP set before wait reselect; jumping!\n");
#endif
      R32(DSP) = dest_addr;
    } else {
      state.wait_reselect = true;
      state.wait_jump = dest_addr;
      state.executing = false;
    }

    return;

  case 3:
#if defined(DEBUG_SYM_SCRIPTS)
    printf("SYM: %08x: SET %s%s%s%s\n", R32(DSP) - 8, sc_carry ? "carry " : "",
           sc_target ? "target " : "", sc_ack ? "ack " : "",
           sc_atn ? "atn " : "");
#endif
    if (sc_ack)
      SB_R8(SOCL, ACK, true);
    if (sc_atn) {
      if (!TB_R8(SOCL, ATN)) {
        SB_R8(SOCL, ATN, true);

        // printf("SET ATN.\n");
        // printf(">");
        // getchar();
      }
    }

    if (sc_target)
      SB_R8(SCNTL0, TRG, true);
    if (sc_carry)
      state.alu.carry = true;
    return;

  case 4:
#if defined(DEBUG_SYM_SCRIPTS)
    printf("SYM: %08x: CLEAR %s%s%s%s\n", R32(DSP) - 8,
           sc_carry ? "carry " : "", sc_target ? "target " : "",
           sc_ack ? "ack " : "", sc_atn ? "atn " : "");
#endif
    if (sc_ack)
      SB_R8(SOCL, ACK, false);
    if (sc_atn) {
      if (TB_R8(SOCL, ATN)) {
        SB_R8(SOCL, ATN, false);

        // printf("RESET ATN.\n");
        // printf(">");
        // getchar();
      }
    }

    if (sc_target)
      SB_R8(SCNTL0, TRG, false);
    if (sc_carry)
      state.alu.carry = false;
    return;

    break;
  }
}

/**
 * Execute one SCRIPTS R/W instruction
 *
 * The R/W instructions perform arithmetic or logic operations on
 * registers. The instruction format in the DCMD and DBC register is as follows:
 *
 * \code
 * +---+-----+-----+
 * |7 6|5 4 3|2 1 0| DCMD Register
 * +---+-----+-----+
 *   |    |     +- 2..0: operator:
 *   |    |                000: data8
 *   |    |                001: reg << 1
 *   |    |                010: reg | data8
 *   |    |                011: reg ^ data8
 *   |    |                100: reg & data8
 *   |    |                101: reg >> 1
 *   |    |                110: reg + data8
 *   |    |                111: reg + data8 + carry
 *   |    +- 3..5: Op Code
 *   |               101: regA = operator(SFBR, data8)
 *   |               110: SFBR = operator(RegA, data8)
 *   |               111: regA = operator(RegA, data8)
 *   +- 6..7 Instruction Type: 01 = R/W
 *
 * +--+------------++---------------++-+-------------+
 * |23|22        16||15            8||7|             | DBC Register
 * +--+------------++---------------++-+-------------+
 *  |  A6--------A0         |         A7
 *  |        +--------------|---------+- 7,22..16: RegA address
 *  |                       |
 *  |                       +- 15..8: Immediate data
 *  +- 23: Use data8/SFBR
 *         0: data8 = Immediate data
 *         1: data8 = SFBR
 * \endcode
 */
void CSym53C810::execute_rw_op() {
  int opcode = (R8(DCMD) >> 3) & 7;
  int oper = (R8(DCMD) >> 0) & 7;
  bool use_data8_sfbr = (GET_DBC() >> 23) & 1;
  int reg_address =
      ((GET_DBC() >> 16) &
       0x7f); //| (GET_DBC() & 0x80); // manual is unclear about bit 7.
  u8 imm_data = (u8)(GET_DBC() >> 8) & 0xff;
  u8 op_data;

#if defined(DEBUG_SYM_SCRIPTS)
  printf("SYM: INS = R/W (opc %d, oper %d, use %d, add %d, imm %02x\n", opcode,
         oper, use_data8_sfbr, reg_address, imm_data);
#endif
  if (use_data8_sfbr)
    imm_data = R8(SFBR);

  if (oper != 0) {
    if (opcode == 5 || reg_address == 0x08) {
      op_data = R8(SFBR);
#if defined(DEBUG_SYM_SCRIPTS)
      printf("SYM: %08x: sfbr (%02x) ", R32(DSP) - 8, op_data);
#endif
    } else {
      op_data = (u8)ReadMem_Bar(0, 1, reg_address, 8);
#if defined(DEBUG_SYM_SCRIPTS)
      printf("SYM: %08x: reg%02x (%02x) ", R32(DSP) - 8, reg_address, op_data);
#endif
    }
  }

  u16 tmp16;

  switch (oper) {
  case 0:
    op_data = imm_data;
#if defined(DEBUG_SYM_SCRIPTS)
    printf("SYM: %08x: %02x ", R32(DSP) - 8, imm_data);
#endif
    break;

  case 1:
    tmp16 = (op_data << 1) + (state.alu.carry ? 1 : 0);
    state.alu.carry = (tmp16 >> 8) & 1;
    op_data = tmp16 & 0xff;
#if defined(DEBUG_SYM_SCRIPTS)
    printf("<< 1 = %02x ", op_data);
#endif
    break;

  case 2:
    op_data |= imm_data;
#if defined(DEBUG_SYM_SCRIPTS)
    printf("| %02x = %02x ", imm_data, op_data);
#endif
    break;

  case 3:
    op_data ^= imm_data;
#if defined(DEBUG_SYM_SCRIPTS)
    printf("^ %02x = %02x ", imm_data, op_data);
#endif
    break;

  case 4:
    op_data &= imm_data;
#if defined(DEBUG_SYM_SCRIPTS)
    printf("& %02x = %02x ", imm_data, op_data);
#endif
    break;

  case 5:
    tmp16 = (op_data >> 1) + (state.alu.carry ? 0x80 : 0x00);
    state.alu.carry = op_data & 1;
    op_data = tmp16 & 0xff;
#if defined(DEBUG_SYM_SCRIPTS)
    printf(">> 1 = %02x ", op_data);
#endif
    break;

  case 6:
    tmp16 = op_data + imm_data;
    state.alu.carry = (tmp16 > 0xff);
    op_data = tmp16 & 0xff;
#if defined(DEBUG_SYM_SCRIPTS)
    printf("+ %02x = %02x (carry %d) ", imm_data, op_data, state.alu.carry);
#endif
    break;

  case 7:
    tmp16 = op_data + imm_data + (state.alu.carry ? 1 : 0);
    state.alu.carry = (tmp16 > 0xff);
    op_data = tmp16 & 0xff;
#if defined(DEBUG_SYM_SCRIPTS)
    printf("+ %02x (w/carry) = %02x (carry %d) ", imm_data, op_data,
           state.alu.carry);
#endif
    break;
  }

  if (opcode == 6 || reg_address == 0x08) {
#if defined(DEBUG_SYM_SCRIPTS)
    printf("-> sfbr.\n");
#endif
    R8(SFBR) = op_data;
  } else {
#if defined(DEBUG_SYM_SCRIPTS)
    printf("-> reg%02x.\n", reg_address);
#endif
    WriteMem_Bar(0, 1, reg_address, 8, op_data);
  }
}

/**
 * Execute one SCRIPTS Transfer Control instruction
 *
 * The Transfer Control instructions perform conditional jumps, calls,
 * returns and interrupts. The instruction format in the DCMD and DBC
 * registers is as follows:
 *
 * \code
 * +---+-----+-----+
 * |7 6|5 4 3|2 1 0| DCMD Register
 * +---+-----+-----+
 *   |    |     +- 0..2: SCSI Phase (I/O, C/D and MSG/ signals):
 *   |    |              The actual SCSI phase is compared against these
 *   |    |              bits.
 *   |    +- 3..5: Op Code
 *   |               000: Jump
 *   |               001: Call
 *   |               010: Return
 *   |               011: Interrupt
 *   |               1xx: reserved
 *   +- 6..7 Instruction Type: 10 = Transfer Control
 *
 * +--+-+--+--+--+--+--+--++---------------++---------------+
 * |23| |21|20|19|18|17|16||15            8||7             0| DBC Register
 * +--+-+--+--+--+--+--+--++---------------++---------------+
 *  |    |  |  |  |  |  |          |             +- 7..0: Data to compare
 *  |    |  |  |  |  |  |          |                      against SFBR
 *  |    |  |  |  |  |  |          +- 15..8: Mask that determines what bits
 *  |    |  |  |  |  |  |                    to compare against SFBR.
 *  |    |  |  |  |  |  +- 16: Wait for valid SCSI phase
 *  |    |  |  |  |  +- 17: Compare Phase
 *  |    |  |  |  +- 18: Compare SFBR data
 *  |    |  |  +- Jump if:
 *  |    |  |       0: Jump/Call/return/Interrupt if the equation is true
 *  |    |  |       0: Jump/Call/return/Interrupt if the equation is false
 *  |    |  +- Interrupt on the Fly
 *  |    +- Carry Test
 *  +- relative Addressing:
 *       0: The value in the DSPS register is an absolute address.
 *       1: The value in the DSPS register is a 24-bit signed
 *          displacement from the current DSP address.
 * \endcode
 *
 * The equation evaluated is one of the following:
 *   - the value of the carry bit (if the Carry Test bit is set)
 *   - equality comparisons of SCSI phase and/or SFBR register data
 *   - true (if none of the compare/carry test bits are set)
 *   .
 *
 * An action is taken when the equation evaluates to either true or false
 * as determined by the "Jump if" bit.
 *
 * Jump:
 *   - Jump to the instruction addressed by the DSPS register.
 *   .
 *
 * Call:
 *   - Store the current DSP register value to the TEMP register.
 *   - Jump to the instruction addressed by the DSPS register.
 *   .
 *
 * Return:
 *   - Jump to the instruction addressed by the TEMP register.
 *   .
 *
 * Interrupt:
 *   - If the Interrupt on the Fly bit is set, raise the INTF interrupt.
 *   - Otherwise:
 *       - Raise the SIR interrupt
 *       - Terminate SCRIPTS execution
 *       - The DSPS value is used as an interrupt vector for the driver.
 *       .
 *   .
 **/
void CSym53C810::execute_tc_op() {
  int opcode = (R8(DCMD) >> 3) & 7;
  int scsi_phase = (R8(DCMD) >> 0) & 7;
  bool relative = (GET_DBC() >> 23) & 1;
  bool carry_test = (GET_DBC() >> 21) & 1;
  bool interrupt_fly = (GET_DBC() >> 20) & 1;
  bool jump_if = (GET_DBC() >> 19) & 1;
  bool cmp_data = (GET_DBC() >> 18) & 1;
  bool cmp_phase = (GET_DBC() >> 17) & 1;
  int cmp_mask = (GET_DBC() >> 8) & 0xff;
  int cmp_dat = (GET_DBC() >> 0) & 0xff;
  u32 dest_addr;

  // wait_valid can be safely ignored, phases are always valid in this ideal
  // world... bool wait_valid = (GET_DBC()>>16) & 1;

  // We'll keep modifying this variable until we know what the result of the
  // comparisons is.
  bool do_it;

  // Relative jump or not? (no effect on Return or Interrupt)
  if (relative)
    dest_addr = R32(DSP) + sext_u32_24(R32(DSPS));
  else
    dest_addr = R32(DSPS);

#if defined(DEBUG_SYM_SCRIPTS)
  printf("SYM: %08x: if (", R32(DSP) - 8);
#endif
  if (carry_test) {
    // All we need to check is the CARRY flag
#if defined(DEBUG_SYM_SCRIPTS)
    printf("(%scarry)", jump_if ? "" : "!");
#endif
    do_it = (state.alu.carry == jump_if);
  } else if (cmp_data || cmp_phase) {
    // We need to compare data and/or phase
    do_it = true;
    if (cmp_data) {
      // compare data
#if defined(DEBUG_SYM_SCRIPTS)
      printf("((data & 0x%02x) %s 0x%02x)", (~cmp_mask) & 0xff,
             jump_if ? "==" : "!=", cmp_dat & ~cmp_mask);
#endif
      if (((R8(SFBR) & ~cmp_mask) == (cmp_dat & ~cmp_mask)) != jump_if)
        do_it = false;
#if defined(DEBUG_SYM_SCRIPTS)
      if (cmp_phase)
        printf(" && ");
#endif
    }

    if (cmp_phase) {
      // Compare phase
#if defined(DEBUG_SYM_SCRIPTS)
      printf("(phase %s %d)", jump_if ? "==" : "!=", scsi_phase);
#endif
      if ((check_phase(scsi_phase) > 0) != jump_if)
        do_it = false;
    }
  } else {

    // no comparison
    do_it = jump_if;
  }

#if defined(DEBUG_SYM_SCRIPTS)
  printf(") ");
#endif
  switch (opcode) {
  case 0:
#if defined(DEBUG_SYM_SCRIPTS)
    printf("jump %x\n", R32(DSPS));
#endif
    if (do_it) {
#if defined(DEBUG_SYM_SCRIPTS)
      printf("SYM: Jumping %08x...\n", dest_addr);
#endif
      R32(DSP) = dest_addr;
    }

    return;
    break;

  case 1:
#if defined(DEBUG_SYM_SCRIPTS)
    printf("call %d\n", R32(DSPS));
#endif
    if (do_it) {
#if defined(DEBUG_SYM_SCRIPTS)
      printf("SYM: Calling %08x...\n", dest_addr);
#endif
      R32(TEMP) = R32(DSP);
      R32(DSP) = dest_addr;
    }

    return;
    break;

  case 2:
#if defined(DEBUG_SYM_SCRIPTS)
    printf("return %d\n", R32(DSPS));
#endif
    if (do_it) {
#if defined(DEBUG_SYM_SCRIPTS)
      printf("SYM: Returning %08x...\n", R32(TEMP));
#endif
      R32(DSP) = R32(TEMP);
    }

    return;
    break;

  case 3:
#if defined(DEBUG_SYM_SCRIPTS)
    printf("interrupt%s.\n", interrupt_fly ? " on the fly" : "");
#endif
    if (do_it) {
#if defined(DEBUG_SYM_SCRIPTS)
      printf("SYM: Interrupt with vector %x...\n", R32(DSPS));
#endif
      if (interrupt_fly)
        RAISE(ISTAT, INTF);
      else
        RAISE(DSTAT, SIR);
    }

    return;
    break;

  default:
    FAILURE_1(NotImplemented,
              "SYM: Transfer Control Instruction with opcode %d is RESERVED.\n",
              opcode);
  }
}

/**
 * Execute one SCRIPTS Load/Store instruction
 *
 * The Load/Store instruction moves data between registers and memory.
 * The memory range could very well map back to the registers through
 * the PCI bus! The instruction format in the DCMD and DBC registers
 * is as follows:
 *
 * \code
 * +-----+-+---+-+-+
 * |7 6 5|4|   |1|0| DCMD Register
 * +-----+-+---+-+-+
 *    |   |     | +- 0: Load/Store
 *    |   |     |         0: Store (register -> memory)
 *    |   |     |         1: Load (memory -> register)
 *    |   |     +- 1: No Flush (no effect)
 *    |   +- 4: DSA Relative
 *    |           0: The value in the DSPS register is absolute.
 *    |           1: The value in the DSPS register is a 24-bit
 *    |              signed offset from DSA.
 *    +- 7..5 Instruction Type: 111 = Load/Store
 *
 * +---------------++---------------++---------------+
 * |23           16||               ||         |2   0| DBC Register
 * +---------------++---------------++---------------+
 *          |                                     +- 2..0: Byte Count
 *          +- 23..16: Register Address
 * \endcode
 *
 * This instructions moves up to 4 bytes between registers and memory.
 **/
void CSym53C810::execute_ls_op() {
  bool is_load = (R8(DCMD) >> 0) & 1;
  bool no_flush = (R8(DCMD) >> 1) & 1;
  bool dsa_relative = (R8(DCMD) >> 4) & 1;
  int regaddr = (GET_DBC() >> 16) & 0x7f;
  int byte_count = (GET_DBC() >> 0) & 7;
  u32 memaddr;

  // Relative Addressing
  if (dsa_relative)
    memaddr = R32(DSA) + sext_u32_24(R32(DSPS));
  else
    memaddr = R32(DSPS);

#if defined(DEBUG_SYM_SCRIPTS)
  printf("SYM: dsa_rel: %d, DSA: %04x, DSPS: %04x, mem %04x.\n", dsa_relative,
         R32(DSA), R32(DSPS), memaddr);
#endif
  if (is_load) {
#if defined(DEBUG_SYM_SCRIPTS)
    printf("SYM: %08x: Load reg%02x", R32(DSP) - 8, regaddr);
    if (byte_count > 1)
      printf("..%02x", regaddr + byte_count - 1);
    printf("from %x.\n", memaddr);
#endif
    // Perform Load Operation
    for (int i = 0; i < byte_count; i++) {
      u8 dat;
      do_pci_read(memaddr + i, &dat, 1, 1);
#if defined(DEBUG_SYM_SCRIPTS)
      printf("SYM: %02x -> reg%02x\n", dat, regaddr + i);
#endif
      WriteMem_Bar(0, 1, regaddr + i, 8, dat);
    }
  } else {
#if defined(DEBUG_SYM_SCRIPTS)
    printf("SYM: %08x: Store reg%02x", R32(DSP) - 8, regaddr);
    if (byte_count > 1)
      printf("..%02x", regaddr + byte_count - 1);
    printf("to %x.\n", memaddr);
#endif
    // Perform Store Operation
    for (int i = 0; i < byte_count; i++) {
      u8 dat = (u8)ReadMem_Bar(0, 1, regaddr + i, 8);
#if defined(DEBUG_SYM_SCRIPTS)
      printf("SYM: %02x <- reg%02x\n", dat, regaddr + i);
#endif
      do_pci_write(memaddr + i, &dat, 1, 1);
    }
  }
}

/**
 * Execute one SCRIPTS Memory Move instruction
 *
 * The Memory Move instruction is used to transfer data from one
 * region in host memory to another region through the SCSI
 * controller's. DMA. The instruction format in the DCMD register
 * is as follows:
 *
 * \code
 * +-----+-------+-+
 * |7 6 5|       |0| DCMD Register
 * +-----+-------+-+
 *    |           +- No Flush (no effect)
 *    +- 7..5 Instruction Type: 110 = Memory Move
 * \endcode
 *
 * This is a 3-DWORD instruction.
 *
 * The DBC register holds the number of bytes to be moved.
 * The DSPS register holds the source address.
 * The TEMP register holds the destination address.
 */
void CSym53C810::execute_mm_op() {
  u32 temp_shadow;
  do_pci_read(R32(DSP), &temp_shadow, 4, 1);
  R32(DSP) += 4;

#if defined(DEBUG_SYM_SCRIPTS)
  printf("SYM: %08x: Memory Move %06x bytes from %08x to %08x.\n",
         R32(DSP) - 12, GET_DBC(), R32(DSPS), temp_shadow);
#endif

  // To speed things up, we set up a buffer and read all data
  // at once, followed by writing all data at once.
  void *buf = malloc(GET_DBC());
  do_pci_read(R32(DSPS), buf, 1, GET_DBC());
  do_pci_write(temp_shadow, buf, 1, GET_DBC());
  free(buf);
  return;
}

/**
 * Execute one SCRIPTS instruction.
 *
 * DSP (DMA Scripts Pointer) contains the address of the next instruction.
 * For each instruction, two DWORDS are read into the 8-bit DCMD (DMA Command),
 * 24-bit DBC (DMA Byte Counter), and 32-bit DSPS (DMA Scripts Pointer Save)
 * registers. For some commands, a third DWORD is read into the 32-bit TEMP
 * register:
 *
 * \code
 *        +--------+------------------------+
 * DSP  : |  DCMD  |          DBC           |
 *        +--------+------------------------+
 * DSP+4: |              DSPS               |
 *        +---------------------------------+
 * DSP+8: |/ / / / / / / TEMP  / / / / / / /|
 *        +---------------------------------+
 * \endcode
 **/
void CSym53C810::execute() {
  int optype;
  int opcode;
  bool is_load_store;

#if defined(DEBUG_SYM_SCRIPTS)
  printf("SYM: INS @ %x   \n", R32(DSP));
#endif

  // Read 2 DWORDS into the DCMD, DBC and DSPS registers.
  do_pci_read(R32(DSP), &R32(DBC), 4, 1);
  do_pci_read(R32(DSP) + 4, &R32(DSPS), 4, 1);

  // Increase DSP to point to the next instruction
  R32(DSP) += 8;

#if defined(DEBUG_SYM_SCRIPTS)
  printf("SYM: INS = %x, %x, %x   \n", R8(DCMD), GET_DBC(), R32(DSPS));
#endif
  /* The two most significant bits of the DCMD register determine the operation
   * type. These are:
   *   00: Block Move
   *   01: I/O or R/W
   *   10: Transfer Control
   *   11: Memory Move or Load And Store
   */
  optype = (R8(DCMD) >> 6) & 3;
  switch (optype) {
  case 0:
    execute_bm_op();
    break;

  case 1:
    opcode = (R8(DCMD) >> 3) & 7;
    if (opcode < 5)
      execute_io_op();
    else
      execute_rw_op();
    break;

  case 2:
    execute_tc_op();
    break;

  case 3:
    is_load_store = (R8(DCMD) >> 5) & 1;
    if (is_load_store)
      execute_ls_op();
    else
      execute_mm_op();
    break;
  }

  // single step mode
  if (TB_R8(DCNTL, SSM)) {
#if defined(DEBUG_SYM_SCRIPTS)
    printf("SYM: Single step...\n");
#endif
    RAISE(DSTAT, SSI);
  }
}

/**
 * Set an interrupt bit.
 *
 * This function checks if any other interrupt bits are active, if so,
 * the interrupt bit is set in the stacked interrupt registers. Otherwise
 * it goes straight to the respective interrupt register.
 *
 * According to the datasheet:
 * "The SYM53C895 stacks interrupts if they occur one after another. If the SIP
 *or DIP bits in the ISTAT register are set (first level), then there is already
 *at least one pending interrupt, and any future interrupts will be stacked in
 *extra registers behind the SIST0, SIST1, and DSTAT registers (second level).
 *When two interrupts have occurred and the two levels of the stack are full,
 *any further interrupts will set additional bits in the extra registers behind
 *SIST0, SIST1 and DSTAT. When the first level of interrupts are cleared, all
 *the interrupts that came in afterward will move into the SIST0, SIST1 and
 *DSTAT. After the first interrupt is cleared by reading the appropriate
 *register, the IRQ/ pin will be deasserted for a minimum of three CLKs; the
 *stacked interrupt(s) will move into the SIST0, SIST1 or DSTAT; and the IRQ/
 *pin will be asserted once again.
 *
 * Since a masked non-fatal interrupt will not set the SIP or DIP bits,
 *interrupt stacking will not occur. A masked, non-fatal interrupt will still
 *post the interrupt in SIST0, but will not assert the IRQ/ pin. Since no
 *interrupt is generated, future interrupts will move right into the SIST0 or
 *SIST1 instead of being stacked behind another interrupt. When another
 *condition occurs that generates an interrupt, the bit corresponding to the
 *earlier masked non-fatal interrupt will still be set."
 **/
void CSym53C810::set_interrupt(int reg, u8 interrupt) {
  // printf("set interrupt %02x, %02x.\n",reg,interrupt);
  switch (reg) {
  case R_DSTAT:
    if (TB_R8(ISTAT, DIP) || TB_R8(ISTAT, SIP)) {
      state.dstat_stack |= interrupt;

      // printf("DSTAT stacked.\n");
    } else {
      R8(DSTAT) |= interrupt;

      // printf("DSTAT.\n");
    }
    break;

  case R_SIST0:
    if (TB_R8(ISTAT, DIP) || TB_R8(ISTAT, SIP)) {
      state.sist0_stack |= interrupt;

      // printf("SIST0 stacked.\n");
    } else {
      R8(SIST0) |= interrupt;

      // printf("SIST0.\n");
    }
    break;

  case R_SIST1:
    if (TB_R8(ISTAT, DIP) || TB_R8(ISTAT, SIP)) {
      state.sist1_stack |= interrupt;

      // printf("SIST1 stacked.\n");
    } else {
      R8(SIST1) |= interrupt;

      // printf("SIST1.\n");
    }
    break;

  case R_ISTAT:

    // printf("ISTAT.\n");
    R8(ISTAT) |= interrupt;
    break;

  default:
    FAILURE_1(NotImplemented, "set_interrupt reg %02x!!\n", reg);
  }

  // printf("--> eval int\n");
  eval_interrupts();

  // printf("<-- eval_int\n");
}

/**
 * Evaluate interrupt status.
 *
 * Check interrupt registers, and determine if an interrupt should be generated.
 **/
void CSym53C810::eval_interrupts() {
  // will_assert: when this boolean value is true at the end of this function,
  // an interrupt will be signalled to the system.
  bool will_assert = false;

  // will_halt: when this boolean value is true at the end of this function,
  // program execution will be halted. (fatal interrupt)
  bool will_halt = false;

  // Check current interrupt status. If no interrupt is active, move interrupt
  // flags from the interrupt stack down.
  //
  // (When an interrupt is signalled, but another interrupt bit is already
  // active, the interrupt doesn't go to the interrupt register, but to the
  // interrupt stack. The interrupt stack, however, doesn't keep track of the
  // order in which interrupts come in, so when the interrupt stack is moved
  // down into the interrupt registers, multiple interrupt bits may become
  // active.)
  if (!R8(SIST0) && !R8(SIST1) && !R8(DSTAT)) {
    R8(SIST0) |= state.sist0_stack;
    R8(SIST1) |= state.sist1_stack;
    R8(DSTAT) |= state.dstat_stack;
    state.sist0_stack = 0;
    state.sist1_stack = 0;
    state.dstat_stack = 0;
  }

  // Check for DMA interrupts.
  if (R8(DSTAT) & DSTAT_FATAL) {
    // DMA interrupt conditions always halt execution (always fatal)
    will_halt = true;

    // printf("  will halt(DSTAT).\n");

    // Set the DMA interrupt pending bit.
    SB_R8(ISTAT, DIP, true);

    // If the interrupt is also enabled in the DIEN register, it will
    // be signalled to the system.
    if (R8(DSTAT) & R8(DIEN) & DSTAT_FATAL) {
      will_assert = true;

      // printf("  will assert(DSTAT).\n");
    }
  } else {
    // Reset the DMA interrupt pending bit. (It may still be set).
    SB_R8(ISTAT, DIP, false);
  }

  // Check for SCSI engine interrupts
  if (R8(SIST0) || R8(SIST1)) {
    // Set the SCSI interrupt pending bit.
    SB_R8(ISTAT, SIP, true);

    // Check if the interrupt is either fatal, or enabled.
    if ((R8(SIST0) & (SIST0_FATAL | R8(SIEN0))) ||
        (R8(SIST1) & (SIST1_FATAL | R8(SIEN1)))) {
      // In either case, stop execution.
      will_halt = true;

      // printf("  will halt(SIST).\n");

      // If the interrupt is enabled, signal it to the system.
      if ((R8(SIST0) & R8(SIEN0)) || (R8(SIST1) & R8(SIEN1))) {
        will_assert = true;

        // printf("  will assert(SIST).\n");
      }
    }
  } else {
    // Reset the SCSI interrupt pending bit (It may still be set).
    SB_R8(ISTAT, SIP, false);
  }

  // Check the interrupt on the fly bit.
  if (TB_R8(ISTAT, INTF)) {
    // Signal this to the system
    will_assert = true;

    // printf("  will assert(INTF).\n");
  }

  // If interrupts are disabled, don't signal any interrupt to the system.
  if (TB_R8(DCNTL, IRQD)) {
    will_assert = false;

    // printf("  won't assert(IRQD).\n");
  }

  // Halt execution if will_halt is true.
  if (will_halt)
    state.executing = false;

  // Assert or de-assert the interrupt line as needed
  if (will_assert != state.irq_asserted) {

    // printf("  doing...%d\n",will_assert);
    do_pci_interrupt(0, will_assert);
    state.irq_asserted = will_assert;
  }
}
