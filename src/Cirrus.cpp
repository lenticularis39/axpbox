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
 * Contains the code for the emulated Cirrus CL GD-5434 Video Card device.
 *
 * $Id: Cirrus.cpp,v 1.23 2008/05/31 15:47:09 iamcamiel Exp $
 *
 * X-1.23       Camiel Vanderhoeven                             31-MAY-2008
 *      Changes to include parts of Poco.
 *
 * X-1.22       Camiel Vanderhoeven                             13-APR-2008
 *      Fixed Doxygen comment.
 *
 * X-1.21       Pepito Grillo                                   25-MAR-2008
 *      Fixed a typo in the last patch.
 *
 * X-1.20       Camiel Vanderhoeven                             24-MAR-2008
 *      Added comments on VGA registers.
 *
 * X-1.19       Camiel Vanderhoeven                             16-MAR-2008
 *      Fixed threading problems with SDL (I hope).
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
 * X-1.15       Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.14       Brian Wheeler                                   27-FEB-2008
 *      Avoid compiler warnings.
 *
 * X-1.13       David Leonard                                   20-FEB-2008
 *      Shut down refresh thread when emulator exits.
 *
 * X-1.12       David Leonard                                   20-FEB-2008
 *      Avoid 'Xlib: unexpected async reply' errors on Linux/Unix/BSD's by
 *      adding some thread interlocking.
 *
 * X-1.11       Camiel Vanderhoeven                             03-JAN-2008
 *      Attempt to get this working for big-endian host architectures.
 *
 * X-1.10       Camiel Vanderhoeven                             02-JAN-2008
 *      Cleanup.
 *
 * X-1.9        Camiel Vanderhoeven                             30-DEC-2007
 *      Print file id on initialization.
 *
 * X-1.8        Camiel Vanderhoeven                             28-DEC-2007
 *      Throw exceptions rather than just exiting when errors occur.
 *
 * X-1.7        Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.6        Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.5        Camiel Vabderhoeven                             11-DEC-2007
 *      Don't claim IO addresses 3d0..3d3, 3d6..3d9 and 3db..3df.
 *
 * X-1.4        Camiel Vabderhoeven                             11-DEC-2007
 *      Don't claim IO addresses 3b0..3b3, 3b6..3b9 and 3bb.
 *
 * X-1.3        Brian Wheeler                                   10-DEC-2007
 *      Made refresh function name unique.
 *
 * X-1.2        Camiel Vanderhoeven                             10-DEC-2007
 *      Don't decode IO addresses 3bc-3bf.
 *
 * X-1.1        Camiel Vanderhoeven                             10-DEC-2007
 *      Initial version in CVS.
 **/
#include "Cirrus.hpp"
#include "AliM1543C.hpp"
#include "StdAfx.hpp"
#include "System.hpp"
#include "gui/gui.hpp"

static unsigned old_iHeight = 0, old_iWidth = 0, old_MSL = 0;

static const u8 ccdat[16][4] = {
    {0x00, 0x00, 0x00, 0x00}, {0xff, 0x00, 0x00, 0x00},
    {0x00, 0xff, 0x00, 0x00}, {0xff, 0xff, 0x00, 0x00},
    {0x00, 0x00, 0xff, 0x00}, {0xff, 0x00, 0xff, 0x00},
    {0x00, 0xff, 0xff, 0x00}, {0xff, 0xff, 0xff, 0x00},
    {0x00, 0x00, 0x00, 0xff}, {0xff, 0x00, 0x00, 0xff},
    {0x00, 0xff, 0x00, 0xff}, {0xff, 0xff, 0x00, 0xff},
    {0x00, 0x00, 0xff, 0xff}, {0xff, 0x00, 0xff, 0xff},
    {0x00, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff},
};

/**
 * Set a specific tile's updated variable.
 *
 * Only reference the array if the tile numbers are within the bounds
 * of the array.  If out of bounds, do nothing.
 **/
#define SET_TILE_UPDATED(xtile, ytile, value)                                  \
  do {                                                                         \
    if (((xtile) < BX_NUM_X_TILES) && ((ytile) < BX_NUM_Y_TILES))              \
      state.vga_tile_updated[(xtile)][(ytile)] = value;                        \
  } while (0)

/**
 * Get a specific tile's updated variable.
 *
 * Only reference the array if the tile numbers are within the bounds
 * of the array.  If out of bounds, return 0.
 **/
#define GET_TILE_UPDATED(xtile, ytile)                                         \
  ((((xtile) < BX_NUM_X_TILES) && ((ytile) < BX_NUM_Y_TILES))                  \
       ? state.vga_tile_updated[(xtile)][(ytile)]                              \
       : 0)

/**
 * Thread entry point.
 *
 * The thread first initializes the GUI, and then starts looping the
 * following actions until interrupted (by StopThread being set to true)
 *   - Handle any GUI events (mouse moves, keypresses)
 *   - Update the GUI to match the screen buffer
 *   - Flush the updated GUI content to the screen
 *   .
 **/
void CCirrus::run() {
  try {
    // initialize the GUI (and let it know our tilesize)
    bx_gui->init(state.x_tilesize, state.y_tilesize);
    for (;;) {
      // Terminate thread if StopThread is set to true
      if (StopThread)
        return;
      // Handle GUI events (100 times per second)
      for (int i = 0; i < 10; i++) {
        bx_gui->lock();
        bx_gui->handle_events();
        bx_gui->unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      // Update the screen (10 times per second)
      bx_gui->lock();
      update();
      bx_gui->flush();
      bx_gui->unlock();
    }
  }

  catch (CException &e) {
    printf("Exception in Cirrus thread: %s.\n", e.displayText().c_str());
    myThreadDead.store(true);
    // Let the thread die...
  }
}

/** Size of ROM image */
static unsigned int rom_max;

/** ROM image */
static u8 option_rom[65536];

/** PCI Configuration Space data block */
static u32 cirrus_cfg_data[64] = {
    /*00*/ 0x00a81013, // CFID: vendor + device
    /*04*/ 0x011f0000, // CFCS: command + status
    /*08*/ 0x03000002, // CFRV: class + revision
    /*0c*/ 0x00000000, // CFLT: latency timer + cache line size
    /*10*/ 0xf8000000, // BAR0: FB
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
    /*3c*/ 0x281401ff, // CFIT: interrupt configuration
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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

/** PCI Configuration Space mask block */
static u32 cirrus_cfg_mask[64] = {
    /*00*/ 0x00000000, // CFID: vendor + device
    /*04*/ 0x0000ffff, // CFCS: command + status
    /*08*/ 0x00000000, // CFRV: class + revision
    /*0c*/ 0x0000ffff, // CFLT: latency timer + cache line size
    /*10*/ 0xfc000000, // BAR0: FB
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
 *
 **/
CCirrus::CCirrus(CConfigurator *cfg, CSystem *c, int pcibus, int pcidev)
    : CVGA(cfg, c, pcibus, pcidev) {
  // initialize state
  memset(&state, 1, sizeof(struct SCirrus_state));
}

/**
 * Initialize the Cirrus device.
 **/
void CCirrus::init() {
  // Register PCI device
  add_function(0, cirrus_cfg_data, cirrus_cfg_mask);

  // Initialize all state variables to 0
  memset((void *)&state, 0, sizeof(state));

  // Register VGA I/O ports at 3b4, 3b5, 3ba, 3c0..cf, 3d4, 3d5, 3da
  add_legacy_io(1, 0x3b4, 2);
  add_legacy_io(3, 0x3ba, 2);
  add_legacy_io(2, 0x3c0, 16);
  add_legacy_io(8, 0x3d4, 2);
  add_legacy_io(9, 0x3da, 1);

  /* The VGA BIOS we use sends text messages to port 0x500.
     We listen for these messages at port 500. */
  add_legacy_io(7, 0x500, 1);
  bios_message_size = 0;
  bios_message[0] = '\0';

  // Legacy video address space: A0000 -> bffff
  add_legacy_mem(4, 0xa0000, 128 * 1024);

  // Reset the base PCI device
  ResetPCI();

  /* The configuration file variable "rom" should point to a VGA BIOS
     image. If not, try "vgabios.bin". */
  FILE *rom = fopen(myCfg->get_text_value("rom", "vgabios.bin"), "rb");
  if (!rom) {
    FAILURE_1(FileNotFound, "cirrus rom file %s not found.",
              myCfg->get_text_value("rom", "vgabios.bin"));
  }

  rom_max = (unsigned)fread(option_rom, 1, 65536, rom);
  fclose(rom);

  // Option ROM address space: C0000
  add_legacy_mem(5, 0xc0000, rom_max);

  state.vga_enabled = 1;
  state.misc_output.color_emulation = 1;
  state.misc_output.enable_ram = 1;
  state.misc_output.horiz_sync_pol = 1;
  state.misc_output.vert_sync_pol = 1;

  state.attribute_ctrl.mode_ctrl.enable_line_graphics = 1;

  state.line_offset = 80;
  state.line_compare = 1023;
  state.vertical_display_end = 399;

  state.attribute_ctrl.video_enabled = 1;
  state.attribute_ctrl.color_plane_enable = 0x0f;

  state.pel.dac_state = 0x01;
  state.pel.mask = 0xff;

  state.graphics_ctrl.memory_mapping = 2; // monochrome text mode

  state.sequencer.reset1 = 1;
  state.sequencer.reset2 = 1;
  state.sequencer.extended_mem = 1; // display mem greater than 64K
  state.sequencer.odd_even = 1;     // use sequential addressing mode

  state.memsize = 0x40000;
  state.memory = new u8[state.memsize];
  memset(state.memory, 0, state.memsize);

  state.last_bpp = 8;

  state.CRTC.reg[0x09] = 16;
  state.graphics_ctrl.memory_mapping = 3; // color text mode
  state.vga_mem_updated = 1;

  printf("%s: $Id: Cirrus.cpp,v 1.23 2008/05/31 15:47:09 iamcamiel Exp $\n",
         devid_string);
}

/**
 * Create and start thread.
 **/
void CCirrus::start_threads() {
  if (!myThread) {
    printf(" cirrus");
    StopThread = false;
    myThread = std::make_unique<std::thread>([this](){ this->run(); });
  }
}

/**
 * Stop and destroy thread.
 **/
void CCirrus::stop_threads() {
  // Signal the thread to stop
  StopThread = true;
  if (myThread) {
    printf(" cirrus");
    // Wait for the thread to end execution
    myThread->join();
    // And delete the Thread object
    myThread = nullptr;
  }
}

/**
 * Destructor.
 **/
CCirrus::~CCirrus() { stop_threads(); }

/**
 * Read from one of the Legacy (fixed-address) memory ranges.
 **/
u32 CCirrus::ReadMem_Legacy(int index, u32 address, int dsize) {
  u32 data = 0;
  switch (index) {
  // IO Port 0x3b4
  case 1:
    data = io_read(address + 0x3b4, dsize);
    break;

  // IO Port 0x3c0..0x3cf
  case 2:
    data = io_read(address + 0x3c0, dsize);
    break;

  // IO Port 0x3ba
  case 3:
    data = io_read(address + 0x3ba, dsize);
    break;

  // VGA Memory
  case 4:
    data = legacy_read(address, dsize);
    break;

  // ROM
  case 5:
    data = rom_read(address, dsize);
    break;

  // IO Port 0x3d4
  case 8:
    data = io_read(address + 0x3d4, dsize);
    break;

  // IO Port 0x3da
  case 9:
    data = io_read(address + 0x3da, dsize);
    break;
  }

  return data;
}

/**
 * Write to one of the Legacy (fixed-address) memory ranges.
 **/
void CCirrus::WriteMem_Legacy(int index, u32 address, int dsize, u32 data) {
  switch (index) {
  // IO Port 0x3b4
  case 1:
    io_write(address + 0x3b4, dsize, data);
    return;

  // IO Port 0x3c0..0x3cf
  case 2:
    io_write(address + 0x3c0, dsize, data);
    return;

  // IO Port 0x3ba
  case 3:
    io_write(address + 0x3ba, dsize, data);
    return;

  // VGA Memory
  case 4:
    legacy_write(address, dsize, data);
    return;

  // BIOS Message IO Port (0x500)
  case 7:
    bios_message[bios_message_size++] = (char)data & 0xff;
    if (((data & 0xff) == 0x0a) || ((data & 0xff) == 0x0d)) {
      if (bios_message_size > 1) {
        bios_message[bios_message_size - 1] = '\0';
        printf("cirrus: %s\n", bios_message);
      }

      bios_message_size = 0;
    }

    return;

  // IO Port 0x3d4
  case 8: /* io port */
    io_write(address + 0x3d4, dsize, data);
    return;

  // IO Port 0x3da
  case 9:
    io_write(address + 0x3da, dsize, data);
    return;
  }
}

/**
 * Read from one of the PCI BAR (configurable address) memory ranges.
 **/
u32 CCirrus::ReadMem_Bar(int func, int bar, u32 address, int dsize) {
  switch (bar) {
  // PCI memory range
  case 0:
    return mem_read(address, dsize);
  }

  return 0;
}

/**
 * Write to one of the PCI BAR (configurable address) memory ranges.
 **/
void CCirrus::WriteMem_Bar(int func, int bar, u32 address, int dsize,
                           u32 data) {
  switch (bar) {
  // PCI Memory range
  case 0:
    mem_write(address, dsize, data);
    return;
  }
}

/**
 * Check if threads are still running.
 **/
void CCirrus::check_state() {
  if (myThreadDead.load())
    FAILURE(Thread, "Cirrus thread has died");
}

static u32 cirrus_magic1 = 0xC1AA4500;
static u32 cirrus_magic2 = 0x0054AA1C;

/**
 * Save state to a Virtual Machine State file.
 **/
int CCirrus::SaveState(FILE *f) {
  long ss = sizeof(state);
  int res;

  if ((res = CPCIDevice::SaveState(f)))
    return res;

  fwrite(&cirrus_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&cirrus_magic2, sizeof(u32), 1, f);
  printf("%s: %d bytes saved.\n", devid_string, (int)ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CCirrus::RestoreState(FILE *f) {
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

  if (m1 != cirrus_magic1) {
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

  if (m2 != cirrus_magic2) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  printf("%s: %d bytes restored.\n", devid_string, (int)ss);
  return 0;
}

/**
 * Read from Framebuffer.
 *
 * Not functional.
 **/
u32 CCirrus::mem_read(u32 address, int dsize) {
  u32 data = 0;

  // printf("cirrus: mem read: %" PRIx64 ", %d, %" PRIx64 "   \n", address, dsize,
  // data);
  return data;
}

/**
 * Write to Framebuffer.
 *
 * Not functional.
 **/
void CCirrus::mem_write(u32 address, int dsize, u32 data) {

  // printf("cirrus: mem write: %" PRIx64 ", %d, %" PRIx64 "   \n", address, dsize,
  // data);
  switch (dsize) {
  case 8:
  case 16:
  case 32:
    break;
  }
}

/**
 * Read from Legacy VGA Memory
 *
 * Calls vga_mem_read to read the data 1 byte at a time.
 **/
u32 CCirrus::legacy_read(u32 address, int dsize) {
  u32 data = 0;
  switch (dsize) {
  case 32:
    data |= (u64)vga_mem_read((u32)address + 0xA0003) << 24;
    data |= (u64)vga_mem_read((u32)address + 0xA0002) << 16;

  case 16:
    data |= (u64)vga_mem_read((u32)address + 0xA0001) << 8;

  case 8:
    data |= (u64)vga_mem_read((u32)address + 0xA0000);
  }

  //  //printf("cirrus: legacy read: %" PRIx64 ", %d, %" PRIx64 "   \n", address,
  //  dsize, data);
  return data;
}

/**
 * Write to Legacy VGA Memory
 *
 * Calls vga_mem_write to write the data 1 byte at a time.
 **/
void CCirrus::legacy_write(u32 address, int dsize, u32 data) {

  //  //printf("cirrus: legacy write: %" PRIx64 ", %d, %" PRIx64 "   \n", address,
  //  dsize, data);
  switch (dsize) {
  case 32:
    vga_mem_write((u32)address + 0xA0002, (u8)(data >> 16));
    vga_mem_write((u32)address + 0xA0003, (u8)(data >> 24));

  case 16:
    vga_mem_write((u32)address + 0xA0001, (u8)(data >> 8));

  case 8:
    vga_mem_write((u32)address + 0xA0000, (u8)(data));
  }
}

/**
 * Read from Option ROM
 */
u32 CCirrus::rom_read(u32 address, int dsize) {
  u32 data = 0x00;
  u8 *x = (u8 *)option_rom;
  if (address <= rom_max) {
    x += address;
    switch (dsize) {
    case 8:
      data = (u32)endian_8((*((u8 *)x)) & 0xff);
      break;
    case 16:
      data = (u32)endian_16((*((u16 *)x)) & 0xffff);
      break;
    case 32:
      data = (u32)endian_32((*((u32 *)x)) & 0xffffffff);
      break;
    }

    // printf("cirrus: rom read: %" PRIx64 ", %d, %" PRIx64 "\n", address,
    // dsize,data);
  } else {
    printf("cirrus: (BAD) rom read: %" PRIu32 "x, %d, %" PRIu32 "x\n", address,
           dsize, data);
  }

  return data;
}

/**
 * Read from I/O Port
 */
u32 CCirrus::io_read(u32 address, int dsize) {
  u32 data = 0;
  if (dsize != 8)
    FAILURE(InvalidArgument, "Unsupported dsize!\n");

  switch (address) {
  case 0x3c0:
    data = read_b_3c0();
    break;

  case 0x3c1:
    data = read_b_3c1();
    break;

  case 0x3c2:
    data = read_b_3c2();
    break;

  case 0x3c3:
    data = read_b_3c3();
    break;

  case 0x3c4:
    data = read_b_3c4();
    break;

  case 0x3c5:
    data = read_b_3c5();
    break;

  case 0x3c9:
    data = read_b_3c9();
    break;

  case 0x3ca:
    data = read_b_3ca();
    break;

  case 0x3cc:
    data = read_b_3cc();
    break;

  case 0x3cf:
    data = read_b_3cf();
    break;

  case 0x3b4:
  case 0x3d4:
    data = read_b_3d4();
    break;

  case 0x3b5:
  case 0x3d5:
    data = read_b_3d5();
    break;

  case 0x3ba:
  case 0x3da:
    data = read_b_3da();
    break;

  default:
    FAILURE_1(NotImplemented, "Unhandled port %x read", address);
  }

  // printf("cirrus: io read: %" PRIx64 ", %d, %" PRIx64 "   \n", address+VGA_BASE,
  // dsize, data);
  return data;
}

/**
 * Write to I/O Port
 *
 * Calls io_write_b to write the data 1 byte at a time.
 */
void CCirrus::io_write(u32 address, int dsize, u32 data) {

  //  printf("cirrus: io write: %" PRIx64 ", %d, %" PRIx64 "   \n", address+VGA_BASE,
  //  dsize, data);
  switch (dsize) {
  case 8:
    io_write_b(address, (u8)data);
    break;

  case 16:
    io_write_b(address, (u8)data);
    io_write_b(address + 1, (u8)(data >> 8));
    break;

  default:
    FAILURE(InvalidArgument, "Weird IO size!");
  }
}

/**
 * Write one byte to a VGA I/O port.
 **/
void CCirrus::io_write_b(u32 address, u8 data) {
  switch (address) {
  case 0x3c0:
    write_b_3c0(data);
    break;

  case 0x3c2:
    write_b_3c2(data);
    break;

  case 0x3c4:
    write_b_3c4(data);
    break;

  case 0x3c5:
    write_b_3c5(data);
    break;

  case 0x3c6:
    write_b_3c6(data);
    break;

  case 0x3c7:
    write_b_3c7(data);
    break;

  case 0x3c8:
    write_b_3c8(data);
    break;

  case 0x3c9:
    write_b_3c9(data);
    break;

  case 0x3ce:
    write_b_3ce(data);
    break;

  case 0x3cf:
    write_b_3cf(data);
    break;

  case 0x3b4:
  case 0x3d4:
    write_b_3d4(data);
    break;

  case 0x3b5:
  case 0x3d5:
    write_b_3d5(data);
    break;

  default:
    FAILURE_1(NotImplemented, "Unhandled port %x write", address);
  }
}

/**
 * Write to the attribute controller I/O port (0x3c0)
 *
 * The attribute controller registers are used to select the 16 color
 * and 64 color palettes used for EGA/CGA compatibility.
 *
 * The attribute registers are accessed in an indexed fashion.
 * The address register is read and written via port 3C0h.
 * The data register is written to port 3C0h and read from port 3C1h.
 * The index and the data are written to the same port, one after
 * another. A flip-flop inside the card keeps track of whether the
 * next write will be handled is an index or data. Because there is
 * no standard method of determining the state of this flip-flop, the
 * ability to reset the flip-flop such that the next write will be
 * handled as an index is provided. This is accomplished by reading
 * the Input Status #1 Register (normally port 3DAh) (the data
 * received is not important.)
 *
 * Attribute registers:
 *   - Palette Index registers (index 0x00 - 0x0f)
 *   - Attribute Mode Control register (index 0x10)
 *   - Overscan Color register (index 0x11)
 *   - Color Plane Enable register (index 0x12)
 *   - Horizontal Pixel Panning register (index 0x13)
 *   - Color Select register (index 0x14)
 *   .
 *
 * \code
 * Attribute Address Register(3C0h)
 * +---+-+---------+
 * |   |5|4 3 2 1 0|
 * +---+-+---------+
 *      ^     ^
 *      |     +-- 0..4: Attribute Address: This field specifies the index
 *      |               value of the attribute register to be read or written
 *      +----------- 5: Palette Address Source: This bit is set to 0 to load
 *                      color values to the registers in the internal palette.
 *                      It is set to 1 for normal operation of the attribute
 *                      controller.
 *
 * Palette Index Registers (index 0x00 - 0x0f)
 * +---+-----------+
 * |   |5 4 3 2 1 0|
 * +---+-----------+
 *           ^
 *           +--- 0..5: Internal Palette Index: These 6-bit registers allow a
 *                      dynamic mapping between the text attribute or graphic
 *                      color input value and the display color on the CRT
 *                      screen. These internal palette values are sent off-chip
 *                      to the video DAC, where they serve as addresses into
 *                      the DAC registers.
 *
 * Attribute Mode Control Register (index 0x10)
 * +-+-+-+-+-+-+-+-+
 * |7|6|5| |3|2|1|0|
 * +-+-+-+-+-+-+-+-+
 *  ^ ^ ^   ^ ^ ^ ^
 *  | | |   | | | +- 0: ATGE - Attribute Controller Graphics Enable:
 *  | | |   | | |         0: Disables the graphics mode of operation.
 *  | | |   | | |         1: Selects the graphics mode of operation.
 *  | | |   | | +--- 1: MONO - Monochrome Emulation: This bit is present and
 *  | | |   | |         programmable in all of the hardware but it apparently
 *  | | |   | |         does nothing.
 *  | | |   | +----- 2: LGE - Line Graphics Enable: This field is used in 9
 *  | | |   |           bit wide character modes to provide continuity for the
 *  | | |   |           horizontal line characters in the range C0h-DFh:
 *  | | |   |             0: the 9th column is replicated from the 8th column.
 *  | | |   |             1: the 9th column is set to the background.
 *  | | |   +------- 3: BLINK - Blink Enable:
 *  | | |                 0: Bit 7 of the attribute selects the background
 *  | | |                    intensity (allows 16 colors for background).
 *  | | |                 1: Bit 7 of the attribute enables blinking.
 *  | | +----------- 5: PPM -- Pixel Panning Mode: Allows the upper half of
 *  | |                 the screen to pan independently of the lower screen.
 *  | |                   0: nothing special occurs during a successful line
 *  | |                      compare (see the Line Compare field.)
 *  | |                   1: upon a successful line compare, the bottom portion
 *  | |                      of the screen is displayed as if the Pixel Shift
 *  | |                      Count and Byte Panning fields are set to 0.
 *  | +------------- 6: 8BIT -- 8-bit Color Enable:
 *  |                     1: The video data is sampled so that eight bits are
 *  |                        available to select a color in the 256-color mode.
 *  |                     0: All other modes.
 *  +--------------- 7: P54S -- Palette Bits 5-4 Select: Selects the source for
 *                      the P5 and P4 video bits that act as inputs to the video
 *                      DAC.
 *                        0: P5 and P4 are the outputs of the Internal Palette
 *                           registers.
 *                        1: P5 and P4 are bits 1 and 0 of the Color Select
 *                           register.
 *
 * Overscan Color Register (index 0x11)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 *         +----- 0..7: Overscan Palette Index: Selects a color from one of the
 *                      DAC registers for the border.
 *
 * Color Plane Enable Register (index 0x12)
 * +-------+-------+
 * |       |3 2 1 0|
 * +-------+-------+
 *             ^
 *             +- 0..3: Color Plane Enable: Setting a bit to 1 enables the
 *                      corresponding display-memory color plane.
 *
 * Horizontal Pixel Panning Register (index 0x13)
 * +-------+-------+
 * |       |3 2 1 0|
 * +-------+-------+
 *             ^
 *             +- 0..3: Pixel Shift Count: These bits select the number of pels
 *                      that the video data is shifted to the left.
 *
 * Color Select Register (index 0x14)
 * +-------+---+---+
 * |       |3 2|1 0|
 * +-------+---+---+
 *           ^   ^
 *           |   +- 0..1: Color Select 5-4: These bits can be used in place of
 *           |            the P4 and P5 bits from the Internal Palette registers
 *           |            to form the  8-bit digital color value to the video
 *DAC. |            Selecting these bits is done in the Attribute Mode | Control
 *register (index 0x10).
 *           +----- 2..3: Color Select 7-6: In modes other than mode 0x13
 *                        (256-color VGA), these are the two most-significant
 *bits of the 8-bit digital color value to the video DAC. \endcode
 **/
void CCirrus::write_b_3c0(u8 value) {
  // Variables to save old state (to detect transitions)
  bool prev_video_enabled;
  bool prev_line_graphics;
  bool prev_int_pal_size;

  /* The flip-flop determines whether the write goes to the index-register
     (address) or the data-register. */
  if (state.attribute_ctrl.flip_flop == 0) {
    // Write goes to the index-register.

    /* The index register also has a bit that controls whether video
       output is enabled or not.
       We check this bit, and compare it to it's previous state, to
       determine whether we need to perform an enable or disable
       transition. */
    prev_video_enabled = state.attribute_ctrl.video_enabled;
    state.attribute_ctrl.video_enabled = (value >> 5) & 0x01;
#if defined(DEBUG_VGA)
    printf("io write 3c0: video_enabled = %u   \n",
           (unsigned)state.attribute_ctrl.video_enabled);
#endif
    if (state.attribute_ctrl.video_enabled == 0) {
      if (prev_video_enabled) {
#if defined(DEBUG_VGA)
        printf("found disable transition   \n");
#endif
        // Video output has been disabled. Clear the screen.
        bx_gui->lock();
        bx_gui->clear_screen();
        bx_gui->unlock();
      }
    } else if (!prev_video_enabled) {
#if defined(DEBUG_VGA)
      printf("found enable transition   \n");
#endif
      // Video output has been enabled. Draw the screen.
      redraw_area(0, 0, old_iWidth, old_iHeight);
    }

    // Determine what register should be addressed.
    value &= 0x1f; /* address = bits 0..4 */
    state.attribute_ctrl.address = value;

    /* Registers 0x00..0x0f are palette selection registers.
       Write a debugging message for all other registers. */
#if defined(DEBUG_VGA)
    if (value > 0x0f)
      printf("io write 3c0: address mode reg=%u   \n", (unsigned)value);
#endif
  } else {
    // Write should go to the data-register.

    // Registers 0x00..0x0f are palette selection registers.
    if (state.attribute_ctrl.address <= 0x0f) {
      // Update palette selection only of there is a change.
      if (value !=
          state.attribute_ctrl.palette_reg[state.attribute_ctrl.address]) {
        // Update the palette selection.
        state.attribute_ctrl.palette_reg[state.attribute_ctrl.address] = value;
        // Requires redrawing the screen.
        redraw_area(0, 0, old_iWidth, old_iHeight);
      }
    } else {
      switch (state.attribute_ctrl.address) {
      // Mode control register
      case 0x10:
        prev_line_graphics =
            state.attribute_ctrl.mode_ctrl.enable_line_graphics;
        prev_int_pal_size =
            state.attribute_ctrl.mode_ctrl.internal_palette_size;
        state.attribute_ctrl.mode_ctrl.graphics_alpha = (value >> 0) & 0x01;
        state.attribute_ctrl.mode_ctrl.display_type = (value >> 1) & 0x01;
        state.attribute_ctrl.mode_ctrl.enable_line_graphics =
            (value >> 2) & 0x01;
        state.attribute_ctrl.mode_ctrl.blink_intensity = (value >> 3) & 0x01;
        state.attribute_ctrl.mode_ctrl.pixel_panning_compat =
            (value >> 5) & 0x01;
        state.attribute_ctrl.mode_ctrl.pixel_clock_select = (value >> 6) & 0x01;
        state.attribute_ctrl.mode_ctrl.internal_palette_size =
            (value >> 7) & 0x01;
        if (((value >> 2) & 0x01) != prev_line_graphics) {
          bx_gui->lock();
          bx_gui->set_text_charmap(
              &state.memory[0x20000 + state.charmap_address]);
          bx_gui->unlock();
          state.vga_mem_updated = 1;
        }

        if (((value >> 7) & 0x01) != prev_int_pal_size) {
          redraw_area(0, 0, old_iWidth, old_iHeight);
        }

#if defined(DEBUG_VGA)
        printf("io write 3c0: mode control: %02x h   \n", (unsigned)value);
#endif
        break;

      // Overscan Color Register
      case 0x11:
        /* We don't do anything with this. Our display doesn't
           show the overscan part of the normal monitor. */
        state.attribute_ctrl.overscan_color = (value & 0x3f);
#if defined(DEBUG_VGA)
        printf("io write 3c0: overscan color = %02x   \n", (unsigned)value);
#endif
        break;

      // Color Plane Enable Register
      case 0x12:
        state.attribute_ctrl.color_plane_enable = (value & 0x0f);
        redraw_area(0, 0, old_iWidth, old_iHeight);
#if defined(DEBUG_VGA)
        printf("io write 3c0: color plane enable = %02x   \n", (unsigned)value);
#endif
        break;

      // Horizontal Pixel Panning Register
      case 0x13:
        state.attribute_ctrl.horiz_pel_panning = (value & 0x0f);
        redraw_area(0, 0, old_iWidth, old_iHeight);
#if defined(DEBUG_VGA)
        printf("io write 3c0: horiz pel panning = %02x   \n", (unsigned)value);
#endif
        break;

      // Color Select Register
      case 0x14:
        state.attribute_ctrl.color_select = (value & 0x0f);
        redraw_area(0, 0, old_iWidth, old_iHeight);
#if defined(DEBUG_VGA)
        printf("io write 3c0: color select = %02x   \n",
               (unsigned)state.attribute_ctrl.color_select);
#endif
        break;

      default:
        FAILURE_1(NotImplemented, "io write 3c0: data-write mode %02x h",
                  (unsigned)state.attribute_ctrl.address);
      }
    }
  }

  // Flip the flip-flop
  state.attribute_ctrl.flip_flop = !state.attribute_ctrl.flip_flop;
}

/**
 * Write to the VGA Miscellaneous Output Register (0x3c2)
 *
 * \code
 * +-+-+-+-+---+-+-+
 * |7|6|5| |3 2|1|0|
 * +-+-+-+-+---+-+-+
 *  ^ ^ ^    ^  ^ ^
 *  | | |    |  | +- 0: I/OAS -- Input/Output Address Select: Selects the CRT
 *  | | |    |  |       controller addresses.
 *  | | |    |  |         0: Compatibility with monochrome adapter
 *  | | |    |  |            (0x3b4,0x3b5,0x03ba)
 *  | | |    |  |         1: Compatibility with color graphics adapter (CGA)
 *  | | |    |  |            (0x3d4,0x3d5,0x03da)
 *  | | |    |  +--- 1: RAM Enable: Controls access from the system:
 *  | | |    |            0: Disables access to the display buffer
 *  | | |    |            1: Enables access to the display buffer
 *  | | |    +--- 2..3: Clock Select: Controls the selection of the dot clocks
 *  | | |               used in driving the display timing:
 *  | | |                 00: Select 25 Mhz clock (320/640 pixel wide modes)
 *  | | |                 01: Select 28 Mhz clock (360/720 pixel wide modes)
 *  | | |                 10: Undefined (possible external clock)
 *  | | |                 11: Undefined (possible external clock)
 *  | | +----------- 5: Odd/Even Page Select: Selects the upper/lower 64K page
 *  | |                 of memory when the system is in an even/odd mode.
 *  | |                   0: Selects the low page.
 *  | |                   1: Selects the high page.
 *  | +------------- 6: Horizontal Sync Polarity
 *  |                     0: Positive sync pulse.
 *  |                     1: Negative sync pulse.
 *  +--------------- 7: Vertical Sync Polarity
 *                        0: Positive sync pulse.
 *                        1: Negative sync pulse.
 * \endcode
 **/
void CCirrus::write_b_3c2(u8 value) {
  state.misc_output.color_emulation = (value >> 0) & 0x01;
  state.misc_output.enable_ram = (value >> 1) & 0x01;
  state.misc_output.clock_select = (value >> 2) & 0x03;
  state.misc_output.select_high_bank = (value >> 5) & 0x01;
  state.misc_output.horiz_sync_pol = (value >> 6) & 0x01;
  state.misc_output.vert_sync_pol = (value >> 7) & 0x01;
#if defined(DEBUG_VGA)
  printf("io write 3c2:   \n");
  printf("  color_emulation = %u   \n",
         (unsigned)state.misc_output.color_emulation);
  printf("  enable_ram = %u   \n", (unsigned)state.misc_output.enable_ram);
  printf("  clock_select = %u   \n", (unsigned)state.misc_output.clock_select);
  printf("  select_high_bank = %u   \n",
         (unsigned)state.misc_output.select_high_bank);
  printf("  horiz_sync_pol = %u   \n",
         (unsigned)state.misc_output.horiz_sync_pol);
  printf("  vert_sync_pol = %u   \n",
         (unsigned)state.misc_output.vert_sync_pol);
#endif
}

/**
 * Write to the VGA sequencer index register (0x3c4)
 *
 * The Sequencer registers control how video data is sent to the DAC.
 *
 * The Sequencer registers are accessed in an indexed fashion. By writing a byte
 * to the Sequencer Index Register (0x3c4) equal to the index of the particular
 * sub-register you wish to access, one can address the data pointed to by that
 * index by reading and writing the Sequencer Data Register (0x3c5).
 *
 * Sequencer registers:
 *   - Reset register (index 0x00)
 *   - Clocking Mode register (index 0x01)
 *   - Map Mask register (index 0x02)
 *   - Character Map Select register (index 0x03)
 *   - Memory Mode register (index 0x04)
 *   .
 *
 * \code
 * Reset register (index 0x00)
 * +-----------+-+-+
 * |           |1|0|
 * +-----------+-+-+
 *              ^ ^
 *              | +- 0: Asynchronous Reset:
 *              |         0: Commands the sequencer to asynchronously clear and
 *              |            halt. Resetting the sequencer with this bit can
 *              |            cause loss of video data.
 *              |         1: Allows the sequencer to function normally.
 *              +--- 1: Sychnronous Reset:
 *                        0: Commands the sequencer to synchronously clear and
 *                           halt.
 *                        1: Allows the sequencer to function normally.
 * Bits 1 and 0 must be 1 to allow the sequencer to operate.
 * To prevent the loss of data, bit 1 must be set to 0 during the active display
 * interval before changing the clock selection. The clock is changed through
 *the Clocking Mode register or the Miscellaneous Output register.
 *
 * Clocking Mode register (index 0x01)
 * +---+-+-+-+-+-+-+
 * |   |5|4|3|2| |0|
 * +---+-+-+-+-+-+-+
 *      ^ ^ ^ ^   ^
 *      | | | |   +- 0: 9/8 Dot Mode: Selects whether a character is 8 or 9 dots
 *      | | | |         wide. This can be used to select between 720 and 640
 *      | | | |         pixel modes (or 360 and 320) and also is used to provide
 *      | | | |         9 bit wide character fonts in text mode:
 *      | | | |           0: Selects 9 dots per character.
 *      | | | |           1: Selects 8 dots per character.
 *      | | | +----- 2: Shift/Load Rate:
 *      | | |             0: Video serializers are loaded every character clock.
 *      | | |             1: Video serializers are loaded every other character
 *      | | |                clock, which is useful when 16 bits are fetched per
 *      | | |                cycle and chained together in the shift registers.
 *      | | +------- 3: Dot Clock Rate:
 *      | |               0: Selects the normal dot clocks derived from the
 *      | |                  sequencer master clock input.
 *      | |               1: The master clock will be divided by 2 to generate
 *      | |                  the dot clock. All other timings are affected
 *      | |                  because they are derived from the dot clock. The
 *dot | |                  clock divided by 2 is used for 320 and 360 horizontal
 *      | |                  PEL modes.
 *      | +--------- 4: Shift Four Enable:
 *      |                 0: Video serializers are loaded every character clock.
 *      |                 1: Video serializers are loaded every fourth character
 *      |                    clock, which is useful when 32 bits are fetched per
 *      |                    cycle and chained together in the shift registers.
 *      +----------- 5: Screen Disable:
 *                        0: Display enabled.
 *                        1: Display blanked. Maximum memory bandwidth assigned
 *to the system.
 *
 * Map Mask register (index 0x02)
 * +-------+-------+
 * |       |3 2 1 0|
 * +-------+-------+
 *             ^
 *             +- 0..3: Memory Plane Write Enable: If a bit is set, then write
 *                      operations will modify the respective plane of display
 *                      memory. If a bit is not set then write operations will
 *not affect the respective plane of display memory.
 *
 * Character Map Select register (index 0x03)
 * +---+-+-+---+---+
 * |   |5|4|3 2|1 0|
 * +---+-+-+---+---+
 *      ^ ^  ^   ^
 *      | +--|---+- 0..1,4: Character Set B Select: This field is used to select
 *the |    |              font that is used in text mode when bit 3 of the
 *attribute |    |              byte for a character is set to 0. (*)
 *      +----+----- 2..3,5: Character Set A Select: This field is used to select
 *the font that is used in text mode when bit 3 of the attribute byte for a
 *character is set to 1. (*)
 *
 * (*) Note that this field is not contiguous in order to provide EGA
 *compatibility. The font selected resides in plane 2 of display memory at the
 *address specified by this field, as follows:
 * +------+---------------+
 * |  val | font at       |
 * +------+---------------+
 * | 000b | 0000h - 1FFFh |
 * | 001b | 4000h - 5FFFh |
 * | 010b | 8000h - 9FFFh |
 * | 011b | C000h - DFFFh |
 * | 100b | 2000h - 3FFFh |
 * | 101b | 6000h - 7FFFh |
 * | 110b | A000h - BFFFh |
 * | 111b | E000h - FFFFh |
 * +------+---------------+
 *
 * Memory Mode register (index 0x04)
 * +-------+-+-+-+-+
 * |       |3|2|1| |
 * +-------+-+-+-+-+
 *          ^ ^ ^
 *          | | +--- 1: Extended Memory:
 *          | |           0: 64 KB of video memory enabled
 *          | |           1: 256 KB of video memory enabled. This bit must be
 *set to 1 to | |              enable the character map selection described for
 *the | |              previous register. | +----- 2: Odd/Even Host Memory Write
 *Adressing Disable: |             0: Even system addresses access maps 0 and 2,
 *while odd system |                addresses access maps 1 and 3. | 1: System
 *addresses sequentially access data within a bit map, |                and the
 *maps are accessed according to the value in the Map |                Mask
 *register (index 0x02).
 *          +------- 3: Chain 4 Enable: This bit controls the map selected
 *during system read operations. 0: Enables system addresses to sequentially
 *access data within a bit map by using the Map Mask register. 1: Causes the two
 *low-order bits to select the map accessed as shown below:
 *                           +----+----+--------------+
 *                           | A0 | A1 | Map Selected |
 *                           +----+----+--------------+
 *                           |  0 |  0 | 0            |
 *                           |  0 |  1 | 1            |
 *                           |  1 |  0 | 2            |
 *                           |  1 |  1 | 3            |
 *                           +----+----+--------------+
 * \endcode
 **/
void CCirrus::write_b_3c4(u8 value) { state.sequencer.index = value; }

/**
 * Write to the VGA sequencer data register (0x3c5)
 *
 * For a description of the Sequencer registers, see CCirrus::write_b_3c4
 **/
void CCirrus::write_b_3c5(u8 value) {
  unsigned i;
  u8 charmap1;
  u8 charmap2;

  switch (state.sequencer.index) {
  // Sequencer: reset register
  case 0:
#if defined(DEBUG_VGA)
    printf("write 0x3c5: sequencer reset: value=0x%02x   \n", (unsigned)value);
#endif
    if (state.sequencer.reset1 && ((value & 0x01) == 0)) {
      state.sequencer.char_map_select = 0;
      state.charmap_address = 0;
      bx_gui->lock();
      bx_gui->set_text_charmap(&state.memory[0x20000 + state.charmap_address]);
      bx_gui->unlock();
      state.vga_mem_updated = 1;
    }

    state.sequencer.reset1 = (value >> 0) & 0x01;
    state.sequencer.reset2 = (value >> 1) & 0x01;
    break;

  // Sequencer: clocking mode register
  case 1:
#if defined(DEBUG_VGA)
    printf("io write 3c5=%02x: clocking mode reg: ignoring   \n",
           (unsigned)value);
#endif
    state.sequencer.reg1 = value & 0x3f;
    state.x_dotclockdiv2 = ((value & 0x08) > 0);
    break;

  // Sequencer: map mask register
  case 2:
    state.sequencer.map_mask = (value & 0x0f);
    for (i = 0; i < 4; i++)
      state.sequencer.map_mask_bit[i] = (value >> i) & 0x01;
    break;

  // Sequencer: character map select register
  case 3:
    state.sequencer.char_map_select = value;
    charmap1 = value & 0x13;
    if (charmap1 > 3)
      charmap1 = (charmap1 & 3) + 4;
    charmap2 = (value & 0x2C) >> 2;
    if (charmap2 > 3)
      charmap2 = (charmap2 & 3) + 4;
    if (state.CRTC.reg[0x09] > 0) {
      state.charmap_address = (charmap1 << 13);
      bx_gui->lock();
      bx_gui->set_text_charmap(&state.memory[0x20000 + state.charmap_address]);
      bx_gui->unlock();
      state.vga_mem_updated = 1;
    }

    if (charmap2 != charmap1)
      printf("char map select: #2=%d (unused)   \n", charmap2);
    break;

  // Sequencer: memory mode register
  case 4:
    state.sequencer.extended_mem = (value >> 1) & 0x01;
    state.sequencer.odd_even = (value >> 2) & 0x01;
    state.sequencer.chain_four = (value >> 3) & 0x01;

#if defined(DEBUG_VGA)
    printf("io write 3c5: index 4:   \n");
    printf("  extended_mem %u   \n", (unsigned)state.sequencer.extended_mem);
    printf("  odd_even %u   \n", (unsigned)state.sequencer.odd_even);
    printf("  chain_four %u   \n", (unsigned)state.sequencer.chain_four);
#endif
    break;

  default:
    FAILURE_1(NotImplemented, "io write 3c5: index %u unhandled",
              (unsigned)state.sequencer.index);
  }
}

/**
 * Write to VGA DAC Pixel Mask register (0x3c6)
 *
 * The pixel inputs (R, G and B) are anded with this value. Set to FFh
 * for normal operation.
 **/
void CCirrus::write_b_3c6(u8 value) {
  state.pel.mask = value;
#if defined(DEBUG_VGA)
  if (state.pel.mask != 0xff)
    printf("io write 3c6: PEL mask=0x%02x != 0xFF   \n", value);
#endif

  // state.pel.mask should be and'd with final value before
  // indexing into color register state.pel.data[]
}

/**
 * Write VGA DAC Address Read Mode register (0x3c7)
 *
 * The Color Registers in the standard VGA provide a mapping between the
 * palette of between 2 and 256 colors to a larger 18-bit color space.
 * This capability allows for efficient use of video memory while
 * providing greater flexibility in color choice. The standard VGA has
 * 256 palette entries containing six bits each of red, green, and blue
 * values. The palette RAM is accessed via a pair of address registers
 * and a data register.
 *
 * To write a palette entry, output the palette entry's index value to
 * the DAC Address Write Mode Register (0x3c8) then perform 3 writes to
 * the DAC Data Register (0x3c9), loading the red, green, then blue
 * values into the palette RAM. The internal write address automatically
 * advances allowing the next value's RGB values to be loaded without
 * having to reprogram the DAC Address Write Mode Register. This allows
 * the entire palette to be loaded in one write operation.
 *
 * To read a palette entry, output the palette entry's index to the DAC
 * Address Read Mode Register (0x3c7). Then perform 3 reads from the DAC
 * Data Register (0x3c9), loading the red, green, then blue values from
 * palette RAM. The internal read address automatically advances
 * allowing the next RGB values to be read without having to reprogram
 * the DAC Address Read Mode Register.
 *
 * The data values are 6-bits each.
 **/
void CCirrus::write_b_3c7(u8 value) {
  state.pel.read_data_register = value;
  state.pel.read_data_cycle = 0;
  state.pel.dac_state = 0x03;
}

/**
 * Write VGA DAC Address Write Mode register (0x3c8)
 *
 * For a description of DAC registers see CCirrus::write_b_3c7
 **/
void CCirrus::write_b_3c8(u8 value) {
  state.pel.write_data_register = value;
  state.pel.write_data_cycle = 0;
  state.pel.dac_state = 0x00;
}

/**
 * Write VGA DAC Data register (0x3c9)
 *
 * For a description of DAC registers see CCirrus::write_b_3c7
 **/
void CCirrus::write_b_3c9(u8 value) {
  switch (state.pel.write_data_cycle) {
  case 0:
    state.pel.data[state.pel.write_data_register].red = value;
    break;

  case 1:
    state.pel.data[state.pel.write_data_register].green = value;
    break;

  case 2: {
    state.pel.data[state.pel.write_data_register].blue = value;
    // Palette write complete. Check if value has changed
    bx_gui->lock();
    bool changed = bx_gui->palette_change(
        state.pel.write_data_register,
        state.pel.data[state.pel.write_data_register].red << 2,
        state.pel.data[state.pel.write_data_register].green << 2,
        state.pel.data[state.pel.write_data_register].blue << 2);
    bx_gui->unlock();
    // If palette value has changed, redraw the screen.
    if (changed)
      redraw_area(0, 0, old_iWidth, old_iHeight);
  } break;
  }

  // Move on to next RGB component
  state.pel.write_data_cycle++;

  // palette entry complete, move on to next one
  if (state.pel.write_data_cycle >= 3) {

    // BX_INFO(("state.pel.data[%u] {r=%u, g=%u, b=%u}",
    //  (unsigned) state.pel.write_data_register,
    //  (unsigned) state.pel.data[state.pel.write_data_register].red,
    //  (unsigned) state.pel.data[state.pel.write_data_register].green,
    //  (unsigned) state.pel.data[state.pel.write_data_register].blue);
    state.pel.write_data_cycle = 0;
    state.pel.write_data_register++;
  }
}

/**
 * Write to VGA Graphics Controller Index Register (0x3ce)
 *
 * The Graphics Controller registers control how the system accesses video RAM.
 *
 * The Graphics registers are accessed in an indexed fashion. By writing a byte
 * to the Graphics Index Register (0x3ce) equal to the index of the particular
 * sub-register you wish to access, one can address the data pointed to by that
 * index by reading and writing the Graphics Data Register (0x3cf).
 *
 * Graphics registers:
 *   - Set/Reset register (index 0x00)
 *   - Enable Set/Reset register (index 0x01)
 *   - Color Compare register (index 0x02)
 *   - Data Rotate register (index 0x03)
 *   - Read Map Select register (index 0x04)
 *   - Graphics Mode register (index 0x05)
 *   - Miscellaneous Graphics register (index 0x06)
 *   - Color Don't Care register (index 0x07)
 *   - Bit Mask register (index 0x08)
 *   .
 *
 * \code
 * Set/Reset register (index 0x00)
 * +-------+-------+
 * |       |3 2 1 0|
 * +-------+-------+
 *             ^
 *             +- 0..3: Set/Reset: Bits 3-0 of this field represent planes 3-0
 *of the VGA display memory. This field is used by Write Mode 0 and Write Mode 3
 *(See the Write Mode field.) In Write Mode 0, if the corresponding bit in the
 *Enable Set/Reset field is set, and in Write Mode 3 regardless of the Enable
 *                      Set/Reset field, the value of the bit in this field is
 *                      expanded to 8 bits and substituted for the data of the
 *                      respective plane and passed to the next stage in the
 *                      graphics pipeline, which for Write Mode 0 is the Logical
 *                      Operation unit and for Write Mode 3 is the Bit Mask
 *unit.
 *
 * Enable Set/Reset Register (index 0x01)
 * +-------+-------+
 * |       |3 2 1 0|
 * +-------+-------+
 *             ^
 *             +- 0..3: Enable Set/Reset: Bits 3-0 of this field represent
 *planes 3-0 of the VGA display memory. This field is used in Write Mode 0 (See
 *the Write Mode field) to select whether data for each plane is derived from
 *host data or from expansion of the respective bit in the Set/Reset field.
 *
 * Color Compare Register (index 0x02)
 * +-------+-------+
 * |       |3 2 1 0|
 * +-------+-------+
 *             ^
 *             +- 0..3: Color Compare: Bits 3-0 of this field represent planes
 *3-0 of the VGA display memory. This field holds a reference color that is used
 *by Read Mode 1 (See the Read Mode field.) Read Mode 1 returns the result of
 *the comparison between this value and a location of display memory, modified
 *by the Color Don't Care field.
 *
 * Data Rotate Register (index 0x03)
 * +-----+---+-----+
 * |     |4 3|2 1 0|
 * +-----+---+-----+
 *         ^    ^
 *         |    +- 0..2: Rotate Count:
 *         |             This field is used in Write Mode 0 and Write Mode 3
 *(See |             the Write Mode field.) In these modes, the host data is |
 *rotated to the right by the value specified by the value of |             this
 *field. A rotation operation consists of moving bits |             7-1 right
 *one position to bits 6-0, simultaneously |             wrapping bit 0 around
 *to bit 7, and is repeated the number |             of times specified by this
 *field.
 *         +------ 3..4: Logical Operation:
 *                       This field is used in Write Mode 0 and Write Mode 2
 *(See the Write Mode field.) The logical operation stage of the graphics
 *pipeline is 32 bits wide (1 byte * 4 planes) and performs the operations on
 *its inputs from the previous stage in the graphics pipeline and the latch
 *register. The latch register remains unchanged and the result is passed on to
 *the next stage in the pipeline. The results based on the value of this field
 *are: 00: Result is input from previous stage unmodified. 01: Result is input
 *from previous stage logical ANDed with latch register. 10: Result is input
 *from previous stage logical ORed with latch register. 11: Result is input from
 *previous stage logical XORed with latch register.
 *
 * Read Map Select register (index 0x04)
 * +-----------+---+
 * |           |1 0|
 * +-----------+---+
 *               ^
 *               +- 0..1: Read Map Select: The value of this field is used in
 *Read Mode 0 (see the Read Mode field) to specify the display memory plane to
 *transfer data from. Due to the arrangement of video memory, this field must be
 *modified four times to read one or more pixels values in the planar video
 *modes.
 *
 * Graphics Mode Register (index 0x05)
 * +-+-+-+-+-+-+---+
 * | |6|5|4|3| |1 0|
 * +-+-+-+-+-+-+---+
 *    ^ ^ ^ ^    ^
 *    | | | |    +- 0..1: Write Mode
 *    | | | |             This field selects between four write modes, simply
 *known | | | |             as Write Modes 0-3, based upon the value of this
 *field: | | | |               00: Write Mode 0: In this mode, the host data is
 *first | | | |                   rotated as per the Rotate Count field, then
 *the | | | |                   Enable Set/Reset mechanism selects data from
 *this or | | | |                   the Set/Reset field. Then the selected
 *Logical | | | |                   Operation is performed on the resulting data
 *and the | | | |                   data in the latch register. Then the Bit
 *Mask field | | | |                   is used to select which bits come from
 *the resulting | | | |                   data and which come from the latch
 *register. Finally, | | | |                   only the bit planes enabled by
 *the Memory Plane Write | | | |                   Enable field are written to
 *memory. | | | |               01: Write Mode 1: In this mode, data is
 *transferred directly | | | |                   from the 32 bit latch register
 *to display memory, | | | |                   affected only by the Memory Plane
 *Write Enable field. | | | |                   The host data is not used in
 *this mode. | | | |               10: Write Mode 2: In this mode, the bits 3-0
 *of the host | | | |                   data are replicated across all 8 bits of
 *their | | | |                   respective planes. Then the selected Logical
 *Operation | | | |                   is performed on the resulting data and the
 *data in the | | | |                   latch register. Then the Bit Mask field
 *is used to | | | |                   select which bits come from the resulting
 *data and which | | | |                   come from the latch register.
 *Finally, only the bit | | | |                   planes enabled by the Memory
 *Plane Write Enable field | | | |                   are written to memory. | |
 *| |               11: Write Mode 3: In this mode, the data in the Set/Reset |
 *| | |                   field is used as if the Enable Set/Reset field were
 *set | | | |                   to 1111b. Then the host data is first rotated as
 *per the | | | |                   Rotate Count field, then logical ANDed with
 *the value of | | | |                   the Bit Mask field. The resulting value
 *is used on the | | | |                   data obtained from the Set/Reset
 *field in the same way | | | |                   that the Bit Mask field would
 *ordinarily be used. to | | | |                   select which bits come from
 *the expansion of the | | | |                   Set/Reset field and which come
 *from the latch register. | | | |                   Finally, only the bit
 *planes enabled by the Memory Plane | | | |                   Write Enable
 *field are written to memory. | | | +--------- 3: Read Mode: | | | This field
 *selects between two read modes, simply known as Read | | |               Mode
 *0, and Read Mode 1, based upon the value of this field: | | | 0: Read Mode 0:
 *In this mode, a byte from one of the four | | |                    planes is
 *returned on read operations. The plane from | | |                    which the
 *data is returned is determined by the value of | | |                    the
 *Read Map Select field. | | |                 1: Read Mode 1: In this mode, a
 *comparison is made between | | |                    display memory and a
 *reference color defined by the Color | | |                    Compare field.
 *Bit planes not set in the Color Don't Care | | |                    field then
 *the corresponding color plane is not considered | | |                    in
 *the comparison. Each bit in the returned result | | | represents one
 *comparison between the reference color, with | | |                    the bit
 *being set if the comparison is true. | | +----------- 4: Host Odd/Even Memory
 *Read Addressing Enable: | |                   0: Selects the standard
 *addressing mode. | |                   1: Selects the odd/even addressing mode
 *used by the IBM CGA | |                      Adapter. | | Normally, the value
 *here follows the value of Memory Mode | |                 register bit 2 in
 *the sequencer." | +------------- 5: Shift Register Interleave Mode: | 1:
 *Directs the shift registers in the graphics controller to | format the serial
 *data stream with even-numbered bits from |                        both maps on
 *even-numbered maps, and odd-numbered bits from |                        both
 *maps on the odd-numbered maps. This bit is used for | modes 4 and 5.
 *    +--------------- 6: 256-Color Shift Mode:
 *                          0: Allows bit 5 to control the loading of the shift
 *registers. 1: Causes the shift registers to be loaded in a manner that
 *                             supports the 256-color mode.
 *
 * Miscellaneous Graphics register (index 0x06)
 * +-------+---+-+-+
 * |       |3 2|1|0|
 * +-------+---+-+-+
 *           ^  ^ ^
 *           |  | +- 0: Alphanumeric Mode Disable:
 *           |  |       This bit controls alphanumeric mode addressing.
 *           |  |         0: Text mode.
 *           |  |         1: Graphics modes, disables character generator
 *latches. |  +--- 1: Chain Odd/Even Enable |            1: Directs the system
 *address bit, A0, to be replaced by a |               higher-order bit. The odd
 *map is then selected when A0 is 1, |               and the even map when A0 is
 *0.
 *           +--- 2..3: Memory Map Select
 *                      This field specifies the range of host memory addresses
 *that is decoded by the VGA hardware and mapped into display memory accesses.
 *The values of this field and their corresponding host memory ranges are: 00:
 *A0000h-BFFFFh (128K region) 01: A0000h-AFFFFh (64K region) 10: B0000h-B7FFFh
 *(32K region) 11: B8000h-BFFFFh (32K region)
 *
 * Color Don't Care register (index 0x07)
 * +-------+-------+
 * |       |3 2 1 0|
 * +-------+-------+
 *             ^
 *             +- 0..3: Color Don't Care: Bits 3-0 of this field represent
 *planes 3-0 of the VGA display memory. This field selects the planes that are
 *used in the comparisons made by Read Mode 1 (See the Read Mode field.) Read
 *Mode 1 returns the result of the comparison between the value of the Color
 *                      Compare field and a location of display memory. If a bit
 *                      in this field is set, then the corresponding display
 *                      plane is considered in the comparison. If it is not set,
 *                      then that plane is ignored for the results of the
 *                      comparison.
 *
 * Bit Mask register (index 0x08)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 *         +----- 0..7: Bit Mask: This field is used in Write Modes 0, 2, and 3
 *                      (See the Write Mode field.) It it is applied to one byte
 *                      of data in all four display planes. If a bit is set,
 *                      then the value of corresponding bit from the previous
 *                      stage in the graphics pipeline is selected; otherwise
 *                      the value of the corresponding bit in the latch register
 *                      is used instead. In Write Mode 3, the incoming data
 *byte, after being rotated is logical ANDed with this byte and the resulting
 *value is used in the same way this field would normally be used by itself.
 * \endcode
 **/
void CCirrus::write_b_3ce(u8 value) {
#if defined(DEBUG_VGA)
  if (value > 0x08) /* ??? */
    printf("io write: 3ce: value > 8   \n");
#endif
  state.graphics_ctrl.index = value;
}

/**
 * Write to VGA Graphics Controller Data Register (0x3cf)
 *
 * For a description of the Graphics registers, see CCirrus::write_b_3ce
 **/
void CCirrus::write_b_3cf(u8 value) {
  u8 prev_memory_mapping;
  bool prev_graphics_alpha;
  bool prev_chain_odd_even;

  /* Graphics Controller Registers 00..08 */
  switch (state.graphics_ctrl.index) {
  case 0: /* Set/Reset */
    state.graphics_ctrl.set_reset = value & 0x0f;
    break;

  case 1: /* Enable Set/Reset */
    state.graphics_ctrl.enable_set_reset = value & 0x0f;
    break;

  case 2: /* Color Compare */
    state.graphics_ctrl.color_compare = value & 0x0f;
    break;

  case 3: /* Data Rotate */
    state.graphics_ctrl.data_rotate = value & 0x07;

    /* ??? is this bits 3..4 or 4..5 */
    state.graphics_ctrl.raster_op = (value >> 3) & 0x03; /* ??? */
    break;

  case 4: /* Read Map Select */
    state.graphics_ctrl.read_map_select = value & 0x03;
#if defined(DEBUG_VGA)
    printf("io write to 03cf = %02x (RMS)   \n", (unsigned)value);
#endif
    break;

  case 5: /* Mode */
    state.graphics_ctrl.write_mode = value & 0x03;
    state.graphics_ctrl.read_mode = (value >> 3) & 0x01;
    state.graphics_ctrl.odd_even = (value >> 4) & 0x01;
    state.graphics_ctrl.shift_reg = (value >> 5) & 0x03;

#if defined(DEBUG_VGA)
    if (state.graphics_ctrl.odd_even)
      printf("io write: 3cf: reg 05: value = %02xh   \n", (unsigned)value);
    if (state.graphics_ctrl.shift_reg)
      printf("io write: 3cf: reg 05: value = %02xh   \n", (unsigned)value);
#endif
    break;

  case 6: /* Miscellaneous */
    prev_graphics_alpha = state.graphics_ctrl.graphics_alpha;
    prev_chain_odd_even = state.graphics_ctrl.chain_odd_even;
    prev_memory_mapping = state.graphics_ctrl.memory_mapping;

    state.graphics_ctrl.graphics_alpha = value & 0x01;
    state.graphics_ctrl.chain_odd_even = (value >> 1) & 0x01;
    state.graphics_ctrl.memory_mapping = (value >> 2) & 0x03;
#if defined(DEBUG_VGA)
    printf("memory_mapping set to %u   \n",
           (unsigned)state.graphics_ctrl.memory_mapping);
    printf("graphics mode set to %u   \n",
           (unsigned)state.graphics_ctrl.graphics_alpha);
    printf("odd_even mode set to %u   \n",
           (unsigned)state.graphics_ctrl.odd_even);
    printf("io write: 3cf: reg 06: value = %02xh   \n", (unsigned)value);
#endif
    if (prev_memory_mapping != state.graphics_ctrl.memory_mapping) {
      redraw_area(0, 0, old_iWidth, old_iHeight);
    }

    if (prev_graphics_alpha != state.graphics_ctrl.graphics_alpha) {
      redraw_area(0, 0, old_iWidth, old_iHeight);
      old_iHeight = 0;
    }
    break;

  case 7: /* Color Don't Care */
    state.graphics_ctrl.color_dont_care = value & 0x0f;
    break;

  case 8: /* Bit Mask */
    state.graphics_ctrl.bitmask = value;
    break;

  default:

    /* ??? */
    FAILURE_1(NotImplemented, "io write: 3cf: index %u unhandled",
              (unsigned)state.graphics_ctrl.index);
  }
}

/**
 * Write to VGA CRTC Index Register (0x3b4 or 0x3d4)
 *
 * The VGA CRTC Registers control how the video is output to the display.
 *
 * The CRTC registers are accessed in an indexed fashion. By writing a byte
 * to the CRTC Index Register (0x3d4) equal to the index of the particular
 * sub-register you wish to access, one can address the data pointed to by that
 * index by reading and writing the CRTC Data Register (0x3d5).
 *
 * CRTC registers:
 *   - Horizontal Total Register (index 0x00)
 *   - End Horizontal Display Register (index 0x01)
 *   - Start Horizontal Blanking Register (index 0x02)
 *   - End Horizontal Blanking Register (index 0x03)
 *   - Start Horizontal Retrace Register (index 0x04)
 *   - End Horizontal Retrace Register (index 0x05)
 *   - Vertical Total Register (index 0x06)
 *   - Overflow Register (index 0x07)
 *   - Preset Row Scan Register (index 0x08)
 *   - Maximum Scan Line Register (index 0x09)
 *   - Cursor Start Register (index 0x0a)
 *   - Cursor End Register (index 0x0b)
 *   - Start Address High Register (index 0x0c)
 *   - Start Address Low Register (index 0x0d)
 *   - Cursor Location High Register (index 0x0e)
 *   - Cursor Location Low Register (index 0x0f)
 *   - Vertical Retrace Start Register (index 0x10)
 *   - Vertical Retrace End Register (index 0x11)
 *   - Vertical Display End Register (index 0x12)
 *   - Offset Register (index 0x13)
 *   - Underline Location Register (index 0x14)
 *   - Start Vertical Blanking Register (index 0x15)
 *   - End Vertical Blanking (index 0x16)
 *   - CRTC Mode Control Register (index 0x17)
 *   - Line Compare Register (index 0x18)
 *   .
 *
 * \code
 * Horizontal Total register (index 0x00)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 * 0..7: Horizontal Total:
 * This field is used to specify the number of character clocks per scan line.
 * This field, along with the dot rate selected, controls the horizontal
 * refresh rate of the VGA by specifying the amount of time one scan line
 * takes.  This field is not programmed with the actual number of character
 * clocks, however. Due to timing factors of the VGA hardware (which, for
 * compatibility purposes has been emulated by VGA compatible chipsets), the
 * actual horizontal total is 5 character clocks more than the value stored in
 * this field, thus one needs to subtract 5 from the actual horizontal total
 * value desired before programming it into this register.
 *
 * End Horizontal Display register (index 0x01)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 * 0..7: End Horizontal Display:
 * This field is used to control the point that the sequencer stops outputting
 * pixel values from display memory, and sequences the pixel value specified by
 * the Overscan Palette Index field for the remainder of the scan line. The
 * overscan begins the character clock after the the value programmed into this
 * field. This register should be programmed with the number of character
 * clocks in the active display - 1. Note that the active display may be
 * affected by the Display Enable Skew field.
 *
 * Start Horizontal Blanking register (index 0x02)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 * 0..7: Start Horizontal Blanking:
 * This field is used to specify the character clock at which the horizontal
 * blanking period begins.  During the horizontal blanking period, the VGA
 * hardware forces the DAC into a blanking state, where all of the intensities
 * output are at minimum value, no matter what color information the attribute
 * controller is sending to the DAC.  This field works in conjunction with the
 * End Horizontal Blanking field to specify the horizontal blanking period.
 * Note that the horizontal blanking can be programmed to appear anywhere within
 * the scan line, as well as being programmed to a value greater than the
 * Horizontal Total field preventing the horizontal blanking from occurring at
 * all.
 *
 * End Horizontal Blanking register (index 0x03)
 * +-+---+---------+
 * |7|6 5|4 3 2 1 0|
 * +-+---+---------+
 *  ^  ^      ^
 *  |  |      +-- 0..4: End Horizontal Blanking:
 *  |  |                Contains bits 4-0 of the End Horizontal Blanking field
 *  |  |                which specifies the end of the horizontal blanking
 *  |  |                period.  Bit 5 is located in bit 7 of the End Horizontal
 *  |  |                Retrace register (index 0x05). After the period has
 *  |  |                begun as specified by the Start Horizontal Blanking
 *  |  |                field, the 6-bit value of this field is compared against
 *  |  |                the lower 6 bits of the character clock. When a match
 *  |  |                occurs, the horizontal blanking signal is disabled. This
 *  |  |                provides from 1 to 64 character clocks although some
 *  |  |                implementations may match in the character clock
 *  |  |                specified by the Start Horizontal Blanking field, in
 *which |  |                case the range is 0 to 63.  Note that if blanking
 *extends |  |                past the end of the scan line, it will end on the
 *first |  |                match of this field on the next scan line. |
 *+--------- 5..6: Display Enable Skew: |                   This field affects
 *the timings of the display enable |                   circuitry in the VGA.
 *The value of this field is the number |                   of character clocks
 *that the display enable "signal" is |                   delayed. In all known
 *VGA cards, this field is always |                   programmed to 0.
 *Programming it to non-zero values results |                   in the overscan
 *being displayed over the number of |                   characters programmed
 *into this field at the beginning of |                   the scan line, as well
 *as the end of the active display |                   being shifted the number
 *of characters programmed into this |                   field. The characters
 *that extend past the normal end of the |                   active display can
 *be garbled in certain circumstances that |                   is dependent on
 *the particular VGA implementation. According |                   to
 *documentation from IBM, "This skew control is needed to | provide sufficient
 *time for the CRT controller to read a |                   character and
 *attribute code from the video buffer, to gain |                   access to
 *the character generator, and go through the |                   Horizontal PEL
 *Panning register in the attribute controller. |                   Each access
 *requires the 'display enable' signal to be |                   skewed one
 *character clock so that the video output is |                   synchronized
 *with the horizontal and vertical retrace |                   signals." as well
 *as "Note: Character skew is not adjustable |                   on the Type 2
 *video and the bits are ignored; however, |                   programs should
 *set these bits for the appropriate skew to |                   maintain
 *compatibility."  This may be required for some early |                   IBM
 *VGA implementations or may be simply an unused "feature" | carried over along
 *with its register description from the IBM |                   EGA
 *implementations that require the use of this field.
 *  +--------------- 7: Enable Vertical Retrace Access:
 *                      This field was used in the IBM EGA to provide access to
 *the light pen input values as the light pen registers were mapped over CRTC
 *indexes 10h-11h. The VGA lacks capability for light pen input, thus this field
 *is normally forced to 1 (although always writing it as 1 might be a good idea
 *for compatibility), which in the EGA would enable access to the vertical
 *retrace fields instead of the light pen fields.
 *
 * Start Horizontal Retrace register (index 0x04)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 * 0..7: Start Horizontal Retrace:
 * This field specifies the character clock at which the VGA begins sending the
 * horizontal synchronization pulse to the display which signals the monitor to
 *retrace back to the left side of the screen. The end of this pulse is
 *controlled by the End Horizontal Retrace field. This pulse may appear anywhere
 *in the scan line, as well as set to a position beyond the Horizontal Total
 *field which effectively disables the horizontal synchronization pulse.
 *
 * End Horizontal Retrace register (index 0x05)
 * +-+---+---------+
 * |7|6 5|4 3 2 1 0|
 * +-+---+---------+
 *  ^  ^      ^
 *  |  |      +-- 0..4: End Horizontal Retrace:
 *  |  |                This field specifies the end of the horizontal retrace
 *period, |  |                which begins at the character clock specified in
 *the Start |  |                Horizontal Retrace field.  The horizontal
 *retrace signal is |  |                enabled until the lower 5 bits of the
 *character counter match |  |                the 5 bits of this field.  This
 *provides for a horizontal |  |                retrace period from 1 to 32
 *character clocks.  Note that some |  |                implementations may
 *match immediately instead of 32 clocks |  |                away, making the
 *effective range 0 to 31 character clocks. |  +--------- 5..6: Horizontal
 *Retrace Skew: |                   This field delays the start of the
 *horizontal retrace period |                   by the number of character
 *clocks equal to the value of this |                   field.  From
 *observation, this field is programmed to 0, with |                   the
 *exception of the 40 column text modes where this field is | set to 1.  The VGA
 *hardware simply acts as if this value is |                   added to the
 *Start Horizontal Retrace field. According to IBM | documentation, "For certain
 *modes, the 'horizontal retrace' |                   signal takes up the entire
 *blanking interval. Some internal |                   timings are generated by
 *the falling edge of the 'horizontal |                   retrace' signal. To
 *ensure that the signals are latched |                   properly, the
 *'retrace' signal is started before the end of |                   the 'display
 *enable' signal and then skewed several character |                   clock
 *times to provide the proper screen centering." This does | not appear to be
 *the case, leading me to believe this is yet |                   another
 *holdout from the IBM EGA implementations that do |                   require
 *the use of this field.
 *  +--------------- 7: End Horizontal Blanking (bit 5):
 *                      This contains bit 5 of the End Horizontal Blanking field
 *in the End Horizontal Blanking register (index 0x03).
 *
 * Vertical Total register (index 0x06)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 * 0..7: Vertical Total
 * This contains the lower 8 bits of the Vertical Total field. Bits 9-8 of this
 *field are located in the Overflow Register (index 0x07). This field determines
 *the number of scanlines in the active display and thus the length of each
 *vertical retrace. This field contains the value of the scanline counter at the
 *beginning of the last scanline in the vertical period.
 *
 * Overflow register (index 0x07)
 * +-+-+-+-+-+-+-+-+
 * |7|6|5|4|3|2|1|0|
 * +-+-+-+-+-+-+-+-+
 *  ^ ^ ^ ^ ^ ^ ^ ^
 *  | | +-|-|-|-|-+- 0,5: Bit 8,9 of Vertical Total (index 0x06)
 *  | +---|-|-|-+--- 1,6: Bit 8,9 of Vertical Display End (index 0x12)
 *  +-----|-|-+----- 2,7: Bit 8,9 of Vertical Retrace Start (index 0x10)
 *        | +--------- 3: Bit 8 of Start Vertical Blanking (index 0x15)
 *        +----------- 4: Bit 8 of Line Compare (index 0x18)
 *
 * Preset Row Scan register (index 0x08)
 * +-+---+---------+
 * | |6 5|4 3 2 1 0|
 * +-+---+---------+
 *     ^      ^
 *     |      +-- 0..4: Preset Row Scan:
 *     |                This field is used when using text mode or any mode with
 *a non-zero |                Maximum Scan Line field (index 0x09) to provide
 *for more precise |                vertical scrolling than the Start Address
 *Register provides. The |                value of this field specifies how many
 *scan lines to scroll the |                display upwards. Valid values range
 *from 0 to the value of the |                Maximum Scan Line field. Invalid
 *values may cause undesired effects |                and seem to be dependent
 *upon the particular VGA implementation.
 *     +--------- 5..6: Byte Panning:
 *                      The value of this field is added to the Start Address
 *Register when calculating the display memory address for the upper left hand
 *pixel or character of the screen. This allows for a maximum shift of 15, 31,
 *or 35 pixels without having to reprogram the Start Address Register.
 *
 * Maximum Scan Line register (index 0x09)
 * +-+-+-+---------+
 * |7|6|5|4 3 2 1 0|
 * +-+-+-+---------+
 *  ^ ^ ^     ^
 *  | | |     +-- 0..4: Maximum Scan Line:
 *  | | |               In text modes, this field is programmed with the
 *character height - 1 | | |               (scan line numbers are zero based.)
 *In graphics modes, a non-zero | | |               value in this field will
 *cause each scan line to be repeated by the | | |               value of this
 *field + 1. | | +----------- 5: Bit 9 of Start Vertical Blanking (index 0x15)
 *  | +------------- 6: Bit 9 of Line Compare (index 0x18)
 *  +--------------- 7: Scan Doubling:
 *                      When this bit is set to 1, 200-scan-line video data is
 *converted to 400-scan-line output. To do this, the clock in the row scan
 *counter is divided by 2, which allows the 200-line modes to be displayed as
 *400 lines on the display (this is called double scanning; each line is
 *                      displayed twice). When this bit is set to 0, the clock
 *to the row scan counter is equal to the horizontal scan rate.
 *
 * Cursor Start Register (index 0x0a)
 * +---+-+---------+
 * |   |5|4 3 2 1 0|
 * +---+-+---------+
 *      ^     ^
 *      |     +-- 0..4: Cursor Scan Line Start:
 *      |               This field controls the appearance of the text-mode
 *cursor by |               specifying the scan line location within a character
 *cell at which |               the cursor should begin, with the top-most scan
 *line in a character |               cell being 0 and the bottom being with the
 *value of the Maximum Scan |               Line field.
 *      +------------5: Cursor Disable:
 *                      This field controls whether or not the text-mode cursor
 *is displayed: 0: Cursor Enabled. 1: Cursor Disabled.
 *
 * Cursor End Register (index 0x0b)
 * +-+---+---------+
 * | |6 5|4 3 2 1 0|
 * +-+---+---------+
 *     ^      ^
 *     |      +-- 0..4: Cursor Scan Line End:
 *     |                This field controls the appearance of the text-mode
 *cursor by |                specifying the scan line location within a
 *character cell at which |                the cursor should end, with the
 *top-most scan line in a character |                cell being 0 and the bottom
 *being with the value of the Maximum Scan |                Line field. If this
 *field is less than the Cursor Scan Line Start |                field, the
 *cursor is not drawn. Some graphics adapters, such as the |                IBM
 *EGA display a split-block cursor instead.
 *     +------------ 5: Cursor Skew:
 *                      This field was necessary in the EGA to synchronize the
 *cursor with internal timing. In the VGA it basically is added to the cursor
 *                      location. In some cases when this value is non-zero and
 *the cursor is near the left or right edge of the screen, the cursor will not
 *appear at all, or a second cursor above and to the left of the actual one may
 *                      appear. This behavior may not be the same on all VGA
 *compatible adapter cards.
 *
 * Start Address High register (index 0x0c)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 * 0..7: Bits 8..15 of the Start Address.
 * Bits 0..7 are in the Start Address Low register (index 0x0d). The Start
 *Address field specifies the display memory address of the upper left pixel or
 *character of the screen. Because the standard VGA has a maximum of 256K of
 *memory, and memory is accessed 32 bits at a time, this 16-bit field is
 *sufficient to allow the screen to start at any memory address. Normally this
 *field is programmed to 0h, except when using virtual resolutions, paging,
 * and/or split-screen operation. Note that the VGA display will wrap around in
 *display memory if the starting address is too high. (This may or may not be
 *desirable, depending on your intentions.)
 *
 * Start Address Low register (index 0x0d)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 * 0..7: Bits 0..7 of the Start Address. See Start Address High register (index
 *0x0c)
 *
 * Cursor Location High register (index 0x0e)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 * 0..7: Bits 8..15 of the Cursor Location.
 * Bits 0..7 are in the Cursor Location Low register (index 0x0d). When the VGA
 *hardware is displaying text mode and the text-mode cursor is enabled, the
 *hardware compares the address of the character currently being displayed with
 *sum of value of this field and the sum of the Cursor Skew field. If the values
 *equal then the scan lines in that character specified by the Cursor Scan Line
 *Start field and the Cursor Scan Line End field are replaced with the
 * foreground color.
 *
 * Cursor Location Low register (index 0x0f)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 * 0..7: Bits 0..7 of the Cursor Location. See Cursor Location High register
 *(index 0x0f)
 *
 * Vertical Retrace Start register (index 0x10)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 * 0..7: Bits 0..7 of Vertical Retrace Start
 * Bits 8 and 9 are in the Overflow Register (index 0x07). This field controls
 *the start of the vertical retrace pulse which signals the display to move up
 *to the beginning of the active display. This field contains the value of the
 *vertical scanline counter at the beginning of the first scanline where the
 *vertical retrace signal is asserted.
 *
 * Vertical Retrace End register (index 0x11)
 * +-+-+---+-------+
 * |7|6|5|4|3 2 1 0|
 * +-+-+---+-------+
 *  ^ ^ ^ ^   ^
 *  | | | |   +-- 0..3: Vertical Retrace End:
 *  | | | |             This field determines the end of the vertical retrace
 *pulse, and thus its | | | |             length. This field contains the lower
 *four bits of the vertical scanline | | | |             counter at the
 *beginning of the scanline immediately after the last | | | | scanline where
 *the vertical retrace signal is asserted. | | | +--------- 4: End Vertical
 *Interrupt | | +----------- 5: Enable Vertical Interrupt | +------------- 6:
 *Memory Refresh Bandwidth: |                   Nearly all video chipsets
 *include a few registers that control memory, bus, |                   or other
 *timings not directly related to the output of the video card. Most | VGA/SVGA
 *implementations ignore the value of this field; however, in the | least, IBM
 *VGA adapters do utilize it and thus for compatibility with these | chipsets
 *this field should be programmed. This register is used in the IBM | VGA
 *hardware to control the number of DRAM refresh cycles per scan line. | The
 *three refresh cycles per scanline is appropriate for the IBM VGA | horizontal
 *frequency of approximately 31.5 kHz. For horizontal frequencies | greater than
 *this, this setting will work as the DRAM will be refreshed more | often.
 *However, refreshing not often enough for the DRAM can cause memory | loss.
 *Thus at some point slower than 31.5 kHz the five refresh cycle setting |
 *should be used. At which particular point this should occur, would require |
 *better knowledge of the IBM VGA's schematics than I have available. |
 *According to IBM documentation, "Selecting five refresh cycles allows use of
 *  |                   the VGA chip with 15.75 kHz displays." which isn't
 *really enough to go by |                   unless the mode you are defining
 *has a 15.75 kHz horizontal frequency.
 *  +--------------- 7: CRTC Registers Protect Enable:
 *                      This field is used to protect the video timing registers
 *from being changed by programs written for earlier graphics chipsets that
 *attempt to program these registers with values unsuitable for VGA timings.
 *When this field is set to 1, the CRTC register indexes 00h-07h ignore write
 *access, with the exception of bit 4 of the Overflow Register, which holds bit
 *8 of the Line Compare field.
 *
 * Vertical Display End register (index 0x12)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 * 0..7: Bits 0..7 of Vertical Display End
 * Bits 8 and 9 are in the Overflow Register (index 0x07). This field contains
 *the value of the vertical scanline counter at the beggining of the scanline
 *immediately after the last scanline of active display.
 *
 * Offset register (index 0x13)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 * 0..7: Offset:
 * This field specifies the address difference between consecutive scan lines or
 *two lines of characters. Beginning with the second scan line, the starting
 *scan line is increased by twice the value of this register multiplied by the
 *current memory address size (byte = 1, word = 2, double-word = 4) each line.
 *For text modes the following equation is used: Offset = Width / (
 *MemoryAddressSize * 2 ) and in graphics mode, the following equation is used:
 *       Offset = Width / ( PixelsPerAddress * MemoryAddressSize * 2 )
 * where Width is the width in pixels of the screen. This register can be
 *modified to provide for a virtual resolution, in which case Width is the width
 *is the width in pixels of the virtual screen. PixelsPerAddress is the number
 *of pixels stored in one display memory address, and MemoryAddressSize is the
 *current memory addressing size.
 *
 * Underline Location register (index 0x14)
 * +-+-+-+---------+
 * | |6|5|4 3 2 1 0|
 * +-+-+-+---------+
 *    ^ ^     ^
 *    | |     +-- 0..4: Underline Location
 *    | |               These bits specify the horizontal scan line of a
 *character row on which an | |               underline occurs. The value
 *programmed is the scan line desired minus 1. | +----------- 5: Divide Memory
 *Address Clock by 4: |                   1: The memory-address counter is
 *clocked with the character clock divided |                      by 4, which is
 *used when doubleword addresses are used.
 *    +------------- 6: Double-Word Addressing:
 *                        1: Memory addresses are doubleword addresses. See the
 *description of the word/byte mode bit (bit 6) in the CRT Mode Control Register
 *(index 0x17)
 *
 * Start Vertical Blanking register (index 0x15)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 * 0..7: Bits 0..7 of Start Vertical Blanking
 * Bit 8 is in the Overflow Register (index 0x07), and bit 9 is in the Maximum
 *Scan Line register (index 0x09). This field determines when the vertical
 *blanking period begins, and contains the value of the vertical scanline
 *counter at the beginning of the first vertical scanline of blanking.
 *
 * End Vertical Blanking register (index 0x16)
 * +-+-------------+
 * | |6 5 4 3 2 1 0|
 * +-+-------------+
 *          ^
 *          +---- 0..6: End Vertical Blanking:
 *                      This field determines when the vertical blanking period
 *ends, and contains the value of the vertical scanline counter at the beginning
 *of the vertical scanline immediately after the last scanline of blanking.
 *
 * CRTC Mode Control Register (index 0x17)
 * +-+-+-+-+-+-+-+-+
 * |7|6|5| |3|2|1|0|
 * +-+-+-+-+-+-+-+-+
 *  ^ ^ ^   ^ ^ ^ ^
 *  | | |   | | | +- 0: Map Display Address 13:
 *  | | |   | | |       This bit selects the source of bit 13 of the output
 *multiplexer: | | |   | | |         0: Bit 0 of the row scan counter is the
 *source. | | |   | | |         1: Bit 13 of the address counter is the source.
 *  | | |   | | |       The CRT controller used on the IBM Color/Graphics
 *Adapter was capable of | | |   | | |       using 128 horizontal scan-line
 *addresses. For the VGA to obtain 640-by-200 | | |   | | |       graphics
 *resolution, the CRT controller is programmed for 100 horizontal | | |   | | |
 *scan lines with two scan-line addresses per character row. Row scan address |
 *| |   | | |       bit 0 becomes the most-significant address bit to the
 *display buffer. | | |   | | |       Successive scan lines of the display image
 *are displaced in 8KB of memory. | | |   | | |       This bit allows
 *compatibility with the graphics modes of earlier adapters. | | |   | | +--- 1:
 *Map Display Address 14: | | |   | |         This bit selects the source of bit
 *14 of the output multiplexer: | | |   | |           0: Bit 1 of the row scan
 *counter is the source. | | |   | |           1: Bit 14 of the address counter
 *is the source. | | |   | +----- 2: Divide Scan Line clock by 2: | | |   | This
 *bit selects the clock that controls the vertical timing counter: | | |   | 0:
 *The horizontal retrace clock. | | |   |             1: The horizontal retrace
 *clock divided by 2. | | |   |           Dividing the clock effectively doubles
 *the vertical resolution of the CRT | | |   |           controller. The
 *vertical counter has a maximum resolution of 1024 scan lines | | |   | because
 *the vertical total value is 10-bits wide. If the vertical counter is | | |   |
 *clocked with the horizontal retrace divided by 2, the vertical resolution is
 *  | | |   |           doubled to 2048 scan lines."
 *  | | |   +------- 3: Divide Memory Address clock by 2:
 *  | | |               This bit selects the clock that controlls the address
 *counter: | | |                 0: The character clock. | | | 1: The character
 *clock divided by 2. | | |               This bit is used to create either a
 *byte or word refresh address for the | | |               display buffer. | |
 *+----------- 5: Address Wrap Select: | |                 This bit selects the
 *memory-address bit, bit MA 13 or MA 15, that appears on | | the output pin MA
 *0, in the word address mode. If the VGA is not in the word | | address mode,
 *bit 0 from the address counter appears on the output pin, MA 0. | | 0: Selects
 *MA 13. Used in applications where only 64KB of video memory is | | present. |
 *|                   1: Selects MA 15. In odd/even mode, this bit should be set
 *to 1 because | |                      256KB of video memory is installed on
 *the system board. | |                 This function maintains compatibility
 *with the IBM Color/Graphics Monitor | |                 Adapter. |
 *+------------- 6: Word/Byte Mode Select: |                     0: Selects the
 *word address mode. The word mode shifts the memory-address | counter bits to
 *the left by one bit; the most-significant bit of the | counter appears on the
 *least-significant bit of the memory address |                        outputs.
 *  |                     1: Selects the byte address mode.
 *  |                   The doubleword bit in the Underline Location register
 *(index 0x14) also |                   controls the addressing. When the
 *doubleword bit is 0, the word/byte bit |                   selects the mode.
 *When the doubleword bit is set to 1, the addressing is | shifted by two bits.
 *  +--------------- 7: Sync Enable:
 *                        0: Disables the horizontal and vertical retrace
 *signals and forces them to an inactive level. 1: Enables the horizontal and
 *vertical retrace signals. This bit does not reset any other registers or
 *signal outputs.
 *
 * Line Compare register (index 0x18)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 *         ^
 * 0..7: Bits 0..7 of Line Compare
 * Bit 8 is in the Overflow Register (index 0x07), and bit 9 is in the Maximum
 *Scan Line register (index 0x09). The Line Compare field specifies the scan
 *line at which a horizontal division can occur, providing for split-screen
 *operation. If no horizontal division is required, this field should be set to
 *3FFh. When the scan line counter reaches the value in the Line Compare field,
 *the current scan line address is reset to 0 and the Preset Row Scan is
 *presumed to be 0. If the Pixel Panning Mode field is set to 1 then the Pixel
 *Shift Count and Byte Panning fields are reset to 0 for the remainder of the
 *display cycle. \endcode
 **/
void CCirrus::write_b_3d4(u8 value) {
  state.CRTC.address = value & 0x7f;
#if defined(DEBUG_VGA)
  if (state.CRTC.address > 0x18)
    printf("write: invalid CRTC register 0x%02x selected",
           (unsigned)state.CRTC.address);
#endif
}

/**
 * Write to VGA CRTC Data Register (0x3b5 or 0x3d5)
 *
 * For a description of CRTC Registers, see CCirrus::write_b_3d4.
 **/
void CCirrus::write_b_3d5(u8 value) {

  /* CRTC Registers */
  if (state.CRTC.address > 0x18) {
#if defined(DEBUG_VGA)
    printf("write: invalid CRTC register 0x%02x ignored",
           (unsigned)state.CRTC.address);
#endif
    return;
  }

  if (state.CRTC.write_protect && (state.CRTC.address < 0x08)) {
    if (state.CRTC.address == 0x07) {
      state.CRTC.reg[state.CRTC.address] &= ~0x10;
      state.CRTC.reg[state.CRTC.address] |= (value & 0x10);
      state.line_compare &= 0x2ff;
      if (state.CRTC.reg[0x07] & 0x10)
        state.line_compare |= 0x100;
      redraw_area(0, 0, old_iWidth, old_iHeight);
      return;
    } else {
      return;
    }
  }

  if (value != state.CRTC.reg[state.CRTC.address]) {
    state.CRTC.reg[state.CRTC.address] = value;
    switch (state.CRTC.address) {
    case 0x07:
      state.vertical_display_end &= 0xff;
      if (state.CRTC.reg[0x07] & 0x02)
        state.vertical_display_end |= 0x100;
      if (state.CRTC.reg[0x07] & 0x40)
        state.vertical_display_end |= 0x200;
      state.line_compare &= 0x2ff;
      if (state.CRTC.reg[0x07] & 0x10)
        state.line_compare |= 0x100;
      redraw_area(0, 0, old_iWidth, old_iHeight);
      break;

    case 0x08:

      // Vertical pel panning change
      redraw_area(0, 0, old_iWidth, old_iHeight);
      break;

    case 0x09:
      state.y_doublescan = ((value & 0x9f) > 0);
      state.line_compare &= 0x1ff;
      if (state.CRTC.reg[0x09] & 0x40)
        state.line_compare |= 0x200;
      redraw_area(0, 0, old_iWidth, old_iHeight);
      break;

    case 0x0A:
    case 0x0B:
    case 0x0E:
    case 0x0F:

      // Cursor size / location change
      state.vga_mem_updated = 1;
      break;

    case 0x0C:
    case 0x0D:

      // Start address change
      if (state.graphics_ctrl.graphics_alpha) {
        redraw_area(0, 0, old_iWidth, old_iHeight);
      } else {
        state.vga_mem_updated = 1;
      }
      break;

    case 0x12:
      state.vertical_display_end &= 0x300;
      state.vertical_display_end |= state.CRTC.reg[0x12];
      break;

    case 0x13:
    case 0x14:
    case 0x17:

      // Line offset change
      state.line_offset = state.CRTC.reg[0x13] << 1;
      if (state.CRTC.reg[0x14] & 0x40)
        state.line_offset <<= 2;
      else if ((state.CRTC.reg[0x17] & 0x40) == 0)
        state.line_offset <<= 1;
      redraw_area(0, 0, old_iWidth, old_iHeight);
      break;

    case 0x18:
      state.line_compare &= 0x300;
      state.line_compare |= state.CRTC.reg[0x18];
      redraw_area(0, 0, old_iWidth, old_iHeight);
      break;
    }
  }
}

/**
 * Read from the attribute controller index register (0x3c0)
 *
 * For a description of the attribute controller registers, see
 *CCirrus::write_b_3c0.
 **/
u8 CCirrus::read_b_3c0() {
  if (state.attribute_ctrl.flip_flop == 0) {

    // BX_INFO(("io read: 0x3c0: flip_flop = 0"));
    return (state.attribute_ctrl.video_enabled << 5) |
           state.attribute_ctrl.address;
  } else {
    FAILURE(NotImplemented, "io read: 0x3c0: flip_flop != 0");
  }
}

/**
 * Read from the attribute controller data register (0x3c1)
 *
 * For a description of the attribute controller registers, see
 *CCirrus::write_b_3c0.
 **/
u8 CCirrus::read_b_3c1() {
  u8 retval;
  switch (state.attribute_ctrl.address) {
  case 0x00:
  case 0x01:
  case 0x02:
  case 0x03:
  case 0x04:
  case 0x05:
  case 0x06:
  case 0x07:
  case 0x08:
  case 0x09:
  case 0x0a:
  case 0x0b:
  case 0x0c:
  case 0x0d:
  case 0x0e:
  case 0x0f:
    retval = state.attribute_ctrl.palette_reg[state.attribute_ctrl.address];
    return (retval);
    break;

  case 0x10: /* mode control register */
    retval = (state.attribute_ctrl.mode_ctrl.graphics_alpha << 0) |
             (state.attribute_ctrl.mode_ctrl.display_type << 1) |
             (state.attribute_ctrl.mode_ctrl.enable_line_graphics << 2) |
             (state.attribute_ctrl.mode_ctrl.blink_intensity << 3) |
             (state.attribute_ctrl.mode_ctrl.pixel_panning_compat << 5) |
             (state.attribute_ctrl.mode_ctrl.pixel_clock_select << 6) |
             (state.attribute_ctrl.mode_ctrl.internal_palette_size << 7);
    return (retval);
    break;

  case 0x11: /* overscan color register */
    return (state.attribute_ctrl.overscan_color);
    break;

  case 0x12: /* color plane enable */
    return (state.attribute_ctrl.color_plane_enable);
    break;

  case 0x13: /* horizontal PEL panning register */
    return (state.attribute_ctrl.horiz_pel_panning);
    break;

  case 0x14: /* color select register */
    return (state.attribute_ctrl.color_select);
    break;

  default:
    FAILURE_1(NotImplemented, "io read: 0x3c1: unknown register 0x%02x",
              (unsigned)state.attribute_ctrl.address);
  }
}

/**
 * Read from the VGA Input Status register (0x3c2)
 *
 * \code
 * +-----+-+-------+
 * |     |4|       |
 * +-----+-+-------+
 *        ^
 *        +--------- 4: Switch Sense:
 *                      Returns the status of the four sense switches as
 *selected by the Clock Select field of the Miscellaneous Output Register (See
 *                      CCirrus::write_b_3c2)
 * \endcode
 **/
u8 CCirrus::read_b_3c2() {
  return 0; // input status register
}

/**
 * Read from the VGA Enable register (0x3c3)
 *
 * (Not sure where this comes from; doesn't seem to be in the VGA specs.)
 **/
u8 CCirrus::read_b_3c3() { return state.vga_enabled; }

/**
 * Read from the VGA sequencer index register (0x3c4)
 *
 * For a description of the Sequencer registers, see CCirrus::write_b_3c4
 **/
u8 CCirrus::read_b_3c4() { return state.sequencer.index; }

/**
 * Read from the VGA sequencer data register (0x3c5)
 *
 * For a description of the Sequencer registers, see CCirrus::write_b_3c4
 **/
u8 CCirrus::read_b_3c5() {
  switch (state.sequencer.index) {
  case 0: /* sequencer: reset */
#if defined(DEBUG_VGA)
    BX_DEBUG(("io read 0x3c5: sequencer reset"));
#endif
    return (state.sequencer.reset1 ? 1 : 0) | (state.sequencer.reset2 ? 2 : 0);
    break;

  case 1: /* sequencer: clocking mode */
#if defined(DEBUG_VGA)
    BX_DEBUG(("io read 0x3c5: sequencer clocking mode"));
#endif
    return state.sequencer.reg1;
    break;

  case 2: /* sequencer: map mask register */
    return state.sequencer.map_mask;
    break;

  case 3: /* sequencer: character map select register */
    return state.sequencer.char_map_select;
    break;

  case 4: /* sequencer: memory mode register */
    return (state.sequencer.extended_mem << 1) |
           (state.sequencer.odd_even << 2) | (state.sequencer.chain_four << 3);
    break;

  default:
    FAILURE_1(NotImplemented, "io read 0x3c5: index %u unhandled",
              (unsigned)state.sequencer.index);
  }
}

/**
 * Read from VGA DAC Data register (0x3c9)
 *
 * For a description of DAC registers see CCirrus::write_b_3c7
 **/
u8 CCirrus::read_b_3c9() {
  u8 retval;
  if (state.pel.dac_state == 0x03) {
    switch (state.pel.read_data_cycle) {
    case 0:
      retval = state.pel.data[state.pel.read_data_register].red;
      break;
    case 1:
      retval = state.pel.data[state.pel.read_data_register].green;
      break;
    case 2:
      retval = state.pel.data[state.pel.read_data_register].blue;
      break;
    default:
      retval = 0; // keep compiler happy
    }

    state.pel.read_data_cycle++;
    if (state.pel.read_data_cycle >= 3) {
      state.pel.read_data_cycle = 0;
      state.pel.read_data_register++;
    }
  } else {
    retval = 0x3f;
  }

  return retval;
}
/**
 * Read from the VGA Feature Control register (index 0x3ca)
 *
 * \code
 * +-----------+-+-+
 * |           |1|0|
 * +-----------+-+-+
 *              ^ ^
 *              | +- 0: Feature Control 0 (reserved)
 *              +--- 1: Feature Control 1 (reserved)
 * \endcode
 **/
u8 CCirrus::read_b_3ca() { return 0; }

/**
 * Write to the VGA Miscellaneous Output Register (0x3cc)
 *
 * For a description of the Miscellaneous Output register, see
 *CCirrus::write_b_3c2
 **/
u8 CCirrus::read_b_3cc() {

  /* Miscellaneous Output / Graphics 1 Position ??? */
  return ((state.misc_output.color_emulation & 0x01) << 0) |
         ((state.misc_output.enable_ram & 0x01) << 1) |
         ((state.misc_output.clock_select & 0x03) << 2) |
         ((state.misc_output.select_high_bank & 0x01) << 5) |
         ((state.misc_output.horiz_sync_pol & 0x01) << 6) |
         ((state.misc_output.vert_sync_pol & 0x01) << 7);
}

/**
 * Read from VGA Graphics Controller Data Register (0x3cf)
 *
 * For a description of the Graphics registers, see CCirrus::write_b_3ce
 **/
u8 CCirrus::read_b_3cf() {
  u8 retval;
  switch (state.graphics_ctrl.index) {
  case 0: /* Set/Reset */
    return (state.graphics_ctrl.set_reset);
    break;

  case 1: /* Enable Set/Reset */
    return (state.graphics_ctrl.enable_set_reset);
    break;

  case 2: /* Color Compare */
    return (state.graphics_ctrl.color_compare);
    break;

  case 3: /* Data Rotate */
    retval = ((state.graphics_ctrl.raster_op & 0x03) << 3) |
             ((state.graphics_ctrl.data_rotate & 0x07) << 0);
    return (retval);
    break;

  case 4: /* Read Map Select */
    return (state.graphics_ctrl.read_map_select);
    break;

  case 5: /* Mode */
    retval = ((state.graphics_ctrl.shift_reg & 0x03) << 5) |
             ((state.graphics_ctrl.odd_even & 0x01) << 4) |
             ((state.graphics_ctrl.read_mode & 0x01) << 3) |
             ((state.graphics_ctrl.write_mode & 0x03) << 0);

#if defined(DEBUG_VGA)
    if (state.graphics_ctrl.odd_even || state.graphics_ctrl.shift_reg)
      BX_DEBUG(("io read 0x3cf: reg 05 = 0x%02x", (unsigned)retval));
#endif
    return (retval);
    break;

  case 6: /* Miscellaneous */
    return ((state.graphics_ctrl.memory_mapping & 0x03) << 2) |
           ((state.graphics_ctrl.odd_even & 0x01) << 1) |
           ((state.graphics_ctrl.graphics_alpha & 0x01) << 0);
    break;

  case 7: /* Color Don't Care */
    return (state.graphics_ctrl.color_dont_care);
    break;

  case 8: /* Bit Mask */
    return (state.graphics_ctrl.bitmask);
    break;

  default:
    FAILURE_1(NotImplemented, "io read: 0x3cf: index %u unhandled",
              (unsigned)state.graphics_ctrl.index);
  }
}

/**
 * Read from VGA CRTC Index Register (0x3b5 or 0x3d5)
 *
 * For a description of CRTC Registers, see CCirrus::write_b_3d4.
 **/
u8 CCirrus::read_b_3d4() { return state.CRTC.address; }

/**
 * Read from VGA CRTC Data Register (0x3b5 or 0x3d5)
 *
 * For a description of CRTC Registers, see CCirrus::write_b_3d4.
 **/
u8 CCirrus::read_b_3d5() {
  if (state.CRTC.address > 0x18) {
    FAILURE_1(NotImplemented, "io read: invalid CRTC register 0x%02x   \n",
              (unsigned)state.CRTC.address);
  }

  return state.CRTC.reg[state.CRTC.address];
}

/**
 * Read from the VGA Input Status 1 register (0x3ba or 0x3da)
 *
 * \code
 * +-------+-+---+-+
 * |       |3|   |0|
 * +-------+-+---+-+
 *          ^     ^
 *          |     +- 0: Display Disabled:
 *          |             1: Indicates a horizontal or vertical retrace
 *interval. This |                bit is the real-time status of the inverted
 *'display |                enable' signal. Programs have used this status bit
 *to |                restrict screen updates to the inactive display intervals
 *          |                in order to reduce screen flicker. The video
 *subsystem is |                designed to eliminate this software requirement;
 *screen |                updates may be made at any time without screen
 *degradation.
 *          +------- 1: Vertical Retrace:
 *                        1: Indicates that the display is in a vertical retrace
 *interval. This bit can be programmed, through the Vertical Retrace End
 *                           register, to generate an interrupt at the start of
 *the vertical retrace. \endcode
 **/
u8 CCirrus::read_b_3da() {

  /* Input Status 1 (color emulation modes) */
  u8 retval = 0;

  // bit3: Vertical Retrace
  //       0 = display is in the display mode
  //       1 = display is in the vertical retrace mode
  // bit0: Display Enable
  //       0 = display is in the display mode
  //       1 = display is not in the display mode; either the
  //           horizontal or vertical retrace period is active
  // using 72 Hz vertical frequency

  /*** TO DO ??? ***
       usec = bx_pc_system.time_usec();
       switch ( ( state.misc_output.vert_sync_pol << 1) |
     state.misc_output.horiz_sync_pol )
       {
         case 0: vertres = 200; break;
         case 1: vertres = 400; break;
         case 2: vertres = 350; break;
         default: vertres = 480; break;
       }
       if ((usec % 13888) < 70) {
         vert_retrace = 1;
       }
       if ((usec % (13888 / vertres)) == 0) {
         horiz_retrace = 1;
       }

       if (horiz_retrace || vert_retrace)
         retval = 0x01;
       if (vert_retrace)
         retval |= 0x08;

       *** TO DO ??? ***/

  /* reading this port resets the flip-flop to address mode */
  state.attribute_ctrl.flip_flop = 0;
  return retval;
}

u8 CCirrus::get_actl_palette_idx(u8 index) {
  return state.attribute_ctrl.palette_reg[index];
}

void CCirrus::redraw_area(unsigned x0, unsigned y0, unsigned width,
                          unsigned height) {
  unsigned xti;

  unsigned yti;

  unsigned xt0;

  unsigned xt1;

  unsigned yt0;

  unsigned yt1;

  unsigned xmax;

  unsigned ymax;

  if ((width == 0) || (height == 0)) {
    return;
  }

  state.vga_mem_updated = 1;

  if (state.graphics_ctrl.graphics_alpha) {

    // graphics mode
    xmax = old_iWidth;
    ymax = old_iHeight;
    xt0 = x0 / X_TILESIZE;
    yt0 = y0 / Y_TILESIZE;
    if (x0 < xmax) {
      xt1 = (x0 + width - 1) / X_TILESIZE;
    } else {
      xt1 = (xmax - 1) / X_TILESIZE;
    }

    if (y0 < ymax) {
      yt1 = (y0 + height - 1) / Y_TILESIZE;
    } else {
      yt1 = (ymax - 1) / Y_TILESIZE;
    }

    for (yti = yt0; yti <= yt1; yti++) {
      for (xti = xt0; xti <= xt1; xti++) {
        SET_TILE_UPDATED(xti, yti, 1);
      }
    }
  } else {

    // text mode
    memset(state.text_snapshot, 0, sizeof(state.text_snapshot));
  }
}

void CCirrus::update(void) {
  unsigned iHeight;

  unsigned iWidth;

  /* no screen update necessary */
  if (state.vga_mem_updated == 0)
    return;

  /* skip screen update when vga/video is disabled or the sequencer is in reset
   * mode */
  if (!state.vga_enabled || !state.attribute_ctrl.video_enabled ||
      !state.sequencer.reset2 || !state.sequencer.reset1)
    return;

  // fields that effect the way video memory is serialized into screen output:
  // GRAPHICS CONTROLLER:
  //   state.graphics_ctrl.shift_reg:
  //     0: output data in standard VGA format or CGA-compatible 640x200 2 color
  //        graphics mode (mode 6)
  //     1: output data in CGA-compatible 320x200 4 color graphics mode
  //        (modes 4 & 5)
  //     2: output data 8 bits at a time from the 4 bit planes
  //        (mode 13 and variants like modeX)
  // if (state.vga_mem_updated==0 || state.attribute_ctrl.video_enabled == 0)
  if (state.graphics_ctrl.graphics_alpha) {
    u8 color;
    unsigned bit_no;
    unsigned r;
    unsigned c;
    unsigned x;
    unsigned y;
    unsigned long byte_offset;
    unsigned long start_addr;
    unsigned xc;
    unsigned yc;
    unsigned xti;
    unsigned yti;

    start_addr = (state.CRTC.reg[0x0c] << 8) | state.CRTC.reg[0x0d];

    // BX_DEBUG(("update: shiftreg=%u, chain4=%u, mapping=%u",
    //  (unsigned) state.graphics_ctrl.shift_reg,
    //  (unsigned) state.sequencer.chain_four,
    //  (unsigned) state.graphics_ctrl.memory_mapping);
    determine_screen_dimensions(&iHeight, &iWidth);
    if ((iWidth != old_iWidth) || (iHeight != old_iHeight) ||
        (state.last_bpp > 8)) {
      bx_gui->dimension_update(iWidth, iHeight);
      old_iWidth = iWidth;
      old_iHeight = iHeight;
      state.last_bpp = 8;
    }

    switch (state.graphics_ctrl.shift_reg) {
    case 0:
      u8 attribute, palette_reg_val, DAC_regno;

      unsigned long line_compare;
      u8 *plane0;
      u8 *plane1;
      u8 *plane2;
      u8 *plane3;

      if (state.graphics_ctrl.memory_mapping == 3) { // CGA 640x200x2
        for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
          for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
            if (GET_TILE_UPDATED(xti, yti)) {
              for (r = 0; r < Y_TILESIZE; r++) {
                y = yc + r;
                if (state.y_doublescan)
                  y >>= 1;
                for (c = 0; c < X_TILESIZE; c++) {
                  x = xc + c;

                  /* 0 or 0x2000 */
                  byte_offset = start_addr + ((y & 1) << 13);

                  /* to the start of the line */
                  byte_offset += (320 / 4) * (y / 2);

                  /* to the byte start */
                  byte_offset += (x / 8);

                  bit_no = 7 - (x % 8);
                  palette_reg_val =
                      (((state.memory[byte_offset]) >> bit_no) & 1);
                  DAC_regno = state.attribute_ctrl.palette_reg[palette_reg_val];
                  state.tile[r * X_TILESIZE + c] = DAC_regno;
                }
              }

              SET_TILE_UPDATED(xti, yti, 0);
              bx_gui->graphics_tile_update(state.tile, xc, yc);
            }
          }
        }
      } else { // output data in serial fashion with each display plane
        // output on its associated serial output.  Standard EGA/VGA format
        plane0 = &state.memory[0 << 16];
        plane1 = &state.memory[1 << 16];
        plane2 = &state.memory[2 << 16];
        plane3 = &state.memory[3 << 16];
        line_compare = state.line_compare;
        if (state.y_doublescan)
          line_compare >>= 1;

        for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
          for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
            if (GET_TILE_UPDATED(xti, yti)) {
              for (r = 0; r < Y_TILESIZE; r++) {
                y = yc + r;
                if (state.y_doublescan)
                  y >>= 1;
                for (c = 0; c < X_TILESIZE; c++) {
                  x = xc + c;
                  if (state.x_dotclockdiv2)
                    x >>= 1;
                  bit_no = 7 - (x % 8);
                  if (y > line_compare) {
                    byte_offset =
                        x / 8 + ((y - line_compare - 1) * state.line_offset);
                  } else {
                    byte_offset = start_addr + x / 8 + (y * state.line_offset);
                  }

                  attribute = (((plane0[byte_offset] >> bit_no) & 0x01) << 0) |
                              (((plane1[byte_offset] >> bit_no) & 0x01) << 1) |
                              (((plane2[byte_offset] >> bit_no) & 0x01) << 2) |
                              (((plane3[byte_offset] >> bit_no) & 0x01) << 3);

                  attribute &= state.attribute_ctrl.color_plane_enable;

                  // undocumented feature ???: colors 0..7 high intensity,
                  // colors 8..15 blinking using low/high intensity. Blinking is
                  // not implemented yet.
                  if (state.attribute_ctrl.mode_ctrl.blink_intensity)
                    attribute ^= 0x08;
                  palette_reg_val = state.attribute_ctrl.palette_reg[attribute];
                  if (state.attribute_ctrl.mode_ctrl.internal_palette_size) {

                    // use 4 lower bits from palette register
                    // use 4 higher bits from color select register
                    // 16 banks of 16-color registers
                    DAC_regno = (palette_reg_val & 0x0f) |
                                (state.attribute_ctrl.color_select << 4);
                  } else {

                    // use 6 lower bits from palette register
                    // use 2 higher bits from color select register
                    // 4 banks of 64-color registers
                    DAC_regno =
                        (palette_reg_val & 0x3f) |
                        ((state.attribute_ctrl.color_select & 0x0c) << 4);
                  }

                  // DAC_regno &= video DAC mask register ???
                  state.tile[r * X_TILESIZE + c] = DAC_regno;
                }
              }

              SET_TILE_UPDATED(xti, yti, 0);
              bx_gui->graphics_tile_update(state.tile, xc, yc);
            }
          }
        }
      }
      break; // case 0

    case 1: // output the data in a CGA-compatible 320x200 4 color graphics
      // mode.  (modes 4 & 5)

      /* CGA 320x200x4 start */
      for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
        for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
          if (GET_TILE_UPDATED(xti, yti)) {
            for (r = 0; r < Y_TILESIZE; r++) {
              y = yc + r;
              if (state.y_doublescan)
                y >>= 1;
              for (c = 0; c < X_TILESIZE; c++) {
                x = xc + c;
                if (state.x_dotclockdiv2)
                  x >>= 1;

                /* 0 or 0x2000 */
                byte_offset = start_addr + ((y & 1) << 13);

                /* to the start of the line */
                byte_offset += (320 / 4) * (y / 2);

                /* to the byte start */
                byte_offset += (x / 4);

                attribute = 6 - 2 * (x % 4);
                palette_reg_val = (state.memory[byte_offset]) >> attribute;
                palette_reg_val &= 3;
                DAC_regno = state.attribute_ctrl.palette_reg[palette_reg_val];
                state.tile[r * X_TILESIZE + c] = DAC_regno;
              }
            }

            SET_TILE_UPDATED(xti, yti, 0);
            bx_gui->graphics_tile_update(state.tile, xc, yc);
          }
        }
      }

      /* CGA 320x200x4 end */
      break; // case 1

    case 2: // output the data eight bits at a time from the 4 bit plane

    // (format for VGA mode 13 hex)
    case 3: // FIXME: is this really the same ???
      if (state.sequencer.chain_four) {
        unsigned long pixely;

        unsigned long pixelx;

        unsigned long plane;

        if (state.misc_output.select_high_bank != 1) {
          FAILURE(NotImplemented, "update: select_high_bank != 1   \n");
        }

        for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
          for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
            if (GET_TILE_UPDATED(xti, yti)) {
              for (r = 0; r < Y_TILESIZE; r++) {
                pixely = yc + r;
                if (state.y_doublescan)
                  pixely >>= 1;
                for (c = 0; c < X_TILESIZE; c++) {
                  pixelx = (xc + c) >> 1;
                  plane = (pixelx % 4);
                  byte_offset = start_addr + (plane * 65536) +
                                (pixely * state.line_offset) + (pixelx & ~0x03);
                  color = state.memory[byte_offset];
                  state.tile[r * X_TILESIZE + c] = color;
                }
              }

              SET_TILE_UPDATED(xti, yti, 0);
              bx_gui->graphics_tile_update(state.tile, xc, yc);
            }
          }
        }
      } else { // chain_four == 0, modeX
        unsigned long pixely;

        // chain_four == 0, modeX
        unsigned long pixelx;

        // chain_four == 0, modeX
        unsigned long plane;

        for (yc = 0, yti = 0; yc < iHeight; yc += Y_TILESIZE, yti++) {
          for (xc = 0, xti = 0; xc < iWidth; xc += X_TILESIZE, xti++) {
            if (GET_TILE_UPDATED(xti, yti)) {
              for (r = 0; r < Y_TILESIZE; r++) {
                pixely = yc + r;
                if (state.y_doublescan)
                  pixely >>= 1;
                for (c = 0; c < X_TILESIZE; c++) {
                  pixelx = (xc + c) >> 1;
                  plane = (pixelx % 4);
                  byte_offset = (plane * 65536) + (pixely * state.line_offset) +
                                (pixelx >> 2);
                  color = state.memory[start_addr + byte_offset];
                  state.tile[r * X_TILESIZE + c] = color;
                }
              }

              SET_TILE_UPDATED(xti, yti, 0);
              bx_gui->graphics_tile_update(state.tile, xc, yc);
            }
          }
        }
      }
      break; // case 2

    default:
      FAILURE_1(NotImplemented, "update: shift_reg == %u   \n",
                (unsigned)state.graphics_ctrl.shift_reg);
    }

    state.vga_mem_updated = 0;
    return;
  } else { // text mode
    unsigned long start_address;
    unsigned long cursor_address;
    unsigned long cursor_x;
    unsigned long cursor_y;
    bx_vga_tminfo_t tm_info;
    unsigned VDE;
    unsigned MSL;
    unsigned cols;
    unsigned rows;
    unsigned cWidth;

    tm_info.start_address =
        2 * ((state.CRTC.reg[12] << 8) + state.CRTC.reg[13]);
    tm_info.cs_start = state.CRTC.reg[0x0a] & 0x3f;
    tm_info.cs_end = state.CRTC.reg[0x0b] & 0x1f;
    tm_info.line_offset = state.CRTC.reg[0x13] << 2;
    tm_info.line_compare = state.line_compare;
    tm_info.h_panning = state.attribute_ctrl.horiz_pel_panning & 0x0f;
    tm_info.v_panning = state.CRTC.reg[0x08] & 0x1f;
    tm_info.line_graphics = state.attribute_ctrl.mode_ctrl.enable_line_graphics;
    tm_info.split_hpanning =
        state.attribute_ctrl.mode_ctrl.pixel_panning_compat;
    if ((state.sequencer.reg1 & 0x01) == 0) {
      if (tm_info.h_panning >= 8)
        tm_info.h_panning = 0;
      else
        tm_info.h_panning++;
    } else {
      tm_info.h_panning &= 0x07;
    }

    // Verticle Display End: find out how many lines are displayed
    VDE = state.vertical_display_end;

    // Maximum Scan Line: height of character cell
    MSL = state.CRTC.reg[0x09] & 0x1f;
    if (MSL == 0) {
#if defined(DEBUG_VGA)
      BX_ERROR(("character height = 1, skipping text update"));
#endif
      return;
    }

    cols = state.CRTC.reg[1] + 1;
    if ((MSL == 1) && (VDE == 399)) {

      // emulated CGA graphics mode 160x100x16 colors
      MSL = 3;
    }

    rows = (VDE + 1) / (MSL + 1);
    if (rows > BX_MAX_TEXT_LINES) {
      BX_PANIC(("text rows>%d: %d", BX_MAX_TEXT_LINES, rows));
      return;
    }

    cWidth = ((state.sequencer.reg1 & 0x01) == 1) ? 8 : 9;
    iWidth = cWidth * cols;
    iHeight = VDE + 1;
    if ((iWidth != old_iWidth) || (iHeight != old_iHeight) ||
        (MSL != old_MSL) || (state.last_bpp > 8)) {
      bx_gui->dimension_update(iWidth, iHeight, MSL + 1, cWidth);
      old_iWidth = iWidth;
      old_iHeight = iHeight;
      old_MSL = MSL;
      state.last_bpp = 8;
    }

    // pass old text snapshot & new VGA memory contents
    start_address = 2 * ((state.CRTC.reg[12] << 8) + state.CRTC.reg[13]);
    cursor_address = 2 * ((state.CRTC.reg[0x0e] << 8) + state.CRTC.reg[0x0f]);
    if (cursor_address < start_address) {
      cursor_x = 0xffff;
      cursor_y = 0xffff;
    } else {
      cursor_x = ((cursor_address - start_address) / 2) % (iWidth / cWidth);
      cursor_y = ((cursor_address - start_address) / 2) / (iWidth / cWidth);
    }

    bx_gui->text_update(state.text_snapshot, &state.memory[start_address],
                        cursor_x, cursor_y, tm_info, rows);

    // screen updated, copy new VGA memory contents into text snapshot
    memcpy(state.text_snapshot, &state.memory[start_address], 2 * cols * rows);
    state.vga_mem_updated = 0;
  }
}

void CCirrus::determine_screen_dimensions(unsigned *piHeight,
                                          unsigned *piWidth) {
  int ai[0x20];
  int i;
  int h;
  int v;
  for (i = 0; i < 0x20; i++)
    ai[i] = state.CRTC.reg[i];

  h = (ai[1] + 1) * 8;
  v = (ai[18] | ((ai[7] & 0x02) << 7) | ((ai[7] & 0x40) << 3)) + 1;

  if (state.graphics_ctrl.shift_reg == 0) {
    *piWidth = 640;
    *piHeight = 480;

    if (state.CRTC.reg[6] == 0xBF) {
      if (state.CRTC.reg[23] == 0xA3 && state.CRTC.reg[20] == 0x40 &&
          state.CRTC.reg[9] == 0x41) {
        *piWidth = 320;
        *piHeight = 240;
      } else {
        if (state.x_dotclockdiv2)
          h <<= 1;
        *piWidth = h;
        *piHeight = v;
      }
    } else if ((h >= 640) && (v >= 480)) {
      *piWidth = h;
      *piHeight = v;
    }
  } else if (state.graphics_ctrl.shift_reg == 2) {
    if (state.sequencer.chain_four) {
      *piWidth = h;
      *piHeight = v;
    } else {
      *piWidth = h;
      *piHeight = v;
    }
  } else {
    if (state.x_dotclockdiv2)
      h <<= 1;
    *piWidth = h;
    *piHeight = v;
  }
}

u8 CCirrus::vga_mem_read(u32 addr) {
  u32 offset;
  u8 *plane0;
  u8 *plane1;
  u8 *plane2;
  u8 *plane3;
  u8 retval = 0;

  switch (state.graphics_ctrl.memory_mapping) {
  case 1: // 0xA0000 .. 0xAFFFF
    if (addr > 0xAFFFF)
      return 0xff;
    offset = addr & 0xFFFF;
    break;

  case 2: // 0xB0000 .. 0xB7FFF
    if ((addr < 0xB0000) || (addr > 0xB7FFF))
      return 0xff;
    offset = addr & 0x7FFF;
    break;

  case 3: // 0xB8000 .. 0xBFFFF
    if (addr < 0xB8000)
      return 0xff;
    offset = addr & 0x7FFF;
    break;

  default: // 0xA0000 .. 0xBFFFF
    offset = addr & 0x1FFFF;
  }

  if (state.sequencer.chain_four) {

    // Mode 13h: 320 x 200 256 color mode: chained pixel representation
    return state.memory[(offset & ~0x03) + (offset % 4) * 65536];
  }

  plane0 = &state.memory[0 << 16];
  plane1 = &state.memory[1 << 16];
  plane2 = &state.memory[2 << 16];
  plane3 = &state.memory[3 << 16];

  /* addr between 0xA0000 and 0xAFFFF */
  if (state.graphics_ctrl.read_mode) {
    u8 color_compare;

    u8 color_dont_care;
    u8 latch0;
    u8 latch1;
    u8 latch2;
    u8 latch3;

    color_compare = state.graphics_ctrl.color_compare & 0x0f;
    color_dont_care = state.graphics_ctrl.color_dont_care & 0x0f;
    latch0 = state.graphics_ctrl.latch[0] = plane0[offset];
    latch1 = state.graphics_ctrl.latch[1] = plane1[offset];
    latch2 = state.graphics_ctrl.latch[2] = plane2[offset];
    latch3 = state.graphics_ctrl.latch[3] = plane3[offset];

    latch0 ^= ccdat[color_compare][0];
    latch1 ^= ccdat[color_compare][1];
    latch2 ^= ccdat[color_compare][2];
    latch3 ^= ccdat[color_compare][3];

    latch0 &= ccdat[color_dont_care][0];
    latch1 &= ccdat[color_dont_care][1];
    latch2 &= ccdat[color_dont_care][2];
    latch3 &= ccdat[color_dont_care][3];

    retval = ~(latch0 | latch1 | latch2 | latch3);
  } else {
    state.graphics_ctrl.latch[0] = plane0[offset];
    state.graphics_ctrl.latch[1] = plane1[offset];
    state.graphics_ctrl.latch[2] = plane2[offset];
    state.graphics_ctrl.latch[3] = plane3[offset];
    retval = state.graphics_ctrl.latch[state.graphics_ctrl.read_map_select];
  }

  return retval;
}

/**
 * Write to Legacy VGA Memory
 **/

void CCirrus::vga_mem_write(u32 addr, u8 value) {
  u32 offset;
  u8 new_val[4];
  unsigned start_addr;
  u8 *plane0;
  u8 *plane1;
  u8 *plane2;
  u8 *plane3;

  /* The memory_mapping bits of the graphics controller determine
   * what window of VGA memory is available.
   *
   *  00: 0xA0000 .. 0xBFFFF (128K)
   *  01: 0xA0000 .. 0xAFFFF (64K) (also used for VGA text mode)
   *  02: 0xB0000 .. 0xB7FFF (32K)
   *  03: 0xB8000 .. 0xBFFFF (32K) (also used for CGA text mode)
   */
  switch (state.graphics_ctrl.memory_mapping) {
  // 0xA0000 .. 0xAFFFF
  case 1:
    if (addr > 0xAFFFF)
      return;
    offset = addr - 0xA0000;
    break;

  // 0xB0000 .. 0xB7FFF
  case 2:
    if ((addr < 0xB0000) || (addr > 0xB7FFF))
      return;
    offset = addr - 0xB0000;
    break;

  // 0xB8000 .. 0xBFFFF
  case 3:
    if (addr < 0xB8000)
      return;
    offset = addr - 0xB8000;
    break;

  // 0xA0000 .. 0xBFFFF
  default:
    offset = addr - 0xA0000;
  }

  start_addr = (state.CRTC.reg[0x0c] << 8) | state.CRTC.reg[0x0d];

  if (state.graphics_ctrl.graphics_alpha) {
    if (state.graphics_ctrl.memory_mapping == 3) {
      // Text mode, and memory 0xB8000 .. 0xBFFFF selected => CGA text mode
      unsigned x_tileno;
      unsigned x_tileno2;
      unsigned y_tileno;

      /* CGA 320x200x4 / 640x200x2 start */
      state.memory[offset] = value;
      offset -= start_addr;
      if (offset >= 0x2000) {
        y_tileno = offset - 0x2000;
        y_tileno /= (320 / 4);
        y_tileno <<= 1; // 2 * y_tileno;
        y_tileno++;
        x_tileno = (offset - 0x2000) % (320 / 4);
        x_tileno <<= 2; //*= 4;
      } else {
        y_tileno = offset / (320 / 4);
        y_tileno <<= 1; // 2 * y_tileno;
        x_tileno = offset % (320 / 4);
        x_tileno <<= 2; //*=4;
      }

      x_tileno2 = x_tileno;
      if (state.graphics_ctrl.shift_reg == 0) {
        x_tileno *= 2;
        x_tileno2 += 7;
      } else {
        x_tileno2 += 3;
      }

      if (state.x_dotclockdiv2) {
        x_tileno /= (X_TILESIZE / 2);
        x_tileno2 /= (X_TILESIZE / 2);
      } else {
        x_tileno /= X_TILESIZE;
        x_tileno2 /= X_TILESIZE;
      }

      if (state.y_doublescan) {
        y_tileno /= (Y_TILESIZE / 2);
      } else {
        y_tileno /= Y_TILESIZE;
      }

      state.vga_mem_updated = 1;
      SET_TILE_UPDATED(x_tileno, y_tileno, 1);
      if (x_tileno2 != x_tileno) {
        SET_TILE_UPDATED(x_tileno2, y_tileno, 1);
      }

      return;

      /* CGA 320x200x4 / 640x200x2 end */
    }

    if (state.graphics_ctrl.memory_mapping != 1) {
      FAILURE_1(NotImplemented, "mem_write: graphics: mapping = %u  \n",
                (unsigned)state.graphics_ctrl.memory_mapping);
    }

    if (state.sequencer.chain_four) {
      unsigned x_tileno;

      unsigned y_tileno;

      // 320 x 200 256 color mode: chained pixel representation
      state.memory[(offset & ~0x03) + (offset % 4) * 65536] = value;
      if (state.line_offset > 0) {
        offset -= start_addr;
        x_tileno = (offset % state.line_offset) / (X_TILESIZE / 2);
        if (state.y_doublescan) {
          y_tileno = (offset / state.line_offset) / (Y_TILESIZE / 2);
        } else {
          y_tileno = (offset / state.line_offset) / Y_TILESIZE;
        }

        state.vga_mem_updated = 1;
        SET_TILE_UPDATED(x_tileno, y_tileno, 1);
      }

      return;
    }
  }

  /* addr between 0xA0000 and 0xAFFFF */
  plane0 = &state.memory[0 << 16];
  plane1 = &state.memory[1 << 16];
  plane2 = &state.memory[2 << 16];
  plane3 = &state.memory[3 << 16];

  switch (state.graphics_ctrl.write_mode) {
    unsigned i;
  // Write mode 0
  case 0: {
    /* Write Mode 0 is the standard and most general write mode.
     * While the other write modes are designed to perform a specific
     * task, this mode can be used to perform most tasks as all five
     * operations are performed on the data:
     *   - The data byte from the host is first rotated as specified
     *     by the Rotate Count field, then is replicated across all
     *     four planes.
     *   - Then the Enable Set/Reset field selects which planes will
     *     receive their values from the host data and which will
     *     receive their data from that plane's Set/Reset field
     *     location.
     *   - Then the operation specified by the Logical Operation
     *     field is performed on the resulting data and the data in
     *     the read latches.
     *   - The Bit Mask field is then used to select between the
     *     resulting data and data from the latch register.
     *   - Finally, the resulting data is written to the display
     *     memory planes enabled in the Memory Plane Write Enable
     *     field.
     *   .
     */
    const u8 bitmask = state.graphics_ctrl.bitmask;
    const u8 set_reset = state.graphics_ctrl.set_reset;
    const u8 enable_set_reset = state.graphics_ctrl.enable_set_reset;

    /* perform rotate on CPU data in case its needed */
    if (state.graphics_ctrl.data_rotate) {
      value = (value >> state.graphics_ctrl.data_rotate) |
              (value << (8 - state.graphics_ctrl.data_rotate));
    }

    new_val[0] = state.graphics_ctrl.latch[0] & ~bitmask;
    new_val[1] = state.graphics_ctrl.latch[1] & ~bitmask;
    new_val[2] = state.graphics_ctrl.latch[2] & ~bitmask;
    new_val[3] = state.graphics_ctrl.latch[3] & ~bitmask;
    switch (state.graphics_ctrl.raster_op) {
    case 0: // replace
      new_val[0] |= ((enable_set_reset & 1) ? ((set_reset & 1) ? bitmask : 0)
                                            : (value & bitmask));
      new_val[1] |= ((enable_set_reset & 2) ? ((set_reset & 2) ? bitmask : 0)
                                            : (value & bitmask));
      new_val[2] |= ((enable_set_reset & 4) ? ((set_reset & 4) ? bitmask : 0)
                                            : (value & bitmask));
      new_val[3] |= ((enable_set_reset & 8) ? ((set_reset & 8) ? bitmask : 0)
                                            : (value & bitmask));
      break;

    case 1: // AND
      new_val[0] |=
          ((enable_set_reset & 1)
               ? ((set_reset & 1) ? (state.graphics_ctrl.latch[0] & bitmask)
                                  : 0)
               : (value & state.graphics_ctrl.latch[0]) & bitmask);
      new_val[1] |=
          ((enable_set_reset & 2)
               ? ((set_reset & 2) ? (state.graphics_ctrl.latch[1] & bitmask)
                                  : 0)
               : (value & state.graphics_ctrl.latch[1]) & bitmask);
      new_val[2] |=
          ((enable_set_reset & 4)
               ? ((set_reset & 4) ? (state.graphics_ctrl.latch[2] & bitmask)
                                  : 0)
               : (value & state.graphics_ctrl.latch[2]) & bitmask);
      new_val[3] |=
          ((enable_set_reset & 8)
               ? ((set_reset & 8) ? (state.graphics_ctrl.latch[3] & bitmask)
                                  : 0)
               : (value & state.graphics_ctrl.latch[3]) & bitmask);
      break;

    case 2: // OR
      new_val[0] |=
          ((enable_set_reset & 1)
               ? ((set_reset & 1) ? bitmask
                                  : (state.graphics_ctrl.latch[0] & bitmask))
               : ((value | state.graphics_ctrl.latch[0]) & bitmask));
      new_val[1] |=
          ((enable_set_reset & 2)
               ? ((set_reset & 2) ? bitmask
                                  : (state.graphics_ctrl.latch[1] & bitmask))
               : ((value | state.graphics_ctrl.latch[1]) & bitmask));
      new_val[2] |=
          ((enable_set_reset & 4)
               ? ((set_reset & 4) ? bitmask
                                  : (state.graphics_ctrl.latch[2] & bitmask))
               : ((value | state.graphics_ctrl.latch[2]) & bitmask));
      new_val[3] |=
          ((enable_set_reset & 8)
               ? ((set_reset & 8) ? bitmask
                                  : (state.graphics_ctrl.latch[3] & bitmask))
               : ((value | state.graphics_ctrl.latch[3]) & bitmask));
      break;

    case 3: // XOR
      new_val[0] |=
          ((enable_set_reset & 1)
               ? ((set_reset & 1) ? (~state.graphics_ctrl.latch[0] & bitmask)
                                  : (state.graphics_ctrl.latch[0] & bitmask))
               : (value ^ state.graphics_ctrl.latch[0]) & bitmask);
      new_val[1] |=
          ((enable_set_reset & 2)
               ? ((set_reset & 2) ? (~state.graphics_ctrl.latch[1] & bitmask)
                                  : (state.graphics_ctrl.latch[1] & bitmask))
               : (value ^ state.graphics_ctrl.latch[1]) & bitmask);
      new_val[2] |=
          ((enable_set_reset & 4)
               ? ((set_reset & 4) ? (~state.graphics_ctrl.latch[2] & bitmask)
                                  : (state.graphics_ctrl.latch[2] & bitmask))
               : (value ^ state.graphics_ctrl.latch[2]) & bitmask);
      new_val[3] |=
          ((enable_set_reset & 8)
               ? ((set_reset & 8) ? (~state.graphics_ctrl.latch[3] & bitmask)
                                  : (state.graphics_ctrl.latch[3] & bitmask))
               : (value ^ state.graphics_ctrl.latch[3]) & bitmask);
      break;

    default:
      FAILURE_1(NotImplemented, "vga_mem_write: write mode 0: op = %u",
                (unsigned)state.graphics_ctrl.raster_op);
    }
  } break;

  // Write mode 1
  case 1:
    /* Write Mode 1 is used to transfer the data in the latches
     * register directly to the screen, affected only by the
     * Memory Plane Write Enable field. This can facilitate
     * rapid transfer of data on byte boundaries from one area
     * of video memory to another or filling areas of the
     * display with a pattern of 8 pixels.
     * When Write Mode 0 is used with the Bit Mask field set to
     * 00000000b the operation of the hardware is identical to
     * this mode.
     */
    for (i = 0; i < 4; i++) {
      new_val[i] = state.graphics_ctrl.latch[i];
    }
    break;

  // Write mode 2
  case 2: {
    /* Write Mode 2 is used to unpack a pixel value packed into
     * the lower 4 bits of the host data byte into the 4 display
     * planes:
     *   - In the byte from the host, the bit representing each
     *     plane will be replicated across all 8 bits of the
     *     corresponding planes.
     *   - Then the operation specified by the Logical Operation
     *     field is performed on the resulting data and the data
     *     in the read latches.
     *   - The Bit Mask field is then used to select between the
     *     resulting data and data from the latch register.
     *   - Finally, the resulting data is written to the display
     *     memory planes enabled in the Memory Plane Write Enable
     *     field.
     *   .
     */
    const u8 bitmask = state.graphics_ctrl.bitmask;

    new_val[0] = state.graphics_ctrl.latch[0] & ~bitmask;
    new_val[1] = state.graphics_ctrl.latch[1] & ~bitmask;
    new_val[2] = state.graphics_ctrl.latch[2] & ~bitmask;
    new_val[3] = state.graphics_ctrl.latch[3] & ~bitmask;
    switch (state.graphics_ctrl.raster_op) {
    case 0: // write
      new_val[0] |= (value & 1) ? bitmask : 0;
      new_val[1] |= (value & 2) ? bitmask : 0;
      new_val[2] |= (value & 4) ? bitmask : 0;
      new_val[3] |= (value & 8) ? bitmask : 0;
      break;

    case 1: // AND
      new_val[0] |= (value & 1) ? (state.graphics_ctrl.latch[0] & bitmask) : 0;
      new_val[1] |= (value & 2) ? (state.graphics_ctrl.latch[1] & bitmask) : 0;
      new_val[2] |= (value & 4) ? (state.graphics_ctrl.latch[2] & bitmask) : 0;
      new_val[3] |= (value & 8) ? (state.graphics_ctrl.latch[3] & bitmask) : 0;
      break;

    case 2: // OR
      new_val[0] |=
          (value & 1) ? bitmask : (state.graphics_ctrl.latch[0] & bitmask);
      new_val[1] |=
          (value & 2) ? bitmask : (state.graphics_ctrl.latch[1] & bitmask);
      new_val[2] |=
          (value & 4) ? bitmask : (state.graphics_ctrl.latch[2] & bitmask);
      new_val[3] |=
          (value & 8) ? bitmask : (state.graphics_ctrl.latch[3] & bitmask);
      break;

    case 3: // XOR
      new_val[0] |= (value & 1) ? (~state.graphics_ctrl.latch[0] & bitmask)
                                : (state.graphics_ctrl.latch[0] & bitmask);
      new_val[1] |= (value & 2) ? (~state.graphics_ctrl.latch[1] & bitmask)
                                : (state.graphics_ctrl.latch[1] & bitmask);
      new_val[2] |= (value & 4) ? (~state.graphics_ctrl.latch[2] & bitmask)
                                : (state.graphics_ctrl.latch[2] & bitmask);
      new_val[3] |= (value & 8) ? (~state.graphics_ctrl.latch[3] & bitmask)
                                : (state.graphics_ctrl.latch[3] & bitmask);
      break;
    }
  } break;

  // Write mode 3
  case 3: {
    /* Write Mode 3 is used when the color written is fairly
     * constant but the Bit Mask field needs to be changed
     * frequently, such as when drawing single color lines or
     * text:
     *   - The value of the Set/Reset field is expanded as if
     *     the Enable Set/Reset field were set to 1111b,
     *     regardless of its actual value.
     *   - The host data is first rotated as specified by the
     *     Rotate Count field, then is ANDed with the Bit
     *     Mask field.
     *   - The resulting value is used where the Bit Mask
     *     field normally would be used, selecting data from
     *     either the expansion of the Set/Reset field or the
     *     latch register.
     *   - Finally, the resulting data is written to the
     *     display memory planes enabled in the Memory Plane
     *     Write Enable field.
     *   .
     */
    const u8 bitmask = state.graphics_ctrl.bitmask & value;
    const u8 set_reset = state.graphics_ctrl.set_reset;

    /* perform rotate on CPU data */
    if (state.graphics_ctrl.data_rotate) {
      value = (value >> state.graphics_ctrl.data_rotate) |
              (value << (8 - state.graphics_ctrl.data_rotate));
    }

    new_val[0] = state.graphics_ctrl.latch[0] & ~bitmask;
    new_val[1] = state.graphics_ctrl.latch[1] & ~bitmask;
    new_val[2] = state.graphics_ctrl.latch[2] & ~bitmask;
    new_val[3] = state.graphics_ctrl.latch[3] & ~bitmask;

    value &= bitmask;

    switch (state.graphics_ctrl.raster_op) {
    case 0: // write
      new_val[0] |= (set_reset & 1) ? value : 0;
      new_val[1] |= (set_reset & 2) ? value : 0;
      new_val[2] |= (set_reset & 4) ? value : 0;
      new_val[3] |= (set_reset & 8) ? value : 0;
      break;

    case 1: // AND
      new_val[0] |=
          ((set_reset & 1) ? value : 0) & state.graphics_ctrl.latch[0];
      new_val[1] |=
          ((set_reset & 2) ? value : 0) & state.graphics_ctrl.latch[1];
      new_val[2] |=
          ((set_reset & 4) ? value : 0) & state.graphics_ctrl.latch[2];
      new_val[3] |=
          ((set_reset & 8) ? value : 0) & state.graphics_ctrl.latch[3];
      break;

    case 2: // OR
      new_val[0] |=
          ((set_reset & 1) ? value : 0) | state.graphics_ctrl.latch[0];
      new_val[1] |=
          ((set_reset & 2) ? value : 0) | state.graphics_ctrl.latch[1];
      new_val[2] |=
          ((set_reset & 4) ? value : 0) | state.graphics_ctrl.latch[2];
      new_val[3] |=
          ((set_reset & 8) ? value : 0) | state.graphics_ctrl.latch[3];
      break;

    case 3: // XOR
      new_val[0] |=
          ((set_reset & 1) ? value : 0) ^ state.graphics_ctrl.latch[0];
      new_val[1] |=
          ((set_reset & 2) ? value : 0) ^ state.graphics_ctrl.latch[1];
      new_val[2] |=
          ((set_reset & 4) ? value : 0) ^ state.graphics_ctrl.latch[2];
      new_val[3] |=
          ((set_reset & 8) ? value : 0) ^ state.graphics_ctrl.latch[3];
      break;
    }
  } break;

  default:
    FAILURE_1(NotImplemented, "vga_mem_write: write mode %u ?",
              (unsigned)state.graphics_ctrl.write_mode);
  }

  // state.sequencer.map_mask determines which bitplanes the write should
  // actually go to
  if (state.sequencer.map_mask & 0x0f) {
    state.vga_mem_updated = 1;
    if (state.sequencer.map_mask & 0x01)
      plane0[offset] = new_val[0];
    if (state.sequencer.map_mask & 0x02)
      plane1[offset] = new_val[1];
    if (state.sequencer.map_mask & 0x04) {
      // Plane 2 contains the character map
      if ((offset & 0xe000) == state.charmap_address) {

        // printf("Updating character map %04x with %02x...\n  ", (offset &
        // 0x1fff), new_val[2]);
        bx_gui->lock();
        bx_gui->set_text_charbyte((u16)(offset & 0x1fff), new_val[2]);
        bx_gui->unlock();
      }

      plane2[offset] = new_val[2];
    }

    if (state.sequencer.map_mask & 0x08)
      plane3[offset] = new_val[3];

    unsigned x_tileno;

    unsigned y_tileno;

    if (state.graphics_ctrl.shift_reg == 2) {
      offset -= start_addr;
      x_tileno = (offset % state.line_offset) * 4 / (X_TILESIZE / 2);
      if (state.y_doublescan) {
        y_tileno = (offset / state.line_offset) / (Y_TILESIZE / 2);
      } else {
        y_tileno = (offset / state.line_offset) / Y_TILESIZE;
      }

      SET_TILE_UPDATED(x_tileno, y_tileno, 1);
    } else {
      if (state.line_compare < state.vertical_display_end) {
        if (state.line_offset > 0) {
          if (state.x_dotclockdiv2) {
            x_tileno = (offset % state.line_offset) / (X_TILESIZE / 16);
          } else {
            x_tileno = (offset % state.line_offset) / (X_TILESIZE / 8);
          }

          if (state.y_doublescan) {
            y_tileno =
                ((offset / state.line_offset) * 2 + state.line_compare + 1) /
                Y_TILESIZE;
          } else {
            y_tileno = ((offset / state.line_offset) + state.line_compare + 1) /
                       Y_TILESIZE;
          }

          SET_TILE_UPDATED(x_tileno, y_tileno, 1);
        }
      }

      if (offset >= start_addr) {
        offset -= start_addr;
        if (state.line_offset > 0) {
          if (state.x_dotclockdiv2) {
            x_tileno = (offset % state.line_offset) / (X_TILESIZE / 16);
          } else {
            x_tileno = (offset % state.line_offset) / (X_TILESIZE / 8);
          }

          if (state.y_doublescan) {
            y_tileno = (offset / state.line_offset) / (Y_TILESIZE / 2);
          } else {
            y_tileno = (offset / state.line_offset) / Y_TILESIZE;
          }

          SET_TILE_UPDATED(x_tileno, y_tileno, 1);
        }
      }
    }
  }
}
