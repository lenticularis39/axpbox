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
 * Contains the definitions for the emulated Symbios SCSI controller.
 *
 * $Id: Sym53C810.h,v 1.9 2008/05/31 15:47:14 iamcamiel Exp $
 *
 * X-1.9        Camiel Vanderhoeven                             31-MAY-2008
 *      Changes to include parts of Poco.
 *
 * X-1.8        Camiel Vanderhoeven                             29-APR-2008
 *      CDiskController is no longer a CPCIDevice. devices that are both
 *      should multiple inherit both.
 *
 * X-1.7        Camiel Vanderhoeven                             25-MAR-2008
 *      Separate functions for different instructions, comments.
 *
 * X-1.6        Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.5        Camiel Vanderhoeven                             13-MAR-2008
 *      Create init(), start_threads() and stop_threads() functions.
 *
 * X-1.4       Camiel Vanderhoeven                             11-MAR-2008
 *      Named, debuggable mutexes.
 *
 * X-1.3        Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.2        Camiel Vanderhoeven                             17-FEB-2008
 *      Comments.
 *
 * X-1.1        Camiel Vanderhoeven                             16-FEB-2008
 *      Created as a spinoff from 53C895 controller, as we couldn't get
 *      that chip to work properly with the OpenVMS driver.
 **/
#if !defined(INCLUDED_SYM53C810_H_)
#define INCLUDED_SYM53C810_H_

#include "DiskController.h"
#include "PCIDevice.h"
#include "SCSIDevice.h"

/**
 * \brief Symbios Sym53C810 SCSI disk controller.
 *
 * \bug Exception below ASTDEL during OpenVMS boot when booting from SCSI.
 *
 * Documentation consulted:
 *  - SCSI 2 (http://www.t10.org/ftp/t10/drafts/s2/s2-r10l.pdf)
 *  - SCSI 3 Multimedia Commands (MMC)
 *(http://www.t10.org/ftp/t10/drafts/mmc/mmc-r10a.pdf)
 *  - SYM53C810A PCI-SCSI I/O Processor
 *(http://ftp.netbsd.org/pub/NetBSD/arch/bebox/doc/810a.pdf)
 *  - Symbios SCSI SCRIPTS Processors Programming Guide
 *(http://la.causeuse.org/hauke/macbsd/symbios_53cXXX_doc/lsilogic-53cXXX-scripts.pdf)
 *  .
 **/
class CSym53C810 : public CPCIDevice,
                   public CDiskController,
                   public CSCSIDevice,
                   public CRunnable {
public:
  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);
  virtual void check_state();

  virtual void run(); // Poco Thread entry point
  virtual void init();
  virtual void start_threads();
  virtual void stop_threads();

  virtual void WriteMem_Bar(int func, int bar, u32 address, int dsize,
                            u32 data);
  virtual u32 ReadMem_Bar(int func, int bar, u32 address, int dsize);

  virtual u32 config_read_custom(int func, u32 address, int dsize, u32 data);
  virtual void config_write_custom(int func, u32 address, int dsize,
                                   u32 old_data, u32 new_data, u32 data);

  virtual void register_disk(class CDisk *dsk, int bus, int dev);

  CSym53C810(CConfigurator *cfg, class CSystem *c, int pcibus, int pcidev);
  virtual ~CSym53C810();

private:
  void write_b_scntl0(u8 value);
  void write_b_scntl1(u8 value);
  void write_b_istat(u8 value);
  u8 read_b_ctest2();
  void write_b_ctest3(u8 value);
  void write_b_ctest4(u8 value);
  void write_b_ctest5(u8 value);
  void write_b_stest2(u8 value);
  void write_b_stest3(u8 value);
  u8 read_b_dstat();
  u8 read_b_sist(int id);
  void write_b_dcntl(u8 value);

  void post_dsp_write();

  int check_phase(int chk_phase);
  void execute_io_op();
  void execute_rw_op();
  void execute_ls_op();
  void execute_mm_op();
  void execute_tc_op();
  void execute_bm_op();
  void execute();

  void eval_interrupts();
  void set_interrupt(int reg, u8 interrupt);
  void chip_reset();

  CThread *myThread;
  CSemaphore mySemaphore;
  CMutex *myRegLock;
  bool StopThread;

  /// The state structure contains all elements that need to be saved to the
  /// statefile.
  struct SSym_state {
    bool irq_asserted;

    union USym_regs {
      u8 reg8[128];
      u16 reg16[64];
      u32 reg32[64];
    } regs;

    struct SSym_alu {
      bool carry;
    } alu;

    u8 ram[4096];

    bool executing;

    bool wait_reselect;
    bool select_timeout;
    int disconnected;
    u32 wait_jump;

    u8 dstat_stack;
    u8 sist0_stack;
    u8 sist1_stack;

    long gen_timer;

    // int phase;
  } state;
};
#endif // !defined(INCLUDED_SYM_H)
