/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Website: https://github.com/lenticularis39/axpbox
 *
 * Forked from: ES40 emulator
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 * Copyright (C) 2007 by Camiel Vanderhoeven
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

/////////////////////////////////////////////////////////////////////////
//
// This file provides macros and types needed for plugins.  It is based on
// the plugin.h file from plex86, but with significant changes to make
// it work in Bochs.
// Plex86 is Copyright (C) 1999-2000  The plex86 developers team
//
/////////////////////////////////////////////////////////////////////////
#ifndef __PLUGIN_H
#define __PLUGIN_H

#define PLUG_load_plugin(cfg, name)                                            \
  { lib##name##_LTX_plugin_init(cfg); }
#define PLUG_unload_plugin(name)                                               \
  { lib##name##_LTX_plugin_fini(); }

#define DECLARE_PLUGIN_INIT_FINI_FOR_MODULE(mod)                               \
  int lib##mod##_LTX_plugin_init(CConfigurator *cfg);                          \
  void lib##mod##_LTX_plugin_fini(void);

#if defined(HAVE_SDL)
DECLARE_PLUGIN_INIT_FINI_FOR_MODULE(sdl)
#endif
#if defined(_WIN32)
DECLARE_PLUGIN_INIT_FINI_FOR_MODULE(win32)
#endif
#if defined(HAVE_X11)
DECLARE_PLUGIN_INIT_FINI_FOR_MODULE(x11)
#endif
#endif /* __PLUGIN_H */
