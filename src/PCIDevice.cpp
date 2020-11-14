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
 * Contains the code for the PCI device class.
 *
 * $Id: PCIDevice.cpp,v 1.18 2008/04/02 13:28:29 iamcamiel Exp $
 *
 * X-1.17       Camiel Vanderhoeven                             02-APR-2008
 *      Fixed compiler warnings.
 *
 * X-1.16       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.15       Brian Wheeler                                   27-FEB-2008
 *      Avoid compiler warnings.
 *
 * X-1.14       Camiel Vanderhoeven                             13-FEB-2008
 *      Added more DEBUG_PCI messages.
 *
 * X-1.13       Camiel Vanderhoeven                             08-FEB-2008
 *      Show originating device name on memory errors.
 *
 * X-1.12       Camiel Vanderhoeven                             06-FEB-2008
 *      Fixed registration of ROM expansion address.
 *
 * X-1.11       Camiel Vanderhoeven                             24-JAN-2008
 *      Added do_pci_read and do_pci_write. Thanks to David Hittner for
 *      suggesting this.
 *
 * X-1.10       Fang Zhe                                        03-JAN-2008
 *      Fixed semicolon error.
 *
 * X-1.9        Camiel Vanderhoeven                             03-JAN-2008
 *      Attempt to make PCI base device endianess-correct.
 *
 * X-1.8        Camiel Vanderhoeven                             29-DEC-2007
 *      Avoid referencing uninitialized data.
 *
 * X-1.7        Camiel Vanderhoeven                             28-DEC-2007
 *      Throw exceptions rather than just exiting when errors occur.
 *
 * X-1.6        Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.5        Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.4        Camiel Vanderhoeven                             10-DEC-2007
 *      Translate a 64-bit PCI access into 2 32-bit accesses.
 *
 * X-1.3        Brian Wheeler                                   10-DEC-2007
 *      More verbose error reporting.
 *
 * X-1.2        Camiel Vanderhoeven                             10-DEC-2007
 *      Removed some printf's.
 *
 * X-1.1        Camiel Vanderhoeven                             10-DEC-2007
 *      Initial version in CVS.
 **/
#include "PCIDevice.h"
#include "StdAfx.h"
#include "System.h"

CPCIDevice::CPCIDevice(CConfigurator *cfg, CSystem *c, int pcibus, int pcidev)
    : CSystemComponent(cfg, c) {
  int i;

  int j;

  for (i = 0; i < 8; i++) {
    device_at[i] = false;
    for (j = 0; j < 8; j++)
      pci_range_is_io[i][j] = false;
  }

  for (i = 0; i < MAX_DEV_RANGES; i++)
    dev_range_is_io[i] = false;

  myPCIBus = pcibus;
  myPCIDev = pcidev;
}

CPCIDevice::~CPCIDevice(void) {}

void CPCIDevice::add_function(int func, u32 data[64], u32 mask[64]) {
  memcpy(std_config_data[func], data, 64 * sizeof(u32));
  memcpy(std_config_mask[func], mask, 64 * sizeof(u32));
#if defined(ES40_BIG_ENDIAN)
  int i;
  for (i = 0; i < 64; i++) {
    std_config_data[func][i] = endian_32(std_config_data[func][i]);
    std_config_mask[func][i] = endian_32(std_config_mask[func][i]);
  }
#endif
  device_at[func] = true;
}

void CPCIDevice::add_legacy_io(int id, u32 base, u32 length) {
  dev_range_is_io[id] = true;
  cSystem->RegisterMemory(this, id,
                          U64(0x00000801fc000000) +
                              (U64(0x0000000200000000) * myPCIBus) + base,
                          length);
}

void CPCIDevice::add_legacy_mem(int id, u32 base, u32 length) {
  dev_range_is_io[id] = false;
  cSystem->RegisterMemory(this, id,
                          U64(0x0000080000000000) +
                              (U64(0x0000000200000000) * myPCIBus) + base,
                          length);
}

u32 CPCIDevice::config_read(int func, u32 address, int dsize) {
  u8 *x;

  u32 data = 0;

  x = (u8 *)pci_state.config_data[func];
  x += address;

  switch (dsize) {
  case 8:
    data = endian_8(*x);
    break;
  case 16:
    data = endian_16(*((u16 *)x));
    break;
  case 32:
    data = endian_32(*((u32 *)x));
    break;
  }

  data = config_read_custom(func, address, dsize, data);

  //  printf("%s(%s).%d config read  %d bytes @ %x = %x\n",myCfg->get_myName(),
  //  myCfg->get_myValue(), func,dsize/8,address, data);
  return data;
}

void CPCIDevice::config_write(int func, u32 address, int dsize, u32 data) {

  //  printf("%s(%s).%d config write %d bytes @ %x = %x\n",myCfg->get_myName(),
  //  myCfg->get_myValue(), func,dsize/8,address, data);
  u8 *x;
  u8 *y;

  u32 mask = 0;
  u32 old_data = 0;
  u32 new_data = 0;

  x = (u8 *)pci_state.config_data[func];
  x += address;
  y = (u8 *)pci_state.config_mask[func];
  y += address;

#if defined(DEBUG_PCI)
  if (address == 0x3c && (data & 0xff) != 0xff)
    printf("%s.%d PCI Interrupt set to %02x.\n", devid_string, func,
           data & 0xff);
#endif
  switch (dsize) {
  case 8:
    data = endian_8(data);
    old_data = (*x) & 0xff;
    mask = (*y) & 0xff;
    new_data = (old_data & ~mask) | (data & mask);
    *x = (u8)new_data;
    break;

  case 16:
    data = endian_16(data);
    old_data = (*((u16 *)x)) & 0xffff;
    mask = (*((u16 *)y)) & 0xffff;
    new_data = (old_data & ~mask) | (data & mask);
    *((u16 *)x) = (u16)new_data;
    break;

  case 32:
    data = endian_32(data);
    old_data = (*((u32 *)x));
    mask = (*((u32 *)y));
    new_data = (old_data & ~mask) | (data & mask);
    *((u32 *)x) = new_data;
    break;
  }

  if (dsize == 32 && ((data & mask) != mask) && ((data & mask) != 0)) {
    switch (address) {
    case 0x10:
    case 0x14:
    case 0x18:
    case 0x1c:
    case 0x20:
    case 0x24:
      register_bar(func, (address - 0x10) / 4, endian_32(new_data),
                   endian_32(mask));
      break;

    case 0x30:
      register_bar(func, 6, endian_32(new_data), endian_32(mask));
      break;
    }
  }

  config_write_custom(func, address, dsize, old_data, new_data, data);
}

void CPCIDevice::register_bar(int func, int bar, u32 data, u32 mask) {
  u32 length = ((~mask) | 1) + 1;

  if ((data & 1) && bar != 6) {

    // io space
    pci_range_is_io[func][bar] = true;

    cSystem->RegisterMemory(this, PCI_RANGE_BASE + (func * 8) + bar,
                            U64(0x00000801fc000000) +
                                (U64(0x0000000200000000) * myPCIBus) +
                                (data & ~0x3),
                            length);
#if defined(DEBUG_PCI)
    printf("%s(%s).%d PCI BAR %d set to IO  % " LL "x, len %x.\n",
           myCfg->get_myName(), myCfg->get_myValue(), func, bar, t, length);
#endif
  } else if ((data & 1) || bar != 6) {

    // io space
    pci_range_is_io[func][bar] = true;

    cSystem->RegisterMemory(this, PCI_RANGE_BASE + (func * 8) + bar,
                            U64(0x0000080000000000) +
                                (U64(0x0000000200000000) * myPCIBus) +
                                (data & ~0xf),
                            length);
#if defined(DEBUG_PCI)
    printf("%s(%s).%d PCI BAR %d set to MEM % " LL "x, len %x.\n",
           myCfg->get_myName(), myCfg->get_myValue(), func, bar, t, length);
#endif
  } else {

    // disabled...
#if defined(DEBUG_PCI)
    printf("%s(%s).%d PCI BAR %d should be disabled...\n", myCfg->get_myName(),
           myCfg->get_myValue(), func, bar);
#endif
  }
}

void CPCIDevice::ResetPCI() {
  int i;

  for (i = 0; i < 8; i++) {
    if (device_at[i]) {
      cSystem->RegisterMemory(this, PCI_RANGE_BASE + (i * 8) + 7,
                              U64(0x00000801fe000000) +
                                  (U64(0x0000000200000000) * myPCIBus) +
                                  (U64(0x0000000000000800) * myPCIDev) +
                                  (U64(0x0000000000000100) * i),
                              0x100);
      memcpy(pci_state.config_data[i], std_config_data[i], 64 * sizeof(u32));
      memcpy(pci_state.config_mask[i], std_config_mask[i], 64 * sizeof(u32));

      config_write(i, 0x10, 32, endian_32(pci_state.config_data[i][4]));
      config_write(i, 0x14, 32, endian_32(pci_state.config_data[i][5]));
      config_write(i, 0x18, 32, endian_32(pci_state.config_data[i][6]));
      config_write(i, 0x1c, 32, endian_32(pci_state.config_data[i][7]));
      config_write(i, 0x20, 32, endian_32(pci_state.config_data[i][8]));
      config_write(i, 0x24, 32, endian_32(pci_state.config_data[i][9]));
      config_write(i, 0x30, 32, endian_32(pci_state.config_data[i][12]));
    }
  }
}

u64 CPCIDevice::ReadMem(int index, u64 address, int dsize) {
  int func;
  int bar;

  if (dsize == 64)
    return ReadMem(index, address, 32) |
           (((u64)ReadMem(index, address + 4, 32)) << 32);

  if (dsize != 8 && dsize != 16 && dsize != 32) {
    FAILURE_5(InvalidArgument,
              "ReadMem: %s(%s) Unsupported dsize %d. (%d, %" PRIx64 ")\n",
              myCfg->get_myName(), myCfg->get_myValue(), dsize, index, address);
  }

  if (index < PCI_RANGE_BASE) {
    if (dev_range_is_io[index] &&
        !(pci_state.config_data[0][1] & endian_32(1))) {
      printf("%s(%s) Legacy IO access with IO disabled from PCI config.\n",
             myCfg->get_myName(), myCfg->get_myValue());
      return 0;
    }

    if (!dev_range_is_io[index] &&
        !(pci_state.config_data[0][1] & endian_32(2))) {
      printf(
          "%s(%s) Legacy memory access with memory disabled from PCI config.\n",
          myCfg->get_myName(), myCfg->get_myValue());
      return 0;
    }

    //    printf("%s(%s) Calling ReadMem_Legacy(%d).\n",myCfg->get_myName(),
    //    myCfg->get_myValue(), index);
    return ReadMem_Legacy(index, (u32)address, dsize);
  }

  index -= PCI_RANGE_BASE;

  bar = index & 7;
  func = (index / 8) & 7;

  if (bar == 7)
    return config_read(func, (u32)address, dsize);

  if (pci_range_is_io[func][bar] &&
      !(pci_state.config_data[func][1] & endian_32(1))) {
    printf("%s(%s).%d PCI IO access with IO disabled from PCI config.\n",
           myCfg->get_myName(), myCfg->get_myValue(), func);
    return 0;
  }

  if (!pci_range_is_io[func][bar] &&
      !(pci_state.config_data[func][1] & endian_32(2))) {
    printf(
        "%s(%s).%d PCI memory access with memory disabled from PCI config.\n",
        myCfg->get_myName(), myCfg->get_myValue(), func);
    return 0;
  }

  //  printf("%s(%s).%d Calling ReadMem_Bar(%d,%d).\n",myCfg->get_myName(),
  //  myCfg->get_myValue(), func,func,bar);
  return ReadMem_Bar(func, bar, (u32)address, dsize);
}

void CPCIDevice::WriteMem(int index, u64 address, int dsize, u64 data) {
  int func;
  int bar;

  if (dsize == 64) {
    WriteMem(index, address, 32, data & U64(0xffffffff));
    WriteMem(index, address + 4, 32, (data >> 32) & U64(0xffffffff));
    return;
  }

  if (dsize != 8 && dsize != 16 && dsize != 32) {
    FAILURE_6(
        InvalidArgument,
        "WriteMem: %s(%s) Unsupported dsize %d. (%d,%" PRIx64 ",%" PRIx64 ")\n",
        myCfg->get_myName(), myCfg->get_myValue(), dsize, index, address, data);
  }

  if (index < PCI_RANGE_BASE) {
    if (dev_range_is_io[index] &&
        !(pci_state.config_data[0][1] & endian_32(1))) {
      printf("%s(%s) Legacy IO access with IO disabled from PCI config.\n",
             myCfg->get_myName(), myCfg->get_myValue());
      return;
    }

    if (!dev_range_is_io[index] &&
        !(pci_state.config_data[0][1] & endian_32(2))) {
      printf(
          "%s(%s) Legacy memory access with memory disabled from PCI config.\n",
          myCfg->get_myName(), myCfg->get_myValue());
      return;
    }

    WriteMem_Legacy(index, (u32)address, dsize, (u32)data);
    return;
  }

  index -= PCI_RANGE_BASE;

  bar = index & 7;
  func = (index / 8) & 7;

  if (bar == 7) {
    config_write(func, (u32)address, dsize, (u32)data);
    return;
  }

  if (pci_range_is_io[func][bar] &&
      !(pci_state.config_data[func][1] & endian_32(1))) {
    printf("%s(%s).%d PCI IO access with IO disabled from PCI config.\n",
           myCfg->get_myName(), myCfg->get_myValue(), func);
    return;
  }

  if (!pci_range_is_io[func][bar] &&
      !(pci_state.config_data[func][1] & endian_32(2))) {
    printf(
        "%s(%s).%d PCI memory access with memory disabled from PCI config.\n",
        myCfg->get_myName(), myCfg->get_myValue(), func);
    return;
  }

  WriteMem_Bar(func, bar, (u32)address, dsize, (u32)data);
}

bool CPCIDevice::do_pci_interrupt(int func, bool asserted) {
  if ((endian_32(pci_state.config_data[func][0x0f]) & 0xff) != 0xff) {
    cSystem->interrupt(endian_32(pci_state.config_data[func][0x0f]) & 0xff,
                       asserted);
    return true;
  } else
    return false;
}

static u32 pci_magic1 = 0xC1095A78;
static u32 pci_magic2 = 0x87A5901C;

/**
 * Save state to a Virtual Machine State file.
 **/
int CPCIDevice::SaveState(FILE *f) {
  long ss = sizeof(pci_state);

  fwrite(&pci_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&pci_state, sizeof(pci_state), 1, f);
  fwrite(&pci_magic2, sizeof(u32), 1, f);
  printf("%s: %d PCI bytes saved.\n", devid_string, (int)ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CPCIDevice::RestoreState(FILE *f) {
  long ss;
  u32 m1;
  u32 m2;
  size_t r;

  r = fread(&m1, sizeof(u32), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (m1 != pci_magic1) {
    printf("%s: PCI MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  r = fread(&ss, sizeof(long), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (ss != sizeof(pci_state)) {
    printf("%s: PCI STRUCT SIZE does not match!\n", devid_string);
    return -1;
  }

  r = fread(&pci_state, sizeof(pci_state), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  r = fread(&m2, sizeof(u32), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (m2 != pci_magic2) {
    printf("%s: PCI MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  printf("%s: %d PCI bytes restored.\n", devid_string, (int)ss);
  return 0;
}

u32 CPCIDevice::ReadMem_Legacy(int index, u32 address, int dsize) {
  FAILURE_2(NotImplemented, "%s(%s) No Legacy read handler installed",
            myCfg->get_myName(), myCfg->get_myValue());
}

void CPCIDevice::WriteMem_Legacy(int index, u32 address, int dsize, u32 data) {
  FAILURE_2(NotImplemented, "%s(%s) No Legacy write handler installed",
            myCfg->get_myName(), myCfg->get_myValue());
}

u32 CPCIDevice::ReadMem_Bar(int func, int bar, u32 address, int dsize) {
  FAILURE_3(NotImplemented, "%s(%s).%d No BAR read handler installed",
            myCfg->get_myName(), myCfg->get_myValue(), func);
}

void CPCIDevice::WriteMem_Bar(int func, int bar, u32 address, int dsize,
                              u32 data) {
  FAILURE_3(NotImplemented, "%s(%s).%d No BAR write handler installed",
            myCfg->get_myName(), myCfg->get_myValue(), func);
}

/**
 * \brief Read data from the PCI bus.
 *
 * Called by the PCI-device to read data off the PCI bus. address is the
 * 32-bit address put on the PCI bus. element_count elements of element_size
 * bytes each will be read in an endian-aware manner.
 **/
void CPCIDevice::do_pci_read(u32 address, void *dest, size_t element_size,
                             size_t element_count) {
  size_t el;
  char *dst = (char *)dest;

  if (element_count == 0)
    return;

  // get the 64-bit system wide address
  u64 phys_addr = cSystem->PCI_Phys(myPCIBus, address);

  // if there is only one element to read, this is a simple ReadMem operation.
  if (element_count == 1) {
    switch (element_size) {
    case 1:
      *(u8 *)dest = (u8)cSystem->ReadMem(phys_addr, 8, this);
      break;

    case 2:
      *(u16 *)dest = (u16)cSystem->ReadMem(phys_addr, 16, this);
      break;

    case 4:
      *(u32 *)dest = (u32)cSystem->ReadMem(phys_addr, 32, this);
      break;

    default:
      FAILURE(InvalidArgument, "Strange element size");
    }

    return;
  }

#if defined(ES40_BIG_ENDIAN)

  // if this is a big-endian host machine, the memcpy method is only valid
  // if we're transferring bytes. Otherwise, endian-conversions need to be done.
  if (element_size == 1) {
#endif

    // get a pointer to system memory if the address is inside main memory
    char *memptr = cSystem->PtrToMem(phys_addr);

    // if the address is inside system memory, a simple memcpy operation is
    // all that is needed.
    if (memptr) {
      memcpy(dest, memptr, element_size * element_count);
      return;
    }

#if defined(ES40_BIG_ENDIAN)
  }
#endif

  // outside main memory, or inside main memory with endian-conversion
  // required we need to do the transfer element-by-element.
  switch (element_size) {
  case 1:
    for (el = 0; el < element_count; el++) {
      *(u8 *)dst = (u8)cSystem->ReadMem(phys_addr, 8, this);
      dst++;
      phys_addr++;
    }
    break;

  case 2: {
    *(u16 *)dst = endian_16((u16)cSystem->ReadMem(phys_addr, 16, this));
    dst += 2;
    phys_addr += 2;
  } break;

  case 4: {
    *(u32 *)dst = endian_32((u32)cSystem->ReadMem(phys_addr, 32, this));
    dst += 4;
    ;
    phys_addr += 4;
  } break;

  default:
    FAILURE(InvalidArgument, "Strange element size");
  }
}

/**
 * \brief Write data to the PCI bus.
 *
 * Called by the PCI-device to write data to the PCI bus. address is the
 * 32-bit address put on the PCI bus. element_count elements of element_size
 * bytes each will be written in an endian-aware manner.
 **/
void CPCIDevice::do_pci_write(u32 address, void *source, size_t element_size,
                              size_t element_count) {
  size_t el;
  char *src = (char *)source;

  if (element_count == 0)
    return;

  // get the 64-bit system wide address
  u64 phys_addr = cSystem->PCI_Phys(myPCIBus, address);

  // if there is only one element to read, this is a simple ReadMem operation.
  if (element_count == 1) {
    switch (element_size) {
    case 1:
      cSystem->WriteMem(phys_addr, 8, *(u8 *)source, this);
      break;
    case 2:
      cSystem->WriteMem(phys_addr, 16, *(u16 *)source, this);
      break;
    case 4:
      cSystem->WriteMem(phys_addr, 32, *(u32 *)source, this);
      break;
    default:
      FAILURE(InvalidArgument, "Strange element size");
    }

    return;
  }

#if defined(ES40_BIG_ENDIAN)

  // if this is a big-endian host machine, the memcpy method is only valid
  // if we're transferring bytes. Otherwise, endian-conversions need to be done.
  if (element_size == 1) {
#endif

    // get a pointer to system memory if the address is inside main memory
    char *memptr = cSystem->PtrToMem(phys_addr);

    // if the address is inside system memory, a simple memcpy operation is
    // all that is needed.
    if (memptr) {
      memcpy(memptr, source, element_size * element_count);
      return;
    }

#if defined(ES40_BIG_ENDIAN)
  }
#endif

  // outside main memory, or inside main memory with endian-conversion
  // required we need to do the transfer element-by-element.
  switch (element_size) {
  case 1:
    for (el = 0; el < element_count; el++) {
      cSystem->WriteMem(phys_addr, 8, *(u8 *)src, this);
      src++;
      phys_addr++;
    }
    break;

  case 2: {
    cSystem->WriteMem(phys_addr, 16, endian_16(*(u16 *)src), this);
    src += 2;
    phys_addr += 2;
  } break;

  case 4: {
    cSystem->WriteMem(phys_addr, 32, endian_32(*(u32 *)src), this);
    src += 4;
    ;
    phys_addr += 4;
  } break;

  default:
    FAILURE(InvalidArgument, "Strange element size");
  }
}
