/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * WWW    : http://es40.org
 * E-mail : camiel@es40.org
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
 * Configuration file creator.
 *
 * $Id: es40-cfg.cpp,v 1.8 2008/05/04 11:45:11 iamcamiel Exp $
 *
 * X-1.7        Camiel Vanderhoeven                             29-APR-2008
 *      Added floppy configuration.
 *
 * X-1.6        Camiel Vanderhoeven                             29-MAR-2008
 *      Replaced SDL with sdl.
 *
 * X-1.5        Camiel Vanderhoeven                             29-MAR-2008
 *      Fix VGA console value.
 *
 * X-1.4        Camiel Vanderhoeven                             29-MAR-2008
 *      Fill in NIC section.
 *
 * X-1.3        Camiel Vanderhoeven                             28-MAR-2008
 *      Fixed CD-ROM question behaviour.
 *
 * X-1.2        Camiel Vanderhoeven                             28-MAR-2008
 *      Properly capitalized "StdAfx.h".
 *
 * X-1.1        Camiel Vanderhoeven                             28-MAR-2008
 *      File created.
 **/

#include "StdAfx.h"

// C++ includes
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(HAVE_PCAP)
#include <pcap.h>
#endif

using namespace std;

// Question classes

#include "FreeTextQuestion.h"
#include "MultipleChoiceQuestion.h"
#include "NumberQuestion.h"
#include "Question.h"
#include "ShrinkingChoiceQuestion.h"

/**
 * Add disks for a controller to the configuration file.
 *
 * \param disk_q: A ShrinkingChoiceQuestion that contains
 *                all allowed disk names for this controller,
 *                and a special answer "none" with a value
 *                of "".
 * \param os:     Output stream for the configuration file.
 **/
void add_disks(ShrinkingChoiceQuestion *disk_q, ostream *os) {
  /* Loop until there are no more disks to be added.
   */
  for (;;) {
    /* If disk_q has only one answer left, this is the
     * "none" answer. This controller can contain no
     * more disks.
     */
    if (disk_q->countAnswers() == 1)
      break;

    /* If the answer value is "", the answer "none"
     * was given. Stop adding disks to this controller.
     */
    if (disk_q->ask() == "")
      break;

    /* Find out if this should be using:
     *   - a disk image file
     *   - a raw device
     *   - a RAM DISK
     */
    MultipleChoiceQuestion type_q;
    type_q.setQuestion("How should " + disk_q->getAnswer() + " be emulated?");
    type_q.setDefault("file");
    type_q.setExplanation("Disks can be emulated in several ways.");
    type_q.addAnswer("file", "file",
                     "The disk uses a disk image on the host system's disk.");
    type_q.addAnswer("device", "device",
                     "The disk uses one of the host system's raw disks.");
    type_q.addAnswer("ramdisk", "ramdisk",
                     "The disk stores it's data in RAM. Volatile.");

    *os << "    " << disk_q->getAnswer() << " = " << type_q.ask() << "\n";
    *os << "    {\n";

    if (type_q.getAnswer() == "file" || type_q.getAnswer() == "device") {
      /* For a file or device, we need to know what
       * file or device to use.
       */
      FreeTextQuestion img_q;
      img_q.setQuestion("What " + type_q.getAnswer() + " should " +
                        disk_q->getAnswer() + " use?");
      img_q.setExplanation("Enter the path to the " + type_q.getAnswer() +
                           " to use for this disk.");
      *os << "      " << type_q.getAnswer() << " = \"" << img_q.ask()
          << "\";\n";
    }

    if (type_q.getAnswer() == "file") {
      /* For a file, we need to know whether to create
       * it when it doesn't exist or not.
       */
      MultipleChoiceQuestion create_q;
      create_q.setQuestion(
          "If the file doesn't exist, do you want us to create it?");
      create_q.setExplanation(
          "The file will be created the first time the emulator runs.");
      create_q.addAnswer("no", "no", "Don't create this file");
      create_q.addAnswer("yes", "yes", "Create this file");
      if (create_q.getAnswer() == "yes") {
        /* If we should create the file, we need to
         * know it's size.
         */
        MultipleChoiceQuestion unit_q;
        unit_q.setQuestion(
            "What unit do you want to use to specify the disk size?");
        unit_q.setExplanation(
            "This is needed to create the file if it doesn't exist.");
        unit_q.addAnswer("KB", "K", "Kilobytes");
        unit_q.addAnswer("MB", "M", "Megabytes");
        unit_q.addAnswer("GB", "G", "Gigabytes");
        unit_q.setDefault("MB");
        unit_q.ask();
        NumberQuestion size_q;
        size_q.setQuestion("How many " + unit_q.getAnswer() +
                           "Bytes should the disk be?");
        size_q.setExplanation(
            "This is needed to create the file if it doesn't exist.");
        size_q.setRange(1, 1024);
        size_q.setDefault("10");
        size_q.ask();
        *os << "      autocreate_size = \"" << size_q.getAnswer()
            << unit_q.getAnswer() << "\";\n";
      }
    }

    if (type_q.getAnswer() == "ramdisk") {
      /* For a RAM DISK, we need to know what
       * size it should be.
       */
      MultipleChoiceQuestion unit_q;
      unit_q.setQuestion(
          "What unit do you want to use to specify the disk size?");
      unit_q.setExplanation("This is needed to create the RAMDISK.");
      unit_q.addAnswer("KB", "K", "Kilobytes");
      unit_q.addAnswer("MB", "M", "Megabytes");
      unit_q.addAnswer("GB", "G", "Gigabytes");
      unit_q.setDefault("MB");
      unit_q.ask();
      NumberQuestion size_q;
      size_q.setQuestion("How many " + unit_q.getAnswer() +
                         "Bytes should the disk be?");
      size_q.setExplanation("This is needed to create the RAMDISK.");
      size_q.setRange(1, 1024);
      size_q.setDefault("10");
      size_q.ask();
      *os << "      size = \"" << size_q.getAnswer() << unit_q.getAnswer()
          << "\";\n";
    }

    /* We need to know whether to emulate this as
     * a cd-rom, or as a hard-disk.
     */
    MultipleChoiceQuestion cdrom_q;
    cdrom_q.setQuestion("Should " + disk_q->getAnswer() +
                        " be a disk or a cd-rom device?");
    cdrom_q.setExplanation("Do you want the OS to see this " +
                           type_q.getAnswer() +
                           " as a hard-disk, or as a cd-rom?");
    cdrom_q.addAnswer("disk", "false", "Hard-disk");
    cdrom_q.addAnswer("cd-rom", "true", "CD-ROM drive");
    cdrom_q.setDefault("disk");
    *os << "      cdrom = " << cdrom_q.getAnswer() << ";\n";

    /* We also need to know whether this is a
     * writeable or a read-only device.
     */
    MultipleChoiceQuestion ro_q;
    ro_q.setQuestion("Should " + disk_q->getAnswer() + " be a read-only disk?");
    ro_q.setExplanation("You might want to write-protect this disk.");
    ro_q.addAnswer("no", "false", "writeable");
    ro_q.addAnswer("yes", "true", "read-only");
    ro_q.setDefault("no");

    if (cdrom_q.ask() == "true") {
      /* CD-ROMs are always read-only.
       */
      ro_q.setAnswer("true");
    } else if (type_q.getAnswer() == "ramdisk") {
      /* Read-only RAM DISKs don't make any sense.
       */
      ro_q.setAnswer("false");
    } else {
      /* Otherwise, ask.
       */
      ro_q.ask();
    }
    *os << "      read_only = " << ro_q.getAnswer() << ";\n";

    /* The user can define a custom model
     * number for the device.
     */
    FreeTextQuestion ft_q;
    ft_q.setQuestion("Would you like to set a disk model number?");
    ft_q.setExplanation("Leave blank to choose the default.");
    if (ft_q.ask() != "")
      *os << "      model_number = \"" << ft_q.getAnswer() << "\";\n";

    /* The user can define a custom revision
     * number for the device.
     */
    ft_q.setQuestion("Would you like to set a revision number?");
    if (ft_q.ask() != "")
      *os << "      rev_number = \"" << ft_q.getAnswer() << "\";\n";

    /* The user can define a custom serial
     * number for the device.
     */
    ft_q.setQuestion("Would you like to set a serial number?");
    if (ft_q.ask() != "")
      *os << "      serial_number = \"" << ft_q.getAnswer() << "\";\n";

    *os << "    }\n\n";
  }
}

/**
 * Entry point for configuration.
 **/
int main_cfg(int argc, char *argv[]) {
  /* Explanation
   */
  printf("We are now going to set up an initial configuration file for the "
         "Emulator.\n");
  printf("This file will be saved as es40.cfg in the current directory.\n\n");
  printf(
      "For more detailed information to the current question, answer '?'.\n");

  /* Open es40.cfg for writing.
   */
  filebuf fb;
  fb.open("es40.cfg", ios::out);
  ostream os(&fb);

  /* **************************** *
   * GUI Choice                   *
   * **************************** */

  MultipleChoiceQuestion gui_q;

  gui_q.setQuestion("Do you want to have a GUI?");
  gui_q.setExplanation(
      "You need a GUI if you want to use an emulated graphics card. You don't "
      "need this for most OS'es. If you don't need this, we recomment that you "
      "answer 'none' to this question.");
  gui_q.setDefault("none");
  gui_q.addAnswer("none", "", "No GUI. Graphics cards are not supported.");

#if defined(HAVE_SDL)
  gui_q.addAnswer("SDL", "sdl",
                  "Simple Directmedia Layer. Preferred GUI mechanism.");
#endif
#if defined(HAVE_X11)
  gui_q.addAnswer("X11", "X11", "Unix X-Windows GUI support.");
#endif
#if defined(_WIN32)
  gui_q.addAnswer("win32", "win32", "Windows 32 GUI support.");
#endif

  if (gui_q.countAnswers() == 1) {
    /* The only valid answer is "none".
     */
    cout << "\nSorry, the GUI is not available! (no SDL, win32 or X11 support "
            "found).\n";
    gui_q.setAnswer("");
  } else {
    /* Ask what GUI to use?
     */
    gui_q.ask();
  }

  if (gui_q.getAnswer() != "") {
    /* Yes, we have a GUI.
     */
    os << "gui = " << gui_q.getAnswer() << "\n";
    os << "{\n";
    os << "}\n\n";
  }

  /* **************************** *
   * Memory Size                  *
   * **************************** */

  MultipleChoiceQuestion mem_q;

  mem_q.setQuestion("How much RAM memory do you want to emulate?");
  mem_q.setExplanation("Your system should have enough free memory to emulate "
                       "the amount you choose here.");
  mem_q.setDefault("256M");

  /* Add memory sizes from 32 MB to 8 GB
   * (25 to 34 bits).
   */
  for (int i = 25; i < 34; i++) {
    string a;
    int j = i;
    if (i < 30) {
      /* Megabyte-range.
       */
      j -= 20;
      a = "M";
    } else {
      /* Gigabyte range.
       */
      j -= 30;
      a = "G";
    }
    mem_q.addAnswer(i2s(1 << j) + a, i2s(i),
                    i2s(1 << j) + a + "Bytes of memory.");
  }

  os << "sys0 = tsunami\n";
  os << "{\n";
  os << "  memory.bits = " << mem_q.ask() << ";\n";

  /* **************************** *
   * ROM Files                    *
   * **************************** */

  FreeTextQuestion rom_q;
  rom_q.setQuestion("Where can the SRM ROM image be found?");
  rom_q.setExplanation("This file is required.");
#if defined(_WIN32)
  rom_q.setDefault("rom\\cl67srmrom.exe");
#elif defined(__VMS)
  rom_q.setDefault("[.ROM]CL67SRMROM.EXE");
#else
  rom_q.setDefault("rom/cl67srmrom.exe");
#endif

  os << "  rom.srm = \"" << rom_q.ask() << "\";\n";

  rom_q.setQuestion("Where should the decompressed ROM image be saved?");
  rom_q.setExplanation(
      "This file will be created the first time the emulator runs.");
#if defined(_WIN32)
  rom_q.setDefault("rom\\decompressed.rom");
#elif defined(__VMS)
  rom_q.setDefault("[.ROM]DECOMPRESSED.ROM");
#else
  rom_q.setDefault("rom/decompressed.rom");
#endif

  os << "  rom.decompressed = \"" << rom_q.ask() << "\";\n";

  rom_q.setQuestion("Where should the Flash ROM image be saved?");
#if defined(_WIN32)
  rom_q.setDefault("rom\\flash.rom");
#elif defined(__VMS)
  rom_q.setDefault("[.ROM]FLASH.ROM");
#else
  rom_q.setDefault("rom/flash.rom");
#endif

  os << "  rom.flash = \"" << rom_q.ask() << "\";\n";

  rom_q.setQuestion("Where should the DPR EEPROM image be saved?");
#if defined(_WIN32)
  rom_q.setDefault("rom\\dpr.rom");
#elif defined(__VMS)
  rom_q.setDefault("[.ROM]DPR.ROM");
#else
  rom_q.setDefault("rom/dpr.rom");
#endif

  os << "  rom.dpr = \"" << rom_q.ask() << "\";\n\n";

  /* **************************** *
   * CPU's                        *
   * **************************** */

  NumberQuestion cpu_q;

  cpu_q.setQuestion("How many CPU's do you want in the system?");
  cpu_q.setRange(1, 4);
  cpu_q.setDefault("1");
  cpu_q.setExplanation(
      "The normal value for the number of CPU's is 1. More CPU's are very "
      "experimental, and currently doesn't work.");

  cpu_q.ask();

  MultipleChoiceQuestion icache_q;

  icache_q.setQuestion("Do you want the ICACHE on the CPU's enabled?");
  icache_q.setExplanation(
      "The ICACHE makes the CPU emulation more accurate, but also slows down "
      "the emulator. Decent operating systems shouldn't depend on this.");
  icache_q.setDefault("no");
  icache_q.addAnswer("yes", "true",
                     "ICACHE enabled. Performance hit, but may be necessary "
                     "for some software.");
  icache_q.addAnswer(
      "no", "false",
      "ICACHE disabled. Better performance, but may not always work.");

  icache_q.ask();

  MultipleChoiceQuestion skip_memtest_hack_q;

  skip_memtest_hack_q.setQuestion("Do you want to skip memtest on SRM start?");
  skip_memtest_hack_q.setExplanation(
      "This makes startup significantly faster, but may not work with some "
      "versions of the firmware.");
  skip_memtest_hack_q.setDefault("no");
  skip_memtest_hack_q.addAnswer(
      "yes", "true",
      "Skip memtest hack enabled. CPU detects the instruction pointer where "
      "the memtest starts and skips it.");
  skip_memtest_hack_q.addAnswer("no", "false",
                                "Skip memtest hack disabled. Firmware checks "
                                "all available memory on startup.");

  skip_memtest_hack_q.ask();

  NumberQuestion mhz_q;

  mhz_q.setQuestion("What should the reported speed of the CPU's be in MHz?");
  mhz_q.setExplanation("This only changes the CPU speed reported to the OS; "
                       "not the speed of the emulation.");
  mhz_q.setRange(10, 1250);
  mhz_q.setDefault("800");

  mhz_q.ask();

  for (int i = 0; i < cpu_q.getNum(); i++) {
    /* Repeat the CPU configuration for each
     * CPU. Differing CPU specs are not supported.
     */
    os << "  cpu" << i << " = ev68cb\n";
    os << "  {\n";
    os << "    speed = " << mhz_q.getAnswer() << "M;\n";
    os << "    icache = " << icache_q.getAnswer() << ";\n";
    os << "    skip_memtest_hack = " << skip_memtest_hack_q.getAnswer()
       << ";\n";
    os << "  }\n\n";
  }

  /* **************************** *
   * Serial Ports                 *
   * **************************** */

  /* There are two serial ports (0 and 1).
   */
  for (int i = 0; i < 2; i++) {
    NumberQuestion port_q;
    port_q.setQuestion("What telnet port should serial " + i2s(i) + " listen?");
    port_q.setRange(1, 65535);
    /* The default ports are 21264 and 21265.
     */
    port_q.setDefault(i2s(21264 + i));
    port_q.setExplanation("You will telnet to this port to establish a "
                          "connection with emulated serial port " +
                          i2s(i) + ".");

    port_q.ask();

    FreeTextQuestion exec_q;
    exec_q.setQuestion(
        "What program should be started automatically for serial " + i2s(i) +
        "?");
#if defined(_WIN32)
    /* On windows, default to
     * C:\Program Files\Putty\Putty.exe
     */
    exec_q.setDefault("C:\\Program Files\\Putty\\Putty.exe");
#else
    /* On other OS'es, default to
     * putty
     */
    exec_q.setDefault("putty");
#endif
    exec_q.setExplanation("Enter the path to a program to start this to create "
                          "an automatic connection with the serial port. Set "
                          "to 'none' to establish the connection manually.");

    exec_q.ask();
    FreeTextQuestion arg_q;

    /* If none was answered, we don't need to
     * ask for arguments.
     */
    if (exec_q.getAnswer() != "none") {
      arg_q.setQuestion("What arguments should the program use to connect to "
                        "the serial port?");
      arg_q.setExplanation("Enter the arguments the program needs.");
      /* This is the argument format for PuTTy.
       */
      arg_q.setDefault("telnet://localhost:" + port_q.getAnswer());

      arg_q.ask();
    }

    os << "  serial" << i << " = serial\n";
    os << "  {\n";
    os << "    port = " << port_q.getAnswer() << ";\n";
    if (exec_q.getAnswer() != "none") {
#if defined(_WIN32)
      /* Quote the program path/name in "",
       * as it may contain spaces.
       */
      os << "    action = \"\"\"" << exec_q.getAnswer() << "\"\" "
         << arg_q.getAnswer() << "\";\n";
#else
      os << "    action = \"" << exec_q.getAnswer() << " " << arg_q.getAnswer()
         << "\";\n";
#endif
    }
    os << "  }\n\n";
  }

  /* **************************** *
   * Floppy Disks                *
   * **************************** */

  MultipleChoiceQuestion fdc_q;

  fdc_q.setQuestion("Do you want a floppy controller in your system?");
  fdc_q.setExplanation(
      "You need a floppy controller if you want to add floppy drives.");
  fdc_q.setDefault("no");
  fdc_q.addAnswer("yes", "fdc", "FDC present.");
  fdc_q.addAnswer("no", "", "FDC not present.");

  if (fdc_q.ask() != "") {
    /* Use a ShrinkingChoiceQuestion; once
     * a disk position has been used, it
     * can't be used again.
     */
    ShrinkingChoiceQuestion fd_q;
    fd_q.setQuestion("Do you want to add any disks to the Floppy controller?");
    fd_q.setDefault("none");
    fd_q.setExplanation("Here, you can add floppy drives to your system.");
    fd_q.addAnswer("none", "", "stop adding disks");
    fd_q.addAnswer("0", "disk0.0", "A:");
    fd_q.addAnswer("1", "disk0.1", "B:");

    os << "  fdc0 = floppy\n";
    os << "  {\n";
    /* Ask what disks to add.
     */
    add_disks(&fd_q, &os);
    os << "  }\n\n";
  }

  /* **************************** *
   * ALi IDE Disks                *
   * **************************** */

  /* Use a ShrinkingChoiceQuestion; once
   * a disk position has been used, it
   * can't be used again.
   */
  ShrinkingChoiceQuestion ide_q;
  ide_q.setQuestion("Do you want to add any disks to the IDE controller?");
  ide_q.setDefault("none");
  ide_q.setExplanation("The IDE controller is mandatory. You can skip this, "
                       "and set up a SCSI controller, too.");
  ide_q.addAnswer("none", "", "stop adding disks");
  ide_q.addAnswer("0.0", "disk0.0", "primary master");
  ide_q.addAnswer("0.1", "disk0.1", "primary slave");
  ide_q.addAnswer("1.0", "disk1.0", "secondary master");
  ide_q.addAnswer("1.1", "disk1.1", "secondary slave");

  os << "  pci0.15 = ali_ide\n";
  os << "  {\n";
  /* Ask what disks to add.
   */
  add_disks(&ide_q, &os);
  os << "  }\n\n";

  /* **************************** *
   * VGA Card                     *
   * **************************** */

  /* Use a ShrinkingChoiceQuestion. Once a
   * card is using a specific PCI slot, it
   * can't be used by another card.
   */
  ShrinkingChoiceQuestion pci_q;

  /* Only add the PCI bus 0 slots, as the
   * VGA card is only supported on hose 0.
   */
  for (int i = 1; i < 5; i++) {
    pci_q.addAnswer("0." + i2s(i), "pci0." + i2s(i), "Bus 0, Slot " + i2s(i));
  }
  pci_q.setExplanation("Only free PCI slots are listed.");

  MultipleChoiceQuestion vga_q;

  if (gui_q.getAnswer() != "") {
    vga_q.setQuestion(
        "What (if any) VGA card do you wish to add to the system?");
    vga_q.setExplanation(
        "Functionality of the different cards is pretty much the same; some "
        "OS'es seem to have a preference, though.");
    vga_q.setDefault("Cirrus");
    vga_q.addAnswer("none", "", "No graphics card");
    vga_q.addAnswer("Cirrus", "cirrus", "Cirrus CL-GD something");
    vga_q.addAnswer("S3", "s3", "S3 Trio 64");
#if defined(HAVE_RADEON)
    /* Radeon support is optional, and currently
     * unreleased, because the specs are only
     * available under an NDA with AMD. Once AMD
     * has publicly released the Radeon 7500 (RV200)
     * specs, the emulated Radeon card will be
     * released.
     */
    vga_q.addAnswer("Radeon", "radeon", "Radeon 7500");
#endif

    vga_q.ask();
  }

  if (vga_q.getAnswer() != "") {
    pci_q.setQuestion("What PCI slot should the " + vga_q.getAnswer() +
                      " card be on?");
    pci_q.setDefault("0.1");

    rom_q.setQuestion("Where can the VGA BIOS ROM image be found?");
    rom_q.setExplanation("This file is required.");
#if defined(_WIN32)
    rom_q.setDefault("rom\\vgabios-0.6a.bin");
#elif defined(__VMS)
    rom_q.setDefault("[.ROM]VGABIOS_0_6A.BIN");
#else
    rom_q.setDefault("rom/vgabios-0.6a.bin");
#endif

    os << "  " << pci_q.ask() << " = " << vga_q.getAnswer() << "\n";
    os << "  {\n";
    os << "    rom = \"" << rom_q.ask() << "\";\n";
    os << "  }\n\n";
  }

  /* **************************** *
   * Free Form PCI Cards          *
   * **************************** */

  /* Add the PCI bus 1 slots. All non-VGA
   * PCI cards can be on either hose 0 or
   * hose 1.
   */
  for (int i = 1; i < 7; i++) {
    pci_q.addAnswer("1." + i2s(i), "pci1." + i2s(i), "Bus 1, Slot " + i2s(i));
  }

  MultipleChoiceQuestion card_q;

  card_q.setQuestion("Would you like to add another PCI card to the system?");
  card_q.setDefault("none");
  card_q.setExplanation("Choose what PCI card you'd like to add. Choose none "
                        "if you have no more cards to add.");
  card_q.addAnswer("none", "", "No more cards to add");
#if defined(HAVE_PCAP)
  card_q.addAnswer("nic", "dec21143", "DEC 21143 Network Interface (1 max)");
#endif
  card_q.addAnswer("scsi", "sym53c810",
                   "Symbios 53C810 narrow SCSI controller");
  card_q.addAnswer(
      "wide scsi", "sym53c895",
      "Symbios 53C895 wide SCSI controller (doesn't work with OpenVMS)");

  /* Loop until there are no more PCI
   * cards to add.
   */
  for (;;) {
    /* If there are no more free PCI slots,
     * stop adding PCI cards.
     */
    if (pci_q.countAnswers() == 0)
      break;

    /* Default to the first available free
     * PCI slot.
     */
    pci_q.setDefault(pci_q.getFirstChoice());

    /* If this answer has been answered with
     * "none", stop adding PCI cards.
     */
    if (card_q.ask() == "")
      break;

    /* Determine where to put this card.
     */
    pci_q.setQuestion("In what PCI slot would you like to put the " +
                      card_q.getAnswer() + " card?");
    os << "  " << pci_q.ask() << " = " << card_q.getAnswer() << "\n";
    os << "  {\n";

    if (card_q.getAnswer() == "dec21143") {
      /* Due to limitations in our network
       * emulation, only one NIC is allowed.
       * Remove it from the list of choices.
       */
      card_q.dropChoice("nic");

#if defined(HAVE_PCAP)
      MultipleChoiceQuestion if_q;
      if_q.setQuestion("What host network interface should we connect to "
                       "(answer ? for a list)?");
      if_q.setExplanation("Choose 'list' to get a list at run-time.");
      if_q.addAnswer("list", "", "Get a list at run-time");

      /* Get a list of network interfaces and
       * add them to the list.
       */
      pcap_if_t *alldevs;
      pcap_if_t *d;
      char errbuf[PCAP_ERRBUF_SIZE];

      if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        /* No devices to add.
         */
        printf("Error in pcap_findalldevs_ex: %s", errbuf);
      } else {
        int i = 1;
        for (d = alldevs; d; d = d->next) {
          // if_q.addAnswer(i2s(i),d->name, string(d->name) + "(" +
          // string(d->description) + ")");
          if_q.addAnswer(i2s(i), d->name, d->name);
          i++;
        }
      }

      if (if_q.ask() != "")
        os << "    adapter = \"" << if_q.getAnswer() << "\";\n";

      FreeTextQuestion mac_q;
      mac_q.setQuestion("What should the NIC's MAC address be?");
      mac_q.setExplanation("This should be unique on your network.");
      mac_q.setDefault("08-00-2B-E5-40-00");
      os << "    mac = \"" << mac_q.ask() << "\";\n";
#endif
    } else if (card_q.getAnswer() == "sym53c810") {
      /* Use a ShrinkingChoiceQuestion; once
       * a disk position has been used, it
       * can't be used again.
       */
      ShrinkingChoiceQuestion disk_q;
      disk_q.setQuestion(
          "Do you want to add any disks to the Sym53C810 controller?");
      disk_q.setDefault("none");
      disk_q.setExplanation(
          "Add disks. Select 'none' if you have no more disks to add.");
      disk_q.addAnswer("none", "", "stop adding disks");
      /* The narrow SCSI controller supports
       * devices at targets 0..6.
       */
      for (int i = 0; i < 7; i++) {
        disk_q.addAnswer(i2s(i), "disk0." + i2s(i), "Target " + i2s(i));
      }
      /* Ask what disks to add.
       */
      add_disks(&disk_q, &os);
    } else if (card_q.getAnswer() == "sym53c895") {
      /* Use a ShrinkingChoiceQuestion; once
       * a disk position has been used, it
       * can't be used again.
       */
      ShrinkingChoiceQuestion disk_q;
      disk_q.setQuestion(
          "Do you want to add any disks to the Sym53C895 controller?");
      disk_q.setDefault("none");
      disk_q.setExplanation(
          "Add disks. Select 'none' if you have no more disks to add.");
      disk_q.addAnswer("none", "", "stop adding disks");
      /* The wide SCSI controller supports
       * devices at targets 0..6 and 8..15.
       */
      for (int i = 0; i < 16; i++) {
        if (i != 7)
          disk_q.addAnswer(i2s(i), "disk0." + i2s(i), "Target " + i2s(i));
      }
      /* Ask what disks to add.
       */
      add_disks(&disk_q, &os);
    }
    os << "  }\n\n";
  }

  MultipleChoiceQuestion mouse_q;
  mouse_q.setQuestion("Would you like to emulate the mouse?");
  mouse_q.setExplanation("The mouse is not really working yet... :-(");
  mouse_q.addAnswer("no", "false", "Disable the mouse");
  mouse_q.addAnswer("yes", "true", "Enable the mouse");
  mouse_q.setDefault("no");

  MultipleChoiceQuestion vgacons_q;
  vgacons_q.setQuestion("Where would you like console output to go?");
  vgacons_q.setExplanation("This is the SRM console option");
  vgacons_q.addAnswer("serial", "false", "Console on serial port 0");
  vgacons_q.addAnswer("graphics", "true", "Console on graphics controller");
  vgacons_q.setDefault("graphics");

  if (vga_q.getAnswer() != "") {
    /* If a VGA card is present, ask about
     * the mouse and the console.
     */
    mouse_q.ask();
    vgacons_q.ask();
  } else {
    /* No VGA card present, mouse support
     * is disabled, and the console goes
     * to serial port 0.
     */
    mouse_q.setAnswer("false");
    vgacons_q.setAnswer("false");
  }

  FreeTextQuestion lpt_q;
  lpt_q.setQuestion("Where would you like printer output to go?");
  lpt_q.setExplanation("Output from the printer port will be saved to this "
                       "file. Leave blank if not wanted.");
  lpt_q.ask();

  os << "  pci0.7 = ali\n";
  os << "  {\n";
  os << "    mouse.enabled = " << mouse_q.getAnswer() << ";\n";
  os << "    vga_console = " << vgacons_q.getAnswer() << ";\n";
  if (lpt_q.getAnswer() != "")
    os << "    lpt.outfile = \"" << lpt_q.getAnswer() << "\"\n";
  os << "  }\n\n";

  /* The USB device is a fixed part, and
   * currently not configurable.
   */
  os << "  pci0.19 = ali_usb\n";
  os << "  {\n";
  os << "  }\n";

  os << "}\n";

  /* Close es40.cfg.
   */
  fb.close();

  /* All is well.
   */
  return 0;
}
