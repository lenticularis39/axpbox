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
 * Contains definitions to use a RAM disk.
 *
 * $Id: DiskRam.h,v 1.10 2008/03/14 15:30:51 iamcamiel Exp $
 *
 * X-1.9        Camiel Vanderhoeven                             12-JAN-2008
 *      Avoid compiler warnings.
 *
 * X-1.8        Camiel Vanderhoeven                             09-JAN-2008
 *      Save disk state to state file.
 *
 * X-1.7        Camiel Vanderhoeven                             06-JAN-2008
 *      Support changing the block size (required for SCSI, ATAPI).
 *
 * X-1.6        Camiel Vanderhoeven                             04-JAN-2008
 *      64-bit file I/O.
 *
 * X-1.5        Camiel Vanderhoeven                             02-JAN-2008
 *      Comments.
 *
 * X-1.4        Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.3        Camiel Vanderhoeven                             20-DEC-2007
 *      Close files and free memory when the emulator shuts down.
 *
 * X-1.2        Brian Wheeler                                   16-DEC-2007
 *      Corrected some weird uses of size_t...
 *
 * X-1.1        Camiel Vanderhoeven                             12-DEC-2007
 *      Initial version in CVS.
 **/
#if !defined(__DISKRAM_H__)
#define __DISKRAM_H__

#include "Disk.h"

/**
 * \brief Emulated disk that uses RAM.
 **/
class CDiskRam : public CDisk {
public:
  CDiskRam(CConfigurator *cfg, CSystem *sys, CDiskController *c, int idebus,
           int idedev);
  virtual ~CDiskRam(void);

  virtual bool seek_byte(off_t_large byte);
  virtual size_t read_bytes(void *dest, size_t bytes);
  virtual size_t write_bytes(void *src, size_t bytes);

protected:
  void *ramdisk;
};
#endif //! defined(__DISKFILE_H__)
