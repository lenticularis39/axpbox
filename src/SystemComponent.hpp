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

#if !defined(INCLUDED_SYSTEMCOMPONENT_H)
#define INCLUDED_SYSTEMCOMPONENT_H

#include "Configurator.hpp"

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
