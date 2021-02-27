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
 * Contains the definitions for the ISA part of the emulated Ali M1543C chipset.
 *
 * $Id: AliM1543C.h,v 1.34 2008/05/31 15:47:08 iamcamiel Exp $
 *
 * X-1.34       Camiel Vanderhoeven                             31-MAY-2008
 *      Changes to include parts of Poco.
 *
 * X-1.33       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.32       Camiel Vanderhoeven                             13-MAR-2008
 *      Create init(), start_threads() and stop_threads() functions.
 *
 * X-1.31       Camiel Vanderhoeven                             11-MAR-2008
 *      Named, debuggable mutexes.
 *
 * X-1.30       Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.29       Camiel Vanderhoeven                             26-FEB-2008
 *      Moved DMA code into it's own class (CDMA)
 *
 * X-1.28       Camiel Vanderhoeven                             12-FEB-2008
 *      Moved keyboard code into it's own class (CKeyboard)
 *
 * X-1.27       Camiel Vanderhoeven                             07-FEB-2008
 *      Comments.
 *
 * X-1.26       Brian Wheeler                                   02-FEB-2008
 *      Completed LPT support so it works with FreeBSD as a guest OS.
 *
 * X-1.25       Camiel Vanderhoeven                             08-JAN-2008
 *      Comments.
 *
 * X-1.24       Camiel Vanderhoeven                             02-JAN-2008
 *      Comments; moved keyboard status register bits to "status" struct.
 *
 * X-1.23       Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.22       Brian wheeler                                   17-DEC-2007
 *      Better DMA support.
 *
 * X-1.21       Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.20       Brian Wheeler                                   11-DEC-2007
 *      Improved timer logic (again).
 *
 * X-1.19       Camiel Vanderhoeven                             10-DEC-2007
 *      Use configurator; move IDE and USB to their own classes.
 *
 * X-1.18       Camiel Vanderhoeven                             7-DEC-2007
 *      Add busmaster_status; add pic_edge_level.
 *
 * X-1.17       Camiel Vanderhoeven                             7-DEC-2007
 *      Generate keyboard interrupts when needed.
 *
 * X-1.16       Camiel Vanderhoeven                             6-DEC-2007
 *      Changed keyboard implementation (with thanks to the Bochs project!!)
 *
 * X-1.15       Brian Wheeler                                   1-DEC-2007
 *      Added console support (using SDL library), corrected timer
 *      behavior for Linux/BSD as a guest OS.
 *
 * X-1.14       Camiel Vanderhoeven                             16-APR-2007
 *      Added ResetPCI()
 *
 * X-1.13       Camiel Vanderhoeven                             11-APR-2007
 *      Moved all data that should be saved to a state file to a structure
 *      "state".
 *
 * X-1.12       Camiel Vanderhoeven                             31-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.11	Camiel Vanderhoeven				3-MAR-2007
 *	Added inline function get_ide_disk, which returns a file handle.
 *
 * X-1.10	Camiel Vanderhoeven				20-FEB-2007
 *	Added member variable to keep error status.
 *
 * X-1.9	Brian Wheeler					20-FEB-2007
 *	Information about IDE disks is now kept in the ide_info structure.
 *
 * X-1.8	Camiel Vanderhoeven				16-FEB-2007
 *	DoClock now returns int.
 *
 * X-1.7	Camiel Vanderhoeven				12-FEB-2007
 *	Formatting.
 *
 * X-1.6	Camiel Vanderhoeven				12-FEB-2007
 *	Added comments.
 *
 * X-1.5        Camiel Vanderhoeven                             9-FEB-2007
 *      Replaced f_ variables with ide_ members.
 *
 * X-1.4        Camiel Vanderhoeven                             9-FEB-2007
 *      Added comments.
 *
 * X-1.3        Brian Wheeler                                   3-FEB-2007
 *      Formatting.
 *
 * X-1.2        Brian Wheeler                                   3-FEB-2007
 *      Includes are now case-correct (necessary on Linux)
 *
 * X-1.1        Camiel Vanderhoeven                             19-JAN-2007
 *      Initial version in CVS.
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#if !defined(INCLUDED_ALIM1543C_H_)
#define INCLUDED_ALIM1543C_H_

#include "PCIDevice.hpp"

#define PIT_OFFSET_MAX 6

// RTC register A (MC_BASE_32_KHz is a divider bits configuration)
#define RTC_REG_A 0x0a
#define RTC_UIP 0x80
#define MC_BASE_32_KHz 0x20

// RTC register B (here most options reside; 0x08 is the unused square wave
// enable pin)
#define RTC_REG_B 0x0b
#define RTC_SET 0x80
#define RTC_PIE 0x40
#define RTC_AIE 0x20
#define RTC_UIE 0x10
#define RTC_DM 0x04
#define RTC_2412 0x02
#define RTC_DSE 0x01

// RTC register C (rest of register is always zero)
#define RTC_REG_C 0x0c
#define RTC_IRQF 0x80
#define RTC_PF 0x40
#define RTC_AF 0x20
#define RTC_UF 0x10

// RTC register D (rest of register is always zero)
#define RTC_REG_D 0x0d
#define RTC_VRT 0x80

/**
 * \brief Emulated ISA part of the ALi M1543C chipset.
 *
 * The ALi M1543C device provides i/o and glue logic support to the system:
 * ISA, DMA, Interrupt, Timer, TOY Clock.
 *
 * Documentation consulted:
 *  - Ali M1543C B1 South Bridge Version 1.20
 *(http://mds.gotdns.com/sensors/docs/ali/1543dScb1-120.pdf)
 *  - MC146818 RTC
 *(https://www.nxp.com/docs/en/data-sheet/MC146818.pdf)
 *  - Keyboard Scancodes, by Andries Brouwer
 *(http://www.win.tue.nl/~aeb/linux/kbd/scancodes.html)
 *  .
 **/
class CAliM1543C : public CPCIDevice {
public:
  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);

  virtual void check_state();
  virtual void WriteMem_Legacy(int index, u32 address, int dsize, u32 data);
  virtual u32 ReadMem_Legacy(int index, u32 address, int dsize);

  void do_pit_clock();

  CAliM1543C(CConfigurator *cfg, class CSystem *c, int pcibus, int pcidev);
  virtual ~CAliM1543C();
  void pic_interrupt(int index, int intno);
  void pic_deassert(int index, int intno);

  void init();
  void start_threads();
  void stop_threads();
  void run();

private:
  std::unique_ptr<std::thread> myThread;
  std::atomic_bool myThreadDead{false};
  CMutex *myRegLock;
  bool StopThread;

  struct tm get_time();

  // REGISTER 61 (NMI)
  u8 reg_61_read();
  void reg_61_write(u8 data);

  // REGISTERS 70 - 73: TOY
  u8 toy_read(u32 address);
  void toy_write(u32 address, u8 data);
  void toy_handle_periodic_interrupt(u8 data);
  void toy_update_irqf();

  // Timer/Counter
  u8 pit_read(u32 address);
  void pit_write(u32 address, u8 data);
  void pit_clock();

  // interrupt controller
  u8 pic_read(int index, u32 address);
  void pic_write(int index, u32 address, u8 data);
  u8 pic_read_vector();
  u8 pic_read_edge_level(int index);
  void pic_write_edge_level(int index, u8 data);

  // LPT controller
  u8 lpt_read(u32 address);
  void lpt_write(u32 address, u8 data);
  void lpt_reset();

  /// The state structure contains all elements that need to be saved to the
  /// statefile.
  struct SAli_state {
    // REGISTER 61 (NMI)
    u8 reg_61;

    // REGISTERS 70 - 73: TOY
    u8 toy_stored_data[256];
    u8 toy_access_ports[4];

    // TOY periodic interrupt last fire
    clock_t toy_pi_last_fire;

    // Timer/Counter
    u32 pit_counter[9];
    u8 pit_status[4];
    u8 pit_mode[4];

    // interrupt controller
    int pic_mode[2];
    u8 pic_intvec[2];
    u8 pic_mask[2];
    u8 pic_asserted[2];
    u8 pic_edge_level[2];

    u8 lpt_data;
    u8 lpt_control;
    u8 lpt_status;
    bool lpt_init;
  } state;

  FILE *lpt;
};

extern CAliM1543C *theAli;
#endif // !defined(INCLUDED_ALIM1543C_H_)
