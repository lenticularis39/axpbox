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
 * Contains the code for the emulated Typhoon Chipset devices.
 *
 * $Id: System.cpp,v 1.79 2008/06/12 07:29:44 iamcamiel Exp $
 *
 * X-1.81       Camiel Vanderhoeven                             12-JUN-2008
 *      Support to keep secondary CPUs waiting until activated from primary.
 *
 * X-1.78       Camiel Vanderhoeven                             02-JUN-2008
 *      Remove hard references to CPU 1 from decompression routine.
 *
 * X-1.77       Camiel Vanderhoeven                             31-MAY-2008
 *      Changes to include parts of Poco.
 *
 * X-1.76       Brian Wheeler                                   29-APR-2008
 *      Added memory map dumping and checking for overlapping memory ranges
 *      (enabled with DUMP_MEMMAP and CHECK_MEM_RANGES, respectively).
 *
 * X-1.75       Camiel Vanderhoeven                             26-MAR-2008
 *      Fix compiler warnings.
 *
 * X-1.74       Pepito Grillo                                   25-MAR-2008
 *      Fixed a typo.
 *
 * X-1.73       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.72       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.71       Camiel Vanderhoeven                             13-MAR-2008
 *      Create init(), start_threads() and stop_threads() functions.
 *
 * X-1.70       Camiel Vanderhoeven                             11-MAR-2008
 *      Named, debuggable mutexes.
 *
 * X-1.68       Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.67       Camiel Vanderhoeven                             04-MAR-2008
 *      Support some basic MP features. (CPUID read from C-Chip MISC
 *      register, inter-processor interrupts)
 *
 * X-1.66       Brian Wheeler                                   02-MAR-2008
 *      Allow large memory sizes (>1GB).
 *
 * X-1.65       Camiel Vanderhoeven                             02-MAR-2008
 *      Natural way to specify large numeric values ("10M") in the config file.
 *
 * X-1.64       Brian Wheeler                                   29-FEB-2008
 *      Do not generate unknown PCI 0 memory messages for legacy VGA
 *      memory region.
 *
 * X-1.63       Brian Wheeler                                   26-FEB-2008
 *      Support reading from Pchip TLBIV and TLBIA registers. (Which are
 *      supposed to be write-only!)
 *
 * X-1.62       David Leonard                                   20-FEB-2008
 *      Flush stdout during decompression progress.
 *
 * X-1.61       Camiel Vanderhoeven                             08-FEB-2008
 *      Show originating device name on memory errors.
 *
 * X-1.60       Camiel Vanderhoeven                             01-FEB-2008
 *      Avoid unnecessary shift-operations to calculate constant values.
 *
 * X-1.59       Camiel Vanderhoeven                             28-JAN-2008
 *      Avoid compiler warnings.
 *
 * X-1.58       Camiel Vanderhoeven                             25-JAN-2008
 *      Added option to disable the icache.
 *
 * X-1.57       Camiel Vanderhoeven                             19-JAN-2008
 *      Run CPU in a separate thread if CPU_THREADS is defined.
 *      NOTA BENE: This is very experimental, and has several problems.
 *
 * X-1.56       Camiel Vanderhoeven                             18-JAN-2008
 *      Process device interrupts after a 100-cpu-cycle delay.
 *
 * X-1.55       Camiel Vanderhoeven                             12-JAN-2008
 *      Comments.
 *
 * X-1.54       Camiel Vanderhoeven                             09-JAN-2008
 *      Let PtrToMemory return NULL when the address is out of range.
 *
 * X-1.53       Camiel Vanderhoeven                             08-JAN-2008
 *      Layout of comments.
 *
 * X-1.52       Camiel Vanderhoeven                             08-JAN-2008
 *      Split out chipset registers.
 *
 * X-1.51       Camiel Vanderhoeven                             07-JAN-2008
 *      Corrected error in last update; csr reg. 0x600, not 0600...
 *
 * X-1.50       Camiel Vanderhoeven                             07-JAN-2008
 *      DMA scatter/gather access. Split out some things.
 *
 * X-1.49       Camiel Vanderhoeven                             02-JAN-2008
 *      Cleanup.
 *
 * X-1.48       Camiel Vanderhoeven                             30-DEC-2007
 *      Comments.
 *
 * X-1.47       Camiel Vanderhoeven                             30-DEC-2007
 *      Fixed error in printf again.
 *
 * X-1.46       Camiel Vanderhoeven                             30-DEC-2007
 *      Fixed error in printf.
 *
 * X-1.45       Camiel Vanderhoeven                             30-DEC-2007
 *      Print file id on initialization.
 *
 * X-1.44       Camiel Vanderhoeven                             29-DEC-2007
 *      Fix memory-leak.
 *
 * X-1.43       Camiel Vanderhoeven                             28-DEC-2007
 *      Throw exceptions rather than just exiting when errors occur.
 *
 * X-1.42       Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.41       Camiel Vanderhoeven                             20-DEC-2007
 *      Close files and free memory when the emulator shuts down.
 *
 * X-1.40       Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.39       Camiel Vanderhoeven                             14-DEC-2007
 *      Commented out SRM IDE READ replacement; doesn't work with SCSI!
 *
 * X-1.38       Camiel Vanderhoeven                             10-DEC-2007
 *      Added get_cpu
 *
 * X-1.37       Camiel Vanderhoeven                             10-DEC-2007
 *      Use configurator.
 *
 * X-1.36       Camiel Vanderhoeven                             6-DEC-2007
 *      Report references to unused PCI space.
 *
 * X-1.35       Camiel Vanderhoeven                             2-DEC-2007
 *      Avoid misprobing of unused PCI configuration space.
 *
 * X-1.34       Camiel Vanderhoeven                             2-DEC-2007
 *      Added support for code profiling, and for direct operations on the
 *      Tsunami/Typhoon's interrupt registers.
 *
 * X-1.33       Brian Wheeler                                   1-DEC-2007
 *   1. Ignore address bits 35- 42 in the physical address; this is
 *      correct according to the Tsunami/Typhoon HRM; which states that
 *       "  The system address space is divided into two parts: system
 *        memory and PIO. This division is indicated by physical memory bit
 *        <43> = 1 for PIO accesses from the CPU [...] In general, bits
 *        <42:35> are don�t cares if bit <43> is asserted. [...] The
 *        Typhoon Cchip supports 32GB of system memory (35 bits total).  "
 *   2. Added support for Ctrl+C and panic.
 *
 * X-1.32       Camiel Vanderhoeven                             17-NOV-2007
 *      Use CHECK_ALLOCATION.
 *
 * X-1.31       Camiel Vanderhoeven                             16-NOV-2007
 *      Replaced PCI_ReadMem and PCI_WriteMem with PCI_Phys.
 *
 * X-1.30       Camiel Vanderhoeven                             05-NOV-2007
 *      Put slow-to-fast clock ratio into #define CLOCK_RATIO. Increased
 *      this to 100,000.
 *
 * X-1.29       Camiel Vanderhoeven                             18-APR-2007
 *      Decompressed ROM image is now identical between big- and small-
 *      endian platforms (put endian_64 around PALbase and PC).
 *
 * X-1.28       Camiel Vanderhoeven                             18-APR-2007
 *      Faster lockstep mechanism (send info 50 cpu cycles at a time)
 *
 * X-1.27       Camiel Vanderhoeven                             16-APR-2007
 *      Remove old address range if a new one is registered (same device/
 *      same index)
 *
 * X-1.26       Camiel Vanderhoeven                             16-APR-2007
 *      Allow configuration strings with spaces in them.
 *
 *
 * X-1.25       Camiel Vanderhoeven                             11-APR-2007
 *      Moved all data that should be saved to a state file to a structure
 *      "state".
 *
 * X-1.24	Camiel Vanderhoeven				10-APR-2007
 *	New mechanism for SRM replacements. Where these need to be executed,
 *	CSystem::LoadROM() puts a special opcode (a CALL_PAL instruction
 *	with an otherwise illegal operand of 0x01234xx) in memory.
 *	CAlphaCPU::DoClock() recognizes these opcodes and performs the SRM
 *	action.
 *
 * X-1.23       Camiel Vanderhoeven                             10-APR-2007
 *      Extended ROM-handling code to favor loading decompressed ROM code
 *      over loading compressed code, and to save decompressed ROM code
 *      during the first time the emulator is run.
 *
 * X-1.22       Camiel Vanderhoeven                             10-APR-2007
 *      Removed obsolete ROM-handling code.
 *
 * X-1.21       Brian Wheeler                                   31-MAR-2007
 *      Removed ; after #endif to avoid compiler warnings.
 *
 * X-1.20       Camiel Vanderhoeven                             26-MAR-2007
 *      Show references to unknown memory regions when DEBUG_UNKMEM is
 *	defined.
 *
 * X-1.19	Camiel Vanderhoeven				1-MAR-2007
 *	Changes for Solaris/SPARC port:
 *   a)	All $-signs in variable names are replaced with underscores.
 *   b) Some functions now get a const char * argument i.s.o. char * to avoid
 *	compiler warnings.
 *   c) If ALIGN_MEM_ACCESS is defined, memory accesses are checked for natural
 *	alignment. If access is not naturally aligned, it is performed one byte
 *	at a time.
 *   d) Accesses to main-memory are byte-swapped on a big-endian architecture.
 *	This is done through the endian_xx macro's, that differ according to
 *	the endianness of the architecture.
 *
 * X-1.18	Camiel Vanderhoeven				28-FEB-2007
 *	In the lockstep-versions of the emulator, perform lockstep
 *	synchronisation for every clock tick.
 *
 * X-1.17	Camiel Vanderhoeven				27-FEB-2007
 *	Removed an unreachable "return 0;" line.
 *
 * X-1.16	Camiel Vanderhoeven				18-FEB-2007
 *	Keep track of the cycle-counter in single-step mode (using the
 *	iSSCycles variable.
 *
 * X-1.15	Camiel Vanderhoeven				16-FEB-2007
 *   a) Provide slow and fast clocks for devices. Typical fast-clocked
 *	devices are the CPU(s); most other devices that need a clock should
 *	probably be slow clock devices.
 *   b) DoClock() was replaced with Run(), which runs until one of the
 *	connected devices returns something other than 0; and SingleStep().
 *   c) Corrected some signed/unsigned integer comparison warnings.
 *
 * X-1.14	Brian Wheeler					13-FEB-2007
 *   a) Corrected some typo's in printf statements.
 *   b) Fixed some compiler warnings (assignment inside if()).
 *
 * X-1.13	Camiel Vanderhoeven				12-FEB-2007
 *	Removed error messages when accessing unknown memory.
 *
 * X-1.12       Camiel Vanderhoeven                             12-FEB-2007
 *      Corrected a signed/unsigned integer comparison warning.
 *
 * X-1.11       Camiel Vanderhoeven                             9-FEB-2007
 *      Added comments.
 *
 * X-1.10	Brian Wheeler					7-FEB-2007
 *	Remove FindConfig function, and load configuration file from the
 *	constructor.
 *
 * X-1.9	Camiel Vanderhoeven				7-FEB-2007
 *   a)	CTraceEngine is no longer instantiated as a member of CSystem.
 *   b)	Calls to trace_dev now use the TRC_DEVx macro's.
 *
 * X-1.8	Camiel Vanderhoeven				3-FEB-2007
 *   a) Removed last conditional for supporting another system than an ES40
 *      (#ifdef DS15)
 *   b) FindConfig() now returns the default value rather than crashing
 *	when none of the standard configuration files can be found.
 *
 * X-1.7        Brian Wheeler                                   3-FEB-2007
 *      Formatting.
 *
 * X-1.6	Brian Wheeler					3-FEB-2007
 *	Replaced several 64-bit values in 0x... syntax with X64(...).
 *
 * X-1.5	Brian Wheeler					3-FEB-2007
 *	Added possibility to load a configuration file.
 *
 * X-1.4	Brian Wheeler					3-FEB-2007
 *	Replaced 1i64 with X64(1) in two instances.
 *
 * X-1.3        Brian Wheeler                                   3-FEB-2007
 *      Scanf and printf statements made compatible with Linux/GCC/glibc.
 *
 * X-1.2        Brian Wheeler                                   3-FEB-2007
 *      Includes are now case-correct (necessary on Linux)
 *
 * X-1.1        Camiel Vanderhoeven                             19-JAN-2007
 *      Initial version in CVS.
 **/
#include "System.hpp"
#include "AlphaCPU.hpp"
#include "DPR.hpp"
#include "StdAfx.hpp"
#include "lockstep.hpp"

#include <ctype.h>
#include <signal.h>
#include <stdlib.h>

#define CLOCK_RATIO 10000

#if defined(LS_MASTER) || defined(LS_SLAVE)
char debug_string[10000] = "";
char *dbg_strptr = debug_string;
#endif

/**
 * Constructor.
 **/
CSystem::CSystem(CConfigurator *cfg) {
  int i;

  if (theSystem != 0)
    FAILURE(Configuration, "More than one system");
  theSystem = this;
  myCfg = cfg;

  iNumComponents = 0;
  iNumMemories = 0;
  iNumCPUs = 0;
  iNumMemoryBits = (int)myCfg->get_num_value("memory.bits", false, 27);

  //  iNumConfig = 0;
#if defined(IDB)
  iSingleStep = 0;
  iSSCycles = 0;
#endif
  for (i = 0; i < 4; i++)
    state.cchip.dim[i] = 0;
  state.cchip.drir = 0;
  state.cchip.misc = U64(0x0000000800000000);
  state.cchip.csc = U64(0x3142444014157803);

  state.dchip.drev = 0x01;
  state.dchip.dsc = 0x43;
  state.dchip.dsc2 = 0x03;
  state.dchip.str = 0x25;

  // initialize pchip data
  for (i = 0; i < 2; i++) {
    memset(&state.pchip[i], 0, sizeof(struct SSys_state::SSys_pchip));
    state.pchip[i].wsba[3] = 2;
  }

  state.pchip[0].pctl = U64(0x0000104401440081);
  state.pchip[1].pctl = U64(0x0000504401440081);

  state.tig.FwWrite = 0;
  state.tig.HaltA = 0;
  state.tig.HaltB = 0;

  state.cpu_lock_flags = 0;

  if (iNumMemoryBits > 30) {

    // size_t may not be big enough, and makes 2^31 negative, so the
    // alloc fails.  We're going to allocate the memory in
    //  2^(iNumMemoryBits-10) chunks of 2^10.
    CHECK_ALLOCATION(memory = calloc(1 << (iNumMemoryBits - 10), 1 << 10));
  } else
    CHECK_ALLOCATION(memory = calloc(1 << iNumMemoryBits, 1));

  cpu_lock_mutex = new CFastMutex("cpu-locking-lock");

  printf("%s(%s): $Id: System.cpp,v 1.79 2008/06/12 07:29:44 iamcamiel Exp $\n",
         cfg->get_myName(), cfg->get_myValue());
}

/**
 * Destructor. Calls the destructors for registered devices, and
 * frees used memory.
 **/
CSystem::~CSystem() {
  int i;

  printf("Freeing memory in use by system...\n");

  for (i = 0; i < iNumComponents; i++)
    delete acComponents[i];

  for (i = 0; i < iNumMemories; i++)
    free(asMemories[i]);

  free(memory);
}

/**
 * free memory, and allocate and clear new memory.
 **/
void CSystem::ResetMem(unsigned int membits) {
  free(memory);
  iNumMemoryBits = membits;
  CHECK_ALLOCATION(memory = calloc(1 << iNumMemoryBits, 1));
}

/**
 * Register a device.
 **/
void CSystem::RegisterComponent(CSystemComponent *component) {
  acComponents[iNumComponents] = component;
  iNumComponents++;
}

void CSystem::UnregisterComponent(CSystemComponent *component) {
 iNumComponents--;
}

/**
 * Get the number of bits that corresponds to the amount of RAM installed.
 * (e.g. 28 = 256 MB, 29 = 512 MB, 30 = 1 GB)
 **/
unsigned int CSystem::get_memory_bits() { return iNumMemoryBits; }

/**
 * Obtain a pointer to system memory.
 **/
char *CSystem::PtrToMem(u64 address) {
  if (address >> iNumMemoryBits) // Non Memory
    return 0;

  return &(((char *)memory)[(int)address]);
}

/**
 * Register a device as being a CPU. Return the CPU number.
 **/
int CSystem::RegisterCPU(class CAlphaCPU *cpu) {
  if (iNumCPUs >= 4)
    return -1;
  acCPUs[iNumCPUs] = cpu;
  iNumCPUs++;
  return iNumCPUs - 1;
}

/**
 * Reserve a range of the 64-bit system address space for a device.
 **/
int CSystem::RegisterMemory(CSystemComponent *component, int index, u64 base,
                            u64 length) {
  struct SMemoryUser *m;
  int i;

#if defined(CHECK_MEM_RANGES)
  for (i = 0; i < iNumMemories; i++) {
    if (component == asMemories[i]->component)
      continue;

    // check for overlaps
    if (base >= asMemories[i]->base &&
        base <= (asMemories[i]->base + asMemories[i]->length - 1)) {
      printf(
          "WARNING: Start address for %s/%d (%016" PRIx64 "-%016" PRIx64 ")\n"
          "  is within memory range of %s/%d (%016" PRIx64 "-%016" PRIx64
          ").\n",
          component->devid_string, index, base, base + length - 1,
          asMemories[i]->component->devid_string, asMemories[i]->index,
          asMemories[i]->base, asMemories[i]->base + asMemories[i]->length - 1);
    }

    if (base + length - 1 >= asMemories[i]->base &&
        base + length - 1 <=
            (asMemories[i]->base + asMemories[i]->length - 1)) {
      printf("WARNING: End address for %s/%d (%016" PRIx64 "-%016" PRIx64 ")\n"
             "  is within memory range of %s/%d (%016" PRIx64 "-%016" PRIx64
             ").\n",
             component->devid_string, index, base, base + length - 1,
             asMemories[i]->component->devid_string, asMemories[i]->index,
             asMemories[i]->base,
             asMemories[i]->base + asMemories[i]->length - 1);
    }
  }
#endif // defined(CHECK_MEM_RANGES)

  for (i = 0; i < iNumMemories; i++) {
    if ((asMemories[i]->component == component) &&
        (asMemories[i]->index == index)) {
      asMemories[i]->base = base;
      asMemories[i]->length = length;
      return 0;
    }
  }

  CHECK_ALLOCATION(
      m = (struct SMemoryUser *)malloc(sizeof(struct SMemoryUser)));
  m->component = component;
  m->base = base;
  m->length = length;
  m->index = index;

  asMemories[iNumMemories] = m;
  iNumMemories++;
  return 0;
}

int got_sigint = 0;

/**
 * Handle a SIGINT by setting a flag that terminates the emulator.
 **/
void sigint_handler(int signum) { got_sigint = 1; }

/**
 * Run the system by clocking the CPU(s) and devices.
 **/
void CSystem::Run() {
  int i;

  int k;

#if defined(DUMP_MEMMAP)
  printf("ES40 Memory Map\n");
  printf("Physical Address Size     Device/Index\n");
  printf("---------------- -------- -------------------------\n");
  for (i = 0; i < iNumMemories; i++) {
    printf("%016" PRIx64 " %8x %s/%d\n", asMemories[i]->base,
           asMemories[i]->length, asMemories[i]->component->devid_string,
           asMemories[i]->index);
  }
#endif // defined(DUMP_MEMMAP)

  /* catch CTRL-C and shutdown gracefully */
  signal(SIGINT, &sigint_handler);

  start_threads();

  for (k = 0;; k++) {
    if (got_sigint)
      FAILURE(Graceful, "CTRL-C detected");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    for (i = 0; i < iNumComponents; i++)
      acComponents[i]->check_state();
#if !defined(HIDE_COUNTER)
#if defined(PROFILE)
    printf("%d | %016" PRIx64 " | %" PRId64 " profiled instructions.  \r", k,
           acCPUs[0]->get_pc(), profiled_insts);
#else  // defined(PROFILE)
    printf("%d | %016" PRIx64 "\r", k, acCPUs[0]->get_pc());
#endif // defined(PROFILE)
#endif // defined(HIDE_COUNTER)
  }

  //  printf ("%%SYS-W-SHUTDOWN: CTRL-C or Device Failed\n");
  //  return 1;
}

/**
 * Do one clock tick. The cpu(s) will execute one single instruction, and
 * some devices may be clocked.
 **/
int CSystem::SingleStep() {
  for (int i = 0; i < iNumCPUs; i++)
    if (!acCPUs[i]->get_waiting())
      acCPUs[i]->execute();

      //  iSingleStep++;
#if defined(LS_MASTER) || defined(LS_SLAVE)
  if (!(iSingleStep % 50)) {
    lockstep_sync_m2s("sync1");
    *dbg_strptr = '\0';
    lockstep_compare(debug_string);
    dbg_strptr = debug_string;
    *dbg_strptr = '\0';
  }
#endif // defined(LS_MASTER) || defined(LS_SLAVE)

  //  if (iSingleStep >= CLOCK_RATIO)
  //  {
  //     iSingleStep = 0;
  //     for(i=0;i<iNumSlowClocks;i++)
  //     {
  //        result = acSlowClocks[i]->DoClock();
  //      if (result)
  //        return result;
  //     }
  //#ifdef IDB
  //     iSSCycles++;
  //#if !defined(LS_SLAVE)
  //     if (bHashing)
  //#endif
  //       printf("%d | %016" PRIx64 "\r",iSSCycles,acCPUs[0]->get_pc());
  //#endif
  //  }
  return 0;
}

#if defined(DEBUG_PORTACCESS)
u64 lastport;
#endif // defined(DEBUG_PORTACCESS)
void CSystem::cpu_lock(int cpuid, u64 address) {
  SCOPED_FM_LOCK(cpu_lock_mutex);

  //  printf("cpu%d: lock %" PRIx64 ".   \n",cpuid,address);
  state.cpu_lock_flags |= (1 << cpuid);
  state.cpu_lock_address[cpuid] = address;
}

bool CSystem::cpu_unlock(int cpuid) {
  SCOPED_FM_LOCK(cpu_lock_mutex);

  bool retval;
  retval = state.cpu_lock_flags & (1 << cpuid);

  //  printf("cpu%d: unlock (%s).   \n",cpuid,retval?"ok":"failed");
  state.cpu_lock_flags &= ~(1 << cpuid);
  return retval;
}

void CSystem::cpu_break_lock(int cpuid, CSystemComponent *source) {
  SCOPED_FM_LOCK(cpu_lock_mutex);
  printf("cpu%d: lock broken by %s.   \n", cpuid, source->devid_string);
  state.cpu_lock_flags &= ~(1 << cpuid);
}

/**
 * \brief Write 8, 4, 2 or 1 byte(s) to a 64-bit system address. This could be
 *memory, internal chipset registers, nothing or some device.
 *
 * Source: HRM, 10.1.1:
 *
 * System Space and Address Map
 *
 * The system address space is divided into two parts: system memory and PIO.
 *This division is indicated by physical memory bit <43> = 1 for PIO accesses
 *from the CPU, and by the PTP bit in the window registers for PTP accesses from
 *the Pchip. While the operating system may choose bit <40> instead of bit <43>
 *to represent PIO space, bit <43> is used throughout this chapter. In general,
 *bits <42:35> are don�t cares if bit <43> is asserted.
 *
 * There is 16GB of PIO space available on the 21272 chipset with 8GB assigned
 *to each Pchip. The Pchip supports up to bit <34> (35 bits total) of system
 *address. However, the Version 1 Cchip only supports 4GB of system memory (32
 *bits total). As described in Chapter 6, the CAPbus protocol between the Pchip
 *and Cchip does support up to bit <34>, as does the Cchip�s interface to the
 *CPU. The Typhoon Cchip supports 32GB of system memory (35 bits total).
 *
 * The system address space is divided as shown in the following table:
 *
 * \code
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Space             | Size   | System Address <43:0>         | Comments |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | System memory     |    4GB | 000.0000.0000 - 000.FFFF.FFFF | Cacheable and
 *prefetchable.     |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          | 8188GB | 001.0000.0000 - 7FF.FFFF.FFFF | � |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip0 PCI memory |    4GB | 800.0000.0000 - 800.FFFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | TIGbus            |    1GB | 801.0000.0000 - 801.3FFF.FFFF | addr<5:0> = 0.
 *Single byte      | |                   |        | | valid in quadword access.
 *| |                   |        |                               | 16MB
 *accessible.                |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          |    1GB | 801.4000.0000 - 801.7FFF.FFFF | � |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip0 CSRs       |  256MB | 801.8000.0000 - 801.8FFF.FFFF | addr<5:0> = 0.
 *Quadword access. |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          |  256MB | 801.9000.0000 - 801.9FFF.FFFF | � |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Cchip CSRs        |  256MB | 801.A000.0000 - 801.AFFF.FFFF | addr<5:0> = 0.
 *Quadword access. |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Dchip CSRs        |  256MB | 801.B000.0000 - 801.BFFF.FFFF | addr<5:0> = 0.
 *All eight bytes  | |                   |        | | in quadword access must be
 *| |                   |        |                               | identical. |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          |  768MB | 801.C000.0000 - 801.EFFF.FFFF | � | | Reserved
 *|  128MB | 801.F000.0000 - 801.F7FF.FFFF | �                               |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip 0 PCI IACK  |   64MB | 801.F800.0000 - 801.FBFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip0 PCI I/O    |   32MB | 801.FC00.0000 - 801.FDFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip0 PCI conf   |   16MB | 801.FE00.0000 - 801.FEFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          |   16MB | 801.FF00.0000 - 801.FFFF.FFFF | � |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip1 PCI memory |    4GB | 802.0000.0000 - 802.FFFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          |    2GB | 803.0000.0000 - 803.7FFF.FFFF | � |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip1 CSRs       |  256MB | 803.8000.0000 - 803.8FFF.FFFF | addr<5:0> = 0,
 *quadword access. |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          | 1536MB | 803.9000.0000 - 803.EFFF.FFFF | � | | Reserved
 *|  128MB | 803.F000.0000 - 803.F7FF.FFFF | �                               |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip 1 PCI IACK  |   64MB | 803.F800.0000 - 803.FBFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip1 PCI I/O    |   32MB | 803.FC00.0000 - 803.FDFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip1 PCI conf   |   16MB | 803.FE00.0000 - 803.FEFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          |   16MB | 803.FF00.0000 - 803.FFFF.FFFF | � | | Reserved
 *| 8172GB | 804.0000.0000 - FFF.FFFF.FFFF | Bits <42:35> are don�t cares if |
 * |                   |        |                               | bit <43> is
 *asserted.           |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * \endcode
 **/
void CSystem::WriteMem(u64 address, int dsize, u64 data,
                       CSystemComponent *source) {
  u64 a;
  int i;
  u8 *p;
#if defined(ALIGN_MEM_ACCESS)
  u64 t64;
  u32 t32;
  u16 t16;
#endif // defined(ALIGN_MEM_ACCESS)
  if (state.cpu_lock_flags) {
    for (i = 0; i < iNumCPUs; i++) {
      if ((state.cpu_lock_flags & (1 << i)) &&
          (!((state.cpu_lock_address[i] ^ address) &
             U64(0x00000807ffffff00))) &&
          (source != acCPUs[i]))
        cpu_break_lock(i, source);
    }
  }

  a = address & U64(0x00000807ffffffff);

  if (a >> iNumMemoryBits) // non-memory
  {

    // check registered device memory ranges
    for (i = 0; i < iNumMemories; i++) {
      if ((a >= asMemories[i]->base) &&
          (a < asMemories[i]->base + asMemories[i]->length)) {
        asMemories[i]->component->WriteMem(
            asMemories[i]->index, a - asMemories[i]->base, dsize, data);
        return;
      }
    }

    if ((a == U64(0x00000801FC000CF8)) && (dsize == 32)) {
      state.cf8_address[0] = (u32)data & 0x00ffffff;
      return;
    }

    if ((a == U64(0x00000803FC000CF8)) && (dsize == 32)) {
      state.cf8_address[1] = (u32)data & 0x00ffffff;
      return;
    }

    if ((a == U64(0x00000801FC000CFC)) && (dsize == 32)) {
      printf("PCI 0 config space write through CF8/CFC mechanism.   \n");
      getc(stdin);
      WriteMem(U64(0x00000801FE000000) | state.cf8_address[0], dsize, data,
               source);
      return;
    }

    if ((a == U64(0x00000803FC000CFC)) && (dsize == 32)) {
      printf("PCI 1 config space write through CF8/CFC mechanism.   \n");
      getc(stdin);
      WriteMem(U64(0x00000803FE000000) | state.cf8_address[1], dsize, data,
               source);
      return;
    }

    if (a >= U64(0x00000801A0000000) && a <= U64(0x00000801AFFFFFFF)) {
      cchip_csr_write((u32)a & 0xFFFFFFF, data, source);
      return;
    }

    if (a >= U64(0x0000080180000000) && a <= U64(0x000008018FFFFFFF)) {
      pchip_csr_write(0, (u32)a & 0xFFFFFFF, data);
      return;
    }

    if (a >= U64(0x0000080380000000) && a <= U64(0x000008038FFFFFFF)) {
      pchip_csr_write(1, (u32)a & 0xFFFFFFF, data);
      return;
    }

    if (a >= U64(0x00000801B0000000) && a <= U64(0x00000801BFFFFFFF)) {
      dchip_csr_write((u32)a & 0xFFFFFFF, (u8)data & 0xff);
      return;
    }

    if (a >= U64(0x0000080100000000) && a <= U64(0x000008013FFFFFFF)) {
      tig_write((u32)a & 0x3FFFFFFF, (u8)data);
      return;
    }

    if (a >= U64(0x801fc000000) && a < U64(0x801fe000000)) {

      // Unused PCI I/O space
      //      if (source)
      //        printf("Write to unknown IO port %" LL"x on PCI 0 from %s \n",a
      //        & U64(0x1ffffff),source->devid_string);
      //      else
      //        printf("Write to unknown IO port %" LL"x on PCI 0   \n",a &
      //        U64(0x1ffffff));
      return;
    }

    if (a >= U64(0x803fc000000) && a < U64(0x803fe000000)) {

      // Unused PCI I/O space
      if (source) {
        printf("Write to unknown IO port %" PRIx64 " on PCI 1 from %s   \n",
               a & U64(0x1ffffff), source->devid_string);
      } else
        printf("Write to unknown IO port %" PRIx64 " on PCI 1   \n",
               a & U64(0x1ffffff));
      return;
    }

    if (a >= U64(0x80000000000) && a < U64(0x80100000000)) {

      // Unused PCI memory space
      u64 paddr = a & U64(0xffffffff);
      if (paddr > 0xb8fff || paddr < 0xb8000) { // skip legacy video
        if (source) {
          printf("Write to unknown memory %" PRIx64 " on PCI 0 from %s   \n",
                 a & U64(0xffffffff), source->devid_string);
        } else
          printf("Write to unknown memory %" PRIx64 " on PCI 0   \n",
                 a & U64(0xffffffff));
      }
    }

    if (a >= U64(0x80200000000) && a < U64(0x80300000000)) {

      // Unused PCI memory space
      if (source) {
        printf("Write to unknown memory %" PRIx64 " on PCI 1 from %s   \n",
               a & U64(0xffffffff), source->devid_string);
      } else
        printf("Write to unknown memory %" PRIx64 " on PCI 1   \n",
               a & U64(0xffffffff));
      return;
    }

#ifdef DEBUG_UNKMEM
    if (source)
      printf("Write to unknown memory %" PRIx64 " from %s   \n", a,
             source->devid_string);
    else
      printf("Write to unknown memory %" PRIx64 "   \n", a);
#endif // defined(DEBUG_UNKMEM)
    return;
  }

  p = (u8 *)memory + a;

  switch (dsize) {
  case 8:
    *((u8 *)p) = (u8)data;
    break;
  case 16:
    *((u16 *)p) = endian_16((u16)data);
    break;
  case 32:
    *((u32 *)p) = endian_32((u32)data);
    break;
  default:
    *((u64 *)p) = endian_64((u64)data);
  }
}

/**
 * \brief Read 8, 4, 2 or 1 byte(s) from a 64-bit system address. This could be
 *memory, internal chipset registers, nothing or some device.
 *
 * Source: HRM, 10.1.1:
 *
 * System Space and Address Map
 *
 * The system address space is divided into two parts: system memory and PIO.
 *This division is indicated by physical memory bit <43> = 1 for PIO accesses
 *from the CPU, and by the PTP bit in the window registers for PTP accesses from
 *the Pchip. While the operating system may choose bit <40> instead of bit <43>
 *to represent PIO space, bit <43> is used throughout this chapter. In general,
 *bits <42:35> are don�t cares if bit <43> is asserted.
 *
 * There is 16GB of PIO space available on the 21272 chipset with 8GB assigned
 *to each Pchip. The Pchip supports up to bit <34> (35 bits total) of system
 *address. However, the Version 1 Cchip only supports 4GB of system memory (32
 *bits total). As described in Chapter 6, the CAPbus protocol between the Pchip
 *and Cchip does support up to bit <34>, as does the Cchip�s interface to the
 *CPU. The Typhoon Cchip supports 32GB of system memory (35 bits total).
 *
 * The system address space is divided as shown in the following table:
 *
 * \code
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Space             | Size   | System Address <43:0>         | Comments |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | System memory     |    4GB | 000.0000.0000 - 000.FFFF.FFFF | Cacheable and
 *prefetchable.     |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          | 8188GB | 001.0000.0000 - 7FF.FFFF.FFFF | � |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip0 PCI memory |    4GB | 800.0000.0000 - 800.FFFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | TIGbus            |    1GB | 801.0000.0000 - 801.3FFF.FFFF | addr<5:0> = 0.
 *Single byte      | |                   |        | | valid in quadword access.
 *| |                   |        |                               | 16MB
 *accessible.                |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          |    1GB | 801.4000.0000 - 801.7FFF.FFFF | � |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip0 CSRs       |  256MB | 801.8000.0000 - 801.8FFF.FFFF | addr<5:0> = 0.
 *Quadword access. |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          |  256MB | 801.9000.0000 - 801.9FFF.FFFF | � |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Cchip CSRs        |  256MB | 801.A000.0000 - 801.AFFF.FFFF | addr<5:0> = 0.
 *Quadword access. |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Dchip CSRs        |  256MB | 801.B000.0000 - 801.BFFF.FFFF | addr<5:0> = 0.
 *All eight bytes  | |                   |        | | in quadword access must be
 *| |                   |        |                               | identical. |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          |  768MB | 801.C000.0000 - 801.EFFF.FFFF | � | | Reserved
 *|  128MB | 801.F000.0000 - 801.F7FF.FFFF | �                               |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip 0 PCI IACK  |   64MB | 801.F800.0000 - 801.FBFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip0 PCI I/O    |   32MB | 801.FC00.0000 - 801.FDFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip0 PCI conf   |   16MB | 801.FE00.0000 - 801.FEFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          |   16MB | 801.FF00.0000 - 801.FFFF.FFFF | � |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip1 PCI memory |    4GB | 802.0000.0000 - 802.FFFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          |    2GB | 803.0000.0000 - 803.7FFF.FFFF | � |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip1 CSRs       |  256MB | 803.8000.0000 - 803.8FFF.FFFF | addr<5:0> = 0,
 *quadword access. |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          | 1536MB | 803.9000.0000 - 803.EFFF.FFFF | � | | Reserved
 *|  128MB | 803.F000.0000 - 803.F7FF.FFFF | �                               |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip 1 PCI IACK  |   64MB | 803.F800.0000 - 803.FBFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip1 PCI I/O    |   32MB | 803.FC00.0000 - 803.FDFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Pchip1 PCI conf   |   16MB | 803.FE00.0000 - 803.FEFF.FFFF | Linear
 *addressing.              |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * | Reserved          |   16MB | 803.FF00.0000 - 803.FFFF.FFFF | � | | Reserved
 *| 8172GB | 804.0000.0000 - FFF.FFFF.FFFF | Bits <42:35> are don�t cares if |
 * |                   |        |                               | bit <43> is
 *asserted.           |
 * +-------------------+--------+-------------------------------+---------------------------------+
 * \endcode
 **/
u64 CSystem::ReadMem(u64 address, int dsize, CSystemComponent *source) {
  u64 a;
  int i;
  u8 *p;

  a = address & U64(0x00000807ffffffff);
  if (a >> iNumMemoryBits) // Non Memory
  {

    // check registered device memory ranges
    for (i = 0; i < iNumMemories; i++) {
      if ((a >= asMemories[i]->base) &&
          (a < asMemories[i]->base + asMemories[i]->length))
        return asMemories[i]->component->ReadMem(
            asMemories[i]->index, a - asMemories[i]->base, dsize);
    }

    if ((a == U64(0x00000801FC000CFC)) && (dsize == 32)) {
      printf("PCI 0 config space read through CF8/CFC mechanism.   \n");
      getc(stdin);
      return ReadMem(U64(0x00000801FE000000) | state.cf8_address[0], dsize,
                     source);
    }

    if ((a == U64(0x00000803FC000CFC)) && (dsize == 32)) {
      printf("PCI 1 config space read through CF8/CFC mechanism.   \n");
      getc(stdin);
      return ReadMem(U64(0x00000803FE000000) | state.cf8_address[1], dsize,
                     source);
    }

    if (a >= U64(0x00000801A0000000) && a <= U64(0x00000801AFFFFFFF))
      return cchip_csr_read((u32)a & 0xFFFFFFF, source);

    if (a >= U64(0x0000080180000000) && a <= U64(0x000008018FFFFFFF))
      return pchip_csr_read(0, (u32)a & 0xFFFFFFF);

    if (a >= U64(0x0000080380000000) && a <= U64(0x000008038FFFFFFF))
      return pchip_csr_read(1, (u32)a & 0xFFFFFFF);

    if (a >= U64(0x00000801B0000000) && a <= U64(0x00000801BFFFFFFF))
      return dchip_csr_read((u32)a & 0xFFFFFFF) * U64(0x0101010101010101);

    if (a >= U64(0x0000080100000000) && a <= U64(0x000008013FFFFFFF))
      return tig_read((u32)a & 0x3FFFFFFF);

    if ((a >= U64(0x801fe000000) && a < U64(0x801ff000000)) ||
        (a >= U64(0x803fe000000) && a < U64(0x803ff000000))) {

      // Unused PCI configuration space
      switch (dsize) {
      case 8:
        return X64_BYTE;
      case 16:
        return X64_WORD;
      case 32:
        return X64_LONG;
      case 64:
        return X64_QUAD;
      }
    }

    if (a >= U64(0x800000c0000) && a < U64(0x801000e0000)) {

      // Unused PCI ROM BIOS space
      return 0;
    }

    if (a >= U64(0x801fc000000) && a < U64(0x801fe000000)) {

      // Unused PCI I/O space
      // if (source)
      //  printf("Read from unknown IO port %" LL"x on PCI 0 from %s   \n",a &
      //  U64(0x1ffffff),source->devid_string);
      // else
      //  printf("Read from unknown IO port %" LL"x on PCI 0   \n",a &
      //  U64(0x1ffffff));
      return 0;
    }

    if (a >= U64(0x803fc000000) && a < U64(0x803fe000000)) {

      // Unused PCI I/O space
      if (source) {
        printf("Read from unknown IO port %" PRIx64 " on PCI 1 from %s   \n",
               a & U64(0x1ffffff), source->devid_string);
      } else
        printf("Read from unknown IO port %" PRIx64 " on PCI 1   \n",
               a & U64(0x1ffffff));
      return 0;
    }

    if (a >= U64(0x80000000000) && a < U64(0x80100000000)) {

      // Unused PCI memory space
      u64 paddr = a & U64(0xffffffff);
      if (paddr > 0xb8fff || paddr < 0xb8000) { // skip legacy video
        if (source) {
          printf("Read from unknown memory %" PRIx64 " on PCI 0 from %s   \n",
                 a & U64(0xffffffff), source->devid_string);
        } else
          printf("Read from unknown memory %" PRIx64 " on PCI 0   \n",
                 a & U64(0xffffffff));
      }

      return 0;
    }

    if (a >= U64(0x80200000000) && a < U64(0x80300000000)) {

      // Unused PCI memory space
      if (source) {
        printf("Read from unknown memory %" PRIx64 " on PCI 1 from %s   \n",
               a & U64(0xffffffff), source->devid_string);
      } else
        printf("Read from unknown memory %" PRIx64 " on PCI 1   \n",
               a & U64(0xffffffff));
      return 0;
    }

#if defined(DEBUG_UNKMEM)
    if (source)
      printf("Read from unknown memory %" PRIx64 " from %s   \n", a,
             source->devid_string);
    else
      printf("Read from unknown memory %" PRIx64 "   \n", a);
#endif // defined(DEBUG_UNKMEM)
    return 0x00;

    //                    return 0x77; // 7f
  }

  p = (u8 *)memory + a;

  switch (dsize) {
  case 8:
    return *((u8 *)p);
  case 16:
    return endian_16(*((u16 *)p));
  case 32:
    return endian_32(*((u32 *)p));
  default:
    return endian_64(*((u64 *)p));
  }
}

/**
 * \brief Read one of the PCHIP registers.
 *
 * Source: HRM, 10.2.5:
 *
 * \code
 * +-------------+---------------+------+----+
 * |Register     | Address       | Type | ## |
 * +-------------+---------------+------+----+
 * | P0-WSBA0    | 801.8000.0000 | RW   | 00 |
 * | P0-WSBA1    | 801.8000.0040 | RW   | 01 |
 * | P0-WSBA2    | 801.8000.0080 | RW   | 02 |
 * | P0-WSBA3    | 801.8000.00C0 | RW   | 03 |
 * +-------------+---------------+------+----+
 * | P0-WSM0     | 801.8000.0100 | RW   | 04 |
 * | P0-WSM1     | 801.8000.0140 | RW   | 05 |
 * | P0-WSM2     | 801.8000.0180 | RW   | 06 |
 * | P0-WSM3     | 801.8000.01C0 | RW   | 07 |
 * +-------------+---------------+------+----+
 * | P0-TBA0     | 801.8000.0200 | RW   | 08 |
 * | P0-TBA1     | 801.8000.0240 | RW   | 09 |
 * | P0-TBA2     | 801.8000.0280 | RW   | 0A |
 * | P0-TBA3     | 801.8000.02C0 | RW   | 0B |
 * +-------------+---------------+------+----+
 * | P0-PCTL     | 801.8000.0300 | RW   | 0C |
 * +-------------+---------------+------+----+
 * | P0-PLAT     | 801.8000.0340 | RW   | 0D |
 * +-------------+---------------+------+----+
 * | P0-RES      | 801.8000.0380 | RW   | 0E |
 * +-------------+---------------+------+----+
 * | P0-PERROR   | 801.8000.03C0 | RW   | 0F |
 * | P0-PERRMASK | 801.8000.0400 | RW   | 10 |
 * | P0-PERRSET  | 801.8000.0440 | WO   | 11 |
 * +-------------+---------------+------+----+
 * | P0-TLBIV    | 801.8000.0480 | WO   | 12 |
 * | P0-TLBIA    | 801.8000.04C0 | WO   | 13 |
 * +-------------+---------------+------+----+
 * | P0-PMONCTL  | 801.8000.0500 | RW   | 14 |
 * | P0-PMONCNT  | 801.8000.0540 | RO   | 15 |
 * +-------------+---------------+------+----+
 * | P0-SPRST    | 801.8000.0800 | WO   | 20 |
 * +-------------+---------------+------+----+
 * \endcode
 *
 * Window Space Base Address Register (WSBAn - RW)
 *
 * Because the information in the WSBAn registers and WSMn registers
 * is used to compare against the PCI address, a clock-domain crossing (from
 * i_sysclk to i_pclko<7:0>) is made when these registers are written.
 *Therefore, for a period of several clock cycles, a window is disabled when its
 *contents are disabled. If PCI bus activity, which accesses the window in
 *question, is not stopped before updating that window, the Pchip might fail to
 *respond with b_devsel_l when it should. This would result in a master abort
 *condition on the PCI bus. Therefore, before a window (base or mask) is
 *updated, all PCI activity accessing that window must be stopped, even if only
 *some activity is being added or deleted.
 *
 * The contents of the window may be read back to confirm that the update has
 *taken place. Then PCI activity through that window can be resumed.
 *
 * \code
 * +-------+---------+---------+------+--------------------------+
 * | Field | Bits    | Type    | Init | Description              |
 * +-------+---------+---------+------+--------------------------+
 * | RES   | <63:40> | MBZ,RAZ | 0    | Reserved                 |
 * +-------+---------+---------+------+--------------------------+
 * | DAC   | <39>    | RW      | 0    | DAC enable (WSBA3 only!) |
 * +-------+---------+---------+------+--------------------------+
 * | RES   | <38:32> | MBZ,RAZ | 0    | Reserved                 |
 * +-------+---------+---------+------+--------------------------+
 * | ADDR  | <31:20> | RW      | 0    | Base address (not used   |
 * |       |         |         |      | DAC enable = 1)          |
 * +-------+---------+---------+------+--------------------------+
 * | RES   | <19:2>  | MBZ,RAZ | 0    | Reserved                 |
 * +-------+---------+---------+------+--------------------------+
 * | SG    | <1>     | RW      | 0    | Scatter-gather           |
 * +-------+---------+---------+------+--------------------------+
 * | ENA   | <0>     | RW      | 0    | Enable                   |
 * +-------+---------+---------+------+--------------------------+
 * \endcode
 *
 * Window Space Mask Register (WSM0, WSM1, WSM2, WSM3 - RW)
 *
 * \code
 * +-------+---------+---------+------+--------------------------+
 * | Field | Bits    | Type    | Init | Description              |
 * +-------+---------+---------+------+--------------------------+
 * | RES   | <63:32> | MBZ,RAZ | 0    | Reserved                 |
 * +-------+---------+---------+------+--------------------------+
 * | AM    | <31:20> | RW      | 0    | Address mask             |
 * +-------+---------+---------+------+--------------------------+
 * | RES   | <19:0>  | MBZ,RAZ | 0    | Reserved                 |
 * +-------+---------+---------+------+--------------------------+
 * \endcode
 *
 * Translated Base Address Register (TBAn - RW)
 *
 * \code
 * +-------+---------+---------+------+--------------------------+
 * | Field | Bits    | Type    | Init | Description              |
 * +-------+---------+---------+------+--------------------------+
 * | RES   | <63:35> | MBZ,RAZ | 0    | Reserved                 |
 * +-------+---------+---------+------+--------------------------+
 * | ADDR  | <34:10> | RW      | 0    | Translated base address  |
 * |       |         |         |      | (if DAC enable = 1, bits |
 * |       |         |         |      | <34:22> are the PT Origin|
 * |       |         |         |      | address <34:22> and bits |
 * |       |         |         |      | <21:10> are ignored)     |
 * +-------+---------+---------+------+--------------------------+
 * | RES   | <9:0>   | MBZ,RAZ | 0    | Reserved                 |
 * +-------+---------+---------+------+--------------------------+
 * \encode
 *
 * Pchip Control Register (PCTL - RW)
 *
 * \code
 * +---------+---------+---------+------+-------------------------------------+
 * | Field   | Bits    | Type    | Init | Description                         |
 * +---------+---------+---------+------+-------------------------------------+
 * | RES     | <63:48> | MBZ,RAZ | 0    | Reserved.                           |
 * +---------+---------+---------+------+-------------------------------------+
 * | PID     | <47:46> | RO      | 1)   | Pchip ID.                           |
 * +---------+---------+---------+------+-------------------------------------+
 * | RPP     | <45>    | RO      | 2)   | Remote Pchip present.               |
 * +---------+---------+---------+------+-------------------------------------+
 * | PTEVRFY | <44>    | RW      |      | PTE verify for DMA read.            |
 * |         |         |         |      |   Val Description                   |
 * |         |         |         |      |   0   If TLB miss, then make DMA    |
 * |         |         |         |      |       read request as soon as possi-|
 * |         |         |         |      |       ble and discard data if PTE   |
 * |         |         |         |      |       was not valid - could cause   |
 * |         |         |         |      |       Cchip nonexistent mem. error. |
 * |         |         |         |      |   1   If TLB miss, then delay read  |
 * |         |         |         |      |       request until PTE is verified |
 * |         |         |         |      |       as valid - no request if not  |
 * |         |         |         |      |       valid.                        |
 * +---------+---------+---------+------+-------------------------------------+
 * | FDWDIS  | <43>    | RW      |      | Fast DMA read cache block wrap      |
 * |         |         |         |      | request disable.                    |
 * |         |         |         |      |   Val Description                   |
 * |         |         |         |      |   0   Normal operation.             |
 * |         |         |         |      |   1   Reserved for testing purposes |
 * |         |         |         |      |       only.                         |
 * +---------+---------+---------+------+-------------------------------------+
 * | FDSDIS  | <42>    | RW      |      | Fast DMA start and SGTE request     |
 * |         |         |         |      | disable.                            |
 * |         |         |         |      |   Val Description                   |
 * |         |         |         |      |   0   Normal operation.             |
 * |         |         |         |      |   1   Reserved for testing purposes |
 * |         |         |         |      |       only.                         |
 * +---------+---------+---------+------+-------------------------------------+
 * | PCLKX   | <41:40> | RO      | 3)   | PCI clock frequency multiplier      |
 * |         |         |         |      |   Val Multiplier                    |
 * |         |         |         |      |   0   x6                            |
 * |         |         |         |      |   1   x4                            |
 * |         |         |         |      |   2   x5                            |
 * |         |         |         |      |   3   Reserved                      |
 * +---------+---------+---------+------+-------------------------------------+
 * | PTPMAX  | <39:36> | RW      | 2    | Maximum PTP requests to Cchip from  |
 * |         |         |         |      | both Pchips until returned on       |
 * |         |         |         |      | CAPbus, modulo 16 (minimum = 2)     |
 * |         |         |         |      | (use 4 for pass 1 Cchip and Dchip). |
 * +---------+---------+---------+------+-------------------------------------+
 * | CRQMAX  | <35:32> | RW      | 1    | Maximum requests to Cchip from both |
 * |         |         |         |      | Pchips until Ack, modulo 16 (use 4  |
 * |         |         |         |      | for Cchip). (Use 3 or less for      |
 * |         |         |         |      | Typhoon because there is one less   |
 * |         |         |         |      | skid buffer in the C4 chip.)        |
 * +---------+---------+---------+------+-------------------------------------+
 * | REV     | <31:24> | RO      | 0    | In conjunction with the state of    |
 * |         |         |         |      | PMONCTL<0>, this field indicates    |
 * |         |         |         |      | the revision of the Pchip.          |
 * +---------+---------+---------+------+-------------------------------------+
 * | CDQMAX  | <23:20> | RW      | 1    | Maximum data transfers to Dchips    |
 * |         |         |         |      | from both Pchips until Ack, modulo  |
 * |         |         |         |      | 16 (use 4 for Dchip). Must be same  |
 * |         |         |         |      | as Cchip CSR CSC<FPQPMAX>.          |
 * +---------+---------+---------+------+-------------------------------------+
 * | PADM    | <19>    | RW      | 4)   | PADbus mode.                        |
 * |         |         |         |      |   Val Mode                          |
 * |         |         |         |      |   0   8-nibble, 8-check bit mode    |
 * |         |         |         |      |   1   4-byte, 4-check bit mode      |
 * +---------+---------+---------+------+-------------------------------------+
 * | ECCEN   | <18>    | RW      | 0    | ECC enable for DMA and SGTE access. |
 * +---------+---------+---------+------+-------------------------------------+
 * | RES     | <17:16> | MBZ,RAZ | 0    | Reserved.                           |
 * +---------+---------+---------+------+-------------------------------------+
 * | PPRI    | <15>    |         | 0    | Arbiter prio group for the Pchip.   |
 * +---------+---------+---------+------+-------------------------------------+
 * | PRIGRP  | <14:8>  | RW      | 0    | Arbiter prio group; one bit per PCI |
 * |         |         |         |      | slot with bits <14:8> corresponding |
 * |         |         |         |      | to input b_req_l<6:0>.              |
 * |         |         |         |      |   Val Group                         |
 * |         |         |         |      |   0   Low-priority group            |
 * |         |         |         |      |   1   High-priority group           |
 * +---------+---------+---------+------+-------------------------------------+
 * | ARBENA  | <7>     | RW      | 0    | Internal arbiter enable.            |
 * +---------+---------+---------+------+-------------------------------------+
 * | MWIN    | <6>     | RW      | 0    | Monster window enable.              |
 * +---------+---------+---------+------+-------------------------------------+
 * | HOLE    | <5>     | RW      | 0    | 512KB-to-1MB window hole enable.    |
 * +---------+---------+---------+------+-------------------------------------+
 * | TGTLAT  | <4>     | RW      | 0    | Target latency timers enable.       |
 * |         |         |         |      |   Val Mode                          |
 * |         |         |         |      |   0   Retry/disconnect after 128    |
 * |         |         |         |      |       PCI clocks without data.      |
 * |         |         |         |      |   1   Retry initial request after   |
 * |         |         |         |      |       32 PCI clocks without data;   |
 * |         |         |         |      |       disconnect subsequent trans-  |
 * |         |         |         |      |       fers after 8 PCI clocks       |
 * |         |         |         |      |       without data.                 |
 * +---------+---------+---------+------+-------------------------------------+
 * | CHAINDIS| <3>     | RW      | 0    | Disable chaining.                   |
 * +---------+---------+---------+------+-------------------------------------+
 * | THDIS   | <2>     | RW      | 0    | Disable antithrash mechan. for TLB. |
 * |         |         |         |      |   Val Mode                          |
 * |         |         |         |      |   0   Normal operation              |
 * |         |         |         |      |   1   Testing purposes only         |
 * +---------+---------+---------+------+-------------------------------------+
 * | FBTB    | <1>     | RW      | 0    | Fast back-to-back enable.           |
 * +---------+---------+---------+------+-------------------------------------+
 * | FDSC    | <0>     | RW      | 0    | Fast discard enable.                |
 * |         |         |         |      |   Val Mode                          |
 * |         |         |         |      |   0   Discard data if no retry      |
 * |         |         |         |      |       after 215 PCI clocks.         |
 * |         |         |         |      |   1   Discard data if no retry      |
 * |         |         |         |      |       after 210 PCI clocks.         |
 * +---------+---------+---------+------+-------------------------------------+
 *
 * 1) This field is initialized from the PID pins.
 * 2) This field is initialized from the assertion of CREQRMT_L pin at system
 *reset. 3) This field is initialized from the PCI i_pclkdiv<1:0> pins. 4) This
 *field is initialized from a decode of the b_cap<1:0> pins. \endcode
 *
 * Pchip Error Register (PERROR - RW)
 *
 * If any of bits <11:0> are set, then this entire register is frozen and the
 *Pchip output signal b_error is asserted. Only bit <0> can be set after that.
 *All other values will be held until all of bits <11:0> are clear. When an
 *error is detected and one of bits <11:0> becomes set, the associated
 *information is captured in bits <63:16> of this register. After the
 *information is captured, the INV bit is cleared, but the information is not
 *valid and should not be used if INV is set.
 *
 * In rare circumstances involving more than one error, INV may remain set
 *because the Pchip cannot correctly capture the SYN, CMD, or ADDR field.
 *
 * Furthermore, if software reads PERROR in a polling loop, or reads PERROR
 *before the Pchip�s error signal is reflected in the Cchip�s DRIR CSR, the INV
 *bit may also be set.
 *
 * To avoid the latter condition, read PERROR only after receiving an IRQ0
 *interrupt, then read the Cchip DIR CSR to determine that this Pchip has
 *detected an error.
 *
 * \code
 * +---------+---------+---------+------+-------------------------------------+
 * | Field   | Bits    | Type    | Init | Description                         |
 * +---------+---------+---------+------+-------------------------------------+
 * | SYN     | <63:56> | RO      | 0    | errors ECC syndrome if CRE or UECC. |
 * +---------+---------+---------+------+-------------------------------------+
 * | CMD     | <55:52> | RO      | 0    | PCI command of transaction when     |
 * |         |         |         |      | error detected if not CRE and not   |
 * |         |         |         |      | UECC. If CRE or UECC, then:         |
 * |         |         |         |      |   Val    Command                    |
 * |         |         |         |      |   0000   DMA read                   |
 * |         |         |         |      |   0001   DMA RMW                    |
 * |         |         |         |      |   0011   SGTE read                  |
 * |         |         |         |      |   Others Reserved                   |
 * +---------+---------+---------+------+-------------------------------------+
 * | INV     | <51>    | RO Rev1 | 0    | Info Not Valid - only meaningful    |
 * |         |         | RAZ Rev0|      | when one of bits <11:0> is set.     |
 * |         |         |         |      | Indicates validity of <SYN>, <CMD>, |
 * |         |         |         |      | and <ADDR> fields.                  |
 * |         |         |         |      |   Val Mode                          |
 * |         |         |         |      |   0   Info fields are valid.        |
 * |         |         |         |      |   1   Info fields are not valid.    |
 * +---------+---------+---------+------+-------------------------------------+
 * | ADDR    | <50:16> | RO      | 0    | If CRE or UECC, then ADDR<50:19> =  |
 * |         |         |         |      | system address <34:3> of erroneous  |
 * |         |         |         |      | quadword and ADDR<18:16> = 0.       |
 * |         |         |         |      | If not CRE and not UECC, then       |
 * |         |         |         |      | ADDR<50:48> = 0; ADDR<47:18> = star-|
 * |         |         |         |      | ting PCI address <31:2> of trans-   |
 * |         |         |         |      | action when error was detected;     |
 * |         |         |         |      | ADDR<17:16> = 00 --> not a DAC      |
 * |         |         |         |      |                      operation;     |
 * |         |         |         |      | ADDR<17:16> = 01 --> via DAC SG     |
 * |         |         |         |      |                      Window 3;      |
 * |         |         |         |      | ADDR<17> = 1 --> via Monster Window |
 * +---------+---------+---------+------+-------------------------------------+
 * | RES     | <15:12> | MBZ,RAZ | 0    | Reserved.                           |
 * +---------+---------+---------+------+-------------------------------------+
 * | CRE     | <11>    | R,W1C   | 0    | Correctable ECC error.              |
 * +---------+---------+---------+------+-------------------------------------+
 * | UECC    | <10>    | R,W1C   | 0    | Uncorrectable ECC error.            |
 * +---------+---------+---------+------+-------------------------------------+
 * | RES     | <9>     | MBZ,RAZ | 0    | Reserved.                           |
 * +---------+---------+---------+------+-------------------------------------+
 * | NDS     | <8>     | R,W1C   | 0    | No b_devsel_l as PCI master.        |
 * +---------+---------+---------+------+-------------------------------------+
 * | RDPE    | <7>     | R,W1C   | 0    | PCI read data parity error as PCI   |
 * |         |         |         |      | master.                             |
 * +---------+---------+---------+------+-------------------------------------+
 * | TA      | <6>     | R,W1C   | 0    | Target abort as PCI master.         |
 * +---------+---------+---------+------+-------------------------------------+
 * | APE     | <5>     | R,W1C   | 0    | Address parity error detected as    |
 * |         |         |         |      | potential PCI target.               |
 * +---------+---------+---------+------+-------------------------------------+
 * | SGE     | <4>     | R,W1C   | 0    | Scatter-gather had invalid page     |
 * |         |         |         |      | table entry.                        |
 * +---------+---------+---------+------+-------------------------------------+
 * | DCRTO   | <3>     | R,W1C   | 0    | Delayed completion retry timeout as |
 * |         |         |         |      | PCI target.                         |
 * +---------+---------+---------+------+-------------------------------------+
 * | PERR    | <2>     | R,W1C   | 0    | b_perr_l sampled asserted.          |
 * +---------+---------+---------+------+-------------------------------------+
 * | SERR    | <1>     | R,W1C   | 0    | b_serr_l sampled asserted.          |
 * +---------+---------+---------+------+-------------------------------------+
 * | LOST    | <0>     | R,W1C   | 0    | Lost an error because it was detec- |
 * |         |         |         |      | ted after this register was frozen, |
 * |         |         |         |      | or while in the process of clearing |
 * |         |         |         |      | this register.                      |
 * +---------+---------+---------+------+-------------------------------------+
 * \endcode
 *
 * Pchip Error Mask Register (PERRMASK - RW)
 *
 * If any of the MASK bits have the value 0, they prevent the setting of the
 *corresponding bit in the PERROR register, regardless of the detection of
 *errors or writing to PERRSET.
 *
 * The default is for all errors to be disabled.
 *
 * Beside masking the reporting of errors in PERROR, certain bits of PERRMASK
 *have the following additional effects:
 *   - If PERRMASK<RDPE> = 0, the Pchip ignores read data parity as the PCI
 *master.
 *   - If PERRMASK<PERR> = 0, the Pchip ignores write data parity as the PCI
 *target.
 *   - If PERRMASK<APE> = 0, the Pchip ignores address parity.
 *   .
 *
 * \code
 * +---------+---------+---------+------+-------------------------------------+
 * | Field   | Bits    | Type    | Init | Description                         |
 * +---------+---------+---------+------+-------------------------------------+
 * | RES     | <63:12> | MBZ,RAZ | 0    | Reserved                            |
 * +---------+---------+---------+------+-------------------------------------+
 * | MASK    | <11:0>  | RW      | 0    | PERROR register bit enables         |
 * +---------+---------+---------+------+-------------------------------------+
 * \endcode
 *
 * Pchip Master Latency Register (PLAT - RW)
 *
 * Bits <15:8> are the master latency timer.
 *
 * Translation Buffer Invalidate Virtual Register (TLBIV - WO)
 *
 * A write to this register invalidates all scatter-gather TLB entries that
 *correspond to PCI addresses whose bits <31:16> and bit 39 match the value
 *written in bits <19:4> and 27 respectively. This invalidates up to eight PTEs
 *at a time, which are the number that can be defined in one 21264 cache block
 *(64 bytes). Because a single TLB PCI tag covers four entries, at most two tags
 *are actually invalidated. PTE bits <22:4> correspond to system address bits
 *<34:16> - where PCI<34:32> must be zeros for scatter- gather window hits - in
 *generating the resulting system address, providing 8-page (8KB) granularity.
 *
 * Translation Buffer Invalidate All Register (TLBIA - WO)
 *
 * A write to this register invalidates the scatter-gather TLB. The value
 *written is ignored.
 **/
u64 CSystem::pchip_csr_read(int num, u32 a) {
  switch (a) {
  case 0x000:
  case 0x040:
  case 0x080:
  case 0x0c0:
    return state.pchip[num].wsba[(a >> 6) & 3];

  case 0x100:
  case 0x140:
  case 0x180:
  case 0x1c0:
    return state.pchip[num].wsm[(a >> 6) & 3];

  case 0x200:
  case 0x240:
  case 0x280:
  case 0x2c0:
    return state.pchip[num].tba[(a >> 6) & 3];

  case 0x300:
    return state.pchip[num].pctl;

  case 0x3c0:
    return state.pchip[num].perr;

  case 0x400:
    return state.pchip[num].perrmask;

  case 0x480: // TLBIV
  case 0x4c0: // TLBIA
    return 0;

  case 0x800: // PCI reset
    return 0;

  default:
    printf("Unknown PCHIP %d CSR %07x read attempted.\n", num, a);
    return 0;
  }
}

/**
 * \brief Write one of the PCHIP registers.
 *
 * For a description of the PCHIP registers, see pchip_csr_read.
 **/
void CSystem::pchip_csr_write(int num, u32 a, u64 data) {
  switch (a) {
  case 0x000:
  case 0x040:
  case 0x080:
    state.pchip[num].wsba[(a >> 6) & 3] = data & U64(0x00000000fff00003);
    return;

  case 0x0c0:
    state.pchip[num].wsba[3] = (data & U64(0x00000080fff00001)) | 2;
    return;

  case 0x100:
  case 0x140:
  case 0x180:
  case 0x1c0:
    state.pchip[num].wsm[(a >> 6) & 3] = data & U64(0x00000000fff00000);
    return;

  case 0x200:
  case 0x240:
  case 0x280:
  case 0x2c0:
    state.pchip[num].tba[(a >> 6) & 3] = data & U64(0x00000007fffffc00);
    return;

  case 0x300:
    state.pchip[num].pctl &= U64(0xffffe300f0300000);
    state.pchip[num].pctl |= (data & U64(0x00001cff0fcfffff));
    return;

  case 0x340:
    state.pchip[num].plat = data;
    return;

  case 0x3c0: // PERR
    return;

  case 0x400:
    state.pchip[num].perrmask = data;
    return;

  case 0x480: // TLBIV
  case 0x4c0: // TLBIA
    return;

  case 0x800: // PCI reset
    for (int i = 0; i < iNumComponents; i++)
      acComponents[i]->ResetPCI();
    return;

  default:
    printf("Unknown PCHIP %d CSR %07x write with %016" PRIx64 " attempted.\n",
           num, a, data);
  }
}

u64 CSystem::cchip_csr_read(u32 a, CSystemComponent *source) {
  CAlphaCPU *cpu = (CAlphaCPU *)source;
  switch (a) {
  case 0x000:
    return state.cchip.csc;

  case 0x080:

    //    printf("MISC: %016" PRIx64 " from CPU %d (@%" PRIx64 ") (other @ %" LL
    //    "x).\n",state.cchip.misc | cpu->get_cpuid(),cpu->get_cpuid(),
    //    cpu->get_pc()-4, acCPUs[1-cpu->get_cpuid()]->get_pc());
    return state.cchip.misc | cpu->get_cpuid();

  case 0x100:

    // WE PUT ALL OUR MEMORY IN A SINGLE ARRAY FOR NOW...
    return ((u64)(iNumMemoryBits - 23) << 12); // size

  case 0x140:
  case 0x180:
  case 0x1c0:

    // WE PUT ALL OUR MEMORY IN A SINGLE ARRAY FOR NOW...
    return 0;

  case 0x200:
  case 0x240:
  case 0x600:
  case 0x640:
    return state.cchip.dim[((a >> 10) & 2) | ((a >> 6) & 1)];

  case 0x280:
  case 0x2c0:
  case 0x680:
  case 0x6c0:
    return state.cchip.drir & state.cchip.dim[((a >> 10) & 2) | ((a >> 6) & 1)];

  case 0x300:
    return state.cchip.drir;

  default:
    printf("Unknown CCHIP CSR %07x read attempted.\n", a);
    return 0;
  }
}

void CSystem::cchip_csr_write(u32 a, u64 data, CSystemComponent *source) {
  CAlphaCPU *cpu = (CAlphaCPU *)source;
  switch (a) {
  case 0x000: // CSC
    state.cchip.csc &= ~U64(0x0777777fff3f0000);
    state.cchip.csc |= (data & U64(0x0777777fff3f0000));
    return;

  case 0x080:                                              // MISC
    state.cchip.misc |= (data & U64(0x00000f0000f00000));  // W1S
    state.cchip.misc &= ~(data & U64(0x0000000010000ff0)); // W1C
    if (data & U64(0x0000000001000000)) {
      state.cchip.misc &= ~U64(0x0000000000ff0000); // Arbitration Clear
      printf("Arbitration clear from CPU %d (@%" PRIx64 ").\n",
             cpu->get_cpuid(), cpu->get_pc() - 4);
    }

    if (data & U64(0x00000000000f0000)) {
      printf("Arbitration %016" PRIx64 " from CPU %d (@%" PRIx64 ")... ", data,
             cpu->get_cpuid(), cpu->get_pc() - 4);
      if (!(state.cchip.misc & U64(0x00000000000f0000))) {
        state.cchip.misc |= (data & U64(0x00000000000f0000)); // Arbitration won
        printf("won  %016" PRIx64 "\n", state.cchip.misc);
      } else
        printf("lost %016" PRIx64 "\n", state.cchip.misc);
    }

    // stop interval timer interrupt
    if (data & U64(0x00000000000000f0)) {
      for (int i = 0; i < iNumCPUs; i++) {
        if (data & (U64(0x10) << i)) {
          acCPUs[i]->irq_h(2, false, 0);

          // printf("*** TIMER interrupt cleared for CPU %d\n",i);
        }
      }
    }

    // stop inter processor interrupt
    if (data & U64(0x0000000000000f00)) {
      for (int i = 0; i < iNumCPUs; i++) {
        if (data & (U64(0x100) << i)) {
          acCPUs[i]->irq_h(3, false, 0);
          printf("*** IP interrupt cleared for CPU %d from CPU %d(@ %" PRIx64
                 ").\n",
                 i, cpu->get_cpuid(), cpu->get_pc() - 4);
        }
      }
    }

    // set inter processor interrupt
    if (data & U64(0x000000000000f000)) {
      for (int i = 0; i < iNumCPUs; i++) {
        if (data & (U64(0x1000) << i)) {
          state.cchip.misc |= U64(0x100) << i;
          acCPUs[i]->irq_h(3, true, 0);
          printf("*** IP interrupt set for CPU %d from CPU %d(@ %" PRIx64 ")\n",
                 i, cpu->get_cpuid(), cpu->get_pc() - 4);

          //          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      }
    }

    return;

  case 0x200:
  case 0x240:
  case 0x600:
  case 0x640:
    state.cchip.dim[((a >> 10) & 2) | ((a >> 6) & 1)] = data;
    return;

  default:
    printf("Unknown CCHIP CSR %07x write with %016" PRIx64 " attempted.\n", a,
           data);
  }
}

u8 CSystem::dchip_csr_read(u32 a) {
  switch (a) {
  case 0x800: // DSC
    return state.dchip.dsc;
  case 0x840: // STR
    return state.dchip.str;
  case 0x880: // DREV
    return state.dchip.drev;
  case 0x8c0: // DSC2
    return state.dchip.dsc2;
  default:
    printf("Unknown DCHIP CSR %07x read attempted.\n", a);
    return 0;
  }
}

void CSystem::dchip_csr_write(u32 a, u8 data) {
  printf("Unknown DCHIP CSR %07x write with %02x attempted.\n", a, data);
}

/**
 * Read a byte from the TIGbus
 *
 * What information we have is sketchy at best. Following is extracted from T64,
 * Although we're not 100% sure that this is actually for ES40:
 *
 * \code
 * +-----------------+--------+
 * | register        | offset |
 * +-----------------+--------+
 * | trr             | 000    |
 * | smir            | 040    | system management IR
 * | cpuir           | 080    | CPU IR
 * | psir            | 0c0    | powe supply IR
 * | mod_info        | 100    |
 * | clk_info        | 140    |
 * | chip_info       | 180    |
 * | tpcr            | 200    |
 * | pll_data        | 280    |
 * | pll_clk         | 2c0    |
 * | ev6_init        | 300    |
 * | csleep          | 340    |
 * | smcr            | 380    |
 * | ttcr            | 3c0    |
 * | clr_irq5        | 400    |
 * | clr_irq4        | 440    |
 * | clr_pwr_flt_det | 480    |
 * | clr_temp_warn   | 4c0    |
 * | clr_temp_fail   | 500    |
 * | ev6_halt        | 5c0    |
 * | srcr0           | 600    |
 * | srcr1           | 640    |
 * | frar0           | 700    |
 * | frar1           | 740    |
 * | fwmr0           | 800    |
 * | fwmr1           | 840    |
 * | fwmr2           | 880    |
 * | fwmr3           | 8c0    |
 * | ipcr0           | a00    | inter-processor communications register for
 *arbiter (?) | ipcr1           | a40    | | ipcr2           | a80    | | ipcr3
 *| ac0    | | ipcr4           | b00    |
 * +-----------------+--------+
 * \endcode
 **/
u8 CSystem::tig_read(u32 a) {
  switch (a) {
  case 0x30000000: // trr
    return 0;
  case 0x30000040: // smir
    return state.tig.FwWrite;
  case 0x30000100: // mod_info
    return 0;
  case 0x300003c0: // ttcr
    return state.tig.HaltA;
  case 0x30000480: // clr_pwr_flt_det
    return 0;
  case 0x300005c0: // ev6_halt
    return state.tig.HaltB;
  case 0x38000180: // Arbiter revision
    return 0xfe;
  default:
    printf("Unknown TIG %08x read attempted.\n", a);
    return 0;
  }
}

void CSystem::tig_write(u32 a, u8 data) {
  switch (a) {
  case 0x30000000: // trr
    return;
  case 0x30000040: // smir
    state.tig.FwWrite = data;
    return;
  case 0x30000100: // mod_info
    printf("Soft reset: %02x\n", data);
    return;
  case 0x300003c0: // ttcr
    state.tig.HaltA = data;
    return;
  case 0x30000480: // clr_pwr_flt_det
    return;
  case 0x300005c0: // ev6_halt
    state.tig.HaltB = data;
    return;
  default:
    printf("Unknown TIG %07x write with %02x attempted.\n", a, data);
  }
}

/**
 * Load ROM contents from file. Try if the decompressed ROM image
 * is available, otherwise create it first.
 **/
int CSystem::LoadROM() {
  FILE *f;
  char *buffer;
  int i;
  int j;
  u64 temp;
  u32 scratch;

  f = fopen(myCfg->get_text_value("rom.decompressed", "decompressed.rom"),
            "rb");
  if (!f) {
    f = fopen(myCfg->get_text_value("rom.srm", "cl67srmrom.exe"), "rb");
    if (!f)
      FAILURE(Runtime, "No original or decompressed SRM ROM image found");
    printf("%%SYS-I-READROM: Reading original ROM image from %s.\n",
           myCfg->get_text_value("rom.srm", "cl67srmrom.exe"));
    for (i = 0; i < 0x240; i++) {
      if (feof(f))
        break;
      (void)!fread(&scratch, 1, 1, f);
    }

    if (feof(f))
      FAILURE(Runtime, "File is too short to be a SRM ROM image");
    buffer = PtrToMem(0x900000);
    while (!feof(f))
      (void)!fread(buffer++, 1, 1, f);
    fclose(f);

    printf("%%SYS-I-DECOMP: Decompressing ROM image.\n0%%");
    acCPUs[0]->set_pc(0x900001);
    acCPUs[0]->set_PAL_BASE(0x900000);
    acCPUs[0]->enable_icache();

    j = 0;
    while (acCPUs[0]->get_clean_pc() > 0x200000) {
      for (i = 0; i < 1800000; i++) {
        SingleStep();
        if (acCPUs[0]->get_clean_pc() < 0x200000)
          break;
      }

      j++;
      if (((j % 5) == 0) && (j < 50))
        printf("%d%%", j * 2);
      else
        printf(".");
      fflush(stdout);
    }

    printf("100%%\n");
    acCPUs[0]->restore_icache();

    f = fopen(myCfg->get_text_value("rom.decompressed", "decompressed.rom"),
              "wb");
    if (!f) {
      printf("%%SYS-W-NOWRITE: Couldn't write decompressed rom to %s.\n",
             myCfg->get_text_value("rom.decompressed", "decompressed.rom"));
    } else {
      printf("%%SYS-I-ROMWRT: Writing decompressed rom to %s.\n",
             myCfg->get_text_value("rom.decompressed", "decompressed.rom"));
      temp = endian_64(acCPUs[0]->get_pc());
      fwrite(&temp, 1, sizeof(u64), f);
      temp = endian_64(acCPUs[0]->get_pal_base());
      fwrite(&temp, 1, sizeof(u64), f);
      buffer = PtrToMem(0);
      fwrite(buffer, 1, 0x200000, f);
      fclose(f);
    }
  } else {
    printf("%%SYS-I-READROM: Reading decompressed ROM image from %s.\n",
           myCfg->get_text_value("rom.decompressed", "decompressed.rom"));
    (void)!fread(&temp, 1, sizeof(u64), f);
    for (int i = 0; i < iNumCPUs; i++)
      acCPUs[i]->set_pc(endian_64(temp));
    (void)!fread(&temp, 1, sizeof(u64), f);
    for (int i = 0; i < iNumCPUs; i++)
      acCPUs[i]->set_PAL_BASE(endian_64(temp));
    buffer = PtrToMem(0);
    (void)!fread(buffer, 1, 0x200000, f);
    fclose(f);
  }

#if !defined(SRM_NO_SPEEDUPS) || !defined(SRM_NO_IDE)
  printf("%%SYM-I-PATCHROM: Patching ROM for speed.\n");
#endif
#if !defined(SRM_NO_SPEEDUPS)
  WriteMem(U64(0x14248), 32, 0xe7e00000, 0); // e7e00000 = BEQ r31, +0
  WriteMem(U64(0x14288), 32, 0xe7e00000, 0);
  WriteMem(U64(0x142c8), 32, 0xe7e00000, 0);
  WriteMem(U64(0x68320), 32, 0xe7e00000, 0);
  WriteMem(U64(0x8bb78), 32, 0xe7e00000, 0); // memory test (aa)
  WriteMem(U64(0x8bc0c), 32, 0xe7e00000, 0); // memory test (bb)
  WriteMem(U64(0x8bc94), 32, 0xe7e00000, 0); // memory test (00)

  // WriteMem(U64(0xb1158),32,0xe7e00000,0);   // CPU sync?
#endif
  printf("%%SYS-I-ROMLOADED: ROM Image loaded successfully!\n");
  return 0;
}

/**
 * \brief Assert or deassert one of 64 possible interrupt lines on the Tsunami
 *chipset.
 *
 * Source: HRM, 6.3:
 *
 * TIGbus and Interrupts
 *
 * The TIGbus supports miscellaneous system logic such as flash ROM and
 * interrupt inputs. The Cchip TIG controller polls interrupts continuously
 * except when a read or write to flash is requested. The 64 possible interrupt
 *inputs are polled eight at a time by selecting a byte with the b_tia<2:0>
 *pins, and asserting b_toe_l to allow the selected byte to be driven onto
 *b_td<7:0>. Using the polled interrupts, the Cchip calculates the b_irq values
 *that should be delivered to the CPUs. When any change occurs in these b_irq
 *values, the Cchip drives the b_irq<3:0> data for both CPUs onto b_td<7:0>, and
 *asserts signal b_tis to strobe it into a register on the module. If there is
 *no flash read or write outstanding, the polling process is repeated. If there
 * is a flash read or write outstanding, it is serviced between interrupt reads
 *after any pending b_irq updates. Thus, the rounds of interrupt polling are not
 *atomic, but the b_irq values reflect the most recently polled interrupts.
 *Furthermore, b_irq<1> may be artificially suppressed for one full polling loop
 *using the CSR bit MISC<DEVSUP>.
 * [...]
 *
 * Device and Error Interrupt Delivery - b_irq<1:0>
 *
 * As interrupts are read into the Cchip through the TIGbus, the corresponding
 *bits are set in DRIR. These bits are ANDed with the mask bits in DIMn and then
 *placed in DIRn. If any bits are set in DIRn<55:0>, then CPUn is interrupted
 *using CPU pin b_irq<1>.
 *
 * Interrupt bits <62:58> cause b_irq<0> to be asserted and are intended for use
 *as error signals. Assertion of interrupt bits <62:58> causes b_irq<0> to be
 *asserted. Interrupt bits <62:61> can be used for Pchip 0 and Pchip 1 errors,
 *respectively. Interrupt bit <63> is special because it is not read from the
 *TIGbus, but is internally generated as the Cchip detected error interrupt
 *(currently used only for NXM requests). Assertion of interrupt bit <63> causes
 *b_irq<0> to be asserted. See Chapter 10 for descriptions of the
 *interrupt-related CSRs (DRIR, DIMn, DIRn, and MISC). A full mask register for
 * each CPU allows software to decide whether to send each of the 64 possible
 *interrupts to either or both CPUs.
 *
 * After handling all known outstanding interrupts, software may suppress
 *b_irq<1> device interrupts to allow the Cchip�s polling mechanism to detect
 *the updated (deasserted) value of the interrupt lines from the PCI devices and
 *thereby avoid giving the CPU �stale� interrupts, which require passive
 *release. The field MISC<DEVSUP> is provided for this purpose. When a CPU
 *writes a one to its bit in MISC<DEVSY> the Cchip deasserts b_irq<1> to that
 *CPU (regardless of the value in the DIRn) until it has completed an entire
 *polling loop. When the Cchip has completed an entire polling loop, b_irq<1>
 *will again reflect the value of DIRn<55:00>.
 *
 * Interval Timer Interrupts - b_irq<2>
 *
 * The interval timer interrupts the Cchip through a dedicated pin, i_intim_l,
 *and is asserted low. When the Cchip sees an asserting (falling) edge of this
 *pin, it asserts MISC<ITINTR> for both CPUs. Pin b_irq<2> remains asserted for
 *each CPU <ITINTR>. When the CPU has finished handling the interrupt, it writes
 *a one to its MISC<ITINTR> bit to clear it. Software can suppress interval
 *timer interrupts for n cycles by writing n into IICn.
 *
 * This table shows TIG Interrupts and IRQ Lines
 *
 * \code
 * +---------------+-----------------+--------+----------------------------------------+
 * | TIG Interrupt | Assertion Level | irq<n> | Use |
 * +---------------+-----------------+--------+----------------------------------------+
 * |            63 | N/A             | irq<0> | N/C (internally generated Cchip
 *error) | |               |                 |        |     (currently NXM only)
 *|
 * +---------------+-----------------+--------+----------------------------------------+
 * |         62:58 | High            | irq<0> | Errors (Pchips, and so on) | |
 *|                 |        | Recommended:                           | | | | |
 ** Bit <62> - Pchip0 error            | |               |                 | |
 ** Bit <61> - Pchip1 error            |
 * +---------------+-----------------+--------+----------------------------------------+
 * |         57:56 | N/A             | N/A    | Reserved |
 * +---------------+-----------------+--------+----------------------------------------+
 * |          55:0 | Low             | irq<1> | PCI devices (level sensitive) |
 * +---------------+-----------------+--------+----------------------------------------+
 * \endcode
 *
 * It is not clear from the documentation how exactly the interval timer is
 *connected to the CChip. It looks like this is tied to the interrupt-line from
 *the real-time clock (TOY), as the periodic interval rate for the TOY is set to
 *1024 Hz by SRM. 1024 Hz is the frequency of the system timer interrupt
 *according to the OpenVMS Alpha Internals and Data Structures Handbook.
 *
 **/
void CSystem::interrupt(int number, bool assert) {
  int i;

  if (number == -1) {

    // timer int...
    state.cchip.misc |= 0xf0;
    for (i = 0; i < iNumCPUs; i++)
      acCPUs[i]->irq_h(2, true, 0); // timer interrupt is immediate
  } else if (assert) {

    //    if (!(state.cchip.drir & (1i64<<number)))
    //      printf("%%TYP-I-INTERRUPT: Interrupt %d asserted.\n",number);
    state.cchip.drir |= (U64(0x1) << number);
  } else {

    //    if (state.cchip.drir & (1i64<<number))
    //      printf("%%TYP-I-INTERRUPT: Interrupt %d deasserted.\n",number);
    state.cchip.drir &= ~(U64(0x1) << number);
  }

  for (i = 0; i < iNumCPUs; i++) {
    if (state.cchip.drir & state.cchip.dim[i] & U64(0x00ffffffffffffff))
      acCPUs[i]->irq_h(1, true, 100); // device interrupts delayed by 100 clocks
    else
      acCPUs[i]->irq_h(1, false, 0);

    if (state.cchip.drir & state.cchip.dim[i] & U64(0xfc00000000000000))
      acCPUs[i]->irq_h(0, true, 100); // device interrupts delayed by 100 clocks
    else
      acCPUs[i]->irq_h(0, false, 0);
  }
}

/**
 * \brief Translate a 32-bit address coming off the PCI bus into a
 * 64-bit system address. Used by PCI devices when accessing
 * memory (or other PCI devices) as bus master.
 *
 * Source: HRM, 10.1.4:
 *
 * DMA Address Translation (PCI-to-System)
 * The 21272 chipset supports some PCI commands as a target and does not support
 * (ignores) others as a target. The Pchip does not respond as a target when it
 *acts as a PCI master.
 *
 * The Pchip ignores all of the following commands as a target:
 *  - Interrupt acknowledge
 *  - Special cycle
 *  - I/O read
 *  - I/O write
 *  - Configuration read
 *  - Configuration write
 *  .
 *
 * The Pchips may respond to the following commands as a target:
 *  - Memory read
 *  - Memory read line
 *  - Memory write
 *  - Memory write and invalidate
 *  - Memory read multiple
 *  - Dual-address cycle: This command is accepted by the Pchip when the address
 *lies inside the DMA monster window.
 *
 * There are two kinds of DMA address translation: direct mapped and
 *scatter-gather mapped. Each type starts by comparing the incoming PCI address
 *with the monster window (if it is enabled and if it is a DAC), and with the
 *four window base and window mask registers (the window base registers also
 *have an enable window bit and a scatter-gather enable bit). This process is
 *shown in the next figure:
 *
 * \code
 *              31       n n-1      20 19    13 12       0
 *             +----------+-----------+--------+----------+
 * PCI Address |    Peripheral Page Number     |  Offset  |
 *             +----------+-----------+--------+----------+
 *             |<-------->|
 *                  ^
 *                  +-----------> COMPARE ----> Hit
 *                  v
 *             |<-------->|
 *              31       n n-1      20
 * Window Base +----------+-----------+
 * Register    |          |   xxxx    |
 *             +----------+-----------+
 *              31       n n-1      20
 * Window Mask +----------+-----------+
 * Register    |   0000   |   1111    | (Determines n)
 *             +----------+-----------+
 * \endcode
 *
 * If the address resides in one of the windows, and the window is enabled, then
 *if the scatter-gather enable bit is set, the translation is as described for
 * PCI_Phys_scatter_gather. Otherwise, the translation described for
 *PCI_Phys_direct_mapped is used.
 *
 * In addition, if the matching window has the PTP bit set, then the result of
 *the address translation is treated as if it had bit <43> set. That is, it is
 *treated like a PIO address from the CPU. Otherwise, the address is a system
 *memory address.
 *
 * Window Hole
 *
 * All window registers are simultaneously subject to a hole that inhibits
 *matching, under the control of the PCTL<HOLE> CSR bit described in
 *Section 10.2.5.4. If that bit is set, the hole is enabled in all windows and
 *has the following extent:
 *  - From PCI address base 512K (address<31:0> = 0008.0000)
 *  - To PCI address limit 1M-1 (address<31:0> = 000F.FFFF)
 *  .
 *
 * If enabled, the hole applies whether or not the PTP bit is set for the
 *window.
 *
 * The documentation is not explicit on this, but the assumption was made that
 *if an address coming off the PCI-bus is not matched, the Pchip doesn't respond
 *to that address, and it is up to other PCI devices to respond to the address.
 *So, if no match is found, we treat the address as an address on the local PCI
 *bus.
 *
 * \todo The documentation mentions a PTP bit set for a window, but the register
 *descriptions don't show a PTP bit in one of the three registers (WSBA, WSM and
 *TBA). So, for now, we can only do peer-to-peer through a scatter-gather PTE.
 *
 * \todo Dual-Acces-Cycle (DAC) access from the PCI bus is not supported. If a
 *device is ever added that uses this, we should probably support it.
 **/
u64 CSystem::PCI_Phys(int pcibus, u32 address) {
  u64 a;
  int j;

#if defined(DEBUG_PCI)
  printf("-------------- PCI MEMORY ACCESS FOR PCI HOSE %d --------------\n",
         pcibus);

  // Step through windows
  for (j = 0; j < 4; j++) {
    printf("WSBA%d: %016" PRIx64 " WSM: %016" PRIx64 " TBA: %016" PRIx64 "\n", j,
           state.pchip[pcibus].wsba[j], state.pchip[pcibus].wsm[j],
           state.pchip[pcibus].tba[j]);
  }

  printf("HOLE: %s\n",
         test_bit_64(state.pchip[pcibus].pctl, 5) ? "enabled" : "disabled");
  printf("--------------------------------------------------------------\n");
#endif
  if (!(state.pchip[pcibus].pctl & PCI_PCTL_HOLE) // hole disabled
      || (address < PCI_PCTL_HOLE_START) ||
      (address > PCI_PCTL_HOLE_END)) // or address outside hole
  {

    // Step through windows
    for (j = 0; j < 4; j++) {
      if ((state.pchip[pcibus].wsba[j] & 1) // window enabled...
          && !((address ^ state.pchip[pcibus].wsba[j]) & 0xfff00000 &
               ~state.pchip[pcibus].wsm[j])) // address in range...
      {
        if (state.pchip[pcibus].wsba[j] & 2) {
          try {
            a = PCI_Phys_scatter_gather(address, state.pchip[pcibus].wsm[j],
                                        state.pchip[pcibus].tba[j]);
          }

          catch (char) {

            // window disabled...
            // not matched; treat as local PCI bus address
            return U64(0x80000000000) | (pcibus * U64(0x200000000)) |
                   (u64)address;
          }
        } else
          a = PCI_Phys_direct_mapped(address, state.pchip[pcibus].wsm[j],
                                     state.pchip[pcibus].tba[j]);
#if defined(DEBUG_PCI)
        printf("PCI memory address %08x translated to %016" PRIx64 "\n", address,
               a);
#endif
        return a;
      }
    }
  }

  // not matched; treat as local PCI bus address
  return U64(0x80000000000) | (pcibus * U64(0x200000000)) | (u64)address;
}

/**
 * Translate a 32-bit address coming off the PCI bus into a 64-bit
 * system address using direct-mapped DMA address translation.
 *
 * Source: HRM, 10.1.4.2:
 *
 * Direct-Mapped DMA Address Translation
 *
 * Direct-mapped addressing uses a base address register, a translated base
 *address (TBA) register, and a mask register. The block of PCI addresses at
 *base address, of a size as determined by the mask register, is translated to a
 *block of addresses at translated base address. Values in the WSMn field other
 *than those shown produce unspecified results.
 *
 * \code
 * +-------------+----------------+---------------------------+
 * | Window Size | WSMn<31:20>    | Translated Address <34:0> |
 * +-------------+----------------+---------------------------+
 * |         1MB | 0000.0000.0000 | TBA<34:20>:ad<19:0>       |
 * +-------------+----------------+---------------------------+
 * |         2MB | 0000.0000.0001 | TBA<34:21>:ad<20:0>       |
 * +-------------+----------------+---------------------------+
 * |         4MB | 0000.0000.0011 | TBA<34:22>:ad<21:0>       |
 * +-------------+----------------+---------------------------+
 * |         8MB | 0000.0000.0111 | TBA<34:23>:ad<22:0>       |
 * |        ...  |           ...  |                ...        |
 * |         2GB | 0111.1111.1111 | TBA<34:31>:ad<30:0>       |
 * +-------------+----------------+---------------------------+
 * |         4GB |            N/A | 000:ad<31:0> (monster     |
 * |             |                | window only)              |
 * +-------------+----------------+---------------------------+
 * \endcode
 **/
u64 CSystem::PCI_Phys_direct_mapped(u32 address, u64 wsm, u64 tba) {
  u64 a;

  wsm &= PCI_WSM_MASK;

  a = (address & (wsm | PCI_ADD_MASK)) | (tba & ~wsm & PCI_TBA_MASK);

  return a;
}

/**
 * Translate a 32-bit address coming off the PCI bus into a 64-bit
 * system address using scatter-gather DMA address translation.
 *
 * If address can't be matched (PTE is invalid), an exception of type char
 * is thrown. The calling function should catch the exception, and do
 * The Right Thing(tm): treat the address as a local PCI-bus address.
 *
 * Source: HRM, 10.1.4.3:
 *
 * Scatter-Gather DMA Address Translation
 *
 * Scatter-gather addressing uses a base address register, a mask register, a
 *translated base address register, and a page table entry (PTE) in system
 *memory. An 8KB page of PCI addresses at base address is translated to an 8KB
 *page of system addresses through one level of indirection. The PTE contains
 *the address of the 8KB page.
 * [...]
 * At TBA is a region (of size SG PTE AREA) of PTEs, each of which is eight
 *bytes. Bits <22:1> of the PTE become bits <34:13> (the 8KB page) of the system
 *address, and bits <12:0> of the PCI address become bits <12:0> (the page
 *offset) of the system address.
 *
 * The following table shows how the address of the page table entry (to be used
 *as part of the final system address) is generated. Values in the WSM field
 *other than those shown produce unspecified results.
 *
 * \code
 * +-------------+-------------+----------------+----------------------+
 * | Window Size | SG PTE AREA | WSMn<31:20>    | PTE Address <34:3>   |
 * +-------------+-------------+----------------+----------------------+
 * |         1MB |         1KB | 0000.0000.0000 | TBA<34:10>:ad<19:13> |
 * +-------------+-------------+----------------+----------------------+
 * |         2MB |         2KB | 0000.0000.0001 | TBA<34:11>:ad<20:13> |
 * +-------------+-------------+----------------+----------------------+
 * |         4MB |         4KB | 0000.0000.0011 | TBA<34:12>:ad<21:13> |
 * +-------------+-------------+----------------+----------------------+
 * |         8MB |         8KB | 0000.0000.0111 | TBA<34:13>:ad<22:13> |
 * |        ...  |        ...  |           ...  |                ...   |
 * |         2GB |         2MB | 0111.1111.1111 | TBA<34:21>:ad<30:13> |
 * +-------------+-------------+----------------+----------------------+
 * |         4GB |         4MB |            N/A | TBA<34:22>:ad<31:13> |
 * |             |             |                | (Window 3 in DAC     |
 * |             |             |                | mode only)           |
 * +-------------+-------------+----------------+----------------------+
 * \endcode
 *
 * The following figure shows the structure of a page table entry in memory. If
 *either bit <31> or bit <28> is set, the address is interpreted as being a
 *peer-to-peer address.
 *
 * \code
 *  63               32 31 30 29 28 27      23 22                   1 0
 * +-------------------+--+-----+--+----------+----------------------+-+
 * |                   |PP|     |PP|          | Page Address <34:13> |V|
 * +-------------------+--+-----+--+----------+----------------------+-+
 *                                                                    +--> V =
 *valid bit \endcode
 *
 * The last figure shows how a page table entry is used in conjunction with an
 *incoming PCI address to generate a system address.
 *
 * \code
 *        PTE <22:1>              PCI address <12:0>
 *             |                         |
 *  34         v              13 12      v         0
 * +----------------------------+-------------------+
 * |   Page addres <34:13>      |  Offset <12:0>    |
 * +----------------------------+-------------------+
 * \endcode
 **/
u64 CSystem::PCI_Phys_scatter_gather(u32 address, u64 wsm, u64 tba) {
  u64 pte_a;

  u64 pte;

  u64 a;

  wsm &= PCI_WSM_MASK;

  pte_a = ((address & (wsm | PCI_PTE_ADD_MASK)) >>
           PCI_PTE_ADD_SHIFT) // ad part of pte address
          | (tba & PCI_PTE_TBA_MASK &
             ~(wsm >> PCI_PTE_ADD_SHIFT)); // tba part of pte address
  pte = ReadMem(pte_a, 64, 0);
  if (pte & 1) {
    a = ((pte << PCI_PTE_SHIFT) & PCI_PTE_MASK) | (address & PCI_PTE_ADD2_MASK);

    if (pte & PCI_PTE_PEER_BIT) // peer-to-peer
      a |= (PHYS_PIO_ACCESS);   // PIO access.
    return a;
  } else {
    throw((char)'0');
  }
}

/**
 * Initialize all devices.
 **/
void CSystem::init() {
  for (int i = 0; i < iNumComponents; i++)
    acComponents[i]->init();
}

void CSystem::start_threads() {
  int i;

  printf("Start threads:");
  for (i = 0; i < iNumComponents; i++) {
#ifdef IDB
    // When running with IDB, the trace engine takes care of managing the CPU,
    // so its thread shouldn't be started.
    if (dynamic_cast<CAlphaCPU *>(acComponents[i]))
      continue;
#endif
    acComponents[i]->start_threads();
  }
  printf("\n");

  for (i = 0; i < iNumCPUs; i++)
    acCPUs[i]->release_threads();
}

void CSystem::stop_threads() {
  printf("Stop threads:");
  for (int i = 0; i < iNumComponents; i++)
    acComponents[i]->stop_threads();
  printf("\n");
}

/**
 * Save system state to a state file.
 **/
void CSystem::SaveState(const char *fn) {
  FILE *f;
  int i;
  unsigned int m;
  unsigned int j;
  int *mem = (int *)memory;
  int int0 = 0;
  unsigned int memints = (1 << iNumMemoryBits) / (unsigned int)sizeof(int);
  u32 temp_32;

  f = fopen(fn, "wb");
  if (f) {
    temp_32 = 0xa1fae540; // MAGIC NUMBER (ALFAES40 ==> A1FAE540 )
    fwrite(&temp_32, sizeof(u32), 1, f);
    temp_32 = 0x00020001; // File Format Version 2.1
    fwrite(&temp_32, sizeof(u32), 1, f);

    // memory
    for (m = 0; m < memints; m++) {
      if (mem[m]) {
        fwrite(&(mem[m]), 1, sizeof(int), f);
      } else {
        j = 0;
        m++;
        while (!mem[m] && (m < memints)) {
          m++;
          j++;
          if ((int)j == -1)
            break;
        }

        if (mem[m])
          m--;
        fwrite(&int0, 1, sizeof(int), f);
        fwrite(&j, 1, sizeof(int), f);
      }
    }

    fwrite(&state, sizeof(state), 1, f);

    // components
    //
    //  Components should also save any non-initial memory-registrations and
    //  re-register upon restore!
    //
    for (i = 0; i < iNumComponents; i++)
      acComponents[i]->SaveState(f);
    fclose(f);
  }
}

/**
 * Restore system state from a state file.
 **/
void CSystem::RestoreState(const char *fn) {
  FILE *f;
  int i;
  unsigned int m;
  unsigned int j;
  int *mem = (int *)memory;
  unsigned int memints = (1 << iNumMemoryBits) / (unsigned int)sizeof(int);
  u32 temp_32;

  f = fopen(fn, "rb");
  if (!f) {
    printf("%%SYS-F-NOFILE: Can't open restore file %s\n", fn);
    return;
  }

  (void)!fread(&temp_32, sizeof(u32), 1, f);
  if (temp_32 != 0xa1fae540) // MAGIC NUMBER (ALFAES40 ==> A1FAE540 )
  {
    printf("%%SYS-F-FORMAT: %s does not appear to be a state file.\n", fn);
    return;
  }

  (void)!fread(&temp_32, sizeof(u32), 1, f);

  if (temp_32 != 0x00020001) // File Format Version 2.1
  {
    printf("%%SYS-I-VERSION: State file %s is a different version.\n", fn);
    return;
  }

  // memory
  for (m = 0; m < memints; m++) {
    (void)!fread(&(mem[m]), 1, sizeof(int), f);
    if (!mem[m]) {
      (void)!fread(&j, 1, sizeof(int), f);
      while (j--) {
        mem[++m] = 0;
      }
    }
  }

  (void)!fread(&state, sizeof(state), 1, f);

  // components
  //
  //  Components should also save any non-initial memory-registrations and
  //  re-register upon restore!
  //
  for (i = 0; i < iNumComponents; i++) {
    if (acComponents[i]->RestoreState(f))
      FAILURE(Runtime, "Unable to restore system state");
  }

  fclose(f);
}

/**
 * Dump memory contents to a file.
 **/
void CSystem::DumpMemory(unsigned int filenum) {
  char file[100];
  int x;
  int *mem = (int *)memory;
  FILE *f;

  sprintf(file, "memory_%012d.dmp", filenum);
  f = fopen(file, "wb");

  x = (1 << iNumMemoryBits) / (unsigned int)sizeof(int) / 2;

  while (!mem[x - 1])
    x--;

  fwrite(mem, 1, x * sizeof(int), f);
  fclose(f);
}

/**
 *  Dump system state to stdout for debugging purposes.
 **/
void CSystem::panic(char *message, int flags) {
  int cpunum;

  int i;
  CAlphaCPU *cpu;
  printf("\n******** SYSTEM PANIC *********\n");
  printf("* %s\n", message);
  printf("*******************************\n");
  for (cpunum = 0; cpunum < iNumCPUs; cpunum++) {
    cpu = acCPUs[cpunum];
    printf("\n==================== STATE OF CPU %d ====================\n",
           cpunum);

    printf("PC: %016" PRIx64 "\n", cpu->get_pc());
#ifdef IDB
    printf("Physical PC: %016" PRIx64 "\n", cpu->get_current_pc_physical());
    printf("Instruction Count: %" PRId64 "\n", cpu->get_instruction_count());
#endif
    printf("\n");

    for (i = 0; i < 32; i++) {
      if (i < 10)
        printf("R");
      printf("%d:%016" PRIx64, i, cpu->get_r(i, false));
      if (i % 4 == 3)
        printf("\n");
      else
        printf(" ");
    }

    printf("\n");
    for (i = 4; i < 8; i++) {
      if (i < 10)
        printf("S");
      printf("%d:%016" PRIx64, i, cpu->get_r(i + 32, false));
      if (i % 4 == 3)
        printf("\n");
      else
        printf(" ");
    }

    for (i = 20; i < 24; i++) {
      if (i < 10)
        printf("S");
      printf("%d:%016" PRIx64, i, cpu->get_r(i + 32, false));
      if (i % 4 == 3)
        printf("\n");
      else
        printf(" ");
    }

    printf("\n");
    for (i = 0; i < 32; i++) {
      if (i < 10)
        printf("F");
      printf("%d:%016" PRIx64, i, cpu->get_f(i));
      if (i % 4 == 3)
        printf("\n");
      else
        printf(" ");
    }
  }

  printf("\n");
#ifdef IDB
  if (flags & PANIC_LISTING) {
    u64 start;

    u64 end;
    start = cpu->get_pc() - 64;
    end = start + 128;
    cpu->listing(start, end, cpu->get_pc());
  }
#endif
  if (flags & PANIC_ASKSHUTDOWN) {
    printf("Stop Emulation? ");

    int c = getc(stdin);
    if (c == 'y' || c == 'Y')
      flags |= PANIC_SHUTDOWN;
  }

  if (flags & PANIC_SHUTDOWN) {
    FAILURE(Abort, "Panic shutdown");
  }

  return;
}

/**
 * Clear the clock interrupt for a specific CPU. Used by the CPU to acknowledge
 *the interrupt.
 **/
void CSystem::clear_clock_int(int ProcNum) {
  state.cchip.misc &= ~(U64(0x10) << ProcNum);
  acCPUs[ProcNum]->irq_h(2, false, 0);
}

#if defined(PROFILE)
u64 profile_buckets[PROFILE_BUCKETS];
u64 profiled_insts;
bool profile_started = false;
#endif
CSystem *theSystem = 0;
