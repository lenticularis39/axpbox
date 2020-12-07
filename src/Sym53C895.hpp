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
 * Contains the definitions for the emulated Symbios SCSI controller.
 *
 * $Id: Sym53C895.h,v 1.20 2008/05/31 15:47:14 iamcamiel Exp $
 *
 * X-1.20       Camiel Vanderhoeven                             31-MAY-2008
 *      Changes to include parts of Poco.
 *
 * X-1.19       Camiel Vanderhoeven                             29-APR-2008
 *      CDiskController is no longer a CPCIDevice. devices that are both
 *      should multiple inherit both.
 *
 * X-1.18       Camiel Vanderhoeven                             25-MAR-2008
 *      Separate functions for different instructions.
 *
 * X-1.17       Camiel Vanderhoeven                             25-MAR-2008
 *      Separate CSym53C895::check_phase() function.
 *
 * X-1.16       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.15       Camiel Vanderhoeven                             13-MAR-2008
 *      Create init(), start_threads() and stop_threads() functions.
 *
 * X-1.14       Camiel Vanderhoeven                             11-MAR-2008
 *      Named, debuggable mutexes.
 *
 * X-1.13       Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.12       Camiel Vanderhoeven                             17-FEB-2008
 *      Comments.
 *
 * X-1.11       Camiel Vanderhoeven                             12-JAN-2008
 *      Use disk's SCSI engine.
 *
 * X-1.10       Camiel Vanderhoeven                             06-JAN-2008
 *      Leave changing the blocksize to the disk itself.
 *
 * X-1.9        Camiel Vanderhoeven                             02-JAN-2008
 *      Comments.
 *
 * X-1.8        Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.7        Camiel Vanderhoeven                             20-DEC-2007
 *      Do reselection on read commands.
 *
 * X-1.6        Camiel Vanderhoeven                             19-DEC-2007
 *      Allow for different blocksizes.
 *
 * X-1.5        Camiel Vanderhoeven                             18-DEC-2007
 *      Selection timeout occurs after the phase is checked the first time.
 *
 * X-1.4        Camiel Vanderhoeven                             17-DEC-2007
 *      Added general timer.
 *
 * X-1.3        Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.2        Camiel Vanderhoeven                             16-DEC-2007
 *      Changed register structure.
 *
 * X-1.1        Camiel Vanderhoeven                             14-DEC-2007
 *      Initial version in CVS
 **/
#if !defined(INCLUDED_SYM53C895_H_)
#define INCLUDED_SYM53C895_H_

#include "DiskController.hpp"
#include "PCIDevice.hpp"
#include "SCSIDevice.hpp"

/**
 * \brief Symbios Sym53C895 SCSI disk controller.
 *
 * \bug PROCGONE bugcheck during OpenVMS boot, probably because proper option
 *ROM is missing.
 *
 * Documentation consulted:
 *  - SCSI 2 (http://www.t10.org/ftp/t10/drafts/s2/s2-r10l.pdf)
 *  - SCSI 3 Multimedia Commands (MMC)
 *(http://www.t10.org/ftp/t10/drafts/mmc/mmc-r10a.pdf)
 *  - SYM53C895 PCI-Ultra2 SCSI I/O Processor
 *(http://www.datasheet4u.com/html/S/Y/M/SYM53C895_LSILogic.pdf.html)
 *  - Symbios SCSI SCRIPTS Processors Programming Guide
 *(http://la.causeuse.org/hauke/macbsd/symbios_53cXXX_doc/lsilogic-53cXXX-scripts.pdf)
 *  .
 **/
class CSym53C895 : public CPCIDevice,
                   public CDiskController,
                   public CSCSIDevice {
public:
  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);
  virtual void check_state();

  void run();
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

  CSym53C895(CConfigurator *cfg, class CSystem *c, int pcibus, int pcidev);
  virtual ~CSym53C895();

private:
  void write_b_scntl0(u8 value);
  void write_b_scntl1(u8 value);
  void write_b_scntl3(u8 value);
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
  u8 read_b_scratcha(int reg);
  u8 read_b_scratchb(int reg);

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

  std::unique_ptr<std::thread> myThread;
  std::atomic_bool myThreadDead{false};
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
