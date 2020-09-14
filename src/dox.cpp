/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * Website: http://es40.org
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Although this is not required, the author would appreciate being
 * notified of, and receiving any modifications you may make to the
 * source code that might serve the general public.
 */

/**
 * \file
 * Contains extra comments for Doxygen.
 *
 * You could read the documentation from this file; but it would probably
 * be easier to go to http://es40.sourceforge.net.
 *
 * $Id: dox.cpp,v 1.20 2008/04/29 07:37:54 iamcamiel Exp $
 **/

/**
 * \mainpage ES40 Emulator Documentation
 *
 * \section intro Introduction
 * Welcome to the documentation for the ES40 Emulator Project. For the
 * main project page, goto http://es40.org.
 *
 * On these pages here, you can find the documented source code, the change
 * log, and more.
 **/

/**
 * \page faq Frequently Asked Questions
 * Here, you can find the answers to a few frequently asked questions.
 *
 * \section a A. Compilation
 * Compilation related issues.
 *
 * \subsection a1 A1. SDL not found
 * Q: I get warnings about SDL.H, sdl.lib, sdlmain.lib or sdl.dll not being
 * found. What do I need?
 *
 * A: You need the Simple DirectMedia Layer (SDL) libraries, if you want to
 * use one of the emulated VGA cards. Visit http://www.libsdl.org. To run
 * the emulator you need the runtime libraries, to compile the emulator you
 * need the development libraries. Make sure your compiler/linker can find
 * the header and library files.
 *
 * \subsection a2 A2. SDL not available
 * Q: SDL is not available for my system, or I don't want to install it.
 * What are my options?
 *
 * A: Compile without HAVE_SDL defined. For Windows, you can download the
 * no_sdl version of the binary package. If you're on Windows, change the
 * configuration file (gui=win32), if your system supports X-Windows (as is
 * the case on most Linux/BSD/UNIX distributions, change it to gui=X11.
 * If your system is not supported, remove the gui section alltogether, as
 * well as any vga-card sections. Graphics support will not be available
 * in this case.
 *
 * \subsection a3 A3. PCAP not found
 * Q: I get warnings about pcap.h, wpcap.lib, wpcap.dll, or libpcap not being
 * found. What do I need?
 *
 * A: You need the libpcap (Linux and others) or winpcap (Windows) packet
 * capture libraries, if you want to use one of the emulated network cards.
 * Visit http://www.tcpdump.org or http://www.winpcap.org. To run the emulator
 * you need the runtime libraries, to compile the emulator you need the
 * developer's pack. Make sure your compiler/linker can find the header and
 * library files.
 *
 * \subsection a4 A4. PCAP not available
 * Q: PCAP is not available for my system, or I don't want to install it.
 * What are my options?
 *
 * A: Your only option is to disable network support, by compiling without
 * HAVE_PCAP defined. For Windows, you can download the no_net version of
 * the binary package.
 *
 * \subsection a5 A5. Endianess issues
 * Q: I'm running the ES40 emulator on a big endian host machine (PowerPC,
 * Sparc) and I get all these strange errors. Network, graphics and disks
 * don't seem to work properly.
 *
 * A: Support for big-endian host architectures has been available in the
 * past, but since the primary developer of the ES40 Emulator no longer
 * has a big endian machine, it has become difficult to support and
 * maintain. If you can, please help us get the emulator working on big-
 * endian architectures again!
 *
 * \section b B. Configuration
 * Configuration related issues.
 *
 * \subsection b1 B1. Which VGA card?
 * Q: Which of the two emulated VGA cards should I choose? S3 Trio64 or
 * Cirrus?
 *
 * A: Short answer: That depends on what the OS you intend to run on the
 * emulator likes. Long answer: Right now, both cards are nearly identical
 * (except for the PCI configuration ID, which tells the OS what card this
 * is). In the future, the emulated Cirrus card may evolve into a more
 * complete card, supporting various SVGA features. For the S3 Trio, this
 * is more difficult, because documentation is harder to find. Since
 * DECwindows on OpenVMS doesn't seem to like either of the cards, support
 * for an entirely different card may be added in the future.
 */

/**
 * \page cons Consulted Documentation
 * The following documents have been useful to us:
 *  - Alpha 21264/EV68CB and 21264/EV68DC Microprocessor Hardware Reference
 *Manual [HRM] (http://download.majix.org/dec/21264ev68cb_ev68dc_hrm.pdf)
 *  - DS-0026A-TE: Alpha 21264B Microprocessor Hardware Reference Manual [HRM]
 *(http://ftp.digital.com/pub/Digital/info/semiconductor/literature/21264hrm.pdf)
 *  - Alpha Architecture Handbook [AHB]
 *(http://ftp.digital.com/pub/Digital/info/semiconductor/literature/alphaahb.pdf)
 *  - Alpha Architecture Reference Manual, fourth edition [ARM]
 *(http://download.majix.org/dec/alpha_arch_ref.pdf)
 *  - Tsunami/Typhoon 21272 Chipset Hardware Reference Manual [HRM]
 *(http://download.majix.org/dec/tsunami_typhoon_21272_hrm.pdf)
 *  - AlphaServer ES40 and AlphaStation ES40 Service Guide [SG]
 *(http://www.dec-store.com/PD_00158.aspx)
 *  - 21143 PCI/Cardbus 10/100Mb/s Ethernet LAN Controller Hardware Reference
 *Manual  [HRM] (http://download.intel.com/design/network/manuals/27807401.pdf)
 *  - AT Attachment with Packet Interface - 5 (ATA/ATAPI-5)
 *(http://www.t13.org/Documents/UploadedDocuments/project/d1321r3-ATA-ATAPI-5.pdf)
 *  - Programming Interface for Bus Master IDE COntroller
 *(http://suif.stanford.edu/%7Ecsapuntz/specs/idems100.ps)
 *  - FDC37C669 PC 98/99 Compliant Super I/O Floppy Disk Controller
 *(http://www.smsc.com/main/datasheets/37c669.pdf)
 *  - SCSI 2 (http://www.t10.org/ftp/t10/drafts/s2/s2-r10l.pdf)
 *  - SCSI 3 Multimedia Commands (MMC)
 *(http://www.t10.org/ftp/t10/drafts/mmc/mmc-r10a.pdf)
 *  - SYM53C895 PCI-Ultra2 SCSI I/O Processor
 *(http://www.datasheet4u.com/html/S/Y/M/SYM53C895_LSILogic.pdf.html)
 *  - SYM53C810A PCI-SCSI I/O Processor
 *(http://ftp.netbsd.org/pub/NetBSD/arch/bebox/doc/810a.pdf)
 *  - Symbios SCSI SCRIPTS Processors Programming Guide
 *(http://la.causeuse.org/hauke/macbsd/symbios_53cXXX_doc/lsilogic-53cXXX-scripts.pdf)
 *  - Ali M1543C B1 South Bridge Version 1.20
 *(http://mds.gotdns.com/sensors/docs/ali/1543dScb1-120.pdf)
 *  - VGADOC4b (http://home.worldonline.dk/~finth/)
 *  .
 **/

/**
 * \page guest_os Guest OS Notes
 *
 * Status for various guest OSes under ES40
 *
 * \section hp DEC/Compaq/HP OS-es
 *
 * \subsection vms OpenVMS
 * Test 8.3
 *   - Using newide, all devices are recognized and use DMA mode, including CDs.
 *   - There is a permissions violation during install.  Do not abort it, and
 *the install will work anyway. (note Camiel: differing results of installation
 *     reported. A lot seems to depend on timing).
 *   .
 *
 * \subsection tru64 Tru64 Unix
 * Tested using 5.1B
 *   - Using the newide device:
 *       - Devices are probed correctly.
 *       - DMA is supported.
 *       - installs and configures.
 *       - es40 system too hot?  remove your dpr.rom and restart!
 *       .
 *   - Using the ali_ide device
 *       - devices timeout on probe.
 *       .
 *   .
 *
 * \section free Free OS'es
 *
 * \subsection fbsd FreeBSD
 * Tested with 6.2
 *   - Using newide
 *      - devices are probed and DMA is used.
 *      - CD-ROM device works, but occasionally times out.
 *      .
 *   .
 *
 * \subsection nbsd NetBSD
 * Tested 3.1.1
 *   - Panics if cirrus device is present.
 *   - Using newide
 *       - all devices are recognized and use DMA mode.
 *       - CD-ROM devices are supported and work for install and normal use
 *       .
 *   - Using ali_ide
 *       - all devices are recognized and pio mode is used.
 *       - CD-ROM devices are not supported
 *       .
 *   .
 *
 * Tested 4.0
 *   - Panics if cirrus device is present.
 *   - Using newide
 *       - recognizes all devices and uses DMA
 *       - CD-ROM cannot be used due to apparent cdrom corruption.
 *       .
 *   - Using ali_ide
 *       - Unknown at this time.
 *       .
 *   .
 *
 * \subsection obsd OpenBSD
 * Tested 4.2
 *   - Crashes immediately upon boot.
 *   .
 *
 * \subsection linux Linux
 * Tested using Debian 4.0 rc 0, centos 4.2, gentoo 2007.0.
 * Always boot with "boot <device> -flags 1" when using a serial console.
 *   - Gentoo + ali_ide: hang after hda size display
 *   - Gentoo + newide: hang after hda size display
 *   - Centos:  hangs after serial probe.
 *   - Debian:  hangs after serial probe.
 *   .
 *
 * \section srm SRM
 * Ok, its not really an OS, but it acts like one.
 *   - Can boot CDROMs with newide device (with cdrom=true)
 *   - Cannot boot CDROMs with ali_ide device (segfault)
 *   .
 * When booting CDROM-type media with the ali_ide device,
 * set the disk is cdrom=false.
 **/

/**
 * \page build_guide Building and Installation Guide
 *
 * \section unix UNIX, BSD and Linux
 * These instructions are valid for most UNIX-like operating systems, including
 *BSD and Linux variants.
 *
 * \subsection unix_pr Prerequisites
 * The following prerequisites are required:
 *   - A bourne shell (/bin/sh) that is capable of running the supplied
 *configure script.
 *   - The GNU C++ compiler or another supported C++ compiler.
 *   - The make program.
 *   - The Poco C++ libraries, version 1.3.2 or higher.
 *   .
 *
 * \subsection unix_bi Building and Installation
 *   - Download the ES40 Emulator sources.
 *   - Extract the source tree to a clean directory.
 *   - Run the configure script, and answer the questions asked.
 *   - Run make.
 *   - Run make install
 *   .
 * \code
 *       $ ./configure
 *       $ make
 *       $ make install
 * \endcode
 *
 * \section vms OpenVMS
 * These instructions are for building the ES40 Emulator on OpenVMS Alpha and
 *OpenVMS Itanium.
 *
 * \subsection vms_pr Prerequisites
 * The following prerequisites are required:
 *   - The DEC C++ compiler (CXX).
 *   - GNV version 2.1 (get this from the HP site, the Sourceforge site is no
 *longer kept up-to-date.
 *   .
 *
 * \subsection vms_b Building
 *   - Download the ES40 Emulator sources.
 *   - Extract the source tree to a clean directory.
 *   - Run the make_vms script, and answer the questions asked.
 *   .
 * \code
 *       $ @MAKE_VMS.COM
 * \endcode
 *
 * \section win Microsoft Windows
 * These instructions are for building the ES40 Emulator on Microsoft Windows.
 *
 * \subsection win_pr Prerequisites
 * The following prerequisites are required:
 *   - Microsoft Visual C++ 2005 or higher.
 *   - The Poco C++ libraries, version 1.3.2 or higher.
 *   .
 *
 * \subsection win_b Building
 *   - Download the ES40 Emulator sources.
 *   - Extract the source tree to a clean directory.
 *   - Go to the directory src/build_win* that matches your Visual C++ version,
 *     and processor-word-size (32 or 64 bit) of your Windows version, and open
 *     the project file found there.
 *   - Build the configurations you need.
 *   .
 **/

/**
 * \page dev_guide Developer's Guide
 * On this page, people who want to contribute can find information.
 *
 * This page is a work in progress.
 *
 * \section d_dev Devices
 * The emulator consists of classes that emulate different devices that are part
 * of a real ES40 system.
 *
 * \subsection d_dev_thr Threading
 * Some devices may need - or want - to perform certain activities in a
 *different thread than the main execution thread. This needs to be done in a
 *platform-independant manner, and for that purpose we are using the Threading
 *part of the Poco-library (http://pocoproject.org/).
 *
 * When multi-threading an application, one should think about the following:
 *   - Efficiency of the thread. Don't let the thread run when there is no work
 *to do.
 *   - Handling exceptions that occur in the thread.
 *   - Properly ending the thread when the emulator exits.
 *   - Protect data-structures that are used across threads.
 *   .
 *
 * Threading code
 *
 * Add the following include files to the header-file for the class:
 * \code
 * #include <Poco/Thread.h>
 * #include <Poco/Runnable.h>
 * #include <Poco/Semaphore.h>
 * #include <Poco/Mutex.h>
 * \endcode
 *
 * The device class should publicly inherit the Poco::Runnable class.
 *
 * The following data members should be added to the class:
 * \code
 *  Poco::Thread myThread;
 *  Poco::Semaphore mySemaphore;
 *  bool StopThread;
 * \endcode
 *
 * The class also needs a method run, that performs the work that
 * needs to run in the separate thread:
 * \code
 * void <class-name>::run()
 * {
 *   try
 *   {
 *     for(;;)
 *     {
 *       mySemaphore.wait();    // main thread will set the semaphore
 *                              // when work needs to be done, or if
 *                              // the thread needs to terminate.
 *       if (StopThread)
 *         return;              // main thread has signalled us that
 *                              // we need to terminate.
 *
 *       < work >;              // do work as long as work needs to be
 *                              // done.
 *     }
 *   }
 *   catch(...) {}              // if an exception occurs, catch it and
 *                              // terminate. The main thread will see
 *                              // that this thread has terminated, and
 *                              // throw a new exception.
 * }
 * \endcode
 *
 * mySemaphore and myThread need to be added to the initializer list on
 * the class constructor:
 * \code
 * ... , mySemaphore(0,1), myThread("<thread-name>")
 * \endcode
 *
 * The thread is started from the constructor:
 * \code
 *   StopThread = false;
 *   myThread.start(*this);
 * \endcode
 *
 * Whenever there is work that the thread needs to do, the thread is
 * signalled with the following code:
 * \code
 *   mySemaphore.set();
 * \endcode
 *
 * The thread should be shut down from the class' destructor:
 * \code
 *   StopThread = true;
 *   mySemaphore.set();		// signal the thread to end
 *   myThread.join();		// wait for the thread to end
 * \endcode
 *
 * To catch unexpected termination of a thread due to an exception,
 * the status of the thread is checked periodically (from DoClock):
 * \code
 *   if(!myThread.isRunning())
 *     FAILURE("thread has died");
 * \endcode
 *
 * If data structures need to be accessed by multiple threads, it may
 * be necessary to protect these structures by a Mutex. These mutexes
 * are class members of type Poco::Mutex, for instance:
 * \code
 *   Poco::Mutex myLock;
 * \endcode
 *
 * The access to the data structures is then surrounded by a lock/unlock
 * construction:
 * \code
 *   myLock::lock();
 *   <access structure>;
 *   myLock::unlock();
 * \endcode
 **/
