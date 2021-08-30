/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Website: https://github.com/lenticularis39/axpbox
 *
 * Forked from: ES40 emulator
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 * Copyright (C) 2007 by Camiel Vanderhoeven
 *
 *  This file is based upon Bochs.
 *
 *  Copyright (C) 2002  MandrakeSoft S.A.
 *
 *    MandrakeSoft S.A.
 *    43, rue d'Aboukir
 *    75002 Paris - France
 *    http://www.linux-mandrake.com/
 *    http://www.mandrakesoft.com/
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include "../Configurator.hpp"

// In case of unknown symbol
#define BX_KEYMAP_UNKNOWN 0xFFFFFFFF

/// Structure of an element of the keymap table
typedef struct {
  u32 baseKey; // base key
  u32 modKey;  // modifier key that must be held down
  s32 ascii;   // ascii equivalent, if any
  u32 hostKey; // value that the host's OS or library recognizes
} BXKeyEntry;

/**
 * \brief Keymap, used to map host keys to scancodes.
 **/
class bx_keymap_c {
public:
  bx_keymap_c(CConfigurator *cfg);
  ~bx_keymap_c(void);

  void loadKeymap(u32 stringToSymbol(const char *));
  void loadKeymap(u32 stringToSymbol(const char *), const char *filename);
  bool isKeymapLoaded();

  BXKeyEntry *findHostKey(u32 hostkeynum);
  BXKeyEntry *findAsciiChar(u8 ascii);
  const char *getBXKeyName(u32 key);

private:
  u32 convertStringToBXKey(const char *);
  CConfigurator *myCfg;

  BXKeyEntry *keymapTable;
  u16 keymapCount;
};

extern bx_keymap_c *bx_keymap;
