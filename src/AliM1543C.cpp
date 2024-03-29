/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Copyright (C) 2020 Martin Vorländer
 * Copyright (C) 2012 Dmitry Kalinkin
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

#include "AliM1543C.hpp"
#include "StdAfx.hpp"
#include "System.hpp"
#include "VGA.hpp"

#ifdef DEBUG_PIC
bool pic_messages = false;
#endif

/* Timer Calibration: Instructions per Microsecond (assuming 1 clock = 1
 * instruction) */
#define IPus 847

u32 ali_cfg_data[64] = {
    /*00*/ 0x153310b9, // CFID: vendor + device
    /*04*/ 0x0200000f, // CFCS: command + status
    /*08*/ 0x060100c3, // CFRV: class + revision
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
    /*3c*/ 0x00000000, // CFIT: interrupt configuration
    0,
    0,
    0,
    0,
    0,
    /*54*/ 0x00000200, //
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

u32 ali_cfg_mask[64] = {
    /*00*/ 0x00000000, // CFID: vendor + device
    /*04*/ 0x00000000, // CFCS: command + status
    /*08*/ 0x00000000, // CFRV: class + revision
    /*0c*/ 0x00000000, // CFLT: latency timer + cache line size
    /*10*/ 0x00000000, // BAR0
    /*14*/ 0x00000000, // BAR1: CBMA
    /*18*/ 0x00000000, // BAR2:
    /*1c*/ 0x00000000, // BAR3:
    /*20*/ 0x00000000, // BAR4:
    /*24*/ 0x00000000, // BAR5:
    /*28*/ 0x00000000, // CCIC: CardBus
    /*2c*/ 0x00000000, // CSID: subsystem + vendor
    /*30*/ 0x00000000, // BAR6: expansion rom base
    /*34*/ 0x00000000, // CCAP: capabilities pointer
    /*38*/ 0x00000000,
    /*3c*/ 0x00000000, // CFIT: interrupt configuration
    /*40*/ 0xffcfff7f,
    /*44*/ 0xff00cbdf,
    /*48*/ 0xffffffff,
    /*4c*/ 0x000000ff,
    /*50*/ 0xffff8fff,
    /*54*/ 0xf0ffff00,
    /*58*/ 0x030f0d7f,
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
CAliM1543C::CAliM1543C(CConfigurator *cfg, CSystem *c, int pcibus, int pcidev)
    : CPCIDevice(cfg, c, pcibus, pcidev) {
  if (theAli != 0)
    FAILURE(Configuration, "More than one Ali");
  theAli = this;
}

/**
 * Initialize the Ali device.
 **/
void CAliM1543C::init() {
  add_function(0, ali_cfg_data, ali_cfg_mask);

  int i;
  char *filename;

  add_legacy_io(1, 0x61, 1);

  state.reg_61 = 0;

  add_legacy_io(2, 0x70, 4);
  cSystem->RegisterMemory(this, 2, U64(0x00000801fc000070), 4);
  for (i = 0; i < 4; i++)
    state.toy_access_ports[i] = 0;
  for (i = 0; i < 256; i++)
    state.toy_stored_data[i] = 0;

  state.toy_stored_data[0x17] = myCfg->get_bool_value("vga_console") ? 1 : 0;

  if (state.toy_stored_data[0x17] && !theVGA) {
    printf("! CONFIGURATION WARNING ! vga_console set to true, but no VGA card "
           "installed.\n");
    state.toy_stored_data[0x17] = 0;
  }

  state.toy_pi_last_fire = 0;

  ResetPCI();

  // PIT Setup
  add_legacy_io(6, 0x40, 4);
  for (i = 0; i < 3; i++)
    state.pit_status[i] = 0x40; // invalid/null counter
  for (i = 0; i < 9; i++)
    state.pit_counter[i] = 0;

  add_legacy_io(7, 0x20, 2);
  add_legacy_io(8, 0xa0, 2);
  add_legacy_io(30, 0x4d0, 2);

  // odd one, byte read in PCI IACK (interrupt acknowledge) cycle. Interrupt
  // vector.
  cSystem->RegisterMemory(this, 20, U64(0x00000801f8000000), 1);

  for (i = 0; i < 2; i++) {
    state.pic_mode[i] = 0;
    state.pic_intvec[i] = 0;
    state.pic_mask[i] = 0;
    state.pic_asserted[i] = 0;
  }

  // Initialize parallel port
  add_legacy_io(27, 0x3bc, 4);
  filename = myCfg->get_text_value("lpt.outfile");
  if (filename) {
    lpt = fopen(filename, "ab");
  } else {
    lpt = NULL;
  }

  lpt_reset();

  myRegLock = new CMutex("ali-reg");

  printf("%s: $Id: AliM1543C.cpp,v 1.66 2008/05/31 15:47:07 iamcamiel Exp $\n",
         devid_string);
}

void CAliM1543C::start_threads() {
  if (!myThread) {
    printf(" ali");
    StopThread = false;
    myThread = std::make_unique<std::thread>([this]() { this->run(); });
  }
}

void CAliM1543C::stop_threads() {
  StopThread = true;
  if (myThread) {
    printf(" ali");
    myThread->join();
    myThread = nullptr;
  }
}

/**
 * Destructor.
 **/
CAliM1543C::~CAliM1543C() {
  stop_threads();

  if (lpt)
    fclose(lpt);
}

/**
 * Calculates VM time based on the configuration specified in the configuration
 * file.
 * @return The time.
 */
struct tm CAliM1543C::get_time() {
  struct tm time_out;
  time_t time_raw;

  // Timezone setting from configuration file
  std::string timezone{myCfg->get_text_value("timezone", "local")};

  // Time base (local or utc)
  std::string timebase;

  // Time offset
  bool offset_present = false;
  long offset;

  // Get raw time
  time(&time_raw);

  // Set time base
  if (timezone.rfind("local") == 0) {
    timebase = "local";
  } else if (timezone.rfind("utc") == 0) {
    timebase = "utc";
  } else {
    FAILURE_1(Configuration, "Invalid timezone %s", timezone.c_str());
  }

  // Characters remaining after time base
  int remaining_chars = timezone.length() - timebase.length();

  if (remaining_chars > 0 &&
      timezone.at(timezone.length() - remaining_chars) == '+') {
    // An offset is included in the timezone
    offset_present = true;
    sscanf(timezone.c_str() + timezone.length() - remaining_chars + 1, "%ld",
           &offset);
    remaining_chars -= std::to_string(offset).length() + 1;

    if (remaining_chars != 1) {
      // Offset type always has one character
      FAILURE_1(Configuration, "Invalid timezone %s", timezone.c_str());
    }
  } else if (remaining_chars > 0) {
    FAILURE_1(Configuration, "Invalid timezone %s", timezone.c_str());
  }

  if (offset_present) {
    // Apply POSIX time offset (seconds, minutes, hours, days)
    switch (timezone.at(timezone.length() - 1)) {
    case 's':
      time_raw += offset;
      break;
    case 'm':
      time_raw += offset * 60;
      break;
    case 'h':
      time_raw += offset * 3600;
      break;
    case 'd':
      time_raw += offset * 86400;
      break;
    case 'M':
    case 'y':
      break;
    default:
      FAILURE_1(Configuration, "Invalid timezone offset type %c",
                timezone.at(timezone.length() - 1));
    }
  }

  // Convert POSIX time to date
  if (timebase == "local") {
#ifdef _WIN32
    localtime_s(&time_out, &time_raw);
#else
    localtime_s(&time_raw, &time_out);
#endif
  } else if (timebase == "utc") {
    gmtime_s(&time_out, &time_raw);
  } else {
    // This shouldn't happen
    FAILURE_1(Configuration, "Invalid timezone %s", timezone.c_str());
  }

  if (offset_present) {
    // Apply date offset (months, years)
    switch (timezone.at(timezone.length() - 1)) {
    case 'M':
      time_out.tm_year += offset / 12;
      time_out.tm_mon += offset % 12;
      break;
    case 'y':
      time_out.tm_year += offset;
      break;
    }

    // Fix day of week
    mktime(&time_out);
  }

  return time_out;
}

/**
 * Read (byte,word,longword) from one of the legacy ranges. Only byte-accesses
 *are supported.
 *
 * Ranges are:
 *  - 1. I/O port 61h
 *  - 2. I/O ports 70h-73h (time-of-year clock)
 *  - 6. I/O ports 40h-43h (programmable interrupt timer)
 *  - 7. I/O ports 20h-21h (primary programmable interrupt controller)
 *  - 8. I/O ports a0h-a1h (secondary (cascaded) programmable interrupt
 *controller)
 *  - 20. PCI IACK address (interrupt vector)
 *  - 27. I/O ports 3bch-3bfh (parallel port)
 *  - 30. I/O ports 4d0h-4d1h (edge/level register of programmable interrupt
 *controller)
 *  .
 **/
u32 CAliM1543C::ReadMem_Legacy(int index, u32 address, int dsize) {
  if (dsize != 8 &&
      index != 20) // when interrupt vector is read, dsize doesn't matter.
  {
    FAILURE_4(
        InvalidArgument,
        "%s: DSize %d reading from legacy memory range # %d at address %02x\n",
        devid_string, dsize, index, address);
  }

  int channel = 0;
  switch (index) {
  case 1:
    return reg_61_read();
  case 2:
    return toy_read(address);
  case 6:
    return pit_read(address);
  case 8:
    channel = 1;
  case 7:
    return pic_read(channel, address);
  case 20:
    return pic_read_vector();
  case 30:
    return pic_read_edge_level(address);
  case 27:
    return lpt_read(address);
  }

  return 0;
}

/**
 * Write (byte,word,longword) to one of the legacy ranges. Only byte-accesses
 *are supported.
 *
 * Ranges are:
 *  - 1. I/O port 61h
 *  - 2. I/O ports 70h-73h (time-of-year clock)
 *  - 6. I/O ports 40h-43h (programmable interrupt timer)
 *  - 7. I/O ports 20h-21h (primary programmable interrupt controller)
 *  - 8. I/O ports a0h-a1h (secondary (cascaded) programmable interrupt
 *controller)
 *  - 12. I/O ports 00h-0fh (primary DMA controller)
 *  - 13. I/O ports c0h-dfh (secondary DMA controller)
 *  - 20. PCI IACK address (interrupt vector)
 *  - 27. I/O ports 3bch-3bfh (parallel port)
 *  - 30. I/O ports 4d0h-4d1h (edge/level register of programmable interrupt
 *controller)
 *  - 33. I/O ports 80h-8fh (DMA controller memory base low page register)
 *  - 34. I/O ports 480h-48fh (DMA controller memory base high page register)
 *  .
 **/
void CAliM1543C::WriteMem_Legacy(int index, u32 address, int dsize, u32 data) {
  if (dsize != 8) {
    FAILURE_4(
        InvalidArgument,
        "%s: DSize %d writing to legacy memory range # %d at address %02x\n",
        devid_string, dsize, index, address);
  }

  int channel = 0;
  switch (index) {
  case 1:
    reg_61_write((u8)data);
    return;
  case 2:
    toy_write(address, (u8)data);
    return;
  case 6:
    pit_write(address, (u8)data);
    return;
  case 8:
    channel = 1;
  case 7:
    pic_write(channel, address, (u8)data);
    return;
  case 30:
    pic_write_edge_level(address, (u8)data);
    return;
  case 27:
    lpt_write(address, (u8)data);
    return;
  }
}

/**
 * Read port 61h (speaker/ miscellaneous).
 *
 * BDW:
 * This may need some expansion to help with timer delays.  It looks like
 * the 8254 flips bits on occasion, and the linux kernel (at least) uses
 *   do {
 *     count++;
 *   } while ((inb(0x61) & 0x20) == 0 && count < TIMEOUT_COUNT);
 * to calibrate the cpu clock.
 *
 * Every 1500 reads the bit gets flipped so maybe the timing will
 * seem reasonable to the OS.
 */
u8 CAliM1543C::reg_61_read() {
#if 0
  static long read_count = 0;
  if(!(state.reg_61 & 0x20))
  {
    if(read_count % 1500 == 0)
      state.reg_61 |= 0x20;
  }
  else
  {
    state.reg_61 &= ~0x20;
  }

  read_count++;
#else
  state.reg_61 &= ~0x20;
  state.reg_61 |= (state.pit_status[2] & 0x80) >> 2;
#endif
  return state.reg_61;
}

/**
 * Write port 61h (speaker/ miscellaneous).
 **/
void CAliM1543C::reg_61_write(u8 data) {
  state.reg_61 = (state.reg_61 & 0xf0) | (((u8)data) & 0x0f);
}

/**
 * Read time-of-year clock ports (70h-73h).
 **/
u8 CAliM1543C::toy_read(u32 address) {
  // printf("%%ALI-I-READTOY: read port %02x: 0x%02x\n", (u32)(0x70 + address),
  // state.toy_access_ports[address]);
  return (u8)state.toy_access_ports[address];
}

/**
 * Write time-of-year clock ports (70h-73h). On a write to port 0, recalculate
 * clock values.
 **/
void CAliM1543C::toy_write(u32 address, u8 data) {
  struct tm stime;
  static long read_count = 0;
  static long hold_count = 0;

  // printf("%%ALI-I-WRITETOY: write port %02x: 0x%02x\n", (u32)(0x70 +
  // address), data);
  state.toy_access_ports[address] = (u8)data;

  switch (address) {
  case 0:
    if ((data & 0x7f) < 14) {
      // Assign VRT (valid RAM and time) bit
      state.toy_stored_data[RTC_REG_D] = RTC_VRT;
      // Update time
      stime = get_time();

      if (state.toy_stored_data[RTC_REG_B] & RTC_DM) {
        // binary
        state.toy_stored_data[0] = (u8)(stime.tm_sec);
        state.toy_stored_data[2] = (u8)(stime.tm_min);
        if (state.toy_stored_data[RTC_REG_B] & RTC_2412) // 24-hour
          state.toy_stored_data[4] = (u8)(stime.tm_hour);
        else
          // 12-hour
          state.toy_stored_data[4] =
              (u8)(((stime.tm_hour / 12) ? 0x80 : 0) | (stime.tm_hour % 12));
        state.toy_stored_data[6] = (u8)(stime.tm_wday + 1);
        state.toy_stored_data[7] = (u8)(stime.tm_mday);
        state.toy_stored_data[8] = (u8)(stime.tm_mon + 1);
        state.toy_stored_data[9] = (u8)(stime.tm_year % 100);
      } else {
        // BCD
        state.toy_stored_data[0] =
            (u8)(((stime.tm_sec / 10) << 4) | (stime.tm_sec % 10));
        state.toy_stored_data[2] =
            (u8)(((stime.tm_min / 10) << 4) | (stime.tm_min % 10));
        if (state.toy_stored_data[0x0b] & 2) // 24-hour
          state.toy_stored_data[4] =
              (u8)(((stime.tm_hour / 10) << 4) | (stime.tm_hour % 10));
        else { // 12-hour
          state.toy_stored_data[4] = (u8)(((stime.tm_hour / 12) ? 0x80 : 0) |
                                          (((stime.tm_hour % 12) / 10) << 4) |
                                          ((stime.tm_hour % 12) % 10));
        }

        state.toy_stored_data[6] = (u8)(stime.tm_wday + 1);
        state.toy_stored_data[7] =
            (u8)(((stime.tm_mday / 10) << 4) | (stime.tm_mday % 10));
        state.toy_stored_data[8] =
            (u8)((((stime.tm_mon + 1) / 10) << 4) | ((stime.tm_mon + 1) % 10));
        state.toy_stored_data[9] = (u8)((((stime.tm_year % 100) / 10) << 4) |
                                        ((stime.tm_year % 100) % 10));
      }

      // SRM initializes the value of A register to 0x26. This means:
      //  xtal speed is set to MC_BASE_32_KHz 32.768KHz (standard)
      //  periodic interrupt rate divisor of 32 = interrupt every 976.562 ms
      //  (1024Hz clock)
      if (state.toy_stored_data[RTC_REG_A] & RTC_UIP) {
        // Once the UIP line goes high, we have to stay high for 2228us.
        hold_count--;
        if (hold_count == 0 || (state.toy_stored_data[RTC_REG_B] & RTC_SET)) {
          // Set UIP low and trigger the related interrupt.
          state.toy_stored_data[RTC_REG_A] &= ~RTC_UIP;
          state.toy_stored_data[RTC_REG_C] |= RTC_UF;
          toy_update_irqf();
          read_count = 0;
        }
      } else {
        // UIP isn't high, so if we're looping and waiting for it to go, it
        // will take 1,000,000/(IPus*3) reads for a 3 instruction loop.
        // If it happens to be a one time read, it'll only throw our
        // calculations off a tiny bit, and they'll be re-synced on the next
        // read-loop.
        read_count++;
        if (read_count > 1000000 / (IPus * 3)) // 3541 @ 847IPus
        {
          state.toy_stored_data[RTC_REG_A] |= RTC_UIP;
          hold_count =
              (2228 / (IPus * 3)) + 1; // .876 @ 847IPus, so we add one.
        }
      }
    }

    toy_handle_periodic_interrupt(data);
    toy_update_irqf();

    // Assign specified data to port so it can be read by the program
    state.toy_access_ports[1] = state.toy_stored_data[data & 0x7f];

    // Register C is cleared after a read, and we don't care if it's a write
    if (data == RTC_REG_C)
      state.toy_stored_data[data & 0x7f] = 0;

    break;
  case 1:
    if (state.toy_access_ports[0] == RTC_REG_B &&
        data & 0x040) // If we're writing to register B, we make register C look
                      // like it fired.
                      // TODO: Do actual interrupt implementation instead of
                      //       a workaround.
      state.toy_stored_data[RTC_REG_C] = 0xf0;
    state.toy_stored_data[state.toy_access_ports[0] & 0x7f] = (u8)data;
    break;

  case 2:
    state.toy_access_ports[3] = state.toy_stored_data[0x80 + (data & 0x7f)];
    break;

  case 3:
    state.toy_stored_data[0x80 + (state.toy_access_ports[2] & 0x7f)] = (u8)data;
    break;
  }
}

/**
 * Handle RTC periodic interrupt.
 **/
void CAliM1543C::toy_handle_periodic_interrupt(u8 data) {
  /*
   See sys/dev/ic/mc146818reg.h and sys/arch/alpha/alpha/mcclock.c in NetBSD and
   the RTC datasheet: https://www.nxp.com/docs/en/data-sheet/MC146818.pdf.
  */
  clock_t now = clock();
  double timedelta = (now - state.toy_pi_last_fire) / (double)CLOCKS_PER_SEC;

  // For the meaning of the period calculation see the table on page 14 of the
  // aforementioned datasheet
  int rate_pow = state.toy_stored_data[RTC_REG_A] & 0x0f;
  double period = (1 << rate_pow) / 65536.0;

  if (state.toy_stored_data[RTC_REG_A] & MC_BASE_32_KHz) {
    if (rate_pow == 0x1) {
      period = 1 / 256.0;
    } else if (rate_pow == 0x2) {
      period = 1 / 128.0;
    }
  }

  if (rate_pow && (timedelta >= period)) {
    // Elapsed time since last check is equal or greater than the specified
    // period - fire the interrupt by setting the PF flag in register C
    // (see page 16 in the datasheet).
    state.toy_stored_data[RTC_REG_C] |= RTC_PF;
    state.toy_pi_last_fire = now;
  }
}

/**
 * Update RTC interrupt request flag
 **/
void CAliM1543C::toy_update_irqf() {
  if ((state.toy_stored_data[RTC_REG_B] & RTC_PIE &&
       state.toy_stored_data[RTC_REG_C] & RTC_PF) ||
      (state.toy_stored_data[RTC_REG_B] & RTC_UIE &&
       state.toy_stored_data[RTC_REG_C] & RTC_UF) ||
      (state.toy_stored_data[RTC_REG_B] & RTC_AIE &&
       state.toy_stored_data[RTC_REG_C] & RTC_AF))
    state.toy_stored_data[RTC_REG_C] |= RTC_IRQF;
  else
    state.toy_stored_data[RTC_REG_C] &= ~RTC_IRQF;
}

/**
 * Read from the programmable interrupt timer ports (40h-43h)
 *
 * BDW:
 * Here's the PIT Traffic during SRM and Linux Startup:
 *
 * SRM
 * PIT Write:  3, 36  = counter 0, load lsb + msb, mode 3
 * PIT Write:  0, 00
 * PIT Write:  0, 00  = 65536 = 18.2 Hz = timer interrupt.
 * PIT Write:  3, 54  = counter 1, msb only, mode 2
 * PIT Write:  1, 12  = 0x1200 = memory refresh?
 * PIT Write:  3, b6  = counter 2, load lsb + msb, mode 3
 * PIT Write:  3, 00
 * PIT Write:  0, 00
 * PIT Write:  0, 00
 *
 * Linux Startup
 * PIT Write:  3, b0  = counter 2, load lsb+msb, mode 0
 * PIT Write:  2, a4
 * PIT Write:  2, ec  = eca4
 * PIT Write:  3, 36  = counter 0, load lsb+msb, mode 3
 * PIT Write:  0, 00
 * PIT Write:  0, 00  = 65536
 * PIT Write:  3, b6  = counter 2, load lsb+msb, mode 3
 * PIT Write:  2, 31
 * PIT Write:  2, 13  = 1331
 **/
u8 CAliM1543C::pit_read(u32 address) {

  // printf("PIT Read: %02" PRIx64 " \n",address);
  u8 data;
  data = 0;
  return data;
}

/**
 * Write to the programmable interrupt timer ports (40h-43h)
 **/
void CAliM1543C::pit_write(u32 address, u8 data) {

  // printf("PIT Write: %02" PRIx64 ", %02x \n",address,data);
  if (address == 3) { // control
    if (data != 0) {
      state.pit_status[address] = data; // last command seen.
      if ((data & 0xc0) >> 6 != 3) {
        state.pit_status[(data & 0xc0) >> 6] = data & 0x3f;
        state.pit_mode[(data & 0xc0) >> 6] = (data & 0x30) >> 4;
      } else {                            // readback command 8254 only
        state.pit_status[address] = 0xc0; // bogus :)
      }
    }
  } else { // a counter
    switch (state.pit_mode[address]) {
    case 0:
      break;

    case 1:
    case 3:
      state.pit_counter[address] =
          (state.pit_counter[address] & 0xff) | data << 8;
      state.pit_counter[address + PIT_OFFSET_MAX] = state.pit_counter[address];
      if (state.pit_mode[address] == 3) {
        state.pit_mode[address] = 2;
      } else
        state.pit_status[address] &= ~0xc0; // no longer high, counter valid.
      break;

    case 2:
      state.pit_counter[address] = (state.pit_counter[address] & 0xff00) | data;

      // two bytes were written with 0x00, so its really 0x10000
      if ((state.pit_status[address] & 0x30) >> 4 == 3 &&
          state.pit_counter[address] == 0) {
        state.pit_counter[address] = 65536;
      }

      state.pit_counter[address + PIT_OFFSET_MAX] = state.pit_counter[address];
      state.pit_status[address] &= ~0xc0; // no longer high, counter valid.
      break;
    }
  }
}

#define PIT_FACTOR 5000
#define PIT_DEC(p) p = (p < PIT_FACTOR ? 0 : p - PIT_FACTOR);

/**
 * Handle the PIT interrupt.
 *
 *  - counter 0 is the 18.2Hz time counter.
 *  - counter 1 is the ram refresh, we don't care.
 *  - counter 2 is the speaker and/or generic timer
 *  .
 **/
void CAliM1543C::pit_clock() {
  int i;
  for (i = 0; i < 3; i++) {
    // decrement the counter.
    if (state.pit_status[i] & 0x40)
      continue;
    PIT_DEC(state.pit_counter[i]);
    switch ((state.pit_status[i] & 0x0e) >> 1) {
    case 0: // interrupt at terminal
      if (!state.pit_counter[i]) {
        state.pit_status[i] |= 0xc0; // out pin high, no count set.
      }
      break;

    case 3: // square wave generator
      if (!state.pit_counter[i]) {
        if (state.pit_status[i] & 0x80) {
          state.pit_status[i] &= ~0x80; // lower output;
        } else {
          state.pit_status[i] |= 0x80; // raise output
          if (i == 0) {
            pic_interrupt(0, 0); // counter 0 is tied to irq 0.
            // printf("Generating timer interrupt.\n");
          }
        }

        state.pit_counter[i] = state.pit_counter[i + PIT_OFFSET_MAX];
      }

      // decrement again, since we want a half-wide square wave.
      PIT_DEC(state.pit_counter[i]);
      break;

    default:
      break; // we don't care to handle it.
    }
  }
}

/**
 * Thread entry point.
 **/
void CAliM1543C::run() {
  try {
    for (;;) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      if (StopThread)
        return;
      do_pit_clock();
    }
  }

  catch (CException &e) {
    printf("Exception in Ali thread: %s.\n", e.displayText().c_str());
    myThreadDead.store(true);
    // Let the thread die...
  }
}

#define PIT_RATIO 1

/**
 * Handle all events that need to be handled on a clock-driven basis.
 *
 * This is a slow-clocked device, which means this DoClock isn't called as often
 *as the CPU's DoClock. Do the following:
 *  - Handle PIT clock.
 *  .
 **/
void CAliM1543C::do_pit_clock() {
  static int pit_counter = 0;
  if (pit_counter++ >= PIT_RATIO) {
    pit_counter = 0;
    pit_clock();
  }
}

#define PIC_STD 0
#define PIC_INIT_0 1
#define PIC_INIT_1 2
#define PIC_INIT_2 3

/**
 * Read a byte from one of the programmable interrupt controller's registers.
 **/
u8 CAliM1543C::pic_read(int index, u32 address) {
  u8 data;

  data = 0;

  if (address == 1)
    data = state.pic_mask[index];

#ifdef DEBUG_PIC
  if (pic_messages)
    printf("%%PIC-I-READ: read %02x from port %" PRId64 " on PIC %d\n", data,
           address, index);
#endif
  return data;
}

/**
 * Read a byte from the edge/level register of one of the programmable interrupt
 *controllers.
 **/
u8 CAliM1543C::pic_read_edge_level(int index) {
  return state.pic_edge_level[index];
}

/**
 * Read the interrupt vector during a PCI IACK cycle.
 **/
u8 CAliM1543C::pic_read_vector() {
  if (state.pic_asserted[0] & 1)
    return state.pic_intvec[0];
  if (state.pic_asserted[0] & 2)
    return state.pic_intvec[0] + 1;
  if (state.pic_asserted[0] & 4) {
    if (state.pic_asserted[1] & 1)
      return state.pic_intvec[1];
    if (state.pic_asserted[1] & 2)
      return state.pic_intvec[1] + 1;
    if (state.pic_asserted[1] & 4)
      return state.pic_intvec[1] + 2;
    if (state.pic_asserted[1] & 8)
      return state.pic_intvec[1] + 3;
    if (state.pic_asserted[1] & 16)
      return state.pic_intvec[1] + 4;
    if (state.pic_asserted[1] & 32)
      return state.pic_intvec[1] + 5;
    if (state.pic_asserted[1] & 64)
      return state.pic_intvec[1] + 6;
    if (state.pic_asserted[1] & 128)
      return state.pic_intvec[1] + 7;
  }

  if (state.pic_asserted[0] & 8)
    return state.pic_intvec[0] + 3;
  if (state.pic_asserted[0] & 16)
    return state.pic_intvec[0] + 4;
  if (state.pic_asserted[0] & 32)
    return state.pic_intvec[0] + 5;
  if (state.pic_asserted[0] & 64)
    return state.pic_intvec[0] + 6;
  if (state.pic_asserted[0] & 128)
    return state.pic_intvec[0] + 7;
  return 0;
}

/**
 * Write a byte to one of the programmable interrupt controller's registers.
 **/
void CAliM1543C::pic_write(int index, u32 address, u8 data) {
  int level;
  int op;
#ifdef DEBUG_PIC
  if (pic_messages)
    printf("%%PIC-I-WRITE: write %02x to port %" PRId64 " on PIC %d\n", data,
           address, index);
#endif
  switch (address) {
  case 0:
    if (data & 0x10)
      state.pic_mode[index] = PIC_INIT_0;
    else
      state.pic_mode[index] = PIC_STD;
    if (data & 0x08) {

      // OCW3
    } else {

      // OCW2
      op = (data >> 5) & 7;
      level = data & 7;
      switch (op) {
      case 1:

        // non-specific EOI
        state.pic_asserted[index] = 0;

        //
        if (index == 1)
          state.pic_asserted[0] &= ~(1 << 2);

        //
        if (!state.pic_asserted[0])
          cSystem->interrupt(55, false);
#ifdef DEBUG_PIC
        pic_messages = false;
#endif
        break;

      case 3:

        // specific EOI
        state.pic_asserted[index] &= ~(1 << level);

        //
        if ((index == 1) && (!state.pic_asserted[1]))
          state.pic_asserted[0] &= ~(1 << 2);

        //
        if (!state.pic_asserted[0])
          cSystem->interrupt(55, false);
#ifdef DEBUG_PIC
        pic_messages = false;
#endif
        break;
      }
    }

    return;

  case 1:
    switch (state.pic_mode[index]) {
    case PIC_INIT_0:
      state.pic_intvec[index] = (u8)data & 0xf8;
      state.pic_mode[index] = PIC_INIT_1;
      return;

    case PIC_INIT_1:
      state.pic_mode[index] = PIC_INIT_2;
      return;

    case PIC_INIT_2:
      state.pic_mode[index] = PIC_STD;
      return;

    case PIC_STD:
      state.pic_mask[index] = data;
      state.pic_asserted[index] &= ~data;
      return;
    }
  }
}

/**
 * Write a byte to the edge/level register of one of the programmable interrupt
 *controllers.
 **/
void CAliM1543C::pic_write_edge_level(int index, u8 data) {
  state.pic_edge_level[index] = data;
}

#define DEBUG_EXPR (index != 0 || (intno != 0 && intno > 4))

/**
 * Assert an interrupt on one of the programmable interrupt controllers.
 **/
void CAliM1543C::pic_interrupt(int index, int intno) {
#ifdef DEBUG_PIC
  if (DEBUG_EXPR) {
    printf("%%PIC-I-INCOMING: Interrupt %d incomming on PIC %d", intno, index);
    pic_messages = true;
  }
#endif

  // do we have this interrupt enabled?
  if (state.pic_mask[index] & (1 << intno)) {
#ifdef DEBUG_PIC
    if (DEBUG_EXPR)
      printf(" (masked)\n");
    pic_messages = false;
#endif
    return;
  }

  if (state.pic_asserted[index] & (1 << intno)) {
#ifdef DEBUG_PIC
    if (DEBUG_EXPR)
      printf(" (already asserted)\n");
#endif
    return;
  }

#ifdef DEBUG_PIC
  if (DEBUG_EXPR)
    printf("\n");
#endif
  state.pic_asserted[index] |= (1 << intno);

  if (index == 1)
    pic_interrupt(0, 2); // cascade
  if (index == 0)
    cSystem->interrupt(55, true);
}

/**
 * De-assert an interrupt on one of the programmable interrupt controllers.
 **/
void CAliM1543C::pic_deassert(int index, int intno) {
  if (!(state.pic_asserted[index] & (1 << intno)))
    return;

  //  printf("De-asserting %d,%d\n",index,intno);
  state.pic_asserted[index] &= ~(1 << intno);
  if (index == 1 && state.pic_asserted[1] == 0)
    pic_deassert(0, 2); // cascade
  if (index == 0 && state.pic_asserted[0] == 0)
    cSystem->interrupt(55, false);
}

static u32 ali_magic1 = 0xA111543C;
static u32 ali_magic2 = 0xC345111A;

/**
 * Save state to a Virtual Machine State file.
 **/
int CAliM1543C::SaveState(FILE *f) {
  long ss = sizeof(state);
  int res;

  if ((res = CPCIDevice::SaveState(f)))
    return res;

  fwrite(&ali_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&ali_magic2, sizeof(u32), 1, f);
  printf("%s: %d bytes saved.\n", devid_string, (int)ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CAliM1543C::RestoreState(FILE *f) {
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

  if (m1 != ali_magic1) {
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

  if (m2 != ali_magic2) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  printf("%s: %d bytes restored.\n", devid_string, (int)ss);
  return 0;
}

/**
 * Parallel Port information:
 * address 0 (R/W):  data pins.  On read, the last byte written is returned.
 *
 *
 * address 1 (R): status register
 * \code
 *   1 0 0 0 0 0 00 <-- default
 *   ^ ^ ^ ^ ^ ^ ^
 *   | | | | | | +- Undefined
 *   | | | | | +--- IRQ (undefined?)
 *   | | | | +----- printer has error condition
 *   | | | +------- printer is not selected.
 *   | | +--------- printer has paper (online)
 *   | +----------- printer is asserting 'ack'
 *   +------------- printer busy (active low).
 * \endcode
 *
 * address 2 (R/W): control register.
 * \code
 *   00 0 0 1 0 1 1  <-- default
 *   ^  ^ ^ ^ ^ ^ ^
 *   |  | | | | | +-- Strobe (active low)
 *   |  | | | | +---- Auto feed (active low)
 *   |  | | | +------ Initialize
 *   |  | | +-------- Select (active low)
 *   |  | +---------- Interrupt Control
 *   |  +------------ Bidirectional control (unimplemented)
 *   +--------------- Unused
 * \endcode
 **/
void CAliM1543C::lpt_reset() {
  state.lpt_data = ~0;
  state.lpt_status = 0xd8;  // busy, ack, online, error
  state.lpt_control = 0x0c; // select, init
  state.lpt_init = false;
}

/**
 * Read a byte from one of the parallel port controller's registers.
 **/
u8 CAliM1543C::lpt_read(u32 address) {
  u8 data = 0;
  switch (address) {
  case 0:
    data = state.lpt_data;
    break;

  case 1:
    data = state.lpt_status;
    if ((state.lpt_status & 0x80) == 0 && (state.lpt_control & 0x01) == 0) {
      if (state.lpt_status & 0x40) { // test ack
        state.lpt_status &= ~0x40;   // turn off ack
      } else {
        state.lpt_status |= 0x40; // set ack.
        state.lpt_status |= 0x80; // set (not) busy.
      }
    }
    break;

  case 2:
    data = state.lpt_control;
  }

#ifdef DEBUG_LPT
  printf("%%LPT-I-READ: port %d = %x\n", address, data);
#endif
  return data;
}

/**
 * Write a byte to one of the parallel port controller's registers.
 **/
void CAliM1543C::lpt_write(u32 address, u8 data) {
#ifdef DEBUG_LPT
  printf("%%LPT-I-WRITE: port %d = %x\n", address, data);
#endif
  switch (address) {
  case 0:
    state.lpt_data = data;
    break;

  case 1:
    break;

  case 2:
    if ((data & 0x04) == 0) {
      state.lpt_init = true;
      state.lpt_status = 0xd8;
    } else {
      if (data & 0x08) {             // select bit
        if (data & 0x01) {           // strobe?
          state.lpt_status &= ~0x80; // we're busy

          // do the write!
          if (lpt && state.lpt_init)
            fputc(state.lpt_data, lpt);
          if (state.lpt_control & 0x10) {
            pic_interrupt(0, 7);
          }
        } else {

          // ?
        }
      }
    }

    state.lpt_control = data;
  }
}

/**
 * Check if threads are still running.
 **/
void CAliM1543C::check_state() {
  if (myThreadDead.load())
    FAILURE(Thread, "ALi thread has died");
}

CAliM1543C *theAli = 0;
