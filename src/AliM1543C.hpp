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
