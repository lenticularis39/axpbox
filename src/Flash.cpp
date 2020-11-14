/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * Website: http://sourceforge.net/projects/es40
 * E-mail : camiel@camicom.com
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
 * Contains the code for the emulated Flash ROM devices.
 *
 * $Id: Flash.cpp,v 1.19 2008/03/24 22:11:50 iamcamiel Exp $
 *
 * X-1.19       Camiel Vanderhoeven                             24-MAR-2008
 *      Comments.
 *
 * X-1.18       Camiel Vanderhoeven                             17-MAR-2008
 *      Always set volatile DPR rom contents.
 *
 * X-1.17       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.16       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.15       Camiel Vanderhoeven                             30-DEC-2007
 *      Print file id on initialization.
 *
 * X-1.14       Camiel Vanderhoeven                             28-DEC-2007
 *      Throw exceptions rather than just exiting when errors occur.
 *
 * X-1.13       Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.12       Camiel Vanderhoeven                             10-DEC-2007
 *      Changes to make the TraceEngine work again after recent changes.
 *
 * X-1.11       Camiel Vanderhoeven                             10-DEC-2007
 *      Use configurator.
 *
 * X-1.10       Camiel Vanderhoeven                             31-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.9        Camiel Vanderhoeven                             16-FEB-2007
 *      Added functions SaveStateF and RestoreStateF.
 *
 * X-1.8	Brian Wheeler					13-FEB-2007
 *	Formatting.
 *
 * X-1.7 	Camiel Vanderhoeven				12-FEB-2007
 *	Added comments.
 *
 * X-1.6        Camiel Vanderhoeven                             9-FEB-2007
 *      Added comments.
 *
 * X-1.5	Camiel Vanderhoeven				7-FEB-2007
 *	Calls to trace_dev now use the TRC_DEVx macro's.
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
#include "Flash.h"
#include "AlphaCPU.h"
#include "StdAfx.h"
#include "System.h"

// These are the modes for our flash-state-machine.
#define MODE_READ 0
#define MODE_STEP1 1
#define MODE_STEP2 2
#define MODE_AUTOSEL 3
#define MODE_PROGRAM 4
#define MODE_ERASE_STEP3 5
#define MODE_ERASE_STEP4 6
#define MODE_ERASE_STEP5 7
#define MODE_CONFIRM_0 8
#define MODE_CONFIRM_1 9

extern CAlphaCPU *cpu[4];

/**
 * Constructor.
 **/
CFlash::CFlash(CConfigurator *cfg, CSystem *c) : CSystemComponent(cfg, c) {
  if (theSROM)
    FAILURE(Configuration, "More than one Flash");
  theSROM = this;
  c->RegisterMemory(this, 0, U64(0x0000080100000000), 0x8000000); // 2MB
  memset(state.Flash, 0xff, 2 * 1024 * 1024);
  RestoreStateF();
  state.mode = MODE_READ;

  printf("%s: $Id: Flash.cpp,v 1.19 2008/03/24 22:11:50 iamcamiel Exp $\n",
         devid_string);
}

/**
 * Destructor.
 **/
CFlash::~CFlash() {}

/**
 * Read a byte from flashmemory.
 * Normally, this returns one byte from flash, however, after some commands
 * sent to the flash-rom, this returns identification or status information.
 **/
u64 CFlash::ReadMem(int index, u64 address, int dsize) {
  u64 data = 0;
  int a = (int)(address >> 6);

  switch (state.mode) {
  case MODE_AUTOSEL:
    switch (a) {
    case 0:
      data = 1; // manufacturer
      break;
    case 1:
      data = 0xad; // device
      break;
    default:
      data = 0;
    }
    break;

  case MODE_CONFIRM_0:
    data = 0x80;
    state.mode = MODE_READ;
    break;

  case MODE_CONFIRM_1:
    data = 0x80;
    state.mode = MODE_CONFIRM_0;
    break;

  default:
    data = state.Flash[a];
  }

  return data;
}

/**
 * Write command or programming data to flash-rom.
 *
 * The state machine for this looks like this:
 * \code
 *         |
 *         v
 *     MODE_READ <---------------------------+
 *         | write 0x5555:0xaa               |
 *         v                                 |
 *     MODE_STEP1 ---------------------------+
 *         | write 0x2aaa:0x55               |
 *         v                                 |
 *     ==MODE_STEP2= ------------------------+
 * 0x80| 0xa0| 0x90| write 0x5555            |
 *     |     |     v                         |
 *     |     | MODE_AUTOSEL (read device id)-+
 *     |     v                               |
 *     | MODE_PROGRAM                        |
 *     |     | write data byte               |
 *     |     +-------------------------------+
 *     v                                     |
 *  MODE_ERASE_STEP3 ------------------------+
 *     | write 0x5555:0xaa                   |
 *     v                                     |
 *  MODE_ERASE_STEP4 ------------------------+
 *     | write 0x2aaa:0x55                   |
 *     v                                     |
 *  MODE_ERASE_STEP4 ------------------------+
 *   | write 0x30  | write 0x5555:0x10       |
 *   | anywhere    v                         |
 *   v           ERASE ENTIRE FLASH          |
 * ERASE BLOCK     |                         |
 *       |         |                         |
 *       v         v                         |
 *      MODE_CONFIRM1                        |
 *          | read 0x80                      |
 *          v                                |
 *      MODE_CONFIRM2                        |
 *          | read 0x80                      |
 *          +--------------------------------+
 * \endcode
 **/
void CFlash::WriteMem(int index, u64 address, int dsize, u64 data) {
  int a = (int)(address >> 6);

  switch (state.mode) {
  case MODE_READ:
  case MODE_AUTOSEL:
    if ((a == 0x5555) && (data == 0xaa)) {
      state.mode = MODE_STEP1;
      return;
    }

    state.mode = MODE_READ;
    return;

  case MODE_STEP1:
    if ((a == 0x2aaa) && (data == 0x55)) {
      state.mode = MODE_STEP2;
      return;
    }

    state.mode = MODE_READ;
    return;

  case MODE_STEP2:
    if (a != 0x5555) {
      state.mode = MODE_READ;
      return;
    }

    switch (data) {
    case 0x90:
      state.mode = MODE_AUTOSEL;
      return;
    case 0xa0:
      state.mode = MODE_PROGRAM;
      return;
    case 0x80:
      state.mode = MODE_ERASE_STEP3;
      return;
    }

    state.mode = MODE_READ;
    return;

  case MODE_ERASE_STEP3:
    if ((a == 0x5555) && (data == 0xaa)) {
      state.mode = MODE_ERASE_STEP4;
      return;
    }

    state.mode = MODE_READ;
    return;

  case MODE_ERASE_STEP4:
    if ((a == 0x2aaa) && (data == 0x55)) {
      state.mode = MODE_ERASE_STEP5;
      return;
    }

    state.mode = MODE_READ;
    return;

  case MODE_ERASE_STEP5:
    if ((a == 0x5555) && (data == 0x10)) {
      memset(state.Flash, 0xff, 1 << 21);
      state.mode = MODE_CONFIRM_1;
      return;
    }

    if (data == 0x30) {
      memset(&state.Flash[(a >> 16) << 16], 0xff, 1 << 16);
      state.mode = MODE_CONFIRM_1;
      return;
    }

    state.mode = MODE_READ;
    return;
  }

  // we must now be in mode program...
  state.Flash[a] = (u8)data;
  state.mode = MODE_READ;
}

/**
 * Save state to a flash rom file.
 **/
void CFlash::SaveStateF(char *fn) {
  FILE *ff;
  ff = fopen(fn, "wb");
  if (ff) {
    SaveState(ff);
    fclose(ff);
    printf("%%FLS-I-SAVEST: Flash state saved to %s\n", fn);
  } else {
    printf("%%FLS-F-NOSAVE: Flash could not be saved to %s\n", fn);
  }
}

/**
 * Save state to the default flash rom file.
 **/
void CFlash::SaveStateF() {
  SaveStateF(myCfg->get_text_value("rom.flash", "flash.rom"));
}

/**
 * Restore state from a flash rom file.
 **/
void CFlash::RestoreStateF(char *fn) {
  FILE *ff;
  ff = fopen(fn, "rb");
  if (ff) {
    RestoreState(ff);
    fclose(ff);
    printf("%%FLS-I-RESTST: Flash state restored from %s\n", fn);
  } else {
    printf("%%FLS-F-NOREST: Flash could not be restored from %s\n", fn);
  }
}

/**
 * Restore state from the default flash rom file.
 **/
void CFlash::RestoreStateF() {
  RestoreStateF(myCfg->get_text_value("rom.flash", "flash.rom"));
}

static u32 flash_magic1 = 0xFF3E3FF3;
static u32 flash_magic2 = 0x3FF3E3FF;

/**
 * Save state to a Virtual Machine State file.
 **/
int CFlash::SaveState(FILE *f) {
  long ss = sizeof(state);
  fwrite(&flash_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&flash_magic2, sizeof(u32), 1, f);
  printf("flash: %ld bytes saved.\n", ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CFlash::RestoreState(FILE *f) {
  long ss;
  u32 m1;
  u32 m2;
  size_t r;

  r = fread(&m1, sizeof(u32), 1, f);
  if (r != 1) {
    printf("flash: unexpected end of file!\n");
    return -1;
  }

  if (m1 != flash_magic1) {
    printf("flash: MAGIC 1 does not match!\n");
    return -1;
  }

  r = fread(&ss, sizeof(long), 1, f);
  if (r != 1) {
    printf("flash: unexpected end of file!\n");
    return -1;
  }

  if (ss != sizeof(state)) {
    printf("flash: STRUCT SIZE does not match!\n");
    return -1;
  }

  r = fread(&state, sizeof(state), 1, f);
  if (r != 1) {
    printf("flash: unexpected end of file!\n");
    return -1;
  }

  r = fread(&m2, sizeof(u32), 1, f);
  if (r != 1) {
    printf("flash: unexpected end of file!\n");
    return -1;
  }

  if (m2 != flash_magic2) {
    printf("flash: MAGIC 1 does not match!\n");
    return -1;
  }

  printf("flash: %ld bytes restored.\n", ss);
  return 0;
}

CFlash *theSROM = 0;
