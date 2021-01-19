/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Copyright (C) 2020 Remy van Elst
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

#include "DiskFile.hpp"
#include "StdAfx.hpp"
#include <fstream>
#include <iostream>

CDiskFile::CDiskFile(CConfigurator *cfg, CSystem *sys, CDiskController *c,
                     int idebus, int idedev)
    : CDisk(cfg, sys, c, idebus, idedev) {

  /* If the filename exists in the config, use that,
   * otherwise, use a default filename */
  filename = myCfg->get_text_value("file");
  if (!filename) {
    defaultFilename = std::string(devid_string) + ".default.img";
    std::cerr << devid_string
              << ": Disk has no filename attached! Assuming default: "
              << defaultFilename << std::endl;
    filename = const_cast<char *>(defaultFilename.c_str());
  }

  /* if the file does not exist, create it. ifs actually checks for
   * accessibility, not for existence, but for compatibility with
   * platforms without c++17/boost and std::filesystem::exists, this
   * is good enough. ifstream.good() returns false if the file does
   * not exist.*/
  std::ifstream ifs(filename, std::ios::binary);
  if (!ifs.good()) {
    std::cerr << devid_string << ": file does not exist: " << filename
              << std::endl;

    /* If the disk file size was not set and the disk file does not exist, do
     * not create it, but exit */
    u64 diskFileSize = myCfg->get_num_value("autocreate_size", false, 0);
    if (!diskFileSize) {
      FAILURE_1(Runtime, "%s: file does not exist and no autocreate_size set.",
                devid_string);
    }

    /* Don't create disk file if it's supposed to be a cdrom */
    if (is_cdrom) {
      FAILURE_1(Runtime, "%s: file does not exist and is configured as cdrom.",
                devid_string);
    }

    createDiskFile(filename, diskFileSize);
  }

  if (!read_only) {
    /* If the file is not configured as readonly, test if we can
     * write to it by opening it in append mode. Just using 'out'
     * would overwrite the contents of the file, which is
     * unwanted. Issue #29. */
    checkFileWritable(filename);
  }

  handle = fopen64(filename, read_only ? "rb" : "rb+");

  // determine size...
  fseek_large(handle, 0, SEEK_END);
  byte_size = ftell_large(handle);
  fseek_large(handle, 0, SEEK_SET);
  state.byte_pos = ftell_large(handle);

  sectors = 32;
  heads = 8;

  // calc_cylinders();
  determine_layout();

  model_number = myCfg->get_text_value("model_number", filename);

  // skip to the filename portion of the path.
  char *p = model_number;
#if defined(_WIN32)
  char x = '\\';
#elif defined(__VMS)
  char x = ']';
#else
  char x = '/';
#endif
  while (*p) {
    if (*p == x)
      model_number = p + 1;
    p++;
  }

  printf("%s: Mounted file %s, %" PRId64 " %zd-byte blocks, %" PRId64 "/%ld/%ld.\n",
         devid_string, filename, byte_size / state.block_size, state.block_size,
         cylinders, heads, sectors);
}

void CDiskFile::checkFileWritable(const std::string& fileName) const {
  std::ofstream ofs(fileName, std::ios::app);
  // is_open for an ofstream checks if the file can be written to
  bool isWritable = (ofs.is_open() && ofs.good());
  if (!isWritable) {
    FAILURE_2(Runtime, "%s: file %s is not writable", devid_string, fileName.c_str())
  }
  ofs.close();
}

void CDiskFile::createDiskFile(const std::string &fileName, u64 diskFileSize) {

  std::ofstream ofs(fileName, std::ios::binary);
  if (ofs.is_open() && ofs.good()) {
    ofs.seekp((diskFileSize)-1);
    ofs.write("", 1);
  } else {
    FAILURE_1(Runtime, "%s: File does not exist and could not be created",
              devid_string);
  }

  std::cout << devid_string << " " << (diskFileSize / 1024 / 1024) << "MB file "
            << filename << " created" << std::endl;
}

CDiskFile::~CDiskFile(void) {
  printf("%s: Closing file.\n", devid_string);
  fclose(handle);
}

bool CDiskFile::seek_byte(off_t_large byte) {
  if (byte >= byte_size) {
    FAILURE_1(InvalidArgument, "%s: Seek beyond end of file!\n", devid_string);
  }

  fseek_large(handle, byte, SEEK_SET);
  state.byte_pos = ftell_large(handle);

  return true;
}

size_t CDiskFile::read_bytes(void *dest, size_t bytes) {
  size_t r;
  r = fread(dest, 1, bytes, handle);
  state.byte_pos = ftell_large(handle);
  return r;
}

size_t CDiskFile::write_bytes(void *src, size_t bytes) {
  if (read_only)
    return 0;

  size_t r;
  r = fwrite(src, 1, bytes, handle);
  state.byte_pos = ftell_large(handle);
  return r;
}
