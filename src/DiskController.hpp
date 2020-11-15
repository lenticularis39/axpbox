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
 * $Id: DiskController.h,v 1.5 2008/04/29 08:03:21 iamcamiel Exp $
 *
 * X-1.5        Camiel Vanderhoeven                             29-APR-2008
 *      CDiskController is no longer a CPCIDevice. devices that are both
 *      should multiple inherit both.
 *
 * X-1.4        Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.3        Camiel Vanderhoeven                             12-JAN-2008
 *      Made register_disk void and virtual.
 *
 * X-1.2        Camiel Vanderhoeven                             02-JAN-2008
 *      Comments.
 *
 * X-1.1        Camiel Vanderhoeven                             12-DEC-2007
 *      Initial version in CVS.
 **/
#if !defined(__DISKCONTROLLER_H__)
#define __DISKCONTROLLER_H__

/**
 * \brief Abstract base class for disk controllers (uses CDisk's)
 **/
class CDiskController {
public:
  CDiskController(int num_busses, int num_devs);
  ~CDiskController(void);

  virtual void register_disk(class CDisk *dsk, int bus, int dev);
  class CDisk *get_disk(int bus, int dev);

private:
  int num_bus;
  int num_dev;

  class CDisk **disks;
};
#endif //! defined(__DISKCONTROLLER_H__)
