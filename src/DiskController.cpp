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

#include "DiskController.hpp"
#include "Disk.hpp"
#include "StdAfx.hpp"

CDiskController::CDiskController(int num_busses, int num_devices) {
  num_bus = num_busses;
  num_dev = num_devices;

  disks = (CDisk **)calloc(num_bus * num_dev, sizeof(CDisk *));
}

CDiskController::~CDiskController(void) { free(disks); }

void CDiskController::register_disk(class CDisk *dsk, int bus, int dev) {
  if (bus >= num_bus)
    FAILURE(Configuration, "Can't register disk: bus number out of range");
  if (dev >= num_dev)
    FAILURE(Configuration, "Can't register disk: device number out of range");

  disks[bus * num_bus + dev] = dsk;
}

class CDisk *CDiskController::get_disk(int bus, int dev) {
  if (bus >= num_bus)
    return 0;
  if (dev >= num_dev)
    return 0;

  return disks[bus * num_bus + dev];
}
