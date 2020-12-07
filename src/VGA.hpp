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
 * Contains the definitions for the VGA base class.
 *
 * $Id: VGA.h,v 1.5 2008/03/14 15:30:52 iamcamiel Exp $
 *
 * X-1.4        Camiel Vanderhoeven                             20-JAN-2008
 *      Added X11 GUI.
 *
 * X-1.3        Camiel Vanderhoeven                             02-JAN-2008
 *      Comments.
 *
 * X-1.2        Brian Wheeler                                   10-DEC-2007
 *      Made include's case-correct.
 *
 * X-1.1        Camiel Vanderhoeven                             10-DEC-2007
 *      Initial version in CVS.
 **/
#if !defined(__VGA_H__)
#define __VGA_H__

#include "PCIDevice.hpp"

/**
 * \brief Abstract base class for PCI VGA cards.
 **/
class CVGA : public CPCIDevice {
public:
  CVGA(class CConfigurator *cfg, class CSystem *c, int pcibus, int pcidev);
  ~CVGA(void);

  virtual u8 get_actl_palette_idx(u8 index) = 0;
  virtual void redraw_area(unsigned x0, unsigned y0, unsigned width,
                           unsigned height) = 0;
};

extern CVGA *theVGA;
#endif
