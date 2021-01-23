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
 * Contains code to use a raw device as a disk image.
 *
 * $Id: DiskDevice.cpp,v 1.8 2008/03/14 15:30:51 iamcamiel Exp $
 *
 * X-1.7        Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.6        Brian Wheeler                                   27-FEB-2008
 *      Avoid compiler warnings.
 *
 * X-1.5        Camiel Vanderhoeven                             13-JAN-2008
 *      Use determine_layout in stead of calc_cylinders.
 *
 * X-1.4        Camiel Vanderhoeven                             09-JAN-2008
 *      Save disk state to state file.
 *
 * X-1.3        Camiel Vanderhoeven                             06-JAN-2008
 *      Set default blocksize to 2048 for cd-rom devices.
 *
 * X-1.2        Camiel Vanderhoeven                             06-JAN-2008
 *      Support changing the block size (required for SCSI, ATAPI).
 *
 * X-1.1        Camiel Vanderhoeven                             05-JAN-2008
 *      Initial version in CVS.
 **/
#include "DiskDevice.hpp"
#include "StdAfx.hpp"

#if defined(_WIN32)
#include <WinIoCtl.h>
#endif
CDiskDevice::CDiskDevice(CConfigurator *cfg, CSystem *sys, CDiskController *c,
                         int idebus, int idedev)
    : CDisk(cfg, sys, c, idebus, idedev) {
  filename = myCfg->get_text_value("device");
  if (!filename) {
    FAILURE_1(Configuration, "%s: Disk has no device attached!\n",
              devid_string);
  }

  if (read_only) {
#if defined(_WIN32)
    buffer = (char *)malloc(2048);
    buffer_size = 2048;
    handle =
        CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
#else
    handle = fopen(filename, "rb");
#endif
  } else {
#if defined(_WIN32)
    handle = CreateFile(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                        NULL, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
#else
    handle = fopen(filename, "rb+");
#endif
  }

#if defined(_WIN32)
  if (handle == INVALID_HANDLE_VALUE) {
    FAILURE_3(Runtime, "%s: Could not open device %s. Error %ld.", devid_string,
              filename, GetLastError());
  }

#else
  if (!handle) {
    FAILURE_2(Runtime, "%s: Could not open device %s.", devid_string, filename);
  }
#endif

  // determine size...
#if defined(_WIN32)
  DISK_GEOMETRY x;
  DWORD bytesret;

  if (!DeviceIoControl(handle, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &x,
                       sizeof(x), &bytesret, NULL)) {
    FAILURE_3(Runtime, "%s: Could not get drive geometry for %s. Error %ld.",
              devid_string, filename, GetLastError());
  }

  sectors = x.SectorsPerTrack;
  heads = x.TracksPerCylinder;
  byte_size = x.Cylinders.QuadPart * x.TracksPerCylinder * x.SectorsPerTrack *
              x.BytesPerSector;
  dev_block_size = x.BytesPerSector;

  LARGE_INTEGER a;
  a.QuadPart = 0;
  SetFilePointerEx(handle, a, (PLARGE_INTEGER)&state.byte_pos, FILE_BEGIN);
#else
  fseek_large(handle, 0, SEEK_END);
  byte_size = ftell_large(handle);
  fseek_large(handle, 0, SEEK_SET);
  state.byte_pos = ftell_large(handle);

  sectors = 32;
  heads = 8;
#endif

  // calc_cylinders();
  determine_layout();

  model_number = myCfg->get_text_value("model_number", filename);

  printf("%s: Mounted device %s, %td %zd-byte blocks, %td/%ld/%ld.\n",
         devid_string, filename, byte_size / state.block_size, state.block_size,
         cylinders, heads, sectors);
}

CDiskDevice::~CDiskDevice(void) {
  printf("%s: Closing file.\n", devid_string);
#if defined(_WIN32)
  if (handle != INVALID_HANDLE_VALUE)
    CloseHandle(handle);
#else
  if (handle)
    fclose(handle);
#endif
}

bool CDiskDevice::seek_byte(off_t_large byte) {
  if (byte >= byte_size) {
    FAILURE_1(InvalidArgument, "%s: Seek beyond end of file!\n", devid_string);
  }

#if defined(_WIN32)
  state.byte_pos = byte;
#else
  fseek_large(handle, byte, SEEK_SET);
  state.byte_pos = ftell_large(handle);
#endif
  return true;
}

size_t CDiskDevice::read_bytes(void *dest, size_t bytes) {

  //  printf("%s: read %d bytes @ %" LL
  //  "d.\n",devid_string,bytes,state.byte_pos);
#if defined(_WIN32)
  off_t_large byte_from = (state.byte_pos / dev_block_size) * dev_block_size;
  off_t_large byte_to =
      (((state.byte_pos + bytes - 1) / dev_block_size) + 1) * dev_block_size;
  DWORD byte_len = (DWORD)(byte_to - byte_from);
  DWORD byte_off = (DWORD)(state.byte_pos - byte_from);
  LARGE_INTEGER a;
  DWORD r;

  if (byte_len > buffer_size) {
    buffer_size = byte_len;
    CHECK_REALLOCATION(buffer, realloc(buffer, buffer_size), char);

    //    printf("%s: buffer enlarged to %d bytes.\n",devid_string,buffer_size);
  }

  a.QuadPart = byte_from;
  SetFilePointerEx(handle, a, NULL, FILE_BEGIN);

  ReadFile(handle, buffer, byte_len, &r, NULL);

  if (r != (byte_len)) {
    printf("%s: Tried to read %d bytes from pos %ld, but could only read %d bytes!\n",
           devid_string, byte_len, byte_from, r);
    printf("%s: Error %ld.\n", devid_string, GetLastError());
  }

  memcpy(dest, buffer + byte_off, bytes);
  state.byte_pos += bytes;
  return bytes;
#else
  size_t r;
  r = fread(dest, 1, bytes, handle);
  state.byte_pos = ftell_large(handle);
  return r;
#endif
}

size_t CDiskDevice::write_bytes(void *src, size_t bytes) {
  if (read_only)
    return 0;

#if defined(_WIN32)
  off_t_large byte_from = (state.byte_pos / dev_block_size) * dev_block_size;
  off_t_large byte_to =
      (((state.byte_pos + bytes - 1) / dev_block_size) + 1) * dev_block_size;
  DWORD byte_len = (DWORD)(byte_to - byte_from);
  DWORD byte_off = (DWORD)(state.byte_pos - byte_from);
  LARGE_INTEGER a;
  DWORD r;

  if (byte_len > buffer_size) {
    buffer_size = byte_len;
    CHECK_REALLOCATION(buffer, realloc(buffer, buffer_size), char);
  }

  if (byte_from != state.byte_pos) {

    // we don't write the entire first block, so we read it
    // from disk first so we don't corrupt the disk
    a.QuadPart = byte_from;
    SetFilePointerEx(handle, a, NULL, FILE_BEGIN);
    ReadFile(handle, buffer, (DWORD)dev_block_size, &r, NULL);
    if (r != (dev_block_size)) {
      printf("%s: Tried to read %d bytes from pos %ld, but could only read %zd bytes!\n",
             devid_string, dev_block_size, byte_from, r);
      FAILURE(InvalidArgument, "Error during device write operation. "
                               "Terminating to avoid disk corruption.");
    }
  }

  if ((byte_to != state.byte_pos + bytes) &&
      (byte_to - byte_from > dev_block_size)) {

    // we don't write the entire last block, so we read it
    // from disk first so we don't corrupt the disk
    a.QuadPart = byte_to - dev_block_size;
    SetFilePointerEx(handle, a, NULL, FILE_BEGIN);
    ReadFile(handle, buffer + byte_len - dev_block_size, (DWORD)dev_block_size,
             &r, NULL);
    if (r != (dev_block_size)) {
      printf("%s: Tried to read %d bytes from pos %ld, but could only read %zd bytes!\n",
             devid_string, dev_block_size, byte_to - dev_block_size, r);
      FAILURE(InvalidArgument, "Error during device write operation. "
                               "Terminating to avoid disk corruption.");
    }
  }

  // add the data we're writing to the buffer
  memcpy(buffer + byte_off, src, bytes);

  a.QuadPart = byte_from;
  SetFilePointerEx(handle, a, NULL, FILE_BEGIN);

  // and write the buffer to disk
  WriteFile(handle, buffer, byte_len, &r, NULL);

  if (r != byte_len) {
    printf("%s: Tried to write %d bytes to pos %ld, but could only write %d bytes!\n",
           devid_string, byte_len, byte_from, r);
    FAILURE(InvalidArgument, "Error during device write operation. Terminating "
                             "to avoid disk corruption.");
  }

  state.byte_pos += bytes;
  return bytes;
#else
  size_t r;
  r = fwrite(src, 1, bytes, handle);
  state.byte_pos = ftell_large(handle);
  return r;
#endif
}
