/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * WWW    : http://es40.org
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
 * Contains definitions for the disk controller base class.
 *
 * $Id: DiskController.cpp,v 1.15 2008/04/29 08:03:21 iamcamiel Exp $
 *
 * X-1.15       Camiel Vanderhoeven                             29-APR-2008
 *      CDiskController is no longer a CPCIDevice. devices that are both
 *      should multiple inherit both.
 *
 * X-1.14       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.13       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.12       Camiel Vanderhoeven                             14-JAN-2008
 *      Removed unreferenced variable.
 *
 * X-1.11       Brian Wheeler                                   13-JAN-2008
 *      Avoid deleting Disk devices twice.
 *
 * X-1.9        Camiel Vanderhoeven                             12-JAN-2008
 *      Made register_disk void and virtual.
 *
 * X-1.8        Camiel Vanderhoeven                             29-DEC-2007
 *      Fix memory-leak.
 *
 * X-1.7        Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.6        Camiel Vanderhoeven                             18-DEC-2007
 *      Initialize pointers to 0 in constructor. (doh!)
 *
 * X-1.5        Camiel Vanderhoeven                             17-DEC-2007
 *      Removed excessive whitespace.
 *
 * X-1.4        Brian Wheeler                                   16-DEC-2007
 *      Added newline at end of file.
 *
 * X-1.3        Camiel Vanderhoeven                             16-DEC-2007
 *      Include Disk.h, so children's destructors can be called.
 *
 * X-1.2        Camiel Vanderhoeven                             14-DEC-2007
 *      Delete children upon destruction.
 *
 * X-1.1        Camiel Vanderhoeven                             12-DEC-2007
 *      Initial version in CVS.
 **/
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
