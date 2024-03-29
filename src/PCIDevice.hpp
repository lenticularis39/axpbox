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

#if !defined(__PCIDEVICE_H__)
#define __PCIDEVICE_H__

#define MAX_DEV_RANGES 50

#define PCI_RANGE_BASE 0x0800

#include "SystemComponent.hpp"

/**
 * \brief Abstract base class for devices on the PCI-bus.
 **/
class CPCIDevice : public CSystemComponent {
public:
  CPCIDevice(class CConfigurator *cfg, class CSystem *c, int pcibus,
             int pcidev);
  ~CPCIDevice(void);
  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);
  virtual void ResetPCI();
  virtual u64 ReadMem(int index, u64 address, int dsize);
  virtual void WriteMem(int index, u64 address, int dsize, u64 data);

  virtual u32 ReadMem_Legacy(int index, u32 address, int dsize);
  virtual void WriteMem_Legacy(int index, u32 address, int dsize, u32 data);

  virtual u32 ReadMem_Bar(int func, int bar, u32 address, int dsize);
  virtual void WriteMem_Bar(int func, int bar, u32 address, int dsize,
                            u32 data);

  u32 config_read(int func, u32 address, int dsize);
  void config_write(int func, u32 address, int dsize, u32 data);

  virtual u32 config_read_custom(int func, u32 address, int dsize, u32 data) {
    return data;
  };
  virtual void config_write_custom(int func, u32 address, int dsize,
                                   u32 old_data, u32 new_data, u32 data){};
  void register_bar(int func, int bar, u32 data, u32 mask);

  void do_pci_read(u32 address, void *dest, size_t element_size,
                   size_t element_count);
  void do_pci_write(u32 address, void *source, size_t element_size,
                    size_t element_count);

protected:
  bool do_pci_interrupt(int func, bool asserted);
  void add_function(int func, u32 data[64], u32 mask[64]);
  void add_legacy_io(int id, u32 base, u32 length);
  void add_legacy_mem(int id, u32 base, u32 length);

  int myPCIBus;
  int myPCIDev;

  u32 std_config_data[8][64];
  u32 std_config_mask[8][64];
  bool device_at[8];

  bool dev_range_is_io[MAX_DEV_RANGES];
  bool pci_range_is_io[8][8];

  /// The PCI state structure contains all elements that need to be saved to the
  /// statefile.
  struct SPCI_state {
    u32 config_data[8][64];
    u32 config_mask[8][64];
  } pci_state;
};
#endif //! defined(__PCIDEVICE_H__)
