/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Copyright (C) 2012 Dmitry Kalinkin
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

#include "AlphaCPU.hpp"
#include "StdAfx.hpp"
#include "TraceEngine.hpp"
#include "cpu_arith.hpp"
#include "cpu_bwx.hpp"
#include "cpu_control.hpp"
#include "cpu_debug.hpp"
#include "cpu_fp_branch.hpp"
#include "cpu_fp_memory.hpp"
#include "cpu_fp_operate.hpp"
#include "cpu_logical.hpp"
#include "cpu_memory.hpp"
#include "cpu_misc.hpp"
#include "cpu_mvi.hpp"
#include "cpu_pal.hpp"
#include "cpu_vax.hpp"
#include "lockstep.hpp"

#if !defined(HAVE_NEW_FP)
#include "es40_float.hpp"
#endif
void CAlphaCPU::release_threads() { mySemaphore.set(); }

void CAlphaCPU::run() {
  try {
    mySemaphore.wait();
    while (state.wait_for_start) {
      if (StopThread)
        return;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    printf("*** CPU%d *** STARTING ***\n", get_cpuid());
    for (;;) {
      if (StopThread)
        return;
      for (int i = 0; i < 1000000; i++)
        execute();
    }
  } catch (CException &e) {
    printf("Exception in CPU thread: %s.\n", e.displayText().c_str());
    myThreadDead.store(true);
    // Let the thread die...
  }
}

/**
 * Constructor.
 **/
CAlphaCPU::CAlphaCPU(CConfigurator *cfg, CSystem *system)
    : CSystemComponent(cfg, system), mySemaphore(0, 1) {}

/**
 * Initialize the CPU.
 **/
void CAlphaCPU::init() {
  memset(&state, 0, sizeof(state));

  cpu_hz = myCfg->get_num_value("speed", true, 500000000);

  state.iProcNum = cSystem->RegisterCPU(this);

  state.wait_for_start = (state.iProcNum == 0) ? false : true;
  icache_enabled = true;
  flush_icache();
  icache_enabled = myCfg->get_bool_value("icache", false);
  skip_memtest_hack = myCfg->get_bool_value("skip_memtest_hack", false);
  skip_memtest_counter = 0;

  tbia(ACCESS_READ);
  tbia(ACCESS_EXEC);

  //  state.fpcr = U64(0x8ff0000000000000);
  state.fpen = true;
  state.i_ctl_other = U64(0x502086);
  state.smc = 1;

  // SROM imitation...
  add_tb(0, 0, U64(0xff61), ACCESS_READ);

#if defined(IDB)
  bListing = false;
#endif

  cc_large = 0;
  prev_cc = 0;
  start_cc = 0;
  prev_time = 0;
  prev_icount = 0;
  start_icount = 0;

#if defined(CONSTANT_TIME_FACTOR)
  cc_per_instruction = CONSTANT_TIME_FACTOR;
#else
  cc_per_instruction = 70;
#endif
  ins_per_timer_int = cpu_hz / 1024;
  next_timer_int = state.iProcNum ? U64(0xFFFFFFFFFFFFFFFF)
                                  : ins_per_timer_int; /* only on CPU 0 */

  state.r[22] = state.r[22 + 32] = state.iProcNum;

  printf(
      "%s(%d): $Id: AlphaCPU.cpp,v 1.82 2009/03/16 01:33:27 iamcamiel Exp $\n",
      devid_string, state.iProcNum);
}

void CAlphaCPU::start_threads() {
  char buffer[5];
  mySemaphore.tryWait(1);
  if (!myThread) {
    sprintf(buffer, "cpu%d", state.iProcNum);
    myThread = std::make_unique<std::thread>([this](){ this->run(); });
    printf(" %s", buffer);
    StopThread = false;
  }
}

void CAlphaCPU::stop_threads() {
  char buffer[5];
  StopThread = true;
  if (myThread) {
    sprintf(buffer, "cpu%d", state.iProcNum);
    mySemaphore.set();
    printf(" %s", buffer);
    myThread->join();
    myThread = nullptr;
  }

  mySemaphore.tryWait(1);
}

/**
 * Destructor.
 **/
CAlphaCPU::~CAlphaCPU() { stop_threads(); }

#if defined(IDB)
char dbg_string[1000];
#if !defined(LS_MASTER) && !defined(LS_SLAVE)
char *dbg_strptr;
#endif

/**
 * \brief Do whatever needs to be done to a debug-string.
 *
 * Used in IDB-mode to handle the disassembly- string. In es40_idb, it is
 * written to the standard output.
 *
 * \param s       Pointer to the debug string.
 **/
void handle_debug_string(char *s) {
#if defined(LS_SLAVE) || defined(LS_MASTER)

  //    lockstep_compare(s);
  *dbg_strptr++ = '\n';
  *dbg_strptr = '\0';
#else
  if (*s)
    printf("%s\n", s);
#endif
}
#endif
#if defined(MIPS_ESTIMATE)

// MIPS_INTERVAL must take longer than 1 second to execute
// or estimate will generate a divide-by-zero error
#define MIPS_INTERVAL 0xfffffff
static time_t saved = 0;
static u64 count;
static double min_mips = 999999999999999.0;
static double max_mips = 0.0;
#endif

/**
 * Check if threads are still running.
 *
 * Calibrate the CPU timing loop.
 **/
void CAlphaCPU::check_state() {
  if (myThreadDead.load())
    FAILURE(Thread, "CPU thread has died");

#if !defined(CONSTANT_TIME_FACTOR)
  if (state.instruction_count > 0) {
    // correct CPU timing loop...
    u64 icount = state.instruction_count;
    u64 cc = cc_large;
    u64 time = start_time.elapsed();
    s64 ce = cc_per_instruction;

    u64 cc_aim = time * cpu_hz / 1000000; // microsecond resolution
    u64 ce_aim = cc_aim / icount;

    s64 icount_lapse = icount - prev_icount;
    s64 cc_diff = cc_aim - cc;
    s64 ce_diff = (u64)((float)cc_diff / (float)icount_lapse);

    s64 ce_new = ce_aim + ce_diff;
    if (ce_new < 0)
      ce_new = 0;
    if (ce_new > 200)
      ce_new = 200;

    if (ce_new != ce) {

      //    printf("                                     time %12" PRId64 " | prev
      //    %12" PRId64 "  \n",time,prev_time); printf("          count lapse %12"
      //    LL "d | curr %12" PRId64 " | prev %12" PRId64 "
      //    \n",icount_lapse,icount,prev_icount); printf("cc %12" PRId64 " | aim
      //    %12" PRId64 " | diff %12" PRId64 " | prev %12" PRId64 "
      //    \n",cc,cc_aim,cc_diff,prev_cc); printf("ce %12" PRId64 " | aim %12" LL
      //    "d | diff %12" PRId64 " | new  %12" PRId64 "
      //    \n",ce,ce_aim,ce_diff,ce_new);
      //    printf("==========================================================================
      //    \n");
      cc_per_instruction = ce_new;
      //    printf("cpu %d speed factor: %d\n",get_cpuid(),ce_new);
    }

    prev_cc = cc;
    prev_icount = icount;
    prev_time = time;
  }
#endif
  return;
}

/**
 * Skip SRM memtest.
 *
 * Hack that skips memory check in SRM.
 **/
inline void CAlphaCPU::skip_memtest() {
  const char *wrong_memskip = "warning: wrong memory check skip\n";
  const char *counter_mismatch = "warning: memory check skip counter mismatch";

  if (!(state.current_pc & U64(0x8b000)) || (skip_memtest_counter >= 5)) {
    return;
  }

  if (state.current_pc == U64(0x8bb90)) {
    if (state.r[5] != U64(0xaaaaaaaaaaaaaaaa)) {
      printf("%s", wrong_memskip);
    } else {
      if (skip_memtest_counter != 0)
        printf("%s", counter_mismatch);
      ++skip_memtest_counter;
      state.r[0] = state.r[4];
    }
  }

  if (state.current_pc == U64(0x8bbe0)) {
    if (state.r[5] != U64(0xaaaaaaaaaaaaaaaa)) {
      printf("%s", wrong_memskip);
    } else {
      if (skip_memtest_counter != 1)
        printf("%s", counter_mismatch);
      ++skip_memtest_counter;
      state.r[16] = 0;
    }
  }

  if (state.current_pc == U64(0x8bc28)) {
    if (state.r[5] != U64(0xaaaaaaaaaaaaaaaa)) {
      printf("%s", wrong_memskip);
    } else {
      if (skip_memtest_counter != 2)
        printf("%s", counter_mismatch);
      ++skip_memtest_counter;
      state.r[8] = state.r[4];
    }
  }

  if (state.current_pc == U64(0x8bc70)) {
    if (state.r[7] != U64(0x5555555555555555)) {
      printf("%s", wrong_memskip);
    } else {
      if (skip_memtest_counter != 3)
        printf("%s", counter_mismatch);
      ++skip_memtest_counter;
      state.r[0] = 0;
    }
  }

  if (state.current_pc == U64(0x8bcb0)) {
    if (state.r[7] != U64(0x5555555555555555)) {
      if (skip_memtest_counter != 4)
        printf("%s", counter_mismatch);
      printf("%s", wrong_memskip);
    } else {
      ++skip_memtest_counter;
      state.r[3] = state.r[4];
    }
  }
}

/**
 * \brief Called each clock-cycle.
 *
 * This is where the actual CPU emulation takes place. Each clocktick, one
 *instruction is processed by the processor. The instruction pipeline is not
 *emulated, things are complicated enough as it is. The one exception is the
 *instruction cache, which is implemented, to accomodate self-modifying code.
 *The instruction cache can be disabled if self-modifying code is not expected.
 **/
void CAlphaCPU::execute() {
  u32 ins;
  int i;
  u64 phys_address;
  u64 temp_64;
  u64 temp_64_1;
  u64 temp_64_2;

  bool pbc;

  int opcode;
  int function;

#if defined(MIPS_ESTIMATE)

  // Calculate simulated performance statistics
  if (++count >= MIPS_INTERVAL) {
    time_t current;
    time(&current);
    if (saved > 0) {
      double secs = difftime(current, saved);
      double ips = MIPS_INTERVAL / secs;
      double mips = ips / 1000000.0;
      if (max_mips < mips)
        max_mips = mips;
      if (min_mips > mips)
        min_mips = mips;
      printf("ES40 MIPS (%3.1f sec):: current: %5.3f, min: %5.3f, max: %5.3f\n",
             secs, mips, min_mips, max_mips);
    }

    saved = current;
    count = 0;
  }
#endif
#if defined(IDB)
  char *funcname = 0;
  dbg_string[0] = '\0';
#if !defined(LS_MASTER) && !defined(LS_SLAVE)
  dbg_strptr = dbg_string;
#endif
#endif
  state.current_pc = state.pc;

  if (skip_memtest_hack)
    skip_memtest();

  // Service interrupts
  if (DO_ACTION) {

    // We're actually executing code. Cycle counter should be updated, interrupt
    // and interrupt timer status needs to be checked, and the next instruction
    // should be fetched from the instruction cache. Increase the cycle counter
    // if it is currently enabled.
    state.instruction_count++;
    cc_large += cc_per_instruction;

    if (cc_large > next_timer_int) {
      next_timer_int += ins_per_timer_int;
      cSystem->interrupt(-1, true);
    }

    if (state.cc_ena) {
      state.cc += cc_per_instruction;
    }

    if (state.check_timers) {

      // There are one or more active delayed irq_h interrupts. Go through the 6
      // irq_h timers, decrease them as needed, and set the interrupt if the
      // timer reaches 0.
      state.check_timers = false;
      for (int i = 0; i < 6; i++) {
        if (state.irq_h_timer[i]) {

          // This timer is active. Decrease it, and check if it reached 0.
          state.irq_h_timer[i]--;
          if (state.irq_h_timer[i]) {

            // The timer hasn't reached 0 yet; check on the timers again next
            // clock tick.
            state.check_timers = true;
          } else {

            // The timer has reached 0. Set the interrupt status, and set the
            // flag that we need to check the interrupt status
            state.eir |= (U64(0x1) << i);
            state.check_int = true;
          }
        }
      }
    }

    if (state.check_int && !(state.pc & 1)) {

      // One or more of the variables that affect interrupt status have changed,
      // and we are not currently inside PALmode. It is not certain that this
      // means we hava an interrupt to service, but we might have. This needs to
      // be checked.

      /*
      if (state.pal_vms) {
        // PALcode base is set to 0x8000; meaning OpenVMS PALcode is currently
      active. In this
        // case, our VMS PALcode replacement routines are valid, and should be
      used as it is
        // faster than using the original PALcode.

        if (state.eir & state.eien & 6)
          if (vmspal_ent_ext_int(state.eir&state.eien & 6))
            return;

        if (state.sir & state.sien & 0xfffc)
          if (vmspal_ent_sw_int(state.sir&state.sien))
            return;

        if (state.asten && (state.aster & state.astrr & ((1<<(state.cm+1))-1) ))
          if (vmspal_ent_ast_int(state.aster & state.astrr &
      ((1<<(state.cm+1))-1) )) return;

        if (state.sir & state.sien)
          if (vmspal_ent_sw_int(state.sir&state.sien))
            return;
      } else
*/
      {

        // PALcode base is set to an unsupported value. We have no choice but to
        // transfer control to PALmode at the PALcode interrupt entry point.
        //        if (state.eir & 8)
        //        {
        //          printf("%s: IP interrupt received%s...\n",devid_string,
        //          (state.eien&8)?"(enabled)":"(masked)");
        //        }
        if ((state.eien & state.eir) || (state.sien & state.sir) ||
            (state.asten &&
             (state.aster & state.astrr & ((1 << (state.cm + 1)) - 1)))) {
          GO_PAL(INTERRUPT);
          return;
        }
      }

      // This point is reached only if there are no more active interrupts. We
      // can safely set check_int to false now to save time on the next CPU
      // clock ticks.
      state.check_int = false;
    }

    // If profiling is enabled, increase the profiling counter for the current
    // block of addresses.
#if defined(PROFILE)
    PROFILE_DO(state.pc);
#endif

    // Get the next instruction from the instruction cache.
    if (get_icache(state.pc, &ins))
      return;
#if defined(IDB)
    current_pc_physical = state.pc_phys;
#endif
  } // if (DO_ACTION)
  else {

    // We're not really executing any code (DO_ACTION is false); that means that
    // we're in a debugging session, and just listing instructions at a
    // particular address. In this case, we treat the program counter as a
    // physical address.
    ins = (u32)(cSystem->ReadMem(state.pc, 32, this));
  }

  // Increase the program counter. The current value is retained in
  // state.current_pc.
  next_pc();

  // Clear "always zero" registers. The last instruction might have written
  // something to one of these registers.
  state.r[31] = 0;
  state.f[31] = 0;

  // Decode and dispatch opcode. This is kept very compact using the OP-macro
  // defined in cpu_debug.h. For the normal emulator, this simply calls the
  // DO_<mnemonic> macro defined in one of the other cpu_*.h files; but for the
  // interactive debugger, it will also do disassembly, where the second
  // parameter to the macro (e.g. R12_R3) determines the formatting applied to
  // the operands. The macro ends with "return 0;".
#if defined(IDB)
  last_instruction = ins;
#endif
  opcode = ins >> 26;
  switch (opcode) {
  case 0x00: // CALL_PAL
    function = ins & 0x1fffffff;
    OP(CALL_PAL, PAL);

  //    switch (function)
  //    {
  //      case 0x123401: OP_FNC(vmspal_int_read_ide, NOP);
  //      default: OP(CALL_PAL,PAL);
  //    }
  case 0x08:
    OP(LDA, MEM);

  case 0x09:
    OP(LDAH, MEM);

  case 0x0a:
    OP(LDBU, MEM);

  case 0x0b:
    OP(LDQ_U, MEM);

  case 0x0c:
    OP(LDWU, MEM);

  case 0x0d:
    OP(STW, MEM);

  case 0x0e:
    OP(STB, MEM);

  case 0x0f:
    OP(STQ_U, MEM);

  case 0x10: // INTA* instructions
    function = (ins >> 5) & 0x7f;
    switch (function) {
    case 0x40:
      OP(ADDL_V, R12_R3);
    case 0x00:
      OP(ADDL, R12_R3);
    case 0x02:
      OP(S4ADDL, R12_R3);
    case 0x49:
      OP(SUBL_V, R12_R3);
    case 0x09:
      OP(SUBL, R12_R3);
    case 0x0b:
      OP(S4SUBL, R12_R3);
    case 0x0f:
      OP(CMPBGE, R12_R3);
    case 0x12:
      OP(S8ADDL, R12_R3);
    case 0x1b:
      OP(S8SUBL, R12_R3);
    case 0x1d:
      OP(CMPULT, R12_R3);
    case 0x60:
      OP(ADDQ_V, R12_R3);
    case 0x20:
      OP(ADDQ, R12_R3);
    case 0x22:
      OP(S4ADDQ, R12_R3);
    case 0x69:
      OP(SUBQ_V, R12_R3);
    case 0x29:
      OP(SUBQ, R12_R3);
    case 0x2b:
      OP(S4SUBQ, R12_R3);
    case 0x2d:
      OP(CMPEQ, R12_R3);
    case 0x32:
      OP(S8ADDQ, R12_R3);
    case 0x3b:
      OP(S8SUBQ, R12_R3);
    case 0x3d:
      OP(CMPULE, R12_R3);
    case 0x4d:
      OP(CMPLT, R12_R3);
    case 0x6d:
      OP(CMPLE, R12_R3);
    default:
      UNKNOWN2;
    }
    break;

  case 0x11: // INTL* instructions
    function = (ins >> 5) & 0x7f;
    switch (function) {
    case 0x00:
      OP(AND, R12_R3);
    case 0x08:
      OP(BIC, R12_R3);
    case 0x14:
      OP(CMOVLBS, R12_R3);
    case 0x16:
      OP(CMOVLBC, R12_R3);
    case 0x20:
      OP(BIS, R12_R3);
    case 0x24:
      OP(CMOVEQ, R12_R3);
    case 0x26:
      OP(CMOVNE, R12_R3);
    case 0x28:
      OP(ORNOT, R12_R3);
    case 0x40:
      OP(XOR, R12_R3);
    case 0x44:
      OP(CMOVLT, R12_R3);
    case 0x46:
      OP(CMOVGE, R12_R3);
    case 0x48:
      OP(EQV, R12_R3);
    case 0x61:
      OP(AMASK, R2_R3);
    case 0x64:
      OP(CMOVLE, R12_R3);
    case 0x66:
      OP(CMOVGT, R12_R3);
    case 0x6c:
      OP(IMPLVER, X_R3);
    default:
      UNKNOWN2;
    }
    break;

  case 0x12: // INTS* instructions
    function = (ins >> 5) & 0x7f;
    switch (function) {
    case 0x02:
      OP(MSKBL, R12_R3);
    case 0x06:
      OP(EXTBL, R12_R3);
    case 0x0b:
      OP(INSBL, R12_R3);
    case 0x12:
      OP(MSKWL, R12_R3);
    case 0x16:
      OP(EXTWL, R12_R3);
    case 0x1b:
      OP(INSWL, R12_R3);
    case 0x22:
      OP(MSKLL, R12_R3);
    case 0x26:
      OP(EXTLL, R12_R3);
    case 0x2b:
      OP(INSLL, R12_R3);
    case 0x30:
      OP(ZAP, R12_R3);
    case 0x31:
      OP(ZAPNOT, R12_R3);
    case 0x32:
      OP(MSKQL, R12_R3);
    case 0x34:
      OP(SRL, R12_R3);
    case 0x36:
      OP(EXTQL, R12_R3);
    case 0x39:
      OP(SLL, R12_R3);
    case 0x3b:
      OP(INSQL, R12_R3);
    case 0x3c:
      OP(SRA, R12_R3);
    case 0x52:
      OP(MSKWH, R12_R3);
    case 0x57:
      OP(INSWH, R12_R3);
    case 0x5a:
      OP(EXTWH, R12_R3);
    case 0x62:
      OP(MSKLH, R12_R3);
    case 0x67:
      OP(INSLH, R12_R3);
    case 0x6a:
      OP(EXTLH, R12_R3);
    case 0x72:
      OP(MSKQH, R12_R3);
    case 0x77:
      OP(INSQH, R12_R3);
    case 0x7a:
      OP(EXTQH, R12_R3);
    default:
      UNKNOWN2;
    }
    break;

  case 0x13: // INTM* instructions
    function = (ins >> 5) & 0x7f;
    switch (function) // ignore /V for now
    {
    case 0x40:
      OP(MULL_V, R12_R3);
    case 0x00:
      OP(MULL, R12_R3);
    case 0x60:
      OP(MULQ_V, R12_R3);
    case 0x20:
      OP(MULQ, R12_R3);
    case 0x30:
      OP(UMULH, R12_R3);
    default:
      UNKNOWN2;
    }
    break;

  case 0x14: // ITFP* instructions
    function = (ins >> 5) & 0x7ff;
    switch (function) {
    case 0x004:
      OP(ITOFS, R1_F3);

    case 0x00a:
    case 0x08a:
    case 0x10a:
    case 0x18a:
    case 0x40a:
    case 0x48a:
    case 0x50a:
    case 0x58a:
      OP(SQRTF, F2_F3);

    case 0x00b:
    case 0x04b:
    case 0x08b:
    case 0x0cb:
    case 0x10b:
    case 0x14b:
    case 0x18b:
    case 0x1cb:
    case 0x50b:
    case 0x54b:
    case 0x58b:
    case 0x5cb:
    case 0x70b:
    case 0x74b:
    case 0x78b:
    case 0x7cb:
      OP(SQRTS, F2_F3);

    case 0x014:
      OP(ITOFF, R1_F3);

    case 0x024:
      OP(ITOFT, R1_F3);

    case 0x02a:
    case 0x0aa:
    case 0x12a:
    case 0x1aa:
    case 0x42a:
    case 0x4aa:
    case 0x52a:
    case 0x5aa:
      OP(SQRTG, F2_F3);

    case 0x02b:
    case 0x06b:
    case 0x0ab:
    case 0x0eb:
    case 0x12b:
    case 0x16b:
    case 0x1ab:
    case 0x1eb:
    case 0x52b:
    case 0x56b:
    case 0x5ab:
    case 0x5eb:
    case 0x72b:
    case 0x76b:
    case 0x7ab:
    case 0x7eb:
      OP(SQRTT, F2_F3);

    default:
      UNKNOWN2;
    }
    break;

  case 0x15: // FLTV* instructions
    function = (ins >> 5) & 0x7ff;
    switch (function) {
    case 0x0a5:
    case 0x4a5:
      OP(CMPGEQ, F12_F3);

    case 0x0a6:
    case 0x4a6:
      OP(CMPGLT, F12_F3);

    case 0x0a7:
    case 0x4a7:
      OP(CMPGLE, F12_F3);

    case 0x03c:
    case 0x0bc:
      OP(CVTQF, F2_F3);

    case 0x03e:
    case 0x0be:
      OP(CVTQG, F2_F3);

    default:
      if (function & 0x200) {
        UNKNOWN2;
      }

      switch (function & 0x7f) {
      case 0x000:
        OP(ADDF, F12_F3);
      case 0x001:
        OP(SUBF, F12_F3);
      case 0x002:
        OP(MULF, F12_F3);
      case 0x003:
        OP(DIVF, F12_F3);
      case 0x01e:
        OP(CVTDG, F2_F3);
      case 0x020:
        OP(ADDG, F12_F3);
      case 0x021:
        OP(SUBG, F12_F3);
      case 0x022:
        OP(MULG, F12_F3);
      case 0x023:
        OP(DIVG, F12_F3);
      case 0x02c:
        OP(CVTGF, F12_F3);
      case 0x02d:
        OP(CVTGD, F2_F3);
      case 0x02f:
        OP(CVTGQ, F2_F3);
      default:
        UNKNOWN2;
      }
      break;
    }
    break;

  case 0x16: // FLTI* instructions
    function = (ins >> 5) & 0x7ff;
    switch (function) {
    case 0x0a4:
    case 0x5a4:
      OP(CMPTUN, F12_F3);

    case 0x0a5:
    case 0x5a5:
      OP(CMPTEQ, F12_F3);

    case 0x0a6:
    case 0x5a6:
      OP(CMPTLT, F12_F3);

    case 0x0a7:
    case 0x5a7:
      OP(CMPTLE, F12_F3);

    case 0x2ac:
    case 0x6ac:
      OP(CVTST, F2_F3);

    default:
      if (((function & 0x600) == 0x200) || ((function & 0x500) == 0x400)) {
        UNKNOWN2;
      }

      switch (function & 0x3f) {
      case 0x00:
        OP(ADDS, F12_F3);
      case 0x01:
        OP(SUBS, F12_F3);
      case 0x02:
        OP(MULS, F12_F3);
      case 0x03:
        OP(DIVS, F12_F3);
      case 0x20:
        OP(ADDT, F12_F3);
      case 0x21:
        OP(SUBT, F12_F3);
      case 0x22:
        OP(MULT, F12_F3);
      case 0x23:
        OP(DIVT, F12_F3);
      case 0x2c:
        OP(CVTTS, F2_F3);
      case 0x2f:
        OP(CVTTQ, F2_F3);
      case 0x3c:
        if ((function & 0x300) == 0x100) {
          UNKNOWN2;
        }
        OP(CVTQS, F2_F3);
      case 0x3e:
        if ((function & 0x300) == 0x100) {
          UNKNOWN2;
        }
        OP(CVTQT, F2_F3);
      default:
        UNKNOWN2;
      }
      break;
    }
    break;

  case 0x17: // FLTL* instructions
    function = (ins >> 5) & 0x7ff;
    switch (function) {
    case 0x010:
      OP(CVTLQ, F2_F3);

    case 0x020:
      OP(CPYS, F12_F3);

    case 0x021:
      OP(CPYSN, F12_F3);

    case 0x022:
      OP(CPYSE, F12_F3);

    case 0x024:
      OP(MT_FPCR, X_F1);

    case 0x025:
      OP(MF_FPCR, X_F1);

    case 0x02a:
      OP(FCMOVEQ, F12_F3);

    case 0x02b:
      OP(FCMOVNE, F12_F3);

    case 0x02c:
      OP(FCMOVLT, F12_F3);

    case 0x02d:
      OP(FCMOVGE, F12_F3);

    case 0x02e:
      OP(FCMOVLE, F12_F3);

    case 0x02f:
      OP(FCMOVGT, F12_F3);

    case 0x030:
    case 0x130:
    case 0x530:
      OP(CVTQL, F12_F3);

    default:
      UNKNOWN2;
    }
    break;

  case 0x18: // MISC* instructions
    function = (ins & 0xffff);
    switch (function) {
    case 0x0000:
      OP(TRAPB, NOP);
    case 0x0400:
      OP(EXCB, NOP);
    case 0x4000:
      OP(MB, NOP);
    case 0x4400:
      OP(WMB, NOP);
    case 0x8000:
      OP(FETCH, NOP);
    case 0xA000:
      OP(FETCH_M, NOP);
    case 0xC000:
      OP(RPCC, X_R1);
    case 0xE000:
      OP(RC, X_R1);
    case 0xE800:
      OP(ECB, NOP);
    case 0xF000:
      OP(RS, X_R1);
    case 0xF800:
      OP(WH64, NOP);
    case 0xFC00:
      OP(WH64EN, NOP);
    default:
      UNKNOWN2;
    }
    break;

  case 0x19: // HW_MFPR
    function = (ins >> 8) & 0xff;
    OP(HW_MFPR, MFPR);

  case 0x1a: // JSR* instructions
    OP(JMP, JMP);

  case 0x1b: // PAL reserved - HW_LD
    function = (ins >> 12) & 0xf;
    if (function & 1) {
      OP(HW_LDQ, HW_LD);
    } else {
      OP(HW_LDL, HW_LD);
    }

  case 0x1c: // FPTI* instructions
    function = (ins >> 5) & 0x7f;
    switch (function) {
    case 0x00:
      OP(SEXTB, R2_R3);
    case 0x01:
      OP(SEXTW, R2_R3);
    case 0x30:
      OP(CTPOP, R2_R3);
    case 0x31:
      OP(PERR, R2_R3);
    case 0x32:
      OP(CTLZ, R2_R3);
    case 0x33:
      OP(CTTZ, R2_R3);
    case 0x34:
      OP(UNPKBW, R2_R3);
    case 0x35:
      OP(UNPKBL, R2_R3);
    case 0x36:
      OP(PKWB, R2_R3);
    case 0x37:
      OP(PKLB, R2_R3);
    case 0x38:
      OP(MINSB8, R12_R3);
    case 0x39:
      OP(MINSW4, R12_R3);
    case 0x3a:
      OP(MINUB8, R12_R3);
    case 0x3b:
      OP(MINUW4, R12_R3);
    case 0x3c:
      OP(MAXUB8, R12_R3);
    case 0x3d:
      OP(MAXUW4, R12_R3);
    case 0x3e:
      OP(MAXSB8, R12_R3);
    case 0x3f:
      OP(MAXSW4, R12_R3);
    case 0x70:
      OP(FTOIT, F1_R3);
    case 0x78:
      OP(FTOIS, F1_R3);
    default:
      UNKNOWN2;
    }
    break;

  case 0x1d: // HW_MTPR
    function = (ins >> 8) & 0xff;
    OP(HW_MTPR, MTPR);

  case 0x1e:
    OP(HW_RET, RET);

  case 0x1f: // HW_ST
    function = (ins >> 12) & 0xf;
    if (function & 1) {
      OP(HW_STQ, HW_ST);
    } else {
      OP(HW_STL, HW_ST);
    }

  case 0x20:
    OP(LDF, FMEM);

  case 0x21:
    OP(LDG, FMEM);

  case 0x22:
    OP(LDS, FMEM);

  case 0x23:
    OP(LDT, FMEM);

  case 0x24:
    OP(STF, FMEM);

  case 0x25:
    OP(STG, FMEM);

  case 0x26:
    OP(STS, FMEM);

  case 0x27:
    OP(STT, FMEM);

  case 0x28:
    OP(LDL, MEM);

  case 0x29:
    OP(LDQ, MEM);

  case 0x2a:
    OP(LDL_L, MEM);

  case 0x2b:
    OP(LDQ_L, MEM);

  case 0x2c:
    OP(STL, MEM);

  case 0x2d:
    OP(STQ, MEM);

  case 0x2e:
    OP(STL_C, MEM);

  case 0x2f:
    OP(STQ_C, MEM);

  case 0x30:
    OP(BR, BR);

  case 0x31:
    OP(FBEQ, FCOND);

  case 0x32:
    OP(FBLT, FCOND);

  case 0x33:
    OP(FBLE, FCOND);

  case 0x34:
    OP(BSR, BSR);

  case 0x35:
    OP(FBNE, FCOND);

  case 0x36:
    OP(FBGE, FCOND);

  case 0x37:
    OP(FBGT, FCOND);

  case 0x38:
    OP(BLBC, COND);

  case 0x39:
    OP(BEQ, COND);

  case 0x3a:
    OP(BLT, COND);

  case 0x3b:
    OP(BLE, COND);

  case 0x3c:
    OP(BLBS, COND);

  case 0x3d:
    OP(BNE, COND);

  case 0x3e:
    OP(BGE, COND);

  case 0x3f:
    OP(BGT, COND);

  default:
    UNKNOWN1;
  }

  return;
}

#if defined(IDB)

/**
 * \brief Produce disassembly-listing without marker
 *
 * \param from    Address of first instruction to be disassembled.
 * \param to      Address of instruction following the last instruction to
 *                be disassembled.
 **/
void CAlphaCPU::listing(u64 from, u64 to) { listing(from, to, 0); }

/**
 * \brief Produce disassembly-listing with marker
 *
 * \param from    Address of first instruction to be disassembled.
 * \param to      Address of instruction following the last instruction to
 *                be disassembled.
 * \param mark    Address of instruction to be underlined with a marker line.
 **/
void CAlphaCPU::listing(u64 from, u64 to, u64 mark) {
  printf("%%CPU-I-LISTNG: Listing from %016" PRIx64 " to %016" PRIx64 "\n", from, to);

  u64 iSavedPC;
  bool bSavedDebug;
  iSavedPC = state.pc;
  bSavedDebug = bDisassemble;
  bDisassemble = true;
  bListing = true;
  for (state.pc = from; state.pc <= to;) {
    execute();
    if (state.pc == mark)
      printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
  }

  bListing = false;
  state.pc = iSavedPC;
  bDisassemble = bSavedDebug;
}

u64 CAlphaCPU::get_instruction_count() { return state.instruction_count; }
#endif
static u32 cpu_magic1 = 0x2126468C;
static u32 cpu_magic2 = 0xC8646212;

/**
 * Save state to a Virtual Machine State file.
 **/
int CAlphaCPU::SaveState(FILE *f) {
  long ss = sizeof(state);

  fwrite(&cpu_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&cpu_magic2, sizeof(u32), 1, f);
  printf("%s: %d bytes saved.\n", devid_string, (int)ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CAlphaCPU::RestoreState(FILE *f) {
  long ss;
  u32 m1;
  u32 m2;
  size_t r;

  r = fread(&m1, sizeof(u32), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (m1 != cpu_magic1) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  r = fread(&ss, sizeof(long), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (ss != sizeof(state)) {
    printf("%s: STRUCT SIZE does not match!\n", devid_string);
    return -1;
  }

  r = fread(&state, sizeof(state), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  r = fread(&m2, sizeof(u32), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (m2 != cpu_magic2) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  printf("%s: %d bytes restored.\n", devid_string, (int)ss);
  return 0;
}

/***************************************************************************/

/**
 * \name TB
 * Translation Buffer related functions
 ******************************************************************************/

//\{

/**
 * \brief Find translation-buffer entry
 *
 * Try to find a translation-buffer entry that maps the page inside which
 * the specified virtual address lies.
 *
 * \param virt    Virtual address to find in translation buffer.
 * \param flags   ACCESS_EXEC determines which translation buffer to use.
 * \return        Number of matching entry, or -1 if no match found.
 **/
int CAlphaCPU::FindTBEntry(u64 virt, int flags) {

  // Use ITB (tb[1]) if ACCESS_EXEC is set, otherwise use DTB (tb[0])
  int t = (flags & ACCESS_EXEC) ? 1 : 0;
  int asn = (flags & ACCESS_EXEC) ? state.asn : state.asn0;

  int rw = (flags & ACCESS_WRITE) ? 1 : 0;

  // Try last match first; this is a good quess, especially in the ITB
  int i = state.last_found_tb[t][rw];
  if (state.tb[t][i].valid &&
      !((state.tb[t][i].virt ^ virt) & state.tb[t][i].match_mask) &&
      (state.tb[t][i].asm_bit || (state.tb[t][i].asn == asn)))
    return i;

  // Otherwise, loop through the TB entries to find a match.
  for (i = 0; i < TB_ENTRIES; i++) {
    if (state.tb[t][i].valid &&
        !((state.tb[t][i].virt ^ virt) & state.tb[t][i].match_mask) &&
        (state.tb[t][i].asm_bit || (state.tb[t][i].asn == asn))) {
      state.last_found_tb[t][rw] = i;
      return i;
    }
  }

  return -1;
}

/**
 * \brief Translate a virtual address to a physical address.
 *
 * Translate a 64-bit virtual address into a 64-bit physical address, using
 * the page table buffers.
 *
 * The following steps are taken to resolve the address:
 *  - See if the address can be found in the translation buffer.
 *  - If not, try to load the right page table entry into the translation
 *    buffer, if this is not possible, trap to the OS.
 *  - Check access privileges.
 *  - Check fault bits.
 *  .
 *
 * \param virt    Virtual address to be translated.
 * \param phys    Pointer to where the physical address is to be returned.
 * \param flags   Set of flags that determine the exact functioning of the
 *                function. A combination of the following flags:
 *                  - ACCESS_READ   Data-read-access.
 *                  - ACCESS_WRITE  Data-write-access.
 *                  - ACCESS_EXEC   Code-read-access.
 *                  - NO_CHECK      Do not perform access checks.
 *                  - VPTE          VPTE access; if this misses, it's a double
 *miss.
 *                  - FAKE          Access is not initiated by executing code,
 *but by the debugger. If a translation can't be found through the translation
 *buffer, don't bother.
 *                  - ALT           Use alt_cm for access checks instead of cm.
 *                  - RECUR         Recursive try. We tried to find this address
 *                                  before, added a TB entry, and now it should
 *sail through.
 *                  - PROBE         Access is for a PROBER or PROBEW access;
 *Don't swap in the page if it is outswapped.
 *                  - PROBEW        Access is for a PROBEW access.
 *                  .
 * \param asm_bit Status of the ASM (address space match) bit in the
 *page-table-entry. \param ins     Instruction currently being executed.
 *Important for the correct handling of traps.
 *
 * \return        0 on success, -1 if address could not be converted without
 *                help (in this case state.pc contains the address of the
 *                next instruction to execute (PALcode or OS entry point).
 **/
int CAlphaCPU::virt2phys(u64 virt, u64 *phys, int flags, bool *asm_bit,
                         u32 ins) {
  int t = (flags & ACCESS_EXEC) ? 1 : 0;
  int i;
  int res;

  int spe = (flags & ACCESS_EXEC) ? state.i_ctl_spe : state.m_ctl_spe;
  int cm = (flags & ALT) ? state.alt_cm : state.cm;
  bool forreal = !(flags & FAKE);

#if defined IDB
  if (bListing) {
    *phys = virt;
    return 0;
  }
#endif
#if defined(DEBUG_TB)
  if (forreal)
#if defined(IDB)
    if (bTB_Debug)
#endif
      printf("TB %" PRIx64 ",%x: ", virt, flags);
#endif

  // try superpage first.
  if (spe && !cm) {
#if defined(DEBUG_TB)
    if (forreal)
#if defined(IDB)
      if (bTB_Debug)
#endif
        printf("try spe...");
#endif

    // HRM 5.3.9: SPE[2], when set, enables superpage mapping when VA[47:46]
    // = 2. In this mode, VA[43:13] are mapped directly to PA[43:13] and
    // VA[45:44] are ignored.
    if (((virt & SPE_2_MASK) == SPE_2_MATCH) && (spe & 4)) {
      *phys = virt & SPE_2_MAP;
      if (asm_bit)
        *asm_bit = false;
#if defined(DEBUG_TB)
      if (forreal)
#if defined(IDB)
        if (bTB_Debug)
#endif
          printf("SPE\n");
#endif
      return 0;
    }

    // SPE[1], when set, enables superpage mapping when VA[47:41] = 7E. In
    // this mode, VA[40:13] are mapped directly to PA[40:13] and PA[43:41] are
    // copies of PA[40] (sign extension).
    else if (((virt & SPE_1_MASK) == SPE_1_MATCH) && (spe & 2)) {
      *phys = (virt & SPE_1_MAP) | ((virt & SPE_1_TEST) ? SPE_1_ADD : 0);
      if (asm_bit)
        *asm_bit = false;
#if defined(DEBUG_TB)
      if (forreal)
#if defined(IDB)
        if (bTB_Debug)
#endif
          printf("SPE\n");
#endif
      return 0;
    }

    // SPE[0], when set, enables superpage mapping when VA[47:30] = 3FFFE.
    // In this mode, VA[29:13] are mapped directly to PA[29:13] and PA[43:30]
    // are cleared.
    else if (((virt & SPE_0_MASK) == SPE_0_MATCH) && (spe & 1)) {
      *phys = virt & SPE_0_MAP;
      if (asm_bit)
        *asm_bit = false;
#if defined(DEBUG_TB)
      if (forreal)
#if defined(IDB)
        if (bTB_Debug)
#endif
          printf("SPE\n");
#endif
      return 0;
    }
  }

  // try to find it in the translation buffer
  i = FindTBEntry(virt, flags);

  if (i < 0) // not found, either trap to PALcode, or try to load the TB entry
             // and try again.
  {
    if (!forreal)       // debugger-lookup of the address
      return -1;        // report failure, and don't look any further
    if (!state.pal_vms) // unknown PALcode
    {

      // transfer execution to PALcode
      state.exc_addr = state.current_pc;
      if (flags & VPTE) {
        state.fault_va = virt;
        state.exc_sum = (u64)REG_1 << 8;
        set_pc(state.pal_base + DTBM_DOUBLE_3 + 1);
      } else if (flags & ACCESS_EXEC) {
        set_pc(state.pal_base + ITB_MISS + 1);
      } else {
        state.fault_va = virt;
        state.exc_sum = (u64)REG_1 << 8;

        u32 opcode = I_GETOP(ins);
        state.mm_stat =
            ((opcode == 0x1b || opcode == 0x1f) ? opcode - 0x18 : opcode) << 4 |
            (flags & ACCESS_WRITE);
        set_pc(state.pal_base + DTBM_SINGLE + 1);
      }

      return -1;
    } else // VMS PALcode
    {
      if (flags & RECUR) // we already tried this
      {
        printf("Translationbuffer RECUR lookup failed!\n");
        return -1;
      }

      state.exc_addr = state.current_pc;
      if (flags & VPTE) {

        // try to handle the double miss. If this needs to transfer control
        // to the OS, it will return non-zero value.
        if ((res = vmspal_ent_dtbm_double_3(flags)))
          return res;

        // Double miss succesfully handled. Try to get the physical address
        // again.
        return virt2phys(virt, phys, flags | RECUR, asm_bit, ins);
      } else if (flags & ACCESS_EXEC) {

        // try to handle the ITB miss. If this needs to transfer control
        // to the OS, it will return non-zero value.
        if ((res = vmspal_ent_itbm(flags)))
          return res;

        // ITB miss succesfully handled. Try to get the physical address again.
        return virt2phys(virt, phys, flags | RECUR, asm_bit, ins);
      } else {
        state.fault_va = virt;
        state.exc_sum = (u64)REG_1 << 8;

        u32 opcode = I_GETOP(ins);
        state.mm_stat =
            ((opcode == 0x1b || opcode == 0x1f) ? opcode - 0x18 : opcode) << 4 |
            (flags & ACCESS_WRITE);

        // try to handle the single miss. If this needs to transfer control
        // to the OS, it will return non-zero value.
        if ((res = vmspal_ent_dtbm_single(flags)))
          return res;

        // Single miss succesfully handled. Try to get the physical address
        // again.
        return virt2phys(virt, phys, flags | RECUR, asm_bit, ins);
      }
    }
  }

  // If we get here, the number of the matching TB entry is in i.
#if defined(DEBUG_TB)
  else {
    if (forreal)
#if defined(IDB)
      if (bTB_Debug)
#endif
        printf("entry %d - ", i);
  }
#endif
  if (!(flags & NO_CHECK)) {

    // check if requested access is allowed
    if (!state.tb[t][i].access[flags & ACCESS_WRITE][cm]) {
#if defined(DEBUG_TB)
      if (forreal)
#if defined(IDB)
        if (bTB_Debug)
#endif
          printf("acv\n");
#endif
      if (flags & ACCESS_EXEC) {

        // handle I-stream access violation
        state.exc_addr = state.current_pc;
        state.exc_sum = 0;
        if (state.pal_vms) {
          if ((res = vmspal_ent_iacv(flags)))
            return res;
        } else {
          set_pc(state.pal_base + IACV + 1);
          return -1;
        }
      } else {

        // Handle D-stream access violation
        state.exc_addr = state.current_pc;
        state.fault_va = virt;
        state.exc_sum = (u64)REG_1 << 8;

        u32 opcode = I_GETOP(ins);
        state.mm_stat =
            ((opcode == 0x1b || opcode == 0x1f) ? opcode - 0x18 : opcode) << 4 |
            (flags & ACCESS_WRITE) | 2;
        if (state.pal_vms) {
          if ((res = vmspal_ent_dfault(flags)))
            return res;
        } else {
          set_pc(state.pal_base + DFAULT + 1);
          return -1;
        }
      }
    }

    // check if requested access doesn't fault
    if (state.tb[t][i].fault[flags & ACCESS_MODE]) {
#if defined(DEBUG_TB)
      if (forreal)
#if defined(IDB)
        if (bTB_Debug)
#endif
          printf("fault\n");
#endif
      if (flags & ACCESS_EXEC) {

        // handle I-stream access fault
        state.exc_addr = state.current_pc;
        state.exc_sum = 0;
        if (state.pal_vms) {
          if ((res = vmspal_ent_iacv(flags)))
            return res;
        } else {
          set_pc(state.pal_base + IACV + 1);
          return -1;
        }
      } else {

        // handle D-stream access fault
        state.exc_addr = state.current_pc;
        state.fault_va = virt;
        state.exc_sum = (u64)REG_1 << 8;

        u32 opcode = I_GETOP(ins);
        state.mm_stat =
            ((opcode == 0x1b || opcode == 0x1f) ? opcode - 0x18 : opcode) << 4 |
            (flags & ACCESS_WRITE) | ((flags & ACCESS_WRITE) ? 8 : 4);
        if (state.pal_vms) {
          if ((res = vmspal_ent_dfault(flags)))
            return res;
        } else {
          set_pc(state.pal_base + DFAULT + 1);
          return -1;
        }
      }
    }
  }

  // No access violations or faults
  // Return the converted address
  *phys = state.tb[t][i].phys | (virt & state.tb[t][i].keep_mask);
  if (asm_bit)
    *asm_bit = state.tb[t][i].asm_bit ? true : false;

#if defined(DEBUG_TB)
  if (forreal)
#if defined(IDB)
    if (bTB_Debug)
#endif
      printf("phys: %" PRIx64 " - OK\n", *phys);
#endif
  return 0;
}

#define GH_0_MATCH U64(0x000007ffffffe000) /* <42:13> */
#define GH_0_PHYS U64(0x00000fffffffe000)  /* <43:13> */
#define GH_0_KEEP U64(0x0000000000001fff)  /* <12:0>  */

#define GH_1_MATCH U64(0x000007ffffff0000)
#define GH_1_PHYS U64(0x00000fffffff0000)
#define GH_1_KEEP U64(0x000000000000ffff)
#define GH_2_MATCH U64(0x000007fffff80000)
#define GH_2_PHYS U64(0x00000ffffff80000)
#define GH_2_KEEP U64(0x000000000007ffff)
#define GH_3_MATCH U64(0x000007ffffc00000)
#define GH_3_PHYS U64(0x00000fffffc00000)
#define GH_3_KEEP U64(0x00000000003fffff)

/**
 * \brief Add translation-buffer entry
 *
 * Add a translation-buffer entry to one of the translation buffers.
 *
 * \param virt    Virtual address.
 * \param pte     Translation in DTB_PTE format (see add_tb_d).
 * \param flags   ACCESS_EXEC determines which translation buffer to use.
 **/
void CAlphaCPU::add_tb(u64 virt, u64 pte_phys, u64 pte_flags, int flags) {
  int t = (flags & ACCESS_EXEC) ? 1 : 0;
  int rw = (flags & ACCESS_WRITE) ? 1 : 0;
  u64 match_mask = 0;
  u64 keep_mask = 0;
  u64 phys_mask = 0;
  int i;
  int asn = (flags & ACCESS_EXEC) ? state.asn : state.asn0;

  switch (pte_flags & 0x60) // granularity hint
  {
  case 0:
    match_mask = GH_0_MATCH;
    phys_mask = GH_0_PHYS;
    keep_mask = GH_0_KEEP;
    break;

  case 0x20:
    match_mask = GH_1_MATCH;
    phys_mask = GH_1_PHYS;
    keep_mask = GH_1_KEEP;
    break;

  case 0x40:
    match_mask = GH_2_MATCH;
    phys_mask = GH_2_PHYS;
    keep_mask = GH_2_KEEP;
    break;

  case 0x60:
    match_mask = GH_3_MATCH;
    phys_mask = GH_3_PHYS;
    keep_mask = GH_3_KEEP;
    break;
  }

  i = FindTBEntry(virt, flags);

  if (i < 0) {
    i = state.next_tb[t];
    state.next_tb[t]++;
    if (state.next_tb[t] == TB_ENTRIES)
      state.next_tb[t] = 0;
  }

  state.tb[t][i].match_mask = match_mask;
  state.tb[t][i].keep_mask = keep_mask;
  state.tb[t][i].virt = virt & match_mask;
  state.tb[t][i].phys = pte_phys & phys_mask;
  state.tb[t][i].fault[0] = (int)pte_flags & 2;
  state.tb[t][i].fault[1] = (int)pte_flags & 4;
  state.tb[t][i].fault[2] = (int)pte_flags & 8;
  state.tb[t][i].access[0][0] = (int)pte_flags & 0x100;
  state.tb[t][i].access[1][0] = (int)pte_flags & 0x1000;
  state.tb[t][i].access[0][1] = (int)pte_flags & 0x200;
  state.tb[t][i].access[1][1] = (int)pte_flags & 0x2000;
  state.tb[t][i].access[0][2] = (int)pte_flags & 0x400;
  state.tb[t][i].access[1][2] = (int)pte_flags & 0x4000;
  state.tb[t][i].access[0][3] = (int)pte_flags & 0x800;
  state.tb[t][i].access[1][3] = (int)pte_flags & 0x8000;
  state.tb[t][i].asm_bit = (int)pte_flags & 0x10;
  state.tb[t][i].asn = asn;
  state.tb[t][i].valid = true;
  state.last_found_tb[t][rw] = i;

#if defined(DEBUG_TB_)
#if defined(IDB)
  if (bTB_Debug)
#endif
  {
    printf("Add TB---------------------------------------\n");
    printf("Map VIRT    %016" PRIx64 "\n", state.tb[i].virt);
    printf("Matching    %016" PRIx64 "\n", state.tb[i].match_mask);
    printf("And keeping %016" PRIx64 "\n", state.tb[i].keep_mask);
    printf("To PHYS     %016" PRIx64 "\n", state.tb[i].phys);
    printf("Read : %c%c%c%c %c\n", state.tb[i].access[0][0] ? 'K' : '-',
           state.tb[i].access[0][1] ? 'E' : '-',
           state.tb[i].access[0][2] ? 'S' : '-',
           state.tb[i].access[0][3] ? 'U' : '-',
           state.tb[i].fault[0] ? 'F' : '-');
    printf("Write: %c%c%c%c %c\n", state.tb[i].access[1][0] ? 'K' : '-',
           state.tb[i].access[1][1] ? 'E' : '-',
           state.tb[i].access[1][2] ? 'S' : '-',
           state.tb[i].access[1][3] ? 'U' : '-',
           state.tb[i].fault[1] ? 'F' : '-');
    printf("Exec : %c%c%c%c %c\n", state.tb[i].access[1][0] ? 'K' : '-',
           state.tb[i].access[1][1] ? 'E' : '-',
           state.tb[i].access[1][2] ? 'S' : '-',
           state.tb[i].access[1][3] ? 'U' : '-',
           state.tb[i].fault[1] ? 'F' : '-');
  }
#endif
}

/**
 * \brief Add translation-buffer entry to the DTB
 *
 * The format of the PTE field is:
 * \code
 *   63 62           32 31     16  15  14  13  12  11  10  9   8  7 6  5  4  3
 *2   1  0
 *  +--+---------------+---------+---+---+---+---+---+---+---+---+-+----+---+-+---+---+-+
 *  |  |  PA <43:13>   |         |UWE|SWE|EWE|KWE|URE|SRE|ERE|KRE| | GH |ASM|
 *|FOW|FOR| |
 *  +--+---------------+---------+---+---+---+---+---+---+---+---+-+----+---+-+---+---+-+
 *                               +-------------------------------+    |   |
 *+-------+ |        |   |       |
 *  (user,supervisor,executive,kernel)(read,write)enable ----+        |   | |
 *                                              granularity hint -----+   | |
 *                                               address space match -----+ |
 *                                                      fault-on-(read,write)
 *----+ \endcode
 *
 * \param virt    Virtual address.
 * \param pte     Translation in DTB_PTE format.
 **/
void CAlphaCPU::add_tb_d(u64 virt, u64 pte) {
  add_tb(virt, pte >> (32 - 13), pte, ACCESS_READ);
}

/**
 * \brief Add translation-buffer entry to the ITB
 *
 * The format of the PTE field is:
 * \code
 *   63              44 43           13 12  11  10  9   8  7 6  5  4  3   0
 *  +------------------+---------------+--+---+---+---+---+-+----+---+-----+
 *  |                  |  PA <43:13>   |  |URE|SRE|ERE|KRE| | GH |ASM|     |
 *  +------------------+---------------+--+---+---+---+---+-+----+---+-----+
 *                                        +---------------+    |   |
 *                                                    |        |   |
 *  (user,supervisor,executive,kernel)read enable ----+        |   |
 *                                       granularity hint -----+   |
 *                                        address space match -----+
 *
 * \endcode
 *
 * \param virt    Virtual address.
 * \param pte     Translation in ITB_PTE format.
 **/
void CAlphaCPU::add_tb_i(u64 virt, u64 pte) {
  add_tb(virt, pte, pte & 0xf70, ACCESS_EXEC);
}

/**
 * \brief Invalidate all translation-buffer entries
 *
 * Invalidate all translation-buffer entries in one of the translation buffers.
 *
 * \param flags   ACCESS_EXEC determines which translation buffer to use.
 **/
void CAlphaCPU::tbia(int flags) {
  int t = (flags & ACCESS_EXEC) ? 1 : 0;
  int i;
  for (i = 0; i < TB_ENTRIES; i++)
    state.tb[t][i].valid = false;
  state.last_found_tb[t][0] = 0;
  state.last_found_tb[t][1] = 0;
  state.next_tb[t] = 0;
}

/**
 * \brief Invalidate all process-specific translation-buffer entries
 *
 * Invalidate all translation-buffer entries that do not have the ASM bit
 * set in one of the translation buffers.
 *
 * \param flags   ACCESS_EXEC determines which translation buffer to use.
 **/
void CAlphaCPU::tbiap(int flags) {
  int t = (flags & ACCESS_EXEC) ? 1 : 0;
  int i;
  for (i = 0; i < TB_ENTRIES; i++)
    if (!state.tb[t][i].asm_bit)
      state.tb[t][i].valid = false;
}

/**
 * \brief Invalidate single translation-buffer entry
 *
 * \param virt    Virtual address for which the entry should be invalidated.
 * \param flags   ACCESS_EXEC determines which translation buffer to use.
 **/
void CAlphaCPU::tbis(u64 virt, int flags) {
  int t = (flags & ACCESS_EXEC) ? 1 : 0;
  int i = FindTBEntry(virt, flags);
  if (i >= 0)
    state.tb[t][i].valid = false;
}

//\}

/**
 * \brief Enable i-cache regardles of config file.
 *
 * Required for SRM-ROM decompression.
 **/
void CAlphaCPU::enable_icache() { icache_enabled = true; }

/**
 * \brief Enable or disable i-cache depending on config file.
 **/
void CAlphaCPU::restore_icache() {
  bool newval;

  newval = myCfg->get_bool_value("icache", false);

  if (!newval)
    flush_icache();

  icache_enabled = newval;
}

#if defined(IDB)
const char *PAL_NAME[] = {
    "HALT",       "CFLUSH",     "DRAINA",     "LDQP",
    "STQP",       "SWPCTX",     "MFPR_ASN",   "MTPR_ASTEN",
    "MTPR_ASTSR", "CSERVE",     "SWPPAL",     "MFPR_FEN",
    "MTPR_FEN",   "MTPR_IPIR",  "MFPR_IPL",   "MTPR_IPL",
    "MFPR_MCES",  "MTPR_MCES",  "MFPR_PCBB",  "MFPR_PRBR",
    "MTPR_PRBR",  "MFPR_PTBR",  "MFPR_SCBB",  "MTPR_SCBB",
    "MTPR_SIRR",  "MFPR_SISR",  "MFPR_TBCHK", "MTPR_TBIA",
    "MTPR_TBIAP", "MTPR_TBIS",  "MFPR_ESP",   "MTPR_ESP",
    "MFPR_SSP",   "MTPR_SSP",   "MFPR_USP",   "MTPR_USP",
    "MTPR_TBISD", "MTPR_TBISI", "MFPR_ASTEN", "MFPR_ASTSR",
    "28",         "MFPR_VPTB",  "MTPR_VPTB",  "MTPR_PERFMON",
    "2C",         "2D",         "MTPR_DATFX", "2F",
    "30",         "31",         "32",         "33",
    "34",         "35",         "36",         "37",
    "38",         "39",         "3A",         "3B",
    "3C",         "3D",         "WTINT",      "MFPR_WHAMI",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "-",          "-",          "-",          "-",
    "BPT",        "BUGCHK",     "CHME",       "CHMK",
    "CHMS",       "CHMU",       "IMB",        "INSQHIL",
    "INSQTIL",    "INSQHIQ",    "INSQTIQ",    "INSQUEL",
    "INSQUEQ",    "INSQUEL/D",  "INSQUEQ/D",  "PROBER",
    "PROBEW",     "RD_PS",      "REI",        "REMQHIL",
    "REMQTIL",    "REMQHIQ",    "REMQTIQ",    "REMQUEL",
    "REMQUEQ",    "REMQUEL/D",  "REMQUEQ/D",  "SWASTEN",
    "WR_PS_SW",   "RSCC",       "READ_UNQ",   "WRITE_UNQ",
    "AMOVRR",     "AMOVRM",     "INSQHILR",   "INSQTILR",
    "INSQHIQR",   "INSQTIQR",   "REMQHILR",   "REMQTILR",
    "REMQHIQR",   "REMQTIQR",   "GENTRAP",    "AB",
    "AC",         "AD",         "CLRFEN",     "AF",
    "B0",         "B1",         "B2",         "B3",
    "B4",         "B5",         "B6",         "B7",
    "B8",         "B9",         "BA",         "BB",
    "BC",         "BD",         "BE",         "BF"};

const char *IPR_NAME[] = {
    "ITB_TAG",     "ITB_PTE",     "ITB_IAP",     "ITB_IA",       "ITB_IS",
    "PMPC",        "EXC_ADDR",    "IVA_FORM",    "IER_CM",       "CM",
    "IER",         "IER_CM",      "SIRR",        "ISUM",         "HW_INT_CLR",
    "EXC_SUM",     "PAL_BASE",    "I_CTL",       "IC_FLUSH_ASM", "IC_FLUSH",
    "PCTR_CTL",    "CLR_MAP",     "I_STAT",      "SLEEP",        "?0001.1000?",
    "?0001.1001?", "?0001.1010?", "?0001.1011?", "?0001.1100?",  "?0001.1101?",
    "?0001.1110?", "?0001.1111?", "DTB_TAG0",    "DTB_PTE0",     "?0010.0010?",
    "?0010.0011?", "DTB_IS0",     "DTB_ASN0",    "DTB_ALTMODE",  "MM_STAT",
    "M_CTL",       "DC_CTL",      "DC_STAT",     "C_DATA",       "C_SHFT",
    "M_FIX",       "?0010.1110?", "?0010.1111?", "?0011.0000?",  "?0011.0001?",
    "?0011.0010?", "?0011.0011?", "?0011.0100?", "?0010.0101?",  "?0010.0110?",
    "?0010.0111?", "?0011.1000?", "?0011.1001?", "?0011.1010?",  "?0011.1011?",
    "?0011.1100?", "?0010.1101?", "?0010.1110?", "?0010.1111?",  "PCTX.00000",
    "PCTX.00001",  "PCTX.00010",  "PCTX.00011",  "PCTX.00100",   "PCTX.00101",
    "PCTX.00110",  "PCTX.00111",  "PCTX.01000",  "PCTX.01001",   "PCTX.01010",
    "PCTX.01011",  "PCTX.01100",  "PCTX.01101",  "PCTX.01110",   "PCTX.01111",
    "PCTX.10000",  "PCTX.10001",  "PCTX.10010",  "PCTX.10011",   "PCTX.10100",
    "PCTX.10101",  "PCTX.10110",  "PCTX.10111",  "PCTX.11000",   "PCTX.11001",
    "PCTX.11010",  "PCTX.11011",  "PCTX.11100",  "PCTX.11101",   "PCTX.11110",
    "PCTX.11111",  "PCTX.00000",  "PCTX.00001",  "PCTX.00010",   "PCTX.00011",
    "PCTX.00100",  "PCTX.00101",  "PCTX.00110",  "PCTX.00111",   "PCTX.01000",
    "PCTX.01001",  "PCTX.01010",  "PCTX.01011",  "PCTX.01100",   "PCTX.01101",
    "PCTX.01110",  "PCTX.01111",  "PCTX.10000",  "PCTX.10001",   "PCTX.10010",
    "PCTX.10011",  "PCTX.10100",  "PCTX.10101",  "PCTX.10110",   "PCTX.10111",
    "PCTX.11000",  "PCTX.11001",  "PCTX.11010",  "PCTX.11011",   "PCTX.11100",
    "PCTX.11101",  "PCTX.11110",  "PCTX.11111",  "?1000.0000?",  "?1000.0001?",
    "?1000.0010?", "?1000.0011?", "?1000.0100?", "?1000.0101?",  "?1000.0110?",
    "?1000.0111?", "?1000.1000?", "?1000.1001?", "?1000.1010?",  "?1000.1011?",
    "?1000.1100?", "?1000.1101?", "?1000.1110?", "?1000.1111?",  "?1001.0000?",
    "?1001.0001?", "?1001.0010?", "?1001.0011?", "?1001.0100?",  "?1001.0101?",
    "?1001.0110?", "?1001.0111?", "?1001.1000?", "?1001.1001?",  "?1001.1010?",
    "?1001.1011?", "?1001.1100?", "?1001.1101?", "?1001.1110?",  "?1001.1111?",
    "DTB_TAG1",    "DTB_PTE1",    "DTB_IAP",     "DTB_IA",       "DTB_IS1",
    "DTB_ASN1",    "?1010.0110?", "?1010.0111?", "?1010.1000?",  "?1010.1001?",
    "?1010.1010?", "?1010.1011?", "?1010.1100?", "?1010.1101?",  "?1010.1110?",
    "?1010.1111?", "?1011.0000?", "?1011.0001?", "?1011.0010?",  "?1011.0011?",
    "?1011.0100?", "?1011.0101?", "?1011.0110?", "?1011.0111?",  "?1011.1000?",
    "?1011.1001?", "?1011.1010?", "?1011.1011?", "?1011.1100?",  "?1011.1101?",
    "?1011.1110?", "?1011.1111?", "CC",          "CC_CTL",       "VA",
    "VA_FORM",     "VA_CTL",      "?1100.0101?", "?1100.0110?",  "?1100.0111?",
    "?1100.1000?", "?1100.1001?", "?1100.1010?", "?1100.1011?",  "?1100.1100?",
    "?1100.1101?", "?1100.1110?", "?1100.1111?", "?1101.0000?",  "?1101.0001?",
    "?1101.0010?", "?1101.0011?", "?1101.0100?", "?1101.0101?",  "?1101.0110?",
    "?1101.0111?", "?1101.1000?", "?1101.1001?", "?1101.1010?",  "?1101.1011?",
    "?1101.1100?", "?1101.1101?", "?1101.1110?", "?1101.1111?",  "?1110.0000?",
    "?1110.0001?", "?1110.0010?", "?1110.0011?", "?1110.0100?",  "?1110.0101?",
    "?1110.0110?", "?1110.0111?", "?1110.1000?", "?1110.1001?",  "?1110.1010?",
    "?1110.1011?", "?1110.1100?", "?1110.1101?", "?1110.1110?",  "?1110.1111?",
    "?1111.0000?", "?1111.0001?", "?1111.0010?", "?1111.0011?",  "?1111.0100?",
    "?1111.0101?", "?1111.0110?", "?1111.0111?", "?1111.1000?",  "?1111.1001?",
    "?1111.1010?", "?1111.1011?", "?1111.1100?", "?1111.1101?",  "?1111.1110?",
    "?1111.1111?",
};
#endif
