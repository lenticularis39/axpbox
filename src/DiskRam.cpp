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
 * Contains code to use a RAM disk.
 *
 * $Id: DiskRam.cpp,v 1.18 2008/03/14 15:30:51 iamcamiel Exp $
 *
 * X-1.17       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.16       Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.15       Camiel Vanderhoeven                             02-MAR-2008
 *      Natural way to specify large numeric values ("10M") in the config file.
 *
 * X-1.14       Camiel Vanderhoeven                             13-JAN-2008
 *      Use determine_layout in stead of calc_cylinders.
 *
 * X-1.12       Camiel Vanderhoeven                             09-JAN-2008
 *      Save disk state to state file.
 *
 * X-1.11       Camiel Vanderhoeven                             06-JAN-2008
 *      Set default blocksize to 2048 for cd-rom devices.
 *
 * X-1.10       Camiel Vanderhoeven                             06-JAN-2008
 *      Support changing the block size (required for SCSI, ATAPI).
 *
 * X-1.9        Camiel Vanderhoeven                             04-JAN-2008
 *      64-bit file I/O.
 *
 * X-1.8        Camiel Vanderhoeven                             02-JAN-2008
 *      Cleanup.
 *
 * X-1.7        Camiel Vanderhoeven                             28-DEC-2007
 *      Throw exceptions rather than just exiting when errors occur.
 *
 * X-1.6        Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.5        Camiel Vanderhoeven                             18-DEC-2007
 *      Byte-sized transfers for SCSI controller.
 *
 * X-1.4        Brian wheeler                                   17-DEC-2007
 *      Changed last cylinder number.
 *
 * X-1.3        Brian Wheeler                                   16-DEC-2007
 *      Fixed case of StdAfx.h.
 *
 * X-1.2        Camiel Vanderhoeven                             14-DEC-2007
 *      Free memory malloc'ed at destruction.
 *
 * X-1.1        Camiel Vanderhoeven                             12-DEC-2007
 *      Initial version in CVS.
 **/
#include "DiskRam.hpp"
#include "StdAfx.hpp"

CDiskRam::CDiskRam(CConfigurator *cfg, CSystem *sys, CDiskController *c,
                   int idebus, int idedev)
    : CDisk(cfg, sys, c, idebus, idedev) {
  byte_size = myCfg->get_num_value("size", false, 512 * 1024 * 1024);

  CHECK_ALLOCATION(ramdisk = malloc((size_t)byte_size));

  state.byte_pos = 0;

  sectors = 32;
  heads = 8;

  // calc_cylinders();
  determine_layout();

  model_number = myCfg->get_text_value("model_number", "ES40RAMDISK");

  printf("%s: Mounted RAMDISK, %" PRId64 " %zd-byte blocks, %" PRId64 "/%ld/%ld.\n",
         devid_string, byte_size / state.block_size, state.block_size,
         cylinders, heads, sectors);
}

CDiskRam::~CDiskRam(void) {
  if (ramdisk) {
    printf("%s: RAMDISK freed.\n", devid_string);
    free(ramdisk);
    ramdisk = 0;
  }
}

bool CDiskRam::seek_byte(off_t_large byte) {
  if (byte >= byte_size) {
    FAILURE_1(InvalidArgument, "%s: Seek beyond end of file!\n", devid_string);
  }

  state.byte_pos = byte;
  return true;
}

size_t CDiskRam::read_bytes(void *dest, size_t bytes) {
  if (state.byte_pos >= byte_size)
    return 0;

  while (state.byte_pos + bytes >= byte_size)
    bytes--;

  memcpy(dest, &(((char *)ramdisk)[state.byte_pos]), bytes);
  state.byte_pos += (unsigned long)bytes;
  return bytes;
}

size_t CDiskRam::write_bytes(void *src, size_t bytes) {
  if (state.byte_pos >= byte_size)
    return 0;

  while (state.byte_pos + bytes >= byte_size)
    bytes--;

  memcpy(&(((char *)ramdisk)[state.byte_pos]), src, bytes);
  state.byte_pos += (unsigned long)bytes;
  return bytes;
}
