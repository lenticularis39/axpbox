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
 * Contains definitions for the base class for devices that connect to the
 *chipset.
 *
 * $Id: SystemComponent.h,v 1.17 2008/03/14 15:30:52 iamcamiel Exp $
 *
 * X-1.16       Camiel Vanderhoeven                             13-MAR-2008
 *      Create init(), start_threads() and stop_threads() functions.
 *
 * X-1.15       Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.14       Camiel Vanderhoeven                             08-FEB-2008
 *      Show originating device name on memory errors.
 *
 * X-1.13       Camiel Vanderhoeven                             19-JAN-2008
 *      Run CPU in a separate thread if CPU_THREADS is defined.
 *      NOTA BENE: This is very experimental, and has several problems.
 *
 * X-1.12       Camiel Vanderhoeven                             02-JAN-2008
 *      Comments.
 *
 * X-1.11       Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.10       Camiel Vanderhoeven                             10-DEC-2007
 *      Use configurator.
 *
 * X-1.9        Camiel Vanderhoeven                             16-APR-2007
 *      Added ResetPCI()
 *
 * X-1.8        Camiel Vanderhoeven                             30-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.7        Camiel Vanderhoeven                             16-FEB-2007
 *      DoClock returns 0.
 *
 * X-1.6        Brian Wheeler                                   13-FEB-2007
 *      Formatting.
 *
 * X-1.5        Camiel Vanderhoeven                             12-FEB-2007
 *      Member cSystem is protected now.
 *
 * X-1.4        Camiel Vanderhoeven                             12-FEB-2007
 *      Added comments.
 *
 * X-1.3        Camiel Vanderhoeven                             9-FEB-2007
 *      Added comments.
 *
 * X-1.2        Brian Wheeler                                   3-FEB-2007
 *      Formatting.
 *
 * X-1.1        Camiel Vanderhoeven                             19-JAN-2007
 *      Initial version in CVS.
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#if !defined(INCLUDED_SYSTEMCOMPONENT_H)
#define INCLUDED_SYSTEMCOMPONENT_H

#include "Configurator.h"

/**
 * \brief Abstract base class for devices that connect to the Typhoon chipset.
 **/
class CSystemComponent {
public:
  virtual int RestoreState(FILE *f) = 0;
  virtual int SaveState(FILE *f) = 0;

  CSystemComponent(class CConfigurator *cfg, class CSystem *system);
  virtual ~CSystemComponent();

  //=== abstract ===
  virtual u64 ReadMem(int index, u64 address, int dsize) { return 0; };
  virtual void WriteMem(int index, u64 address, int dsize, u64 data){};
  virtual void check_state(){};
  virtual void ResetPCI(){};
  virtual void init(){};
  virtual void start_threads(){};
  virtual void stop_threads(){};

  char *devid_string;

protected:
  class CSystem *cSystem;
  class CConfigurator *myCfg;
};
#endif // !defined(INCLUDED_SYSTEMCOMPONENT_H)
