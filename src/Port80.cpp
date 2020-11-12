/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * WWW    : http://sourceforge.net/projects/es40
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
 * Contains the code for the emulated Port 80 device.
 *
 * $Id: Port80.cpp,v 1.12 2008/03/14 15:30:51 iamcamiel Exp $
 *
 * X-1.11       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.10       Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.9        Camiel Vanderhoeven                             10-DEC-2007
 *      Use configurator.
 *
 * X-1.8        Camiel Vanderhoeven                             31-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.7	Brian Wheeler					13-FEB-2007
 *	Formatting.
 *
 * X-1.6 	Camiel Vanderhoeven				12-FEB-2007
 *	Added comments.
 *
 * X-1.5        Camiel Vanderhoeven                             9-FEB-2007
 *      Added comments.
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
 * \author Camiel Vanderhoeven (camiel@camicom.com / www.camicom.com)
 **/
#include "Port80.h"
#include "StdAfx.h"
#include "System.h"

/**
 * Constructor.
 **/
CPort80::CPort80(CConfigurator *cfg, CSystem *c) : CSystemComponent(cfg, c) {
  c->RegisterMemory(this, 0, U64(0x00000801fc000080), 1);
  state.p80 = 0;
}

/**
 * Destructor.
 **/
CPort80::~CPort80() {}

/**
 * Read from port 80.
 * Returns the value last written to port 80.
 **/
u64 CPort80::ReadMem(int index, u64 address, int dsize) { return state.p80; }

/**
 * Write to port 80.
 **/
void CPort80::WriteMem(int index, u64 address, int dsize, u64 data) {
  state.p80 = (u8)data;
}

static u32 p80_magic1 = 0x80FFAA80;
static u32 p80_magic2 = 0xAA8080FF;

/**
 * Save state to a Virtual Machine State file.
 **/
int CPort80::SaveState(FILE *f) {
  long ss = sizeof(state);

  fwrite(&p80_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&p80_magic2, sizeof(u32), 1, f);
  printf("%s: %ld bytes saved.\n", devid_string, ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CPort80::RestoreState(FILE *f) {
  long ss;
  u32 m1;
  u32 m2;
  size_t r;

  r = fread(&m1, sizeof(u32), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (m1 != p80_magic1) {
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

  if (m2 != p80_magic2) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  printf("%s: %ld bytes restored.\n", devid_string, ss);
  return 0;
}
