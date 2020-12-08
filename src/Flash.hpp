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
 * Contains the definitions for the emulated Flash ROM devices.
 *
 * $Id: Flash.h,v 1.12 2008/03/14 15:30:51 iamcamiel Exp $
 *
 * X-1.11       Camiel Vanderhoeven                             02-JAN-2008
 *      Comments.
 *
 * X-1.10        Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.9        Camiel Vanderhoeven                             10-DEC-2007
 *      Changes to make the TraceEngine work again after recent changes.
 *
 * X-1.8        Camiel Vanderhoeven                             10-DEC-2007
 *      Use configurator.
 *
 * X-1.7        Camiel Vanderhoeven                             30-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.6        Camiel Vanderhoeven                             16-FEB-2007
 *      Added SaveStateF and RestoreStateF functions.
 *
 * X-1.5        Camiel Vanderhoeven                             12-FEB-2007
 *      Formatting.
 *
 * X-1.4        Camiel Vanderhoeven                             12-FEB-2007
 *      Added comments.
 *
 * X-1.3        Camiel Vanderhoeven                             7-FEB-2007
 *      Added comments.
 *
 * X-1.2        Brian Wheeler                                   3-FEB-2007
 *      Formatting.
 *
 * X-1.1        Camiel Vanderhoeven                             19-JAN-2007
 *      Initial version in CVS.
 *
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#if !defined(INCLUDED_FLASH_H)
#define INCLUDED_FLASH_H

#include "SystemComponent.hpp"

/**
 * \brief Emulated flash memory.
 *
 * Flash memory is only used for storing configuration data (such as SRM console
 *variables), it is not used for firmware.
 **/
class CFlash : public CSystemComponent {
public:
  virtual void WriteMem(int index, u64 address, int dsize, u64 data);
  virtual u64 ReadMem(int index, u64 address, int dsize);
  CFlash(CConfigurator *cfg, class CSystem *c);
  virtual ~CFlash();
  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);
  void SaveStateF();
  void RestoreStateF();
  void SaveStateF(char *fn);
  void RestoreStateF(char *fn);

protected:
  /// The state structure contains all elements that need to be saved to the
  /// statefile.
  struct SFlash_state {
    u8 Flash[2 * 1024 * 1024];
    int mode;
  } state;
};

extern CFlash *theSROM;
#endif // !defined(INCLUDED_FLASH_H)
