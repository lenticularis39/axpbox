/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * WWW    : http://www.es40.org
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

//#define CONSTANT_TIME_FACTOR 100

/**
 * \file
 * Contains the code for the emulated DecChip 21264CB EV68 Alpha processor.
 **/
#include "AlphaCPU.hpp"
#include "StdAfx.hpp"
#ifdef ES40_JIT
#include "jit/jitengine.hpp"
#endif
#include "AliM1543C.hpp"
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
#include "diag_rpcc.hpp"
#include "lockstep.hpp"

#include <algorithm>
#include <unordered_map>
#include <vector>
#if defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h> // _mm_setcsr: pin host MXCSR for the JIT SSE FP path
#endif

void CAlphaCPU::release_threads() {
  try {
    mySemaphore.set();
  } catch (const std::overflow_error &) {
    // Already signaled, nothing to do
  }
}

void CAlphaCPU::run() {
  try {
    mySemaphore.wait();
    while (state.wait_for_start) {
      if (StopThread)
        return;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    printf("*** CPU%d *** STARTING ***\n", get_cpuid());

    // Idle detection is single-CPU only. On SMP, OpenVMS can hand work to an
    // idle CPU with a memory store and no interrupt, which a sleeping CPU
    // would not see until its sleep ends. Checked here, not in idle_init(),
    // because every CPU is registered by the time the threads start.
    if (idle_mode && cSystem->get_cpu_num() > 1) {
      printf("%%CPU-W-IDLE: cpu%d: idle detection supports one CPU only "
             "(%d configured); idle detection is off\n",
             get_cpuid(), cSystem->get_cpu_num());
      idle_mode = 0;
    }
    if (idle_mode)
      printf("%%CPU-I-IDLE: cpu%d host idle detection: vms (IPL %d, max "
             "sleep %llu us)\n",
             get_cpuid(), idle_ipl,
             (unsigned long long)(idle_max_sleep_ns / 1000));

#if defined(_M_X64) || defined(__x86_64__)
    // Pin host SSE state for the JIT FP path: round-nearest, exceptions masked,
    // FTZ/DAZ off (denormal results must materialize to hit the interp-bail).
    _mm_setcsr(0x1F80);
#endif

    // Re-base the timing-calibration epoch to when execution actually begins:
    // a secondary parks (wait_for_start) before the primary releases it, so
    // leaving start_time at init time makes check_state() derive a wildly
    // wrong cc_per_instruction (huge elapsed wall-time vs ~0 instructions).
    start_time = std::chrono::steady_clock::now();
    next_timer_fire = start_time;
    tick_last_fire = start_time;
    cc_last_sync = start_time;
    cc_large = 0;
    state.instruction_count = 0;
    prev_icount = 0;
    prev_cc = 0;
    prev_time = 0;

    for (;;) {
      if (StopThread)
        return;
#ifdef ES40_JIT
      jit_run(2000);
#else
      if (idle_mode || idle_probe) {
        for (int i = 0; i < 2000; i++) {
          execute();
          idle_point();
        }
      } else {
        for (int i = 0; i < 2000; i++)
          execute();
      }
#endif
      if (cSystem && cSystem->IsSystemResetRequested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
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
  last_dtb_virt[0] = last_dtb_virt[1] = 0;

  cpu_hz = myCfg->get_num_value("speed", true, 500000000);
#ifdef ES40_JIT
  // With the JIT, PALcode runs natively (compiled like any other guest code)
  // rather than being shortcut by the high-level vmspal routines, so the
  // replacement is force-disabled
  vmspal_lle_enabled = true;
#else
  vmspal_lle_enabled = myCfg->get_bool_value("palcode.vms.nohle", false);
#endif

  state.iProcNum = cSystem->RegisterCPU(this);

  idle_init();

#ifdef ES40_JIT
  if (!m_jit)
    m_jit = new CJitEngine((int)state.iProcNum);
  {
    // Tell the JIT the byte offsets (from `this`) of the fields its inline load
    // fast path reads, so compiled code can address them via [cpu + offset].
    CJitEngine::JitOffsets o;
    // Slot [0][0] of the read cache; the inline load adds
    // dpc_index(va)*dpc_stride to reach the indexed slot, so these are the base
    // (slot 0) field offsets.
    o.dpc_valid =
        (uint32_t)((char *)&data_page_cache[0][0].valid - (char *)this);
    o.dpc_virt_page =
        (uint32_t)((char *)&data_page_cache[0][0].virt_page - (char *)this);
    o.dpc_phys_base =
        (uint32_t)((char *)&data_page_cache[0][0].phys_base - (char *)this);
    o.dpc_host_base =
        (uint32_t)((char *)&data_page_cache[0][0].host_base - (char *)this);
    o.dpc_cm = (uint32_t)((char *)&data_page_cache[0][0].cm - (char *)this);
    o.dpc_asn = (uint32_t)((char *)&data_page_cache[0][0].asn - (char *)this);
    o.dpc_stride = (uint32_t)sizeof(data_page_cache[0][0]);
    o.dpc_mask = (uint32_t)kDpcMask;
    o.dpc_write_row = (uint32_t)((char *)&data_page_cache[1][0] -
                                 (char *)&data_page_cache[0][0]);
    o.state_cm = (uint32_t)((char *)&state.cm - (char *)this);
    o.state_asn0 = (uint32_t)((char *)&state.asn0 - (char *)this);
    o.dram_ptr = (uint32_t)((char *)&dram_ptr - (char *)this);
    o.dram_size = (uint32_t)((char *)&dram_size - (char *)this);
    o.state_pc = (uint32_t)((char *)&state.pc - (char *)this);
    o.jit_budget = (uint32_t)((char *)&m_jit_budget - (char *)this);
    o.check_int = (uint32_t)((char *)&state.check_int - (char *)this);
    o.check_timers = (uint32_t)((char *)&state.check_timers - (char *)this);
    o.link_from = (uint32_t)((char *)&m_link_from - (char *)this);
    o.fpen = (uint32_t)((char *)&state.fpen - (char *)this);
    o.exc_sum = (uint32_t)((char *)&state.exc_sum - (char *)this);
    o.f_base = (uint32_t)((char *)&state.f[0] - (char *)this);
    o.fpcr = (uint32_t)((char *)&state.fpcr - (char *)this);
    o.exc_addr = (uint32_t)((char *)&state.exc_addr - (char *)this);
    o.pal_base = (uint32_t)((char *)&state.pal_base - (char *)this);
    o.sde = (uint32_t)((char *)&state.sde - (char *)this);
    m_jit->set_offsets(o);
  }
#endif

  state.wait_for_start = (state.iProcNum == 0) ? false : true;
  skip_memtest_hack = myCfg->get_bool_value("skip_memtest_hack", false);
  icache_enabled = true;
  flush_icache();

  tbia(ACCESS_READ);
  tbia(ACCESS_EXEC);

  //  state.fpcr = U64(0x8ff0000000000000);
  state.fpen = true;
  state.i_ctl_other = U64(0x502086);
  state.smc = 1;

  // SROM imitation...
  add_tb(0, 0, U64(0xff61), ACCESS_READ, state.asn0);

#if defined(IDB)
  bListing = false;
#endif
  myThread = nullptr;

  cc_large = 0;
  prev_cc = 0;
  start_cc = 0;
  prev_time = 0;
  prev_icount = 0;
  start_icount = 0;

  start_time = std::chrono::steady_clock::now();
  next_timer_fire = start_time;

#if defined(CONSTANT_TIME_FACTOR)
  cc_per_instruction = CONSTANT_TIME_FACTOR;
#else
  cc_per_instruction = 70;
#endif

  state.r[22] = state.r[22 + 32] = state.iProcNum;

  dram_ptr = cSystem->PtrToMem(0);
  dram_size = U64(1) << cSystem->get_memory_bits();

  flush_data_page_cache();

  seq_line_ptr = nullptr;
  seq_offset = 0;
  seq_remaining = 0;
  seq_next_pc = 0;

  printf("%s(%d): $Id$\n", devid_string, state.iProcNum);
}

void CAlphaCPU::ResetForSystemReset() {
  const int savedProcNum = state.iProcNum;

  memset(&state, 0, sizeof(state));
  last_dtb_virt[0] = last_dtb_virt[1] = 0;
  state.iProcNum = savedProcNum;

  cpu_hz = myCfg->get_num_value("speed", true, 500000000);

  state.wait_for_start = (state.iProcNum == 0) ? false : true;
  icache_enabled = true;
  flush_icache();

  tbia(ACCESS_READ);
  tbia(ACCESS_EXEC);

  state.fpen = true;
  state.i_ctl_other = U64(0x502086);
  state.smc = 1;

  // SROM imitation...
  add_tb(0, 0, U64(0xff61), ACCESS_READ, state.asn0);

  myThread = nullptr;

  cc_large = 0;
  prev_cc = 0;
  start_cc = 0;
  prev_time = 0;
  prev_icount = 0;
  start_icount = 0;

  start_time = std::chrono::steady_clock::now();
  next_timer_fire = start_time;

#if defined(CONSTANT_TIME_FACTOR)
  cc_per_instruction = CONSTANT_TIME_FACTOR;
#else
  cc_per_instruction = 70;
#endif

  state.r[22] = state.r[22 + 32] = state.iProcNum;

  dram_ptr = cSystem->PtrToMem(0);
  dram_size = U64(1) << cSystem->get_memory_bits();

  flush_data_page_cache();

  seq_line_ptr = nullptr;
  seq_offset = 0;
  seq_remaining = 0;
  seq_next_pc = 0;
}

void CAlphaCPU::start_threads() {
  char buffer[5];
  mySemaphore.tryWait(1);
  if (!myThread) {
    sprintf(buffer, "cpu%d", state.iProcNum);
    printf(" %s", buffer);
    StopThread = false;
    myThread = std::make_unique<std::thread>([this]() { this->run(); });
  }
}

void CAlphaCPU::stop_threads() {
  char buffer[5];
  {
    std::lock_guard<std::mutex> lk(idle_mtx);
    StopThread = true;
  }
  idle_cv.notify_all(); // an idle-sleeping CPU thread must see StopThread now
  if (myThread) {
    mySemaphore.set();
    sprintf(buffer, "cpu%d", state.iProcNum);
    printf(" %s", buffer);
    myThread->join();
    myThread = nullptr;
  }

  mySemaphore.tryWait(1);
}

// ============================================================================
//
// Host idle detection (cpu config `idle_detect`, default "off").
//
// An idle OpenVMS guest spins in its idle loop, which costs a whole host core.
// With `idle_detect = "vms"`, the CPU thread checks between execute() batches
// (every <= 512 instructions) whether the guest is sitting in its idle loop,
// and if so sleeps until the interval timer is next due or an interrupt
// arrives.
//
// "Idle" = VMS PALcode loaded (PAL_BASE 0x8000), kernel mode, not PALmode,
// IPL = `idle_detect.ipl` (the OpenVMS idle loop's IPL, 3 = IPL$_RESCHED), no
// software interrupt requested, nothing pending or deliverable, and no
// interrupt delivered -- sustained over 4 consecutive batch boundaries.
// The idle loop leaves when interrupt-level code makes a process computable;
// every interrupt wakes the sleeper and resets the streak, so the guest gets
// to run its idle loop again before the next sleep.
//
// Measured on OpenVMS Alpha V7.2-1, V7.3-2, V8.4 and VSI V8.4-2L2: an idle
// system spends 99.7-99.8% of its batch boundaries in kernel mode at IPL 3.
// The idle loop is not a pure poll (it sets and clears flags, bumps a counter
// and calls a queue-scan routine each pass), so it cannot be recognised by an
// unchanged state; the IPL is the signature.
//
// Guest time is unaffected: RPCC (state.cc) and the Cchip interval timer are
// wall-clock paced at each batch top, so the batch after a sleep advances
// RPCC by the real time slept and fires the due tick. The sleep deadline is
// the moment the next tick is due (CPU 0), so ticks are not delayed beyond
// host wake-up latency; the timer's count-preserving schedule repays any
// lateness. irq_h() wakes a sleeping CPU at once (device, console, NIC, IPI).
//
// AXPBOX_IDLE_PROBE=<seconds> (any mode, including off) prints a periodic
// summary of what the detector sees to stderr; it never changes behaviour.
//
// ============================================================================

namespace {
struct SIdlePcStat {
  u64 count = 0;
  u64 p22 = 0;
  int ipl = 0;
};
} // namespace

struct CIdleProbe {
  std::chrono::steady_clock::time_point last;
  std::chrono::nanoseconds interval{std::chrono::seconds(10)};
  u64 boundaries = 0, pal = 0, kern = 0, other = 0, eligible = 0;
  u64 sleeps = 0, wake_irq = 0, slept_ns = 0, over_ns = 0, over_max_ns = 0;
  u64 ipl_hist[32] = {};
  u64 ticks = 0, period_ns = 0, lag_max_ns = 0; // interval timer (CPU 0)
  std::unordered_map<u64, SIdlePcStat> pcs;
  std::unordered_map<u64, u64> r26s; // return addresses (callers) at samples
};

void CAlphaCPU::idle_init() {
  const char *m = myCfg->get_text_value("idle_detect", "off");
  idle_mode = 0;
  if (m && !strcasecmp(m, "vms"))
    idle_mode = 1;
  else if (m && strcasecmp(m, "off"))
    printf("%%CPU-W-IDLE: unknown idle_detect mode \"%s\" (use \"off\" or "
           "\"vms\"); idle detection is off\n",
           m);

  u64 us = myCfg->get_num_value("idle_detect.max_sleep_us", false, 1000);
  if (us < 50)
    us = 50;
  if (us > 100000)
    us = 100000;
  idle_max_sleep_ns = us * 1000;

  // OpenVMS Alpha idles in kernel mode at IPL 3 (see the comment above
  // idle_init).
  u64 ipl = myCfg->get_num_value("idle_detect.ipl", false, 3);
  if (ipl > 31)
    ipl = 3;
  idle_ipl = (int)ipl;

#ifdef ES40_JIT
  // Not wired into jit_run() yet.
  if (idle_mode) {
    printf("%%CPU-W-IDLE: idle detection is not supported in JIT builds; "
           "idle detection is off\n");
    idle_mode = 0;
  }
#endif

  idle_streak = 0;

  const char *pe = getenv("AXPBOX_IDLE_PROBE");
  if (pe && *pe && !idle_probe) {
    idle_probe = new CIdleProbe;
    double secs = atof(pe);
    if (secs < 1)
      secs = 10;
    idle_probe->interval = std::chrono::nanoseconds((u64)(secs * 1e9));
    idle_probe->last = std::chrono::steady_clock::now();
  }
}

void CAlphaCPU::idle_wake() {
  // Taking the mutex orders this notify after a sleeper that has checked its
  // predicate but not yet blocked (it holds the mutex until it blocks).
  { std::lock_guard<std::mutex> lk(idle_mtx); }
  idle_cv.notify_all();
}

void CAlphaCPU::idle_point() {
  const u64 pc = state.pc;
  const int ipl = (int)((state.r[32 + 22] >> 8) & 0x1f); // VMS PAL: PS in p22
  const bool kern = !(pc & 1) && state.cm == 0;

  // Idle = sustained presence in kernel mode at the VMS idle-loop IPL, with
  // no interrupt delivered and none requested, pending or deliverable, for
  // idle_need consecutive batch boundaries (>= 4 x 512 instructions, several
  // passes of the idle loop). The OpenVMS Alpha idle loop runs at IPL 3
  // (IPL$_RESCHED); real IPL-3 work (the reschedule path) is short and is
  // entered by a software interrupt, which resets the streak.
  const bool quiet = kern && state.pal_base == U64(0x8000) && ipl == idle_ipl &&
                     idle_irq_count == idle_irq_last && !state.sir &&
                     !state.check_int && !state.check_timers &&
                     !(state.eir & state.eien);
  idle_irq_last = idle_irq_count;
  idle_streak = quiet ? idle_streak + 1 : 0;
  const bool eligible = idle_streak >= idle_need;

  CIdleProbe *pr = idle_probe;
  if (pr) {
    pr->boundaries++;
    if (pc & 1)
      pr->pal++;
    else if (kern) {
      pr->kern++;
      pr->ipl_hist[ipl]++;
      pr->r26s[state.r[26]]++;
      SIdlePcStat &ps = pr->pcs[pc];
      ps.count++;
      ps.ipl = ipl;
      ps.p22 = state.r[32 + 22];
    } else
      pr->other++;
    if (eligible)
      pr->eligible++;
  }

  if (idle_mode && eligible)
    idle_sleep();

  if (pr && (pr->boundaries & 255) == 0) {
    const auto now = std::chrono::steady_clock::now();
    if (now - pr->last >= pr->interval)
      idle_probe_report(now);
  }
}

// Sleep until the next interval-timer tick is due (CPU 0), an interrupt is
// raised, the thread is stopped, or idle_max_sleep_ns has passed.
void CAlphaCPU::idle_sleep() {
  const auto now = std::chrono::steady_clock::now();
  auto deadline = now + std::chrono::nanoseconds(idle_max_sleep_ns);
  if (state.iProcNum == 0) {
    // When the interval timer is next due: execute()'s batch-top test is
    // now >= next_timer_fire and now - tick_last_fire >= period/2.
    const u64 period_ns = theAli ? theAli->get_interval_period_ns() : 0;
    auto due = next_timer_fire;
    if (period_ns) {
      const auto paced =
          tick_last_fire + std::chrono::nanoseconds(period_ns / 2);
      if (paced > due)
        due = paced;
    }
    if (due < deadline)
      deadline = due;
  }
  if (deadline <= now + std::chrono::microseconds(20))
    return;

  bool woke_by_event;
  {
    std::unique_lock<std::mutex> lk(idle_mtx);
    idle_sleeping.store(true);
    woke_by_event = idle_cv.wait_until(lk, deadline, [this] {
      return StopThread || state.check_int || state.check_timers ||
             (state.eir & state.eien);
    });
    idle_sleeping.store(false);
  }
  idle_streak = 0; // re-prove idleness before sleeping again

  CIdleProbe *pr = idle_probe;
  if (pr) {
    const auto after = std::chrono::steady_clock::now();
    pr->sleeps++;
    pr->slept_ns +=
        (u64)std::chrono::duration_cast<std::chrono::nanoseconds>(after - now)
            .count();
    if (woke_by_event)
      pr->wake_irq++;
    else if (after > deadline) {
      const u64 o = (u64)std::chrono::duration_cast<std::chrono::nanoseconds>(
                        after - deadline)
                        .count();
      pr->over_ns += o;
      if (o > pr->over_max_ns)
        pr->over_max_ns = o;
    }
  }
}

// Interval-timer accounting for the probe (interpreter lane, CPU 0): VMS
// keeps time by counting these ticks, so ticks fired vs the nominal rate and
// the schedule backlog show whether idle sleeping can make the guest clock
// drift.
void CAlphaCPU::idle_probe_tick(std::chrono::steady_clock::time_point now,
                                u64 period_ns) {
  CIdleProbe *pr = idle_probe;
  pr->ticks++;
  pr->period_ns = period_ns;
  const s64 lag = (s64)std::chrono::duration_cast<std::chrono::nanoseconds>(
                      now - next_timer_fire)
                      .count();
  if (lag > (s64)pr->lag_max_ns)
    pr->lag_max_ns = (u64)lag;
}

void CAlphaCPU::idle_probe_report(std::chrono::steady_clock::time_point now) {
  CIdleProbe *pr = idle_probe;
  const double secs =
      std::chrono::duration_cast<std::chrono::duration<double>>(now - pr->last)
          .count();
  const double b = pr->boundaries ? (double)pr->boundaries : 1.0;
  const u64 timed = pr->sleeps - pr->wake_irq;
  fprintf(stderr,
          "IDLEPROBE cpu%d %.1fs mode=%s pal_base=%llx batches=%llu "
          "pal=%.1f%% kern=%.1f%% other=%.1f%% eligible=%.1f%% "
          "sleeps=%llu (irq-woken %llu) slept=%.1f%% overshoot avg=%.0fus "
          "max=%.0fus\n",
          state.iProcNum, secs, idle_mode ? "vms" : "off",
          (unsigned long long)state.pal_base,
          (unsigned long long)pr->boundaries, 100.0 * pr->pal / b,
          100.0 * pr->kern / b, 100.0 * pr->other / b, 100.0 * pr->eligible / b,
          (unsigned long long)pr->sleeps, (unsigned long long)pr->wake_irq,
          100.0 * pr->slept_ns / (secs * 1e9),
          timed ? pr->over_ns / 1e3 / timed : 0.0, pr->over_max_ns / 1e3);
  if (pr->period_ns)
    fprintf(stderr,
            "IDLEPROBE cpu%d  ticks=%llu = %.2f Hz (nominal %.2f Hz) max "
            "lag=%.0fus backlog now=%.0fus\n",
            state.iProcNum, (unsigned long long)pr->ticks, pr->ticks / secs,
            1e9 / pr->period_ns, pr->lag_max_ns / 1e3,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                now - next_timer_fire)
                    .count() /
                1e3);
  fprintf(stderr, "IDLEPROBE cpu%d  kernel IPL histogram:", state.iProcNum);
  for (int i = 0; i < 32; i++)
    if (pr->ipl_hist[i])
      fprintf(stderr, " %d:%.1f%%", i, 100.0 * pr->ipl_hist[i] / b);
  fprintf(stderr, "\n");

  std::vector<std::pair<u64, SIdlePcStat>> v(pr->pcs.begin(), pr->pcs.end());
  std::sort(v.begin(), v.end(), [](const auto &x, const auto &y) {
    return x.second.count > y.second.count;
  });
  for (size_t i = 0; i < v.size() && i < 8; i++)
    fprintf(stderr, "IDLEPROBE cpu%d  pc=%016llx %.1f%% ipl=%d p22=%016llx\n",
            state.iProcNum, (unsigned long long)v[i].first,
            100.0 * v[i].second.count / b, v[i].second.ipl,
            (unsigned long long)v[i].second.p22);
  // 256-byte regions (loop bodies / routines) and their code, for
  // disassembly by hand.
  std::unordered_map<u64, u64> regions;
  for (const auto &e : pr->pcs)
    regions[e.first & ~U64(0xff)] += e.second.count;
  std::vector<std::pair<u64, u64>> rv(regions.begin(), regions.end());
  std::sort(rv.begin(), rv.end(),
            [](const auto &x, const auto &y) { return x.second > y.second; });
  fprintf(stderr, "IDLEPROBE cpu%d  %zu distinct kernel PCs; top regions:",
          state.iProcNum, pr->pcs.size());
  for (size_t i = 0; i < rv.size() && i < 12; i++)
    fprintf(stderr, " %llx:%.1f%%", (unsigned long long)rv[i].first,
            100.0 * rv[i].second / b);
  fprintf(stderr, "\n");
  std::vector<std::pair<u64, u64>> cv(pr->r26s.begin(), pr->r26s.end());
  std::sort(cv.begin(), cv.end(),
            [](const auto &x, const auto &y) { return x.second > y.second; });
  fprintf(stderr, "IDLEPROBE cpu%d  top r26:", state.iProcNum);
  for (size_t i = 0; i < cv.size() && i < 10; i++)
    fprintf(stderr, " %llx:%.1f%%", (unsigned long long)cv[i].first,
            100.0 * cv[i].second / b);
  fprintf(stderr, "\n");
  for (size_t r = 0; r < rv.size() && r < 4; r++) {
    const u64 base = rv[r].first;
    fprintf(stderr, "IDLEPROBE cpu%d  code@%016llx:", state.iProcNum,
            (unsigned long long)base);
    for (int i = 0; i < 64; i++) {
      u64 phys;
      bool asm_bit;
      if (virt2phys(base + 4 * i, &phys, ACCESS_EXEC | FAKE, &asm_bit, 0)) {
        fprintf(stderr, " --------");
        continue;
      }
      fprintf(stderr, " %08x", (u32)cSystem->ReadMem(phys, 32, this));
    }
    fprintf(stderr, "\n");
  }

  CIdleProbe fresh;
  fresh.interval = pr->interval;
  fresh.last = now;
  *pr = std::move(fresh);
}

// ============================================================================
//
// Alpha FPCR layout (all meaningful bits are in the upper 32 bits):
//
//   63    SUM      - Summary Bit (SUM).
//   62    INED     - Inexact Disable (INED).
//   61    UNFD     - Underflow Disable (UNFD)
//   60    UNDZ     - Underflow to Zero (UNDZ)
//   59:58 DYN      -  Dynamic Rounding Mode (DYN)
//   57    IOV      - Integer Overflow (IOV)
//   56    INE      - Inexact Result (INE)
//   55    UNF      - Underflow (UNF).
//   54    OVF      - Overflow (OVF)
//   53    DZE      - Division by Zero (DZE)
//   52    INV      - Invalid Operation (INV)
//   51    OVFD     - Overflow Disable (OVFD)
//   50    DZED     - Division by Zero Disable (DZED)
//   49    INVD     - Invalid Operation Disable (INVD)
//   48    DNZ      - Denormal Operands to Zero (DNZ)
//   47    DNOD     - Denormal Operand Exception Disable (DNOD)
//   46:0  Reserved, must be read as zero
//
//   Alpha Architecture Reference Manual, 4th ed. [ARM]:
//     Section 4.7.8   - Floating-Point Control Register (FPCR)
//     Section 4.7.8.1 - Accessing the FPCR
//     Section 4.7.8.2 - Default Values of the FPCR
//     Section 4.10.4  - Move from/to Floating-Point Control Register
//   at least according to this one
//   https://download.majix.org/dec/alpha_arch_ref.pdf
// ============================================================================

u64 CAlphaCPU::read_fpcr_arch() const {
  u64 val = state.fpcr & ~(FPCR_RAZ | U64(0x00000000FFFFFFFF));

  if (val & FPCR_ERR)
    val |= FPCR_SUM;
  else
    val &= ~FPCR_SUM;

  return val;
}

void CAlphaCPU::write_fpcr_arch(u64 arch_val) {
  u64 val = arch_val & ~(FPCR_RAZ | U64(0x00000000FFFFFFFF));

  if (val & FPCR_ERR)
    val |= FPCR_SUM;
  else
    val &= ~FPCR_SUM;

  state.fpcr = val;
}

/**
 * Destructor.
 **/
CAlphaCPU::~CAlphaCPU() {
  stop_threads();
  delete idle_probe;
}

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
#include <time.h>
#endif

// ASN switch: bump the chain epoch so compiled chain edges revalidate through
// the asn-keyed lookup paths (the chain guard checks tag+epoch only). No-op in
// non-JIT builds.
void CAlphaCPU::jit_note_asn_change() {
#ifdef ES40_JIT
  if (m_jit)
    m_jit->note_itb_invalidate();
#endif
}

#ifdef ES40_JIT
void CAlphaCPU::jit_flush_blocks() {
  if (m_jit)
    m_jit->flush();
}

// ASM-bit-clear icache flush (process/ASN switch, ITB_IAP): keep the global PAL
// blocks compiled instead of recompiling them on every such flush (the dominant
// JIT cost in PAL-heavy code).
void CAlphaCPU::jit_flush_blocks_asm() {
  if (m_jit)
    m_jit->flush_non_global();
}

void CAlphaCPU::jit_run(int budget) {
  if (m_jit)
    m_jit->reclaim_if_pending(); // deferred code reclaim, here at a safe point
                                 // (no compiled frame live)
  const auto now = std::chrono::steady_clock::now();
  cc_last_sync += std::chrono::nanoseconds(
      g_diag_excluded_ns); // keep device-diagnostic print stalls out of the
                           // RPCC (diag_rpcc.h)
  g_diag_excluded_ns = 0;
  if (cc_last_sync > now)
    cc_last_sync = now; // a stall can't exceed the batch's real elapsed; never
                        // bill negative
  auto cc_delta = now - cc_last_sync;
  cc_last_sync = now;
  // Wall-clock RPCC: advance the cycle counter by real elapsed time * cpu_hz
  // (when enabled) so it tracks the configured CPU frequency no matter how
  // fast/bursty the JIT runs. Cap (not drop) odd deltas at 1s: dropping made
  // the cc run slow through early-boot device-init stalls, and SRM's
  // cycles-per-tick calibration locked that in as a too-low CPU speed (the
  // 357MHz bug).
  if (state.cc_ena) {
    if (cc_delta > std::chrono::seconds(1))
      cc_delta = std::chrono::seconds(1);
    state.cc +=
        (u64)std::chrono::duration_cast<std::chrono::nanoseconds>(cc_delta)
            .count() *
        cpu_hz / 1000000000ULL;
  }

  // Drive the Cchip interval timer once per dispatch batch (CPU0 only), not
  // once per instruction the way the in-execute() poll did.
  if (state.iProcNum == 0) {
    if (now >= next_timer_fire) {
      const u64 period_ns = theAli ? theAli->get_interval_period_ns() : 0;
      if (period_ns) {
        // Count-preserving, paced catch-up: the schedule advances one period
        // per fire so ticks lost to a busy/stalled CPU0 thread are repaid and
        // the guests' tick-counted clocks (VMS never resyncs) stay true to wall
        // time. Repayment is paced to >= half a period between fires (max 2x
        // nominal, never a burst - burst/compressed ticks skew RPCC-vs-tick
        // calibrations). Backlog beyond 1s (debugger pause, host sleep) is
        // dropped.
        if (now - tick_last_fire >= std::chrono::nanoseconds(period_ns / 2)) {
          cSystem->interrupt(-1, true);
          tick_last_fire = now;
          next_timer_fire += std::chrono::nanoseconds(period_ns);
          if (now - next_timer_fire > std::chrono::seconds(1))
            next_timer_fire = now;
        }
      } else {
        cSystem->interrupt(-1, true);
        tick_last_fire = now;
        next_timer_fire = now + std::chrono::seconds(1);
      }
    }
  }

  while (budget > 0) {
    const u64 start_virt = state.pc;
    const u32 start_asn = (u32)state.asn;

    // Resolve the block's physical start side-effect-free (FAKE = no fault, no
    // TB fill) so execute() stays the sole I-stream fetcher; covers
    // superpage/KSEG (no TB entry). phys validates a compiled block vs the live
    // translation -- virtual+ASN keying misses remaps.
    u64 start_phys = 0;
    bool start_asm = false;
    bool have_phys = true;
    if (start_virt & 1) // PALmode: physically addressed, always ASM
    {
      start_phys = start_virt & ~U64(1);
      start_asm = true;
    } else {
      // Fast path: icache hit-probe reads phys straight from the line,
      // read-only (no fill, no fault) -- get_icache's hit geometry minus the
      // side effects. Covers warm code.
      const int ici =
          (int)(((start_virt & ~U64(3)) >> 11) & (ICACHE_ENTRIES - 1));
      if (icache_enabled && state.icache[ici].valid &&
          (state.icache[ici].asn == state.asn || state.icache[ici].asm_bit) &&
          state.icache[ici].address == (start_virt & ICACHE_MATCH_MASK)) {
        start_phys =
            state.icache[ici].p_address + (start_virt & ICACHE_BYTE_MASK);
        start_asm = state.icache[ici].asm_bit;
      }
      // Slow path: superpage/KSEG + cold pages (FAKE = no side effect; miss ->
      // interpret).
      else if (virt2phys(start_virt, &start_phys, ACCESS_EXEC | FAKE,
                         &start_asm, 0) != 0)
        have_phys = false;
    }

    // Side-effect-free exec virt -> live physical (icache probe, else FAKE
    // virt2phys). Re-resolves a trace segment's live mapping without a TB fill
    // / fault; mirrors the dispatch-top resolution above.
    auto live_exec_phys = [&](u64 v, u64 *op) -> bool {
      v &= ~U64(3); // strip the PALmode tag bit / align -- the physical is
                    // page-determined
      const int li = (int)((v >> 11) & (ICACHE_ENTRIES - 1));
      if (icache_enabled && state.icache[li].valid &&
          (state.icache[li].asn == state.asn || state.icache[li].asm_bit) &&
          state.icache[li].address == (v & ICACHE_MATCH_MASK)) {
        *op = state.icache[li].p_address + (v & ICACHE_BYTE_MASK);
        return true;
      }
      bool ad;
      return virt2phys(v, op, ACCESS_EXEC | FAKE, &ad, 0) == 0;
    };

    // Trace tier: gated lookup BEFORE the block cache. In non-debug release a
    // hot trace runs straight through and side-exits back here with state.pc
    // set, then `continue`; the block cache stays the universal fallback. Under
    // JIT_VERIFY the production run is gated OFF, traces still FORM (below),
    // but they're exercised + compared inside the block verify (next stage)
    // rather than driving execution unchecked.
#ifndef JIT_VERIFY
    // Same run-gates as the block path below: a compiled trace has no
    // per-instruction interrupt poll, so DON'T enter it while an
    // interrupt/timer is pending (the interpreter must service it, or the CPU
    // livelocks -> OS sanity-timer bugcheck), nor a PALmode trace without SDE,
    // nor one that would overrun the budget.
    auto trace_segs_live = [&](CJitEngine::TraceFragment *tf) -> bool {
      // The head's live phys is checked by trace_ok; re-resolve each INTERIOR
      // segment too. An interior page remapped to a different physical with
      // IDENTICAL bytes is invisible to the source hash.
      for (uint32_t s = 1; s < tf->n_segs; ++s) {
        u64 sp;
        if (!live_exec_phys(tf->segs[s].guest_pc, &sp) ||
            sp != tf->segs[s].phys_pc) {
          m_jit->note_trace_stale();
          return false;
        }
      }
      return true;
    };
    if (have_phys && m_jit->traces_enabled() && !state.check_int &&
        !state.check_timers) {
      CJitEngine::TraceFragment *t = m_jit->trace_lookup(start_virt, start_asn);
      if (t && (int)t->n_instr <= budget && (!(t->head_tag & 1) || state.sde) &&
          m_jit->trace_ok(t, start_phys, (const uint8_t *)dram_ptr) &&
          trace_segs_live(t)) {
        m_jit_budget = budget; // ceiling for the trace (its loads/bails honor
                               // it like a block)
#ifdef JIT_STATS
        const uint64_t _trace_t0 = jit_rdtsc();
#endif
        const u32 done = ((CJitEngine::JitFn)t->code)(this, &state.r[0]);
#ifdef JIT_STATS
        const uint64_t _trace_tsc = jit_rdtsc() - _trace_t0;
#endif
        state.r[31] = 0;
        break_seq_icache(); // the trace wrote state.pc natively; drop the stale
                            // cursor
        state.instruction_count += done;
        cc_large += (u64)done * cc_per_instruction;
        budget -= done;
#ifdef JIT_STATS
        cc_last_sync +=
            std::chrono::nanoseconds(m_jit->note_exec(done, 0, _trace_tsc, 0));
        m_jit->trace_entered();
        if (done < (u32)t->n_instr)
          m_jit->trace_exited(); // ran fewer than the trace's first-pass span
                                 // -> a side-exit/underrun
#endif
        if (done > 0)
          continue; // progress: re-dispatch at the trace's exit PC
      }
    }
#endif

    // Hot path: virtual+ASN lookup, phys-validated (skipped on a translation
    // miss).
    CJitEngine::JitBlock *b =
        have_phys ? m_jit->lookup(start_virt, start_asn) : nullptr;
    if (have_phys && !b) // lazy-flushed survivor? hash-revalidate in place (no
                         // interpreted pass)
      b = m_jit->revalidate_flushed(start_virt, start_asn, start_phys,
                                    (const uint8_t *)dram_ptr);

    // A valid block whose phys no longer matches = a page remap the virtual key
    // can't see. The cold path below re-records it; log it -- it's the smoking
    // gun for stale-chain bugs.
    if (b && b->code && b->phys != start_phys) {
      static int n_stale = 0;
      if (n_stale++ < 20)
        printf("[JIT][CPU%d] DISPATCH STALE: pc=%016llx block_phys=%016llx "
               "live_phys=%016llx\n",
               (int)state.iProcNum, (unsigned long long)start_virt,
               (unsigned long long)b->phys, (unsigned long long)start_phys);
    }

    // Run the compiled safe prefix natively when available -- but not while an
    // interrupt or delayed timer is pending. Compiled blocks don't run the
    // per-instruction polls, so run the interpreter.
    if (b && b->code && b->phys == start_phys && (int)b->prefix_len <= budget &&
        !state.check_int && !state.check_timers &&
        (!(b->tag & 1) ||
         state.sde)) // PALmode block: its shadow-register remap assumes SDE
    {
      b->vgen = m_jit->vgen(); // phys validated + lookup proved flush-fresh:
                               // refresh the chain epoch
      // after a block has dispatched 8x, promote it to a single-block trace (a
      // future trace head). Dispatch-counted for now -- it undercounts chained
      // loops, but they still surface here when a chain breaks
      if (m_jit->traces_enabled() && ++b->hot == 8 &&
          !m_jit->trace_lookup(start_virt, start_asn)) {
        const CJitEngine::HelperSet hs = {(void *)&CAlphaCPU::jit_read,
                                          (void *)&CAlphaCPU::jit_write,
                                          (void *)&CAlphaCPU::jit_opcdec,
                                          (void *)&CAlphaCPU::jit_hw_mfpr,
                                          (void *)&CAlphaCPU::jit_read_phys,
                                          (void *)&CAlphaCPU::jit_hw_mtpr,
                                          (void *)&CAlphaCPU::jit_write_phys,
                                          (void *)&CAlphaCPU::jit_indirect,
                                          (void *)&CAlphaCPU::jit_read_locked,
                                          (void *)&CAlphaCPU::jit_stc,
                                          (void *)&CAlphaCPU::jit_misc,
                                          (void *)&CAlphaCPU::jit_read_vpte,
                                          (void *)&CAlphaCPU::jit_read_wchk,
                                          (void *)&CAlphaCPU::jit_itof,
                                          (void *)&CAlphaCPU::jit_ftoi,
                                          (void *)&CAlphaCPU::jit_fltl,
                                          (void *)&CAlphaCPU::jit_fp_read,
                                          (void *)&CAlphaCPU::jit_fp_write,
                                          (void *)&CAlphaCPU::jit_fltv};
        // Loop/superblock FUSION: follow each block's STATIC taken branch
        // target, growing the trace, until the chain branches back to the head
        // (compile_trace then closes the loop) or hits a non-branch / non-live
        // target / a non-head cycle / the segment cap. Works under verify too
        // (b->link is only set by production chaining).
        CJitEngine::JitBlock *blist[CJitEngine::kMaxTraceSegs] = {b};
        u32 nb = 1;
        for (CJitEngine::JitBlock *cur = b; nb < CJitEngine::kMaxTraceSegs;) {
          const u32 *aw = (const u32 *)((const u8 *)dram_ptr + cur->phys);
          const u32 lop = aw[cur->prefix_len - 1];
          const u32 lopc = lop >> 26;
          if (!(lopc == 0x30 || lopc == 0x34 || (lopc >= 0x38 && lopc <= 0x3f)))
            break; // not PC-relative
          const int64_t disp =
              (int64_t)((uint64_t)(lop & 0x1FFFFF) << 43) >> 43;
          const u64 spc =
              (((cur->tag & ~U64(1)) + 4 * (u64)(cur->prefix_len - 1)) + 4 +
               (u64)(disp * 4)) |
              (cur->tag & 1);
          if (spc == b->tag)
            break; // back-edge to the head -> compile_trace closes the loop
          CJitEngine::JitBlock *succ = m_jit->lookup(spc, start_asn);
          if (!succ || succ == b || !succ->code || succ->prefix_len == 0)
            break;
          u64 sp;
          if (!live_exec_phys(spc, &sp) || sp != succ->phys)
            break; // successor remapped since compile -> stale, don't fuse
          bool dup = false;
          for (u32 j = 0; j < nb; ++j)
            if (blist[j] == succ) {
              dup = true;
              break;
            }
          if (dup)
            break;
          blist[nb++] = succ;
          cur = succ;
        }
        m_jit->compile_trace(m_jit->trace_slot(start_virt), blist, nb,
                             (const uint8_t *)dram_ptr, dram_size, hs);
      }
#ifdef JIT_VERIFY
      // Interpret the prefix (authoritative), recording each loaded value so
      // the compiled pass can replay it instead of re-reading memory. Skip the
      // compare if an interrupt/trap diverts us mid-prefix.
      u64 snap[64]; // 64: capture the PALshadow bank (r32..r63) for PALmode
                    // blocks
      memcpy(snap, state.r, sizeof(snap));
      u64 f_pre[64]; // FP file before the interp pass -- restored before the
                     // compiled pass below
      memcpy(f_pre, state.f, sizeof(f_pre)); // so the compiled FP ops read the
                                             // same f[] the interp did
      // I_CTL/CM/SIRR + PCTX fields (HW_MFPR 0x40-7f reads
      // astrr/aster/fpen/ppcen) are read-modify-written; a HW_MFPR reads, a
      // later HW_MTPR writes, so the interp pass writes before the compiled
      // pass's HW_MFPR reads. Snapshot the read-back fields here and restore
      // them before b->code() below -- like snap/f_pre do for GPRs/FP.
      const bool ictl_sde_pre = state.sde, ictl_hwe_pre = state.hwe;
      const int ictl_spe_pre = state.i_ctl_spe,
                ictl_vam_pre = state.i_ctl_va_mode;
      const u64 ictl_vptb_pre = state.i_ctl_vptb,
                ictl_other_pre = state.i_ctl_other;
      const int cm_pre = state.cm,
                sir_pre = state.sir; // CM/SIRR read-back (HW_MFPR CM/SIRR)
      const int aster_pre = state.aster,
                astrr_pre = state.astrr; // AST/FPEN/PPCEN read-back via the
      const int fpen_pre = state.fpen,
                ppcen_pre = state.ppcen; // PCTX group (HW_MFPR 0x40-7f)
      const u32 *vw = (const u32 *)((const u8 *)dram_ptr + b->phys);
      u32 vn = 0; // loads recorded for replay
      u32 sn = 0; // stores recorded for the compiled-pass compare
      u64 vpc = start_virt;
      u32 cur_len = b->prefix_len; // current interpreted block's length + start
                                   // tag; step 3
      u64 cur_tag = b->tag; // advances these per segment across a fused trace
      u32 n_interp = 0; // ops the interp ran over the trace's fused span (vs
                        // t->code's done)
      bool clean = true;
      CJitEngine::TraceFragment *vtr =
          m_jit->traces_enabled() ? m_jit->trace_lookup(start_virt, start_asn)
                                  : nullptr;
      if (vtr && !m_jit->trace_ok(vtr, start_phys, (const uint8_t *)dram_ptr))
        vtr = nullptr; // stale -> verify as a plain block
      const u32 nseg =
          vtr ? vtr->n_segs
              : 1; // interp the trace's full fused span; else just block b
      for (u32 seg = 0; seg < nseg && clean; ++seg) {
        if (vtr) {
          cur_len = vtr->segs[seg].n_instr;
          cur_tag = vtr->segs[seg].guest_pc;
          vw = (const u32 *)((const u8 *)dram_ptr + vtr->segs[seg].phys_pc);
          vpc = cur_tag;
        }
        for (u32 k = 0; k < cur_len; ++k) {
          // Compute the load's effective address from the live registers BEFORE
          // executing it (Rb may be the load's own dest), to compare against
          // the JIT.
          const u32 ins = vw[k];
          const u32 opc = ins >> 26;
          const int lra = (ins >> 21) & 0x1F;
          // HW_LD physical (0x1b, func 0/1) is a load too: the compiled form
          // replays through this same vlog, but its address is physical
          // (untranslated) with a 12-bit disp. Func 5 (quad VPTE) too -- its
          // logged va is virtual, jit_read_vpte's replay key.
          const bool is_hwld = (opc == 0x1b) && ((((ins >> 12) & 0xf) <= 1) ||
                                                 (((ins >> 12) & 0xf) == 5) ||
                                                 (((ins >> 12) & 0xf) == 10));
          // RPCC/RC/RS (MISC 0x18) and ISUM (HW_MFPR 0x19 fn 0x0d) read CPU
          // state the verify can't re-derive; the compiled forms pull their
          // value from this same load log (jit_misc / jit_hw_mfpr replay it),
          // so log them like loads.
          const u32 miscfn = (ins & 0xFFFF);
          // Compiled misc reads: RC/RS (0xE000/0xF000) for all Ra incl. 31 (the
          // flag-only side-effect forms compile too); RPCC (0xC000) only when
          // it has a GPR dest (Ra!=31).
          const bool is_miscrd =
              (opc == 0x18) && (miscfn == 0xE000 || miscfn == 0xF000 ||
                                (miscfn == 0xC000 && lra != 31));
          const bool is_isum =
              (opc == 0x19) &&
              (((ins >> 8) & 0xff) == 0x0d); // ISUM: async interrupt-summary
          const bool is_fpld = (opc == 0x22 || opc == 0x23 || opc == 0x20 ||
                                opc == 0x21); // LDS/LDT/LDF/LDG: dest is f[lra]
          // Loads/ISUM/LDS/LDT only log when they have a dest (lra!=31); the
          // misc reads are logged regardless -- a compiled RC/RS Ra==31 still
          // consumes a replay slot, so keep the index in sync.
          const bool isld =
              ((opc == 0x28 || opc == 0x29 || opc == 0x0a || opc == 0x0c ||
                opc == 0x2a || opc == 0x2b || opc == 0x0b || is_hwld ||
                is_isum || is_fpld) &&
               lra != 31) ||
              is_miscrd; // +LDBU/LDWU +LDx_L +LDQ_U +RPCC/RC/RS +ISUM +LDS/LDT
          u64 eva = 0;
          if (isld && !is_miscrd &&
              !is_isum) // misc/ISUM reads have no effective address -- only a
                        // logged value
          {
            const int lrb = (ins >> 16) & 0x1F;
            const int ldisp =
                is_hwld ? (int)((int32_t)(ins << 20) >> 20) // HW_LD: 12-bit
                        : (int)(int16_t)(ins & 0xFFFF);     // LDx:   16-bit
            eva = (lrb == 31 ? (u64)0 : state.r[RREG(lrb)]) + (u64)ldisp;
            if (opc == 0x0b)
              eva &= ~U64(7); // LDQ_U: address forced to 8-byte alignment
          }
          // Stores touch memory, not GPRs, so record (addr,value) for the
          // compiled-pass compare. Ra (lra) is the value source; Rb is the
          // base. HW_ST physical (0x1f func 0/1) stores too, with a physical
          // (untranslated) address and a 12-bit disp.
          const bool is_sc =
              (opc == 0x2e ||
               opc == 0x2f); // STL_C/STQ_C: store-conditional (success in Ra)
          const bool is_hwst = (opc == 0x1f) && (((ins >> 12) & 0xf) <= 1);
          const bool is_fpst =
              (opc == 0x26 || opc == 0x27 || opc == 0x24 ||
               opc == 0x25); // STS/STT/STF/STG: value source is f[lra]
          const bool isst = (opc == 0x2c || opc == 0x2d || opc == 0x0d ||
                             opc == 0x0e || opc == 0x0f || is_sc || is_hwst ||
                             is_fpst); // +STx_C +STQ_U +STS/STT
          u64 sva = 0, sval = 0;
          if (isst) {
            const int srb = (ins >> 16) & 0x1F;
            const int sdisp =
                is_hwst ? (int)((int32_t)(ins << 20) >> 20) // HW_ST: 12-bit
                        : (int)(int16_t)(ins & 0xFFFF);     // STx:   16-bit
            sva = (srb == 31 ? (u64)0 : state.r[RREG(srb)]) + (u64)sdisp;
            if (opc == 0x0f)
              sva &= ~U64(7); // STQ_U: address forced to 8-byte alignment
            if (is_fpst)
              sval = (opc == 0x27)   ? state.f[lra]
                     : (opc == 0x26) ? (u64)ieee_sts(state.f[lra])
                     : (opc == 0x24) ? (u64)vax_stf(state.f[lra])
                                     : vax_stg(state.f[lra]); // STT/STS/STF/STG
            else
              sval = (lra == 31 ? (u64)0 : state.r[RREG(lra)]);
          }
          // Computed jump (JMP/JSR/RET): target = Rb & ~3, taken before
          // execute() (the jump's target uses the old Rb, even if Ra==Rb gets
          // the return address after).
          u64 jtgt = 0;
          if (opc == 0x1a ||
              opc == 0x1e) // JMP/JSR/RET (Rb & ~3) or HW_RET/HWREI (Rb & ~2)
          {
            const int jrb = (ins >> 16) & 0x1F;
            const u64 jmask = (opc == 0x1e) ? ~U64(2) : ~U64(3);
            jtgt = (jrb == 31 ? (u64)0 : state.r[RREG(jrb)]) & jmask;
            if (opc == 0x1a)
              jtgt |= cur_tag & 3; // DO_JMP: mode bits come from the current pc
          }
          execute();
          --budget;
          vpc += 4;
          if (state.pc != vpc) {
            // The terminator (a compiled branch at the last index) diverges to
            // its target -- expected. But execute() services an async
            // interrupt/trap at the TOP, before the instruction runs, so the
            // interpreter can divert to a PAL handler at the terminator WITHOUT
            // executing the branch. Accept the divergence only when state.pc is
            // the branch's actual target; otherwise it's an interrupt/trap and
            // the compiled block (which doesn't model interrupts -- the
            // dispatcher's !check_int guard handles that) legitimately differs,
            // so skip the compare instead of flagging a false mismatch.
            bool ok_branch = false;
            if (k == cur_len - 1 &&
                (opc == 0x30 || opc == 0x34 || (opc >= 0x38 && opc <= 0x3f))) {
              const int64_t bdisp =
                  (int64_t)((uint64_t)(ins & 0x1FFFFF) << 43) >> 43;
              const u64 tgt =
                  vpc + (u64)(bdisp * 4); // vpc == branch_pc + 4 (fall-through)
              ok_branch = (state.pc == tgt);
            } else if (k == cur_len - 1 && (opc == 0x1a || opc == 0x1e)) {
              ok_branch = (state.pc == jtgt); // computed jump (JMP/HW_RET)
                                              // reached its register target
            } else if (k == cur_len - 1 && opc == 0x00) {
              // CALL_PAL vectored to its PALcode entry (pal_base | offset); the
              // kernel-mode path never traps, but accept the OPCDEC vector too.
              const u32 func = ins & 0x1FFFFFFF;
              const u64 voff = (u64)0x2000 | ((u64)(func & 0x80) << 5) |
                               ((u64)(func & 0x3f) << 6) | U64(1);
              ok_branch = (state.pc == (state.pal_base | voff)) ||
                          (state.pc == (state.pal_base | OPCDEC | U64(1)));
            }
            if (!ok_branch)
              clean = false;
            break;
          }
          if (isld && vn < 64) {
            m_jit_vaddr[vn] = eva;
            m_jit_vlog[vn] =
                is_fpld
                    ? state.f[lra]
                    : state.r[RREG(
                          lra)]; // FP dest -> f[]; shadow-aware GPR otherwise
            vn++;
          }
          if (isst && sn < 64) {
            m_jit_slog_addr[sn] = sva;
            m_jit_slog_val[sn] = sval;
            m_jit_slog_success[sn] =
                is_sc
                    ? state.r[RREG(lra)]
                    : (u64)1; // STx_C result (post-execute); ordinary store = 1
            sn++;
          }
        }
        if (!clean)
          break; // fault/divergence in this segment -> stop (compare skipped)
        n_interp += cur_len; // this segment ran fully
        if (vtr && seg + 1 < nseg && state.pc != vtr->segs[seg + 1].guest_pc)
          break; // path left the fused trace -> side-exit
      }          // end per-segment interp loop
      if (clean) {
        // HW_MTPR verify: a compiled block writes IPR fields directly in LIVE
        // state (its GPR writes go to jr scratch). Snapshot the writable IPR
        // set after the interp pass (authoritative); below we compare the
        // compiled pass's IPR writes and roll the live fields back. Mostly pure
        // stores; I_CTL is read-modify-written, so it's also reset pre-pass
        // (ictl_*_pre, above). Keep this list in sync with jit_hw_mtpr.
        auto cap_iprs = [&](u64 *d) {
          d[0] = state.last_tb_virt;
          d[1] = last_dtb_virt[0];
          d[2] = last_dtb_virt[1];
          d[3] = state.pctr_ctl;
          d[4] = state.dc_ctl;
          d[5] = (u64)state.cc_offset;
          d[6] = (u64)(u32)state.alt_cm;
          // IER enables; check_int not snapshotted (rolling it back could
          // suppress a poll)
          d[7] = (u64)(u32)state.asten;
          d[8] = (u64)(u32)state.sien;
          d[9] = (u64)(u32)state.pcen;
          d[10] = (u64)(u32)state.cren;
          d[11] = (u64)(u32)state.slen;
          d[12] = (u64)(u32)state.eien;
          d[13] =
              state.exc_sum;  // FPSTART clears it (compiled ITOFx/FTOIx/FLTL)
          d[14] = state.fpcr; // MT_FPCR (compiled FLTL) writes it
          d[15] = (u64)state.sde;
          d[16] = (u64)state.hwe; // I_CTL (compiled as terminator)
          d[17] = (u64)(u32)state.i_ctl_spe;
          d[18] = (u64)(u32)state.i_ctl_va_mode;
          d[19] = state.i_ctl_vptb;
          d[20] = state.i_ctl_other;
          d[21] = (u64)(u32)state.cm;
          d[22] = (u64)(u32)state.sir; // CM, SIRR
          d[23] = (u64)(u32)state.aster;
          d[24] = (u64)(u32)state.astrr; // 0x40-group ASTER/ASTRR
          d[25] = (u64)(u32)state.fpen;
          d[26] = (u64)(u32)state.ppcen; // 0x40-group FPEN/PPCEN
        };
        auto put_iprs = [&](const u64 *s) {
          state.last_tb_virt = s[0];
          last_dtb_virt[0] = s[1];
          last_dtb_virt[1] = s[2];
          state.pctr_ctl = s[3];
          state.dc_ctl = s[4];
          state.cc_offset = (u32)s[5];
          state.alt_cm = (int)s[6];
          state.asten = (int)s[7];
          state.sien = (int)s[8];
          state.pcen = (int)s[9];
          state.cren = (int)s[10];
          state.slen = (int)s[11];
          state.eien = (int)s[12];
          state.exc_sum = s[13];
          state.fpcr = s[14];
          state.sde = (bool)s[15];
          state.hwe = (bool)s[16];
          state.i_ctl_spe = (int)s[17];
          state.i_ctl_va_mode = (int)s[18];
          state.i_ctl_vptb = s[19];
          state.i_ctl_other = s[20];
          state.cm = (int)s[21];
          state.sir = (int)s[22];
          state.aster = (int)s[23];
          state.astrr = (int)s[24];
          state.fpen = (int)s[25];
          state.ppcen = (int)s[26];
        };
        u64 ipr_interp[27];
        cap_iprs(ipr_interp);
        // FP file: compiled ITOFx writes state.f live (like the IPR writes);
        // snapshot the interp pass's result, compare after the compiled pass,
        // then restore.
        u64 f_interp[64];
        memcpy(f_interp, state.f, sizeof(f_interp));
        u64 jr[64]; // 64: a compiled PALmode block may touch the shadow bank
        memcpy(jr, snap, sizeof(jr));
        memcpy(state.f, f_pre,
               sizeof(f_pre)); // restore the FP file like the GPRs: the
                               // compiled pass reads pre-interp f
        state.sde = ictl_sde_pre;
        state.hwe = ictl_hwe_pre; // restore I_CTL too: a compiled HW_MFPR must
                                  // read the pre-interp value
        state.i_ctl_spe = ictl_spe_pre;
        state.i_ctl_va_mode = ictl_vam_pre;
        state.i_ctl_vptb = ictl_vptb_pre;
        state.i_ctl_other = ictl_other_pre;
        state.cm = cm_pre;
        state.sir = sir_pre; // ...and CM/SIRR
        state.aster = aster_pre;
        state.astrr = astrr_pre; // ...and the PCTX read-backs (a
        state.fpen = fpen_pre;
        state.ppcen = ppcen_pre; // HW_MFPR PCTX reads these live)
        const u64 interp_pc =
            state.pc; // interpreter is authoritative for the PC
        const u32 n_stores_interp =
            sn; // interp's RECORDED store count (sn), NOT the replay cursor
                // m_jit_slog_i; a compiled pass must consume exactly this many
                // (catches a missing/extra store)
        m_jit_vreplay = true;
        m_jit_vlog_i = 0;
        m_jit_slog_i = 0;
        const u32 done =
            b->code(this, jr); // also writes state.pc (the JIT's next PC)
        m_jit_vreplay = false;
        if (!vtr && done == b->prefix_len) {
          if (state.pc != interp_pc) {
            // Dump the JIT's source (DRAM at b->phys, vw[]) vs the icache (what
            // the interpreter actually fetches), word by word. If the middle
            // words differ the icache holds a stale/different version the JIT
            // never compiled.
            const int icl = (int)((start_virt >> 11) & (ICACHE_ENTRIES - 1));
            const u32 wb = (u32)((start_virt >> 2) & ICACHE_INDEX_MASK);
            printf("[JIT][VERIFY] PC MISMATCH at %016llx: interp=%016llx "
                   "jit=%016llx plen=%u\n",
                   (unsigned long long)start_virt,
                   (unsigned long long)interp_pc, (unsigned long long)state.pc,
                   b->prefix_len);
            printf("   dram:");
            for (u32 w = 0; w < b->prefix_len && w < 16; ++w)
              printf(" %08x", vw[w]);
            printf("\n   icch:");
            for (u32 w = 0; w < b->prefix_len && w < 16; ++w)
              printf(" %08x",
                     endian_32(
                         state.icache[icl].data[(wb + w) & ICACHE_INDEX_MASK]));
            printf("\n   r1 i=%016llx j=%016llx  r2 i=%016llx j=%016llx  r5 "
                   "snap=%016llx\n",
                   (unsigned long long)state.r[1], (unsigned long long)jr[1],
                   (unsigned long long)state.r[2], (unsigned long long)jr[2],
                   (unsigned long long)snap[5]);
          }
          u64 ipr_jit[27];
          cap_iprs(ipr_jit); // compiled pass wrote IPRs into live state; check
                             // vs interp
          for (int ii = 0; ii < 27; ii++)
            if (ipr_jit[ii] != ipr_interp[ii])
              printf("[JIT][VERIFY] IPR MISMATCH at %016llx slot %d: "
                     "interp=%016llx jit=%016llx\n",
                     (unsigned long long)start_virt, ii,
                     (unsigned long long)ipr_interp[ii],
                     (unsigned long long)ipr_jit[ii]);
          for (int fi = 0; fi < 64; fi++)
            if (state.f[fi] != f_interp[fi])
              printf("[JIT][VERIFY] FP MISMATCH at %016llx f%d: interp=%016llx "
                     "jit=%016llx\n",
                     (unsigned long long)start_virt, fi,
                     (unsigned long long)f_interp[fi],
                     (unsigned long long)state.f[fi]);
          if (m_jit_slog_i != n_stores_interp)
            printf("[JIT][VERIFY] STORE COUNT MISMATCH at %016llx: interp=%u "
                   "jit=%u\n",
                   (unsigned long long)start_virt, n_stores_interp,
                   m_jit_slog_i);
          cc_last_sync += std::chrono::nanoseconds(
              m_jit->verify_compare(start_virt, state.r, jr, vw,
                                    b->prefix_len) +
              g_diag_excluded_ns); // don't bill the verify progress-print OR
                                   // the PCI decode-off diag stall to the RPCC
          g_diag_excluded_ns =
              0; // consumed here in the verify path so jit_run's later sync
                 // doesn't double-count it
        }
        // verify extension: the production trace-run hook is gated off under
        // JIT_VERIFY, so exercise the trace HERE; re-restore pre-interp state,
        // run t->code on a 2nd scratch (replaying the same logged loads), then
        // compare to the authoritative interp. This makes the trace's own
        // state.pc write + frame visible to the differential harness.
        if (m_jit->traces_enabled()) {
          CJitEngine::TraceFragment *tr =
              m_jit->trace_lookup(start_virt, start_asn);
          if (tr &&
              m_jit->trace_ok(tr, start_phys, (const uint8_t *)dram_ptr) &&
              [&] {
                for (uint32_t s = 1; s < tr->n_segs; ++s) {
                  u64 sp;
                  if (!live_exec_phys(tr->segs[s].guest_pc, &sp) ||
                      sp != tr->segs[s].phys_pc)
                    return false;
                }
                return true;
              }()) { // verify now mirrors the production hook's interior
                     // live-phys check
            memcpy(state.f, f_pre, sizeof(f_pre));
            state.sde = ictl_sde_pre;
            state.hwe = ictl_hwe_pre;
            state.i_ctl_spe = ictl_spe_pre;
            state.i_ctl_va_mode = ictl_vam_pre;
            state.i_ctl_vptb = ictl_vptb_pre;
            state.i_ctl_other = ictl_other_pre;
            state.cm = cm_pre;
            state.sir = sir_pre;
            state.aster = aster_pre;
            state.astrr = astrr_pre;
            state.fpen = fpen_pre;
            state.ppcen = ppcen_pre;
            u64 jr_t[64];
            memcpy(jr_t, snap, sizeof(jr_t));
            m_jit_vreplay = true;
            m_jit_vlog_i = 0;
            m_jit_slog_i = 0;
            const u32 done_t = ((CJitEngine::JitFn)tr->code)(this, jr_t);
            m_jit_vreplay = false;
            if (done_t != n_interp)
              printf("[JIT][VERIFY] TRACE COUNT MISMATCH at %016llx: "
                     "interp_span=%u trace_done=%u\n",
                     (unsigned long long)start_virt, n_interp, done_t);
            if (done_t == n_interp) {
              if (state.pc != interp_pc)
                printf("[JIT][VERIFY] TRACE PC MISMATCH at %016llx: "
                       "interp=%016llx trace=%016llx (n=%u)\n",
                       (unsigned long long)start_virt,
                       (unsigned long long)interp_pc,
                       (unsigned long long)state.pc, n_interp);
              u64 ipr_jit_t[27];
              cap_iprs(ipr_jit_t); // trace wrote IPRs into live state; check vs
                                   // interp (parity with the block path)
              for (int ii = 0; ii < 27; ii++)
                if (ipr_jit_t[ii] != ipr_interp[ii])
                  printf("[JIT][VERIFY] TRACE IPR MISMATCH at %016llx slot %d: "
                         "interp=%016llx trace=%016llx\n",
                         (unsigned long long)start_virt, ii,
                         (unsigned long long)ipr_interp[ii],
                         (unsigned long long)ipr_jit_t[ii]);
              for (int fi = 0; fi < 64; fi++)
                if (state.f[fi] != f_interp[fi])
                  printf("[JIT][VERIFY] TRACE FP MISMATCH at %016llx f%d: "
                         "interp=%016llx trace=%016llx\n",
                         (unsigned long long)start_virt, fi,
                         (unsigned long long)f_interp[fi],
                         (unsigned long long)state.f[fi]);
              if (m_jit_slog_i != n_stores_interp)
                printf("[JIT][VERIFY] TRACE STORE COUNT MISMATCH at %016llx: "
                       "interp=%u trace=%u\n",
                       (unsigned long long)start_virt, n_stores_interp,
                       m_jit_slog_i);
              m_jit->verify_compare(start_virt, state.r, jr_t, vw, n_interp);
            }
          }
        }
        state.pc =
            interp_pc; // restore; the block's PC write was only for the check
        put_iprs(ipr_interp); // roll back the compiled pass's live IPR writes
                              // (verify-only)
        memcpy(state.f, f_interp, sizeof(f_interp)); // ...and its FP writes
        break_seq_icache(); // compiled pass + raw pc restore bypassed set_pc
      }
      continue;
#else
      // A predecessor block's epilogue cache-missed and asked to be linked
      // here. Now that we know this block is live and compiled, patch its
      // successor pointer so it jumps straight in instead of returning
      if (m_link_from) {
        m_jit->note_link_bail();
        m_jit->note_link_edge((CJitEngine::JitBlock *)m_link_from, b->tag);
        CJitEngine::JitBlock *lf = (CJitEngine::JitBlock *)m_link_from;
        bool in = false; // poly-link: cache b in the source's successor slots
        for (int i = 0; i < CJitEngine::kLinkSlots; ++i)
          if (lf->link[i] == b)
            in = true; // skip if already cached (it just went stale)
        if (!in) {
          for (int i = CJitEngine::kLinkSlots - 1; i > 0; --i)
            lf->link[i] = lf->link[i - 1];
          lf->link[0] = b;
        } // else round-robin insert
        m_link_from = nullptr;
      }
      m_jit_budget =
          budget; // ceiling for compiled chains (epilogue stops at it)
#ifdef JIT_STATS
      const uint64_t _comp_t0 = jit_rdtsc();
#endif
      const u32 done = b->code(this, &state.r[0]);
#ifdef JIT_STATS
      const uint64_t _comp_tsc =
          jit_rdtsc() - _comp_t0; // host cycles in this compiled chain
#endif
      state.r[31] = 0;
      break_seq_icache(); // compiled block wrote pc natively (no set_pc); drop
                          // the stale cursor
      // state.pc is written by the compiled block itself (next PC, or the bail
      // PC). Account for the compiled ops: instruction count (+ cc_large for
      // the legacy speed calibration). state.cc (RPCC) is no longer advanced
      // per-instruction. wall-clock * cpu_hz at the jit_run boundary above, so
      // a hot compiled loop can't run the cycle counter ahead of real time
      state.instruction_count += done;
      cc_large += (u64)done * cc_per_instruction;
      budget -= done;
#ifdef JIT_STATS
      cc_last_sync += std::chrono::nanoseconds(m_jit->note_exec(
          done, 0, _comp_tsc,
          0)); // don't bill the stats-print stall to the wall-clock RPCC
#endif
      if (done > 0)
        continue; // progress made; done==0 (faulting first insn) falls through
#endif
    }

    // Miss path (cold): the up-front translation gave start_phys/start_asm
    // (when have_phys). We're not running a compiled block here, so drop any
    // pending link request, interpret the block, record it, and compile its
    // prefix. Count the link-miss bail before dropping it.
    if (m_link_from)
      m_jit->note_link_bail();
    m_link_from = nullptr;
    u32 n = 0;
    u64 expected = start_virt;
#ifdef JIT_STATS
    const uint64_t _interp_t0 = jit_rdtsc();
#endif
    while (budget > 0) {
      execute();
      --budget;
      ++n;
      expected += 4;
      if (state.pc != expected)
        break;
    }
#ifdef JIT_STATS
    cc_last_sync += std::chrono::nanoseconds(m_jit->note_exec(
        0, n, 0, jit_rdtsc() - _interp_t0)); // don't bill the stats-print stall
                                             // to the wall-clock RPCC
#endif
    // Record only translatable block starts (a translation miss left have_phys
    // false).
    if (have_phys && state.pc != expected) {
      CJitEngine::JitBlock *nb =
          m_jit->record(start_virt, start_phys, start_asn, start_asm, n,
                        (const uint8_t *)dram_ptr);
      if (!nb->compiled)
        m_jit->compile_block(
            nb, (const uint8_t *)dram_ptr, dram_size,
            (void *)&CAlphaCPU::jit_read, (void *)&CAlphaCPU::jit_write,
            (void *)&CAlphaCPU::jit_opcdec, (void *)&CAlphaCPU::jit_hw_mfpr,
            (void *)&CAlphaCPU::jit_read_phys, (void *)&CAlphaCPU::jit_hw_mtpr,
            (void *)&CAlphaCPU::jit_write_phys,
            (void *)&CAlphaCPU::jit_indirect,
            (void *)&CAlphaCPU::jit_read_locked, (void *)&CAlphaCPU::jit_stc,
            (void *)&CAlphaCPU::jit_misc, (void *)&CAlphaCPU::jit_read_vpte,
            (void *)&CAlphaCPU::jit_read_wchk, (void *)&CAlphaCPU::jit_itof,
            (void *)&CAlphaCPU::jit_ftoi, (void *)&CAlphaCPU::jit_fltl,
            (void *)&CAlphaCPU::jit_fp_read, (void *)&CAlphaCPU::jit_fp_write,
            (void *)&CAlphaCPU::jit_fltv);
    }
  }
}

// JIT load helper (static). Reads size_bits from virtual address va into *out,
// mirroring DATA_PHYS_NT's normal-read fast path. Returns 0 on success, or 1 on
// a translation fault / unaligned access - the caller bails to the interpreter
int CAlphaCPU::jit_read(CAlphaCPU *cpu, u64 va, int size_bits, u64 *out) {
  const u64 amask = (u64)(size_bits / 8) - 1;
  if (va & amask)
    return 1; // unaligned: let the interpreter handle it

  u64 phys;
  const u64 vp = va & ~U64(0x1FFF);
  SDataPageCache &dpc =
      cpu->data_page_cache[0][dpc_index(va)]; // direct-mapped by virt page
  if (dpc.valid && dpc.virt_page == vp && dpc.cm == cpu->state.cm &&
      dpc.asn == cpu->state.asn0) {
    phys = dpc.phys_base | (va & U64(0x1FFF));
  } else {
    // Side-effect-free TB fast path. NOT virt2phys - that walks the page table
    // and vectors faults as a side effect, which (re-done by the interpreter
    // after we bail) corrupts state. Bail on a TB miss or any access fault and
    // let the interpreter do the side-effect translation ~  it fills this cache
    // so the next compiled run hits. Mirrors virt2phys's read path.
    const int i = cpu->FindTBEntry(va, ACCESS_READ);
    if (i < 0)
      return 1; // TB miss
    const auto &e = cpu->state.tb[TB_INDEX_DATA][i];
    if (!e.access[0][cpu->state.cm])
      return 1; // protection (ACV)
    if (e.fault[0])
      return 1; // fault-on-read (FOR)
    phys = e.phys | (va & e.keep_mask);
    dpc.virt_page = vp;
    dpc.phys_base = phys & ~U64(0x1FFF);
    dpc.host_base = ((phys | U64(0x1FFF)) < cpu->dram_size)
                        ? ((u64)cpu->dram_ptr + (phys & ~U64(0x1FFF)))
                        : 0;
    dpc.cm = cpu->state.cm;
    dpc.asn = cpu->state.asn0;
    dpc.valid = true;
  }

  // DRAM only: bail on MMIO so the interpreter does device reads (side effects
  // + ordering). This MUST precede the verify replay: production bails here,
  // and a device byte/word read is NOT size-truncated (LDBU/LDWU use READ_VIRT,
  // no sext func), so replaying an MMIO value and then re-truncating it with
  // movzx would falsely mismatch the interpreter. Bailing -> the compiled block
  // stops at the load (done < prefix_len) and the verify skips the compare,
  // matching prod.
  if (phys >= cpu->dram_size)
    return 1;

  // Verify replay: return the value the interpreter pass loaded here, rather
  // than re-reading (another CPU may have written it)
  if (cpu->m_jit_vreplay) {
    if (va != cpu->m_jit_vaddr[cpu->m_jit_vlog_i]) {
      static int n = 0;
      if (n++ < 50)
        printf(
            "[JIT] LOAD ADDR MISMATCH: compiled va=%016llx interp va=%016llx\n",
            (unsigned long long)va,
            (unsigned long long)cpu->m_jit_vaddr[cpu->m_jit_vlog_i]);
    }
    *out = cpu->m_jit_vlog[cpu->m_jit_vlog_i++];
    return 0;
  }

  *out = dram_read(cpu->dram_ptr, phys, size_bits);
  return 0;
}

// JIT FP load helper (static). LDS/LDT: f[fa] = convert(MEM[va]) per
// DO_LDS/DO_LDT. descr packs size (low 16) and fmt (1=S/ieee_lds, 0=T/raw, high
// bits). FPSTART (fpen -> FEN bail; exc_sum=0). Production reuses jit_read's
// side-effect-free cache read; verify replays the interp's CONVERTED f-value
// (FP loads join the load log), so it consumes m_jit_vlog like an integer load.
int CAlphaCPU::jit_fp_read(CAlphaCPU *cpu, u64 va, u32 fa, u32 descr) {
  if (cpu->state.fpen == 0)
    return 1; // FEN trap (FPSTART)
  cpu->state.exc_sum = 0;
  if (fa == 31)
    return 0; // f31 dest: interp skips the read
  if (cpu->m_jit_vreplay) {
    const u32 i = cpu->m_jit_vlog_i++;
    if (va != cpu->m_jit_vaddr[i]) {
      static int n = 0;
      if (n++ < 50)
        printf("[JIT] FP LOAD ADDR MISMATCH: compiled va=%016llx interp "
               "va=%016llx\n",
               (unsigned long long)va, (unsigned long long)cpu->m_jit_vaddr[i]);
    }
    cpu->state.f[fa] =
        cpu->m_jit_vlog[i]; // logged converted value (no re-convert)
    return 0;
  }
  u64 raw;
  if (jit_read(cpu, va, (int)(descr & 0xffff), &raw))
    return 1;          // production cache read
  switch (descr >> 16) // fmt: 0=T raw, 1=S ieee, 2=F vax, 3=G vax
  {
  case 1:
    cpu->state.f[fa] = cpu->ieee_lds((u32)raw);
    break; // LDS
  case 2:
    cpu->state.f[fa] = cpu->vax_ldf((u32)raw);
    break; // LDF
  case 3:
    cpu->state.f[fa] = cpu->vax_ldg(raw);
    break; // LDG
  default:
    cpu->state.f[fa] = raw;
    break; // LDT raw
  }
  return 0;
}

// JIT FP store helper (static). STS/STT: MEM[va] = convert(f[fa]) per
// DO_STS/DO_STT. Routes through jit_write so verify compares vs the store log
// (FP stores join it) and production writes.
int CAlphaCPU::jit_fp_write(CAlphaCPU *cpu, u64 va, u32 fa, u32 descr) {
  if (cpu->state.fpen == 0)
    return 1; // FEN trap (FPSTART)
  cpu->state.exc_sum = 0;
  u64 value;
  switch (descr >> 16) // fmt: 0=T raw, 1=S ieee, 2=F vax, 3=G vax
  {
  case 1:
    value = (u64)cpu->ieee_sts(cpu->state.f[fa]);
    break; // STS
  case 2:
    value = (u64)cpu->vax_stf(cpu->state.f[fa]);
    break; // STF
  case 3:
    value = cpu->vax_stg(cpu->state.f[fa]);
    break; // STG
  default:
    value = cpu->state.f[fa];
    break; // STT raw
  }
  return jit_write(cpu, va, (int)(descr & 0xffff), value);
}

// JIT MISC read helper (static). RPCC (sel 0): the wall-clock-pinned cycle
// counter (DO_RPCC, JIT lane). RC (sel 1) / RS (sel 2): read the interrupt
// flag, then clear / set it. All three read state the differential verify can't
// re-derive (cc advances only at the jit_run boundary; the flag is consumed by
// the read), so in verify we replay the interp pass's value -- like a load.
u64 CAlphaCPU::jit_misc(CAlphaCPU *cpu, u32 sel) {
  if (cpu->m_jit_vreplay)
    return cpu->m_jit_vlog[cpu->m_jit_vlog_i++]; // replay; no re-read, no
                                                 // double side effect

  switch (sel) {
  case 0: // RPCC: Ra = cc_offset : cc[31:0]
    return ((u64)cpu->state.cc_offset << 32) |
           (cpu->state.cc & U64(0xffffffff));
  case 1: // RC: Ra = bIntrFlag; bIntrFlag = false
  {
    u64 v = cpu->state.bIntrFlag ? 1 : 0;
    cpu->state.bIntrFlag = false;
    return v;
  }
  default: // RS (sel 2): Ra = bIntrFlag; bIntrFlag = true
  {
    u64 v = cpu->state.bIntrFlag ? 1 : 0;
    cpu->state.bIntrFlag = true;
    return v;
  }
  }
}

// JIT int->FP move helper (static). ITOFx: f[fc] = fmt(value). Mirrors DO_ITOFx
// incl. FPSTART (fpen==0 -> return 1, the FEN-trap bail; exc_sum cleared). fmt:
// 0=T raw, 1=S, 2=F.
int CAlphaCPU::jit_itof(CAlphaCPU *cpu, u32 fc, u64 value, u32 fmt) {
  if (cpu->state.fpen == 0)
    return 1;             // FEN trap: the interpreter vectors it
  cpu->state.exc_sum = 0; // FPSTART
  if (fmt == 1)
    cpu->state.f[fc] = cpu->ieee_lds((u32)value);
  else if (fmt == 2)
    cpu->state.f[fc] = cpu->vax_ldf(SWAP_VAXF((u32)value));
  else
    cpu->state.f[fc] = value;
  return 0;
}

// JIT FP->int move helper (static). FTOIx: *out = fmt(f[fa]). Same FPSTART
// bail. fmt: 0=T, 1=S.
int CAlphaCPU::jit_ftoi(CAlphaCPU *cpu, u32 fa, u32 fmt, u64 *out) {
  if (cpu->state.fpen == 0)
    return 1;             // FEN trap: the interpreter vectors it
  cpu->state.exc_sum = 0; // FPSTART
  *out = (fmt == 1) ? sext_u64_32(cpu->ieee_sts(cpu->state.f[fa]))
                    : cpu->state.f[fa];
  return 0;
}

// JIT FLTL non-arithmetic helper (static). FPCR moves, sign-copies, FP
// conditional moves and the CVTLQ/CVTQL bit rearrangements -- mirrored verbatim
// from cpu_fp_operate.h; classify only routes these funcs here (no FP math, no
// /V trap forms). Returns 1 = FEN-trap bail.
int CAlphaCPU::jit_fltl(CAlphaCPU *cpu, u32 ins) {
  if (cpu->state.fpen == 0)
    return 1;             // FEN trap: the interpreter vectors it
  cpu->state.exc_sum = 0; // FPSTART
  u64 *f = cpu->state.f;
  const u32 fa = (ins >> 21) & 0x1f, fb = (ins >> 16) & 0x1f, fc = ins & 0x1f;
  switch ((ins >> 5) & 0x7ff) {
  case 0x010: // CVTLQ
    f[fc] = sext_u64_32(((f[fb] >> 32) & 0xC0000000) |
                        ((f[fb] >> 29) & 0x3FFFFFFF));
    break;
  case 0x020:
    f[fc] = (f[fa] & FPR_SIGN) | (f[fb] & ~FPR_SIGN);
    break; // CPYS
  case 0x021:
    f[fc] = ((f[fa] & FPR_SIGN) ^ FPR_SIGN) | (f[fb] & ~FPR_SIGN);
    break;    // CPYSN
  case 0x022: // CPYSE
    f[fc] = (f[fa] & (FPR_SIGN | FPR_EXP)) | (f[fb] & ~(FPR_SIGN | FPR_EXP));
    break;
  case 0x024:
    cpu->write_fpcr_arch(f[fa]);
    break; // MT_FPCR
  case 0x025:
    f[fa] = cpu->read_fpcr_arch();
    break; // MF_FPCR (dest = Fa)
  case 0x02a:
    if ((f[fa] & ~FPR_SIGN) == 0)
      f[fc] = f[fb];
    break; // FCMOVEQ
  case 0x02b:
    if ((f[fa] & ~FPR_SIGN) != 0)
      f[fc] = f[fb];
    break; // FCMOVNE
  case 0x02c:
    if ((f[fa] & FPR_SIGN) && (f[fa] & ~FPR_SIGN) != 0)
      f[fc] = f[fb];
    break; // FCMOVLT
  case 0x02d:
    if (!(f[fa] & FPR_SIGN) || (f[fa] & ~FPR_SIGN) == 0)
      f[fc] = f[fb];
    break; // FCMOVGE
  case 0x02e:
    if ((f[fa] & FPR_SIGN) || (f[fa] & ~FPR_SIGN) == 0)
      f[fc] = f[fb];
    break; // FCMOVLE
  case 0x02f:
    if (!(f[fa] & FPR_SIGN) && (f[fa] & ~FPR_SIGN) != 0)
      f[fc] = f[fb];
    break;      // FCMOVGT
  case 0x030: { // CVTQL (no /V form)
    u64 cvtql_src = f[fb];
    f[fc] = ((cvtql_src & U64(0xC0000000)) << 32) |
            ((cvtql_src & U64(0x3FFFFFFF)) << 29);
    if (FPR_GETSIGN(cvtql_src) ? (cvtql_src < U64(0xFFFFFFFF80000000))
                               : (cvtql_src > U64(0x000000007FFFFFFF)))
      cpu->write_fpcr_arch(cpu->state.fpcr | FPCR_IOV);
    break;
  }
  }
  return 0;
}

// JIT VAX-FP arithmetic helper (static). FLTV (0x15): mirrors the interpreter's
// vax_* dispatch into f[fc]. FPSTART bail (return 1) if FP disabled. Returns 2
// when the op delivered an arith trap: vax_trap GO_PALs (sets state.pc +
// exc_sum), so exc_sum != 0 detects it and the caller honors state.pc -- the op
// runs exactly once (no re-execute). Returns 0 on the common no-trap path.
// Fc==31 is gated out in classify.
int CAlphaCPU::jit_fltv(CAlphaCPU *cpu, u32 ins) {
  if (cpu->state.fpen == 0)
    return 1; // FEN trap (FPSTART): op not run
  cpu->state.exc_sum = 0;
  u64 *f = cpu->state.f;
  const u32 fa = (ins >> 21) & 0x1f, fb = (ins >> 16) & 0x1f, fc = ins & 0x1f;
  const u32 fn = (ins >> 5) & 0x7ff;
  UFP u;
  switch (fn) {
  case 0x0a5:
  case 0x4a5:
    f[fc] =
        (cpu->vax_fcmp(f[fa], f[fb], ins) == 0) ? U64(0x4000000000000000) : 0;
    break; // CMPGEQ
  case 0x0a6:
  case 0x4a6:
    f[fc] =
        (cpu->vax_fcmp(f[fa], f[fb], ins) < 0) ? U64(0x4000000000000000) : 0;
    break; // CMPGLT
  case 0x0a7:
  case 0x4a7:
    f[fc] =
        (cpu->vax_fcmp(f[fa], f[fb], ins) <= 0) ? U64(0x4000000000000000) : 0;
    break; // CMPGLE
  case 0x03c:
  case 0x0bc:
    f[fc] = cpu->vax_cvtif(f[fb], ins, DT_F);
    break; // CVTQF
  case 0x03e:
  case 0x0be:
    f[fc] = cpu->vax_cvtif(f[fb], ins, DT_G);
    break; // CVTQG
  default:
    switch (fn & 0x7f) {
    case 0x000:
      f[fc] = cpu->vax_fadd(f[fa], f[fb], ins, DT_F, false);
      break; // ADDF
    case 0x001:
      f[fc] = cpu->vax_fadd(f[fa], f[fb], ins, DT_F, true);
      break; // SUBF
    case 0x002:
      f[fc] = cpu->vax_fmul(f[fa], f[fb], ins, DT_F);
      break; // MULF
    case 0x003:
      f[fc] = cpu->vax_fdiv(f[fa], f[fb], ins, DT_F);
      break; // DIVF
    case 0x01e:
      cpu->vax_unpack_d(f[fb], &u, ins);
      f[fc] = cpu->vax_rpack(&u, ins, DT_G);
      break; // CVTDG
    case 0x020:
      f[fc] = cpu->vax_fadd(f[fa], f[fb], ins, DT_G, false);
      break; // ADDG
    case 0x021:
      f[fc] = cpu->vax_fadd(f[fa], f[fb], ins, DT_G, true);
      break; // SUBG
    case 0x022:
      f[fc] = cpu->vax_fmul(f[fa], f[fb], ins, DT_G);
      break; // MULG
    case 0x023:
      f[fc] = cpu->vax_fdiv(f[fa], f[fb], ins, DT_G);
      break; // DIVG
    case 0x02c:
      cpu->vax_unpack(f[fb], &u, ins);
      f[fc] = cpu->vax_rpack(&u, ins, DT_F);
      break; // CVTGF
    case 0x02d:
      cpu->vax_unpack(f[fb], &u, ins);
      f[fc] = cpu->vax_rpack_d(&u, ins);
      break; // CVTGD
    case 0x02f:
      f[fc] = cpu->vax_cvtfi(f[fb], ins);
      break; // CVTGQ
    }
  }
  return cpu->state.exc_sum
             ? 2
             : 0; // exc_sum != 0 => vax_trap GO_PAL'd -> caller honors state.pc
}

// JIT load-locked helper (static). LDx_L: the verify-checked load PLUS cpu_lock
// -- the LL/SC exclusive monitor. cpu_lock is per-CPU + atomic + idempotent.
int CAlphaCPU::jit_read_locked(CAlphaCPU *cpu, u64 va, int size_bits,
                               u64 *out) {
  const u64 amask = (u64)(size_bits / 8) - 1;
  if (va & amask)
    return 1; // unaligned: let the interpreter handle it

  u64 phys;
  const u64 vp = va & ~U64(0x1FFF);
  SDataPageCache &dpc = cpu->data_page_cache[0][dpc_index(va)];
  if (dpc.valid && dpc.virt_page == vp && dpc.cm == cpu->state.cm &&
      dpc.asn == cpu->state.asn0) {
    phys = dpc.phys_base | (va & U64(0x1FFF));
  } else {
    const int i = cpu->FindTBEntry(va, ACCESS_READ);
    if (i < 0)
      return 1; // TB miss
    const auto &e = cpu->state.tb[TB_INDEX_DATA][i];
    if (!e.access[0][cpu->state.cm])
      return 1; // protection (ACV)
    if (e.fault[0])
      return 1; // fault-on-read (FOR)
    phys = e.phys | (va & e.keep_mask);
    dpc.virt_page = vp;
    dpc.phys_base = phys & ~U64(0x1FFF);
    dpc.host_base = ((phys | U64(0x1FFF)) < cpu->dram_size)
                        ? ((u64)cpu->dram_ptr + (phys & ~U64(0x1FFF)))
                        : 0;
    dpc.cm = cpu->state.cm;
    dpc.asn = cpu->state.asn0;
    dpc.valid = true;
  }

  if (phys >=
      cpu->dram_size) // MMIO: interpreter handles the I/O-space locked path
    return 1;

  CSystem::CLLSCDRAMGuard llsc_guard(cpu->cSystem, true);
  if (cpu->m_jit_vreplay) {
    if (va != cpu->m_jit_vaddr[cpu->m_jit_vlog_i]) {
      static int n = 0;
      if (n++ < 50)
        printf("[JIT] LDx_L ADDR MISMATCH: compiled va=%016llx interp "
               "va=%016llx\n",
               (unsigned long long)va,
               (unsigned long long)cpu->m_jit_vaddr[cpu->m_jit_vlog_i]);
    }
    *out = cpu->m_jit_vlog[cpu->m_jit_vlog_i++]; // the interp pass's (already
                                                 // sign-extended) value
  } else {
    const u64 raw = dram_read(cpu->dram_ptr, phys, size_bits);
    *out = (size_bits == 32)
               ? sext_u64_32(raw)
               : raw; // LDL_L sign-extends; LDQ_L is the full quad
  }

  // Establish the LL exclusive monitor (DO_LDx_L's READ_VIRT_LOCK_F makes the
  // same cpu_lock call).
  cpu->cSystem->cpu_lock(cpu->state.iProcNum, phys, *out);
  return 0;
}

// JIT HW_LD VPTE helper (static). HW_LD func 5 (the DTBMISS PTE fetch): a
// virtual read access- checked vs KERNEL, not cm (DO_HW_LDQ case 5).
// Side-effect-free TB probe; bails on miss so the interpreter vectors the
// double miss. Skips the data_page_cache (keyed by current cm).
int CAlphaCPU::jit_read_vpte(CAlphaCPU *cpu, u64 va, int size_bits, u64 *out) {
  const u64 amask = (u64)(size_bits / 8) - 1;
  if (va & amask)
    return 1; // unaligned: let the interpreter handle it

  const int i = cpu->FindTBEntry(va, ACCESS_READ);
  if (i < 0)
    return 1; // TB miss (double miss)
  const auto &e = cpu->state.tb[TB_INDEX_DATA][i];
  if (!e.access[0][0])
    return 1; // protection: kernel read access
  if (e.fault[0])
    return 1; // fault-on-read (FOR)
  const u64 phys = e.phys | (va & e.keep_mask);

  if (phys >= cpu->dram_size) // MMIO: bail before the replay (mirrors jit_read)
    return 1;

  if (cpu->m_jit_vreplay) {
    if (va != cpu->m_jit_vaddr[cpu->m_jit_vlog_i]) {
      static int n = 0;
      if (n++ < 50)
        printf(
            "[JIT] VPTE ADDR MISMATCH: compiled va=%016llx interp va=%016llx\n",
            (unsigned long long)va,
            (unsigned long long)cpu->m_jit_vaddr[cpu->m_jit_vlog_i]);
    }
    *out = cpu->m_jit_vlog[cpu->m_jit_vlog_i++];
    return 0;
  }

  *out = dram_read(cpu->dram_ptr, phys, size_bits);
  return 0;
}

// JIT HW_LD WrChk helper (static). HW_LD func 0xa (DO_HW_LDL case 10, HRM TYPE
// 1012 WrChk): a longword VIRTUAL read that ALSO requires WRITE access -- the
// interpreter ACVs if read OR write protection is clear (virt2phys WRCHK) and
// faults FOR/FOW. Side-effect-free TB probe mirroring jit_read_vpte but checked
// vs CURRENT mode (not kernel); bails (interp re-runs + vectors the fault) on
// miss/protection/fault/MMIO. Verify replays the value like any load.
int CAlphaCPU::jit_read_wchk(CAlphaCPU *cpu, u64 va, int size_bits, u64 *out) {
  const u64 amask = (u64)(size_bits / 8) - 1;
  if (va & amask)
    return 1; // unaligned: let the interpreter handle it

  const int i = cpu->FindTBEntry(va, ACCESS_READ);
  if (i < 0)
    return 1; // TB miss
  const auto &e = cpu->state.tb[TB_INDEX_DATA][i];
  const int cm = cpu->state.cm;
  if (!e.access[0][cm])
    return 1; // no read access (ACV)
  if (!e.access[1][cm])
    return 1; // no write access -- WrChk fails (ACV)
  if (e.fault[0] || e.fault[1])
    return 1; // FOR/FOW: bail so the interpreter vectors the fault
  const u64 phys = e.phys | (va & e.keep_mask);

  if (phys >=
      cpu->dram_size) // MMIO: bail before the replay (mirrors jit_read_vpte)
    return 1;

  if (cpu->m_jit_vreplay) {
    if (va != cpu->m_jit_vaddr[cpu->m_jit_vlog_i]) {
      static int n = 0;
      if (n++ < 50)
        printf(
            "[JIT] WCHK ADDR MISMATCH: compiled va=%016llx interp va=%016llx\n",
            (unsigned long long)va,
            (unsigned long long)cpu->m_jit_vaddr[cpu->m_jit_vlog_i]);
    }
    *out = cpu->m_jit_vlog[cpu->m_jit_vlog_i++];
    return 0;
  }

  *out = dram_read(cpu->dram_ptr, phys, size_bits);
  return 0;
}

// JIT HW_LD helper (static). Physical read of size_bits at phys -> *out with NO
// translation - the PALmode HW_LD physical longword/quadword forms (func 0/1).
// Aligns like READ_PHYS_NT. Verify replays the interpreter's value (race-free);
// MMIO bails so the interpreter does the ordered device read. Returns 0 on
// success, 1 on a bail.
int CAlphaCPU::jit_read_phys(CAlphaCPU *cpu, u64 phys, int size_bits,
                             u64 *out) {
  // MMIO: bail before the replay so verify models production (which bails
  // here). A device read isn't size-truncated, so a replayed+re-truncated value
  // would falsely mismatch. dram_size is page-aligned and the align below only
  // rounds within 8 bytes, so this raw check is exact.
  if (phys >= cpu->dram_size)
    return 1;

  if (cpu->m_jit_vreplay) {
    if (phys != cpu->m_jit_vaddr[cpu->m_jit_vlog_i]) {
      static int n = 0;
      if (n++ < 50)
        printf("[JIT] HW_LD ADDR MISMATCH: compiled pa=%016llx interp "
               "pa=%016llx\n",
               (unsigned long long)phys,
               (unsigned long long)cpu->m_jit_vaddr[cpu->m_jit_vlog_i]);
    }
    *out = cpu->m_jit_vlog[cpu->m_jit_vlog_i++];
    return 0;
  }

  phys &= ~((u64)(size_bits / 8) - 1); // align like READ_PHYS_NT (ALIGN_PHYS)
  *out = dram_read(cpu->dram_ptr, phys, size_bits);
  return 0;
}

// JIT store helper (static). Writes size_bits of value to virtual address va,
// mirroring jit_read's side-effect-free translation. Returns 0 on success, 1 on
// fault/unaligned.
int CAlphaCPU::jit_write(CAlphaCPU *cpu, u64 va, int size_bits, u64 value) {
  const u64 amask = (u64)(size_bits / 8) - 1;
  if (va & amask)
    return 1; // unaligned: let the interpreter handle it

  // Verify: the interpreter pass already performed (and recorded) this store.
  // Compare rather than write -- stores change memory, not GPRs, so the
  // differential GPR check can't see them; this is how compiled stores get
  // validated.
  if (cpu->m_jit_vreplay) {
    const u32 i = cpu->m_jit_slog_i++;
    if (va != cpu->m_jit_slog_addr[i] || value != cpu->m_jit_slog_val[i]) {
      static int n = 0;
      if (n++ < 50)
        printf("[JIT] STORE MISMATCH: compiled va=%016llx val=%016llx  interp "
               "va=%016llx val=%016llx\n",
               (unsigned long long)va, (unsigned long long)value,
               (unsigned long long)cpu->m_jit_slog_addr[i],
               (unsigned long long)cpu->m_jit_slog_val[i]);
    }
    return 0;
  }

  u64 phys;
  const u64 vp = va & ~U64(0x1FFF);
  SDataPageCache &dpc =
      cpu->data_page_cache[1][dpc_index(va)]; // direct-mapped by virt page
  if (dpc.valid && dpc.virt_page == vp && dpc.cm == cpu->state.cm &&
      dpc.asn == cpu->state.asn0) {
    phys = dpc.phys_base | (va & U64(0x1FFF));
  } else {
    // Side-effect-free TB fast path on the write cache [1]; bail on a TB miss
    // or access fault so the interpreter does the side-effecting translation
    // (filling this cache, so the next compiled run hits). NOT virt2phys -- it
    // vectors faults as a side effect.
    const int i = cpu->FindTBEntry(va, ACCESS_WRITE);
    if (i < 0)
      return 1; // TB miss
    const auto &e = cpu->state.tb[TB_INDEX_DATA][i];
    if (!e.access[1][cpu->state.cm])
      return 1; // protection (ACV)
    if (e.fault[1])
      return 1; // fault-on-write (FOW)
    phys = e.phys | (va & e.keep_mask);
    dpc.virt_page = vp;
    dpc.phys_base = phys & ~U64(0x1FFF);
    dpc.host_base = ((phys | U64(0x1FFF)) < cpu->dram_size)
                        ? ((u64)cpu->dram_ptr + (phys & ~U64(0x1FFF)))
                        : 0;
    dpc.cm = cpu->state.cm;
    dpc.asn = cpu->state.asn0;
    dpc.valid = true;
  }

  if (phys < cpu->dram_size)
    dram_write(cpu->dram_ptr, phys, size_bits, value);
  else
    cpu->cSystem->WriteMem(phys, size_bits, value, cpu);
  return 0;
}

// JIT HW_ST helper (static). Physical write of size_bits = value at phys with
// NO translation - the PALmode HW_ST physical longword/quadword forms (func
// 0/1). Aligns like WRITE_PHYS_NT. Verify compares against the interpreter's
// recorded store (stores change memory, not GPRs); MMIO bails so the
// interpreter does the ordered device write. Returns 0 on success, 1 on a bail.
int CAlphaCPU::jit_write_phys(CAlphaCPU *cpu, u64 phys, int size_bits,
                              u64 value) {
  if (cpu->m_jit_vreplay) {
    const u32 i = cpu->m_jit_slog_i++;
    if (phys != cpu->m_jit_slog_addr[i] || value != cpu->m_jit_slog_val[i]) {
      static int n = 0;
      if (n++ < 50)
        printf("[JIT] HW_ST STORE MISMATCH: compiled pa=%016llx val=%016llx  "
               "interp pa=%016llx val=%016llx\n",
               (unsigned long long)phys, (unsigned long long)value,
               (unsigned long long)cpu->m_jit_slog_addr[i],
               (unsigned long long)cpu->m_jit_slog_val[i]);
    }
    return 0;
  }

  phys &= ~((u64)(size_bits / 8) - 1); // align like WRITE_PHYS_NT (ALIGN_PHYS)
  if (phys >= cpu->dram_size)
    return 1; // MMIO: let the interpreter do the ordered write
  dram_write(cpu->dram_ptr, phys, size_bits, value);
  return 0;
}

// JIT store-conditional helper (static). Same-address STx_C uses the emulator's
// CAS-backed MP model; different-address same-line STx_C stores without
// comparing against the LDx_L datum.
u64 CAlphaCPU::jit_stc(CAlphaCPU *cpu, u64 va, int size_bits, u64 value) {
  if (cpu->m_jit_vreplay) {
    const u32 i = cpu->m_jit_slog_i++;
    const u64 success = cpu->m_jit_slog_success[i];
    if (va != cpu->m_jit_slog_addr[i] ||
        (success && value != cpu->m_jit_slog_val[i])) {
      static int n = 0;
      if (n++ < 50)
        printf("[JIT] STx_C MISMATCH: compiled va=%016llx val=%016llx ok=%llu  "
               "interp va=%016llx val=%016llx\n",
               (unsigned long long)va, (unsigned long long)value,
               (unsigned long long)success,
               (unsigned long long)cpu->m_jit_slog_addr[i],
               (unsigned long long)cpu->m_jit_slog_val[i]);
    }
    return success;
  }

  const u64 amask = (u64)(size_bits / 8) - 1;
  if (va & amask)
    return U64(
        0x100); // unaligned (also a page-cross): the interpreter handles it

  // Side-effect-free write-path translation (mirror jit_write); bail to the
  // interpreter on a TB miss / protection / fault-on-write so it does the
  // side-effecting translation.
  u64 phys;
  const u64 vp = va & ~U64(0x1FFF);
  SDataPageCache &dpc = cpu->data_page_cache[1][dpc_index(va)];
  if (dpc.valid && dpc.virt_page == vp && dpc.cm == cpu->state.cm &&
      dpc.asn == cpu->state.asn0) {
    phys = dpc.phys_base | (va & U64(0x1FFF));
  } else {
    const int i = cpu->FindTBEntry(va, ACCESS_WRITE);
    if (i < 0)
      return U64(0x100); // TB miss
    const auto &e = cpu->state.tb[TB_INDEX_DATA][i];
    if (!e.access[1][cpu->state.cm])
      return U64(0x100); // protection (ACV)
    if (e.fault[1])
      return U64(0x100); // fault-on-write (FOW)
    phys = e.phys | (va & e.keep_mask);
    dpc.virt_page = vp;
    dpc.phys_base = phys & ~U64(0x1FFF);
    dpc.host_base = ((phys | U64(0x1FFF)) < cpu->dram_size)
                        ? ((u64)cpu->dram_ptr + (phys & ~U64(0x1FFF)))
                        : 0;
    dpc.cm = cpu->state.cm;
    dpc.asn = cpu->state.asn0;
    dpc.valid = true;
  }

  u64 expected = 0;
  bool same_address = false;
  CSystem::CLLSCDRAMGuard llsc_guard(cpu->cSystem, phys < cpu->dram_size);
  if (!cpu->cSystem->cpu_take_lock(cpu->state.iProcNum, phys, &expected,
                                   &same_address))
    return 0; // lock lost -> SC fails
  if (phys < cpu->dram_size) {
    if (same_address)
      return dram_cas(cpu->dram_ptr, phys, expected, value, size_bits) ? 1 : 0;
    dram_write(cpu->dram_ptr, phys, size_bits, value);
    return 1;
  }
  cpu->cSystem->WriteMem(phys, size_bits, value, cpu); // MMIO conditional store
  return 1;
}

/* CALL_PAL OPCDEC trap: a privileged function (< 0x40) attempted in user mode.
   Mirrors GO_PAL(OPCDEC) -- save the faulting PC in EXC_ADDR, vector to the
   PALcode OPCDEC entry, and clear the load-lock flag */
void CAlphaCPU::jit_opcdec(CAlphaCPU *cpu, u64 cpc) {
  cpu->state.exc_addr = cpc;
  cpu->set_pc(cpu->state.pal_base | OPCDEC | U64(1));
  cpu->cSystem->cpu_clear_lock(cpu->state.iProcNum);
}

/* HW_MFPR (PALmode): return the IPR selected by (ins>>8)&0xff. */
u64 CAlphaCPU::jit_hw_mfpr(CAlphaCPU *cpu, u32 ins, u64 cur) {
  const auto &state = cpu->state;
  const u32 function = (ins >> 8) & 0xff;

  // ISUM (0x0d) reads the live async interrupt-request lines (eir/slr/crr/pcr),
  // which the differential verify can't re-derive - changes between the interp
  // and compiled passes. Replay the interp value here
  if (cpu->m_jit_vreplay && function == 0x0d)
    return cpu->m_jit_vlog[cpu->m_jit_vlog_i++];

  if ((function & 0xc0) == 0x40) // PCTX
    return ((u64)state.asn << 39) | ((u64)state.astrr << 9) |
           ((u64)state.aster << 5) | (state.fpen ? U64(0x1) << 2 : 0) |
           (state.ppcen ? U64(0x1) << 1 : 0);

  switch (function) {
  case 0x05:
    return state.pmpc; // PMPC
  case 0x06:
    return state.exc_addr; // EXC_ADDR
  case 0x07:
    return cpu->va_form(state.exc_addr, true); // IVA_FORM
  case 0x08:
  case 0x09:
  case 0x0a:
  case 0x0b: // IER_CM / CM / IER
    return (((u64)state.eien) << 33) | (((u64)state.slen) << 32) |
           (((u64)state.cren) << 31) | (((u64)state.pcen) << 29) |
           (((u64)state.sien) << 13) | (((u64)state.asten) << 13) |
           (((u64)state.cm) << 3);
  case 0x0c:
    return ((u64)state.sir) << 13; // SIRR
  case 0x0d:                       // ISUM (production path: read the live async
    // interrupt-request lines; the verify replays via the m_jit_vreplay
    // short-circuit at the top).
    return (((u64)(state.eir & state.eien)) << 33) |
           (((u64)(state.slr & state.slen)) << 32) |
           (((u64)(state.crr & state.cren)) << 31) |
           (((u64)(state.pcr & state.pcen)) << 29) |
           (((u64)(state.sir & state.sien)) << 13) |
           (((u64)(((U64(0x1) << (state.cm + 1)) - 1) & state.aster &
                   state.astrr & (state.asten * 0x3)))
            << 3) |
           (((u64)(((U64(0x1) << (state.cm + 1)) - 1) & state.aster &
                   state.astrr & (state.asten * 0xc)))
            << 7);
  case 0x0f:
    return state.exc_sum; // EXC_SUM
  case 0x10:
    return state.pal_base; // PAL_BASE
  case 0x11:               // I_CTL
    return state.i_ctl_other | (((u64)CPU_CHIP_ID) << 24) |
           (u64)state.i_ctl_vptb | (((u64)state.i_ctl_va_mode) << 15) |
           (state.hwe ? U64(0x1) << 12 : 0) | (state.sde ? U64(0x1) << 7 : 0) |
           (((u64)state.i_ctl_spe) << 3);
  case 0x14:
    return state.pctr_ctl; // PCTR_CTL
  case 0x16:
    return state.i_stat; // I_STAT
  case 0x27:
    return state.mm_stat; // MM_STAT
  case 0x2a:
    return state.dc_stat; // DC_STAT
  case 0x2b:
    return 0; // C_DATA
  case 0xc0:
    return (((u64)state.cc_offset) << 32) | (state.cc & U64(0xffffffff)); // CC
  case 0xc2:
    return state.fault_va; // VA
  case 0xc3:
    return cpu->va_form(state.va_form_va, false); // VA_FORM
  }
  return cur; // unknown IPR: DO_HW_MFPR's UNKNOWN2 leaves Ra unchanged
}

/* HW_MTPR (PALmode): the IPR write selected by `function` (value = Rb). Mirrors
 * DO_HW_MTPR (cpu_pal.h) verbatim. Pure stores are verify-compared via the IPR
 * snapshot; the TB fills forward to add_tb_i/_d (idempotent, so the verify
 * double-run is safe); the check_int=true kick (IER/CM/SIRR/AST) can only force
 * an interrupt poll, never suppress one. The 0x40-7f ASN write (bit 0) is
 * excluded -- classify() never compiles it (it flushes the dpc + bumps the asn
 * epoch). */
void CAlphaCPU::jit_hw_mtpr(CAlphaCPU *cpu, u32 function, u64 value) {
  // 0x40-0x7f bitmask group: ASTER/ASTRR/PPCEN/FPEN field stores (+check_int
  // for the AST bits). The ASN write (bit 0, dpc flush + asn-epoch bump) is
  // never compiled -- classify() routes it to OP_NONE.
  if ((function & 0xc0) == 0x40) {
    if (function & 2) {
      cpu->state.aster = (int)(value >> 5) & 0xf;
      cpu->state.check_int = true;
    }
    if (function & 4) {
      cpu->state.astrr = (int)(value >> 9) & 0xf;
      cpu->state.check_int = true;
    }
    if (function & 8)
      cpu->state.ppcen = (int)(value >> 1) & 1;
    if (function & 16)
      cpu->state.fpen = (int)(value >> 2) & 1;
    return;
  }
  switch (function) {
  case 0x00:
    cpu->state.last_tb_virt = value;
    break; // ITB_TAG
  case 0x01:
    cpu->add_tb_i(cpu->state.last_tb_virt, value);
    break; // ITB_PTE (ITB fill)
  case 0x02:
    cpu->tbiap(ACCESS_EXEC);
    break; // ITB_IAP (process ITB invalidate)
  case 0x03:
    cpu->tbia(ACCESS_EXEC);
    break; // ITB_IA (invalidate all ITB)
  case 0x04:
    cpu->tbis(value, ACCESS_EXEC);
    break; // ITB_IS (single ITB invalidate)
  case 0x13:
    cpu->flush_icache();
    break;   // IC_FLUSH (lazy flush + deferred reclaim)
  case 0x09: // CM (current mode)
    cpu->state.cm = (int)(value >> 3) & 3;
    cpu->state.check_int = true;
    break;
  case 0x0b: // IER_CM: write CM, then fall into IER
    cpu->state.cm = (int)(value >> 3) & 3;
    cpu->state.check_int = true;
    [[fallthrough]];
  case 0x0a: // IER
    cpu->state.asten = (int)(value >> 13) & 1;
    cpu->state.sien = (int)(value >> 13) & 0xfffe;
    cpu->state.pcen = (int)(value >> 29) & 3;
    cpu->state.cren = (int)(value >> 31) & 1;
    cpu->state.slen = (int)(value >> 32) & 1;
    cpu->state.eien = (int)(value >> 33) & 0x3f;
    cpu->state.check_int = true; // newly enabled pending ints must be polled
    break;
  case 0x0c: // SIRR (software interrupt request)
    cpu->state.sir = (int)(value >> 13) & 0xfffe;
    cpu->state.check_int = true;
    break;
  case 0x11: // I_CTL (terminator; mirrors DO_HW_MTPR)
    cpu->state.i_ctl_other =
        (value & U64(0x00000000006e2f67)) |
        U64(0x0000000000100000); // bit 20 hardwired-on (EV6/EV68)
    cpu->state.i_ctl_vptb = sext_u64_48(value & U64(0x0000ffffc0000000));
    cpu->state.i_ctl_spe = (int)((value >> 3) & 7);
    cpu->state.sde = (value >> 7) & 1;
    cpu->state.hwe = (value >> 12) & 1;
    cpu->state.i_ctl_va_mode = (int)(value >> 15) & 3;
    break;
  case 0x14:
    cpu->state.pctr_ctl = value & U64(0xffffffffffffffdf);
    break; // PCTR_CTL
  case 0x20:
    cpu->last_dtb_virt[0] = value;
    break; // DTB_TAG0
  case 0x21:
    cpu->add_tb_d(cpu->last_dtb_virt[0], value, 0);
    break; // DTB_PTE0 (DTB fill)
  case 0x26:
    cpu->state.alt_cm = (int)(value & 3);
    break; // DTB_ALTMODE
  case 0x29:
    cpu->state.dc_ctl = value;
    break; // DC_CTL
  case 0xa0:
    cpu->last_dtb_virt[1] = value;
    break; // DTB_TAG1
  case 0xa1:
    cpu->add_tb_d(cpu->last_dtb_virt[1], value, 1);
    break; // DTB_PTE1 (DTB fill)
  case 0xc0:
    cpu->state.cc_offset = (u32)(value >> 32);
    break; // CC
  }
}

// JIT indirect-jump chaining helper (static). For a compiled JMP/HW_RET, look
// up the block at the runtime target in this CPU's block cache (the
// dispatcher's own lookup -- validates valid + tag + asn/asm_global) and return
// its chained re-entry point if it's compiled and runnable in the current
// context; else null, so the compiled jump bails to the dispatcher. Keying on
// the actual target lets any number of distinct targets chain without the old
// single-slot link thrashing on varying jumps.
void *CAlphaCPU::jit_indirect(CAlphaCPU *cpu, u64 target) {
  cpu->m_jit->note_jmp_attempt();
  CJitEngine::JitBlock *b = cpu->m_jit->lookup(target, (u32)cpu->state.asn);
  if (!b || !b->jit_body)
    return nullptr;
  if (target & 1) {
    // PALmode target: the I-stream is physically addressed (not paged), so no
    // remap / no stale risk. A PALmode block's shadow-register remap assumes
    // SDE -- honor the dispatcher's guard.
    if (!cpu->state.sde)
      return nullptr;
    cpu->m_jit->note_jmp_hit();
    return b->jit_body;
  }
  // Native target. FAST PATH: validated under the current epoch (lookup proved
  // flush-fresh, so a vgen mismatch here means an ITB invalidate) -- chain with
  // no re-translation.
  const u64 gen = cpu->m_jit->vgen();
  if (b->vgen == gen) {
    cpu->m_jit->note_jmp_hit();
    return b->jit_body;
  }
  // SLOW PATH (only right after an ITB invalidate): the in-frame chain bypasses
  // the dispatcher's `b->phys == start_phys` staleness check (see the
  // hot-path). Virtual+ASN keying can't see a page remap, so a stale block
  // (same tag+ASN, but the vpage now maps different physical bytes) would
  // tail-execute as wrong code -> OPCDEC / garbage.
  const int i = cpu->FindTBEntry(target, ACCESS_EXEC);
  if (i < 0)
    return nullptr; // ITB miss: let the dispatcher fault it in
  const auto &e = cpu->state.tb[TB_INDEX_ITB][i];
  if ((e.phys | (target & e.keep_mask)) != b->phys) {
    // Caught a stale block: the page was remapped without flushing the JIT
    // cache. Bail so the dispatcher re-records/recompiles. Rate-limited log --
    // this firing confirms the stale chain.
    static int n = 0;
    if (n++ < 50)
      printf("[JIT][CPU%d] INDIRECT STALE: target=%016llx block_phys=%016llx "
             "-- recompiling\n",
             (int)cpu->state.iProcNum, (unsigned long long)target,
             (unsigned long long)b->phys);
    return nullptr;
  }
  b->vgen = gen; // re-validated -> fast path henceforth
  cpu->m_jit->note_jmp_hit();
  return b->jit_body;
}
#endif

/**
 * Check if threads are still running.
 *
 * Calibrate the CPU timing loop.
 **/
void CAlphaCPU::check_state() {
  if (myThreadDead.load())
    FAILURE(Thread, "CPU thread has died");

  // Debug aid: AXPBOX_PC_SAMPLE=1 prints the guest PC on every check_state
  // poll (~100 ms) -- identifies guest-side hangs/loops on headless runs.
  static const char *pc_sample = getenv("AXPBOX_PC_SAMPLE");
  if (pc_sample && *pc_sample == '1')
    fprintf(stderr, "PCSAMPLE cpu%d pc=%016llx icount=%llu\n",
            (int)state.iProcNum, (unsigned long long)state.pc,
            (unsigned long long)state.instruction_count);

#if !defined(CONSTANT_TIME_FACTOR)
  if (state.instruction_count > 0) {
    // correct CPU timing loop...
    u64 icount = state.instruction_count;
    u64 cc = cc_large;
    u64 time = std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - start_time)
                   .count();
    s64 ce = cc_per_instruction;

    u64 cc_aim = time * cpu_hz / 1000000; // microsecond resolution
    u64 ce_aim = cc_aim / icount;

    s64 icount_lapse = icount - prev_icount;
    s64 cc_diff = cc_aim - cc;
    s64 ce_diff = (s64)((float)cc_diff / (float)icount_lapse);

    s64 ce_new = ce_aim + ce_diff;
    if (ce_new < 1)
      ce_new = 1;
    if (ce_new > 200)
      ce_new = 200;

    if (ce_new != ce) {

      //    printf("                                     time %12" PRId64 " |
      //    prev %12" PRId64 "  \n",time,prev_time); printf("          count
      //    lapse %12" PRId64 " | curr %12" PRId64 " | prev %12" PRId64 "
      //    \n",icount_lapse,icount,prev_icount); printf("cc %12" PRId64 " | aim
      //    %12" PRId64 " | diff %12" PRId64 " | prev %12" PRId64 "
      //    \n",cc,cc_aim,cc_diff,prev_cc); printf("ce %12" PRId64 " | aim %12"
      //    PRId64 " | diff %12" PRId64 " | new  %12" PRId64 "
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
  int i = 0;
  u64 phys_address;
  u64 temp_64;
  u64 temp_64_1;
  u64 temp_64_2;
  UFP ufp1;
  UFP ufp2;

  bool pbc = false;

  int opcode;
  int function;

#ifndef ES40_JIT
  // ---- Batch loop: execute up to 512 instructions before returning ----
  int _batch_budget = 512;
  u64 _cc_accum = 0;     // accumulated cycle counts (flushed every 32 insns)
  int _icount_accum = 0; // accumulated instruction count
  const u64 _cc_per_ins = cc_per_instruction; // cache in register
#else
  const u64 _cc_per_ins = cc_per_instruction;
#endif

#ifndef ES40_JIT
  {
    const auto now = std::chrono::steady_clock::now();

    // Wall-clock RPCC at batch granularity - same semantics as the JIT build.
    // The old per-instruction advance ran at cc_per_instruction rate, which
    // lags real time while the check_state feedback converges; SRM's
    // cycles-per-tick speed calibration measured that lag consistently and
    // locked in a low CPU speed.
    if (cc_last_sync > now)
      cc_last_sync = now;
    auto cc_delta = now - cc_last_sync;
    cc_last_sync = now;
    if (state.cc_ena) {
      if (cc_delta > std::chrono::seconds(1))
        cc_delta = std::chrono::seconds(1);
      state.cc +=
          (u64)std::chrono::duration_cast<std::chrono::nanoseconds>(cc_delta)
              .count() *
          cpu_hz / 1000000000ULL;
    }

    // Poll the wall-clock Cchip interval timer once per execute() batch
    // (~512 instructions) rather than every 32;
    if (state.iProcNum == 0) {
      if (now >= next_timer_fire) {
        const u64 period_ns = theAli ? theAli->get_interval_period_ns() : 0;
        if (period_ns) {
          // Count-preserving, paced catch-up: the schedule advances one period
          // per fire so ticks lost to a busy/stalled CPU0 thread are repaid and
          // the guests' tick-counted clocks (VMS never resyncs) stay true to
          // wall time. Repayment is paced to >= half a period between fires
          // (max 2x nominal, never a burst - burst/compressed ticks skew
          // RPCC-vs-tick calibrations). Backlog beyond 1s (debugger pause, host
          // sleep) is dropped.
          if (now - tick_last_fire >= std::chrono::nanoseconds(period_ns / 2)) {
            if (idle_probe)
              idle_probe_tick(now, period_ns);
            cSystem->interrupt(-1, true);
            tick_last_fire = now;
            next_timer_fire += std::chrono::nanoseconds(period_ns);
            if (now - next_timer_fire > std::chrono::seconds(1))
              next_timer_fire = now;
          }
        } else {
          cSystem->interrupt(-1, true);
          tick_last_fire = now;
          next_timer_fire = now + std::chrono::seconds(1);
        }
      }
    }
  }

_next_instruction:
  if (--_batch_budget <= 0) {
    // Flush remaining accumulated counters before returning. state.cc is
    // wall-clock (advanced at batch top); _cc_accum only feeds cc_large for the
    // legacy check_state speed-factor feedback.
    state.instruction_count += _icount_accum;
    cc_large += _cc_accum;
    return;
  }
#endif

#if defined(MIPS_ESTIMATE)

  // Calculate simulated performance statistics
  if (++count >= MIPS_INTERVAL) {
    clock_t current = clock();

    if (saved > 0) {
      double secs = (current - saved) / (double)CLOCKS_PER_SEC;
      double ips = MIPS_INTERVAL / secs;
      double mips = ips / 1000000.0;
      if (max_mips < mips)
        max_mips = mips;
      if (min_mips > mips)
        min_mips = mips;
      printf("AXPbox MIPS (%3.1f sec):: current: %5.3f, min: %5.3f, max: "
             "%5.3f\n",
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

  //--------------------------------------------------------------------------------
  // This section skips the memory check in SRM. Set the define in config_debug
  // for the memory check to run.
  //--------------------------------------------------------------------------------
  // All five SRM mem-test patch points are in page 0x8b000 and only fire
  // during early SRM boot. Gate on the page so every other instruction pays
  // one compare instead of five.
  if (skip_memtest_hack && (state.current_pc & ~U64(0xFFF)) == U64(0x8b000)) {
    if (state.current_pc == U64(0x8bb90)) {
      if (state.r[5] != U64(0xaaaaaaaaaaaaaaaa)) {
        printf("wrong memory check skip!\n");
      } else {
        state.r[0] = state.r[4];
      }
    }

    if (state.current_pc == U64(0x8bbe0)) {
      if (state.r[5] != U64(0xaaaaaaaaaaaaaaaa)) {
        printf("wrong memory check skip!\n");
      } else {
        state.r[16] = 0;
      }
    }

    if (state.current_pc == U64(0x8bc28)) {
      if (state.r[5] != U64(0xaaaaaaaaaaaaaaaa)) {
        printf("wrong memory check skip!\n");
      } else {
        state.r[8] = state.r[4];
      }
    }

    if (state.current_pc == U64(0x8bc70)) {
      if (state.r[7] != U64(0x5555555555555555)) {
        printf("wrong memory check skip1!\n");
      } else {
        state.r[0] = 0;
      }
    }

    if (state.current_pc == U64(0x8bcb0)) {
      if (state.r[7] != U64(0x5555555555555555)) {
        printf("wrong memory check skip2!\n");
      } else {
        state.r[3] = state.r[4];
      }
    }
  }
  //--------------------------------------------------------------------------------
  // end of skip memory test section
  //--------------------------------------------------------------------------------

  // Service interrupts
  if (DO_ACTION) {
#ifndef ES40_JIT
    // We're actually executing code. Cycle counter should be updated, interrupt
    // and interrupt timer status needs to be checked, and the next instruction
    // should be fetched from the instruction cache. Increase the cycle counter
    // if it is currently enabled.
    _icount_accum++;
    _cc_accum += _cc_per_ins;

    if ((_batch_budget & 31) == 0) {
      // Flush accumulated counters to state. state.cc is wall-clock (batch
      // top); _cc_accum only feeds cc_large for the check_state speed-factor
      // feedback.
      state.instruction_count += _icount_accum;
      _icount_accum = 0;
      cc_large += _cc_accum;
      _cc_accum = 0;

      // There are one or more active delayed irq_h interrupts. Go through the 6
      // irq_h timers, decrease them as needed, and set the interrupt if the
      // timer reaches 0. Batch to reduce memory ops.
      if (state.check_timers) {
        state.check_timers = false;
        for (int j = 0; j < 6; j++) {
          if (state.irq_h_timer[j]) {
            if (state.irq_h_timer[j] <= 32) {
              state.irq_h_timer[j] = 0;
              state.eir |= (U64(0x1) << j);
              // The timer hasn't reached 0 yet; check on the timers again next
              // clock tick.
              state.check_int = true;
            } else {
              // The timer has reached 0. Set the interrupt status, and set the
              // flag that we need to check the interrupt status
              state.irq_h_timer[j] -= 32;
              state.check_timers = true;
            }
          }
        }
      }
    }
#else
    // New unwound interpreter / future JIT path

    state.instruction_count++;
    cc_large += _cc_per_ins;
    // state.cc (RPCC) is pinned to wall-clock * cpu_hz at the jit_run boundary,
    // not advanced per-instruction here - interpreter can't run RPCC ahead of
    // wall time.

    // Process delayed irq_h timers one instruction at a time.
    if (state.check_timers) {
      state.check_timers = false;
      for (int ti = 0; ti < 6; ti++) {
        if (state.irq_h_timer[ti]) {
          if (state.irq_h_timer[ti] <= 1) {
            state.irq_h_timer[ti] = 0;
            state.eir |= (U64(0x1) << ti);
            state.check_int = true;
          } else {
            state.irq_h_timer[ti]--;
            state.check_timers = true;
          }
        }
      }
    }
#endif

    if (state.check_int && !(state.pc & 1)) {
      // Clear the interrupt doorbell up front. A remote irq_h() (e.g. an IPI
      // from another CPU) sets eir then re-raises check_int; clearing first
      // guarantees a cross-thread re-raise is re-checked next tick rather than
      // lost by a late clear.
      state.check_int = false;

      // One or more of the variables that affect interrupt status have changed,
      // and we are not currently inside PALmode. It is not certain that this
      // means we hava an interrupt to service, but we might have. This needs to
      // be checked.

      if (state.pal_vms) {
        // PALcode base is set to 0x8000; meaning OpenVMS PALcode is currently
        // active. In this case, our VMS PALcode replacement routines are valid,
        // and should be used as it is faster than using the original PALcode.

        // irq<1>=device, irq<2>=timer, irq<3>=IPI. (Was 0x6 = device+timer
        // only, which stranded incoming IPIs under VMS PALcode -> CPUSPINWAIT.)
        if ((state.eir & state.eien & 0xe) || (state.sir & state.sien) ||
            (state.asten &&
             (state.aster & state.astrr & ((1 << (state.cm + 1)) - 1))))
          idle_irq_count++; // host idle: an interrupt is being delivered
        if (state.eir & state.eien & 0xe)
          if (vmspal_ent_ext_int(state.eir & state.eien & 0xe))
            return;

        if (state.sir & state.sien & 0xfffc)
          if (vmspal_ent_sw_int(state.sir & state.sien))
            return;

        if (state.asten &&
            (state.aster & state.astrr & ((1 << (state.cm + 1)) - 1)))
          if (vmspal_ent_ast_int(state.aster & state.astrr &
                                 ((1 << (state.cm + 1)) - 1)))
            return;

        if (state.sir & state.sien)
          if (vmspal_ent_sw_int(state.sir & state.sien))
            return;
      } else

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
          idle_irq_count++; // host idle: an interrupt is being delivered
          GO_PAL(INTERRUPT);
          seq_remaining = 0;
#ifndef ES40_JIT
          goto _next_instruction;
#else
          return;
#endif
        }
      }
    }

    // If profiling is enabled, increase the profiling counter for the current
    // block of addresses.
#if defined(PROFILE)
    PROFILE_DO(state.pc);
#endif

#ifndef ES40_JIT
    // ---- Fast sequential icache path ----
    // If PC matches expected sequential address and we have words remaining
    // in the current icache line, read directly without any lookup.
    if (state.pc == seq_next_pc && seq_remaining > 0) {
      ins = endian_32(seq_line_ptr[seq_offset]);
#if defined(DEBUG_ARC)
      if (state.current_pc < 0x10000) {
        printf("PC=%016" PRIx64 " ins=%08x\n", state.current_pc, ins);
      }
#endif
      seq_offset++;
      seq_remaining--;
      seq_next_pc += 4;
      // state.pc_phys += 4;
#if defined(IDB)
      current_pc_physical = state.pc_phys;
#endif
    } else {
      // Full icache lookup
      if (get_icache(state.pc, &ins))
        goto _next_instruction;

      // Set up sequential tracking from the cache hit
      if (icache_enabled && !(state.pc & 1)) {
        int _siq_line = state.last_found_icache;
        seq_line_ptr = state.icache[_siq_line].data;
        int _siq_word = (int)((state.pc >> 2) & ICACHE_INDEX_MASK);
        seq_offset = _siq_word + 1;
        seq_remaining = ICACHE_LINE_SIZE - seq_offset;
        seq_next_pc = (state.pc & ~U64(0x3)) + 4;
      } else {
        seq_remaining = 0;
      }

#if defined(IDB)
      current_pc_physical = state.pc_phys;
#endif
    }
#else
    // Fast sequential icache path -- the same fast path the batched interpreter
    // uses above. The JIT lane calls execute() once per instruction, so this
    // cursor persists across calls within an interpreted run. A compiled block
    // can't remap (it runs no PAL/TB ops), and any flush or IMB resets the
    // cursor through break_seq_icache(), so stale lines can't be read. A
    // TB-miss returns to the dispatcher (which re-dispatches at the fault
    // handler).
    if (state.pc == seq_next_pc && seq_remaining > 0) {
      ins = endian_32(seq_line_ptr[seq_offset]);
      seq_offset++;
      seq_remaining--;
      seq_next_pc += 4;
    } else {
      if (get_icache(state.pc, &ins))
        return;

      if (icache_enabled && !(state.pc & 1)) {
        int _siq_line = state.last_found_icache;
        seq_line_ptr = state.icache[_siq_line].data;
        int _siq_word = (int)((state.pc >> 2) & ICACHE_INDEX_MASK);
        seq_offset = _siq_word + 1;
        seq_remaining = ICACHE_LINE_SIZE - seq_offset;
        seq_next_pc = (state.pc & ~U64(0x3)) + 4;
      } else {
        seq_remaining = 0;
      }
    }

#if defined(IDB)
    current_pc_physical = state.pc_phys;
#endif

#endif
  } // if (DO_ACTION)
  else {

    // We're not really executing any code (DO_ACTION is false); that means that
    // we're in a debugging session, and just listing instructions at a
    // particular address. In this case, we treat the program counter as a
    // physical address.
    ins = (u32)(cSystem->ReadMem(state.pc, 32, this));
#if defined(DEBUG_ARC)
    if (state.current_pc < 0x10000) {
      printf("PC=%016" PRIx64 " ins=%08x\n", state.current_pc, ins);
    }
#endif
  }

  // Increase the program counter. The current value is retained in
  // state.current_pc. This must go through next_pc(): the icache-disabled
  // fetch path in get_icache() relies on next_pc() keeping pc_phys and
  // rem_ins_in_page in sync with pc. A bare pc += 4 leaves pc_phys frozen,
  // so every subsequent fetch on the page re-reads the same physical word
  // (SRM runs away into pattern-filled memory during the console banner).
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
    case 0x4800:
      OP(IMB, NOP);
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

  case 0x19: // HW_MFPR (PALRES)
    /* HRM 6.4 / 6.8.2: PALRES opcodes (0x19/0x1B/0x1D/0x1E/0x1F) raise
     * OPCDEC unless executing in PALmode or in kernel mode with I_CTL[HWE]
     * set. Matches brokenpipe palres_access_check(). */
    if (!(state.pc & 1) && !(state.cm == 0 && state.hwe)) {
      GO_PAL(OPCDEC);
      ES40_EXECUTE_END();
    }
    function = (ins >> 8) & 0xff;
    OP(HW_MFPR, MFPR);

  case 0x1a: // JSR* instructions
    OP(JMP, JMP);

  case 0x1b: // PAL reserved - HW_LD (PALRES)
    if (!(state.pc & 1) && !(state.cm == 0 && state.hwe)) {
      GO_PAL(OPCDEC);
      ES40_EXECUTE_END();
    }
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

  case 0x1d: // HW_MTPR (PALRES)
    if (!(state.pc & 1) && !(state.cm == 0 && state.hwe)) {
      GO_PAL(OPCDEC);
      ES40_EXECUTE_END();
    }
    function = (ins >> 8) & 0xff;
    OP(HW_MTPR, MTPR);

  case 0x1e: // HW_RET (PALRES)
    if (!(state.pc & 1) && !(state.cm == 0 && state.hwe)) {
      GO_PAL(OPCDEC);
      ES40_EXECUTE_END();
    }
    OP(HW_RET, RET);

  case 0x1f: // HW_ST (PALRES)
    if (!(state.pc & 1) && !(state.cm == 0 && state.hwe)) {
      GO_PAL(OPCDEC);
      ES40_EXECUTE_END();
    }
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

#ifndef ES40_JIT
  goto _next_instruction;
#else
  return;
#endif
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
  printf("%%CPU-I-LISTNG: Listing from %016" PRIx64 " to %016" PRIx64 "\n",
         from, to);

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
  last_dtb_virt[0] = last_dtb_virt[1] = 0;

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

  // Use ITB (tb[1]) if ACCESS_EXEC is set, otherwise the unified Dstream TB
  // (tb[0]).
  int t = (flags & ACCESS_EXEC) ? TB_INDEX_ITB : TB_INDEX_DATA;
  int asn = state.asn;

  int rw = (flags & ACCESS_WRITE) ? 1 : 0;

#define TB_ASN_MATCH(entry)                                                    \
  ((entry).asm_bit || ((entry).asn == ((t == TB_INDEX_ITB) ? asn : state.asn0)))

  // Try last match first; this is a good quess, especially in the ITB
  int i = state.last_found_tb[t][rw];
  if (state.tb[t][i].valid &&
      !((state.tb[t][i].virt ^ virt) & state.tb[t][i].match_mask) &&
      TB_ASN_MATCH(state.tb[t][i]))
    return i;

  // Otherwise, loop through the TB entries to find a match.
  for (i = 0; i < TB_ENTRIES; i++) {
    if (state.tb[t][i].valid &&
        !((state.tb[t][i].virt ^ virt) & state.tb[t][i].match_mask) &&
        TB_ASN_MATCH(state.tb[t][i])) {
      state.last_found_tb[t][rw] = i;
      return i;
    }
  }

#undef TB_ASN_MATCH
  return -1;
}

static inline u64 alpha_sext_u64_43(u64 a) {
  return (a & U64(0x0000040000000000)) ? (a | U64(0xfffff80000000000))
                                       : (a & U64(0x000007ffffffffff));
}

static inline bool alpha_valid_va_form(u64 virt, bool va48) {
  return (va48 ? sext_u64_48(virt) : alpha_sext_u64_43(virt)) == virt;
}

int CAlphaCPU::initiate_acv_fault(u64 virt, int flags, u32 ins) {
  int res;

  state.exc_addr = state.current_pc;

  if (flags & ACCESS_EXEC) {
    state.exc_sum = 0;
    if (state.pal_vms) {
      res = vmspal_ent_iacv(flags);
      return res ? res : -1;
    }

    set_pc(state.pal_base + IACV + 1);
    return -1;
  }

  state.fault_va = virt;
  state.va_form_va = virt;
  state.exc_sum = ((u64)REG_1 & 0x1f)
                  << 8; /* HRM 5.2.13: EXC_SUM REG[12:8] is 5 bits */

  u32 opcode = I_GETOP(ins);
  state.mm_stat = ((opcode == 0x1b || opcode == 0x1f) ? opcode - 0x18 : opcode)
                      << 4 |
                  (flags & ACCESS_WRITE) | 2;
  if (state.pal_vms) {
    res = vmspal_ent_dfault(flags);
    return res ? res : -1;
  }

  set_pc(state.pal_base + DFAULT + 1);
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
  int t = (flags & ACCESS_EXEC) ? TB_INDEX_ITB : TB_INDEX_DATA;
  int i;
  int res;

  int spe = (flags & ACCESS_EXEC) ? state.i_ctl_spe : state.m_ctl_spe;
  /*
   * Access-check current mode selection.
   *  - VPTE (HRM 6.4.1 Table 6-3): Virtual/VPTE accesses are LD_VPTE
   *    page-table fetches and use *kernel mode* for permission checks
   *    regardless of executing CM.
   *  - ALT (HRM 6.4.1 Table 6-3 row 1102, Table 6-4 row 1102): the
   *    /alt variants use DTB_ALT_MODE for permission checks.
   *  - Otherwise: current mode.
   * Order matters: VPTE wins over ALT (VPTE is never combined with ALT in
   * any valid HW_LD encoding, but the precedence is clear).
   */
  int cm = (flags & VPTE) ? 0 : (flags & ALT) ? state.alt_cm : state.cm;
  bool forreal = !(flags & FAKE);
  bool va48 = (flags & ACCESS_EXEC) ? (state.i_ctl_va_mode & 1)
                                    : (state.va_ctl_va_mode & 1);

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

  if (!alpha_valid_va_form(virt, va48)) {
#if defined(DEBUG_TB)
    if (forreal)
#if defined(IDB)
      if (bTB_Debug)
#endif
        printf("acv-va-form\n");
#endif
    if (!forreal)
      return -1;
    return initiate_acv_fault(virt, flags, ins);
  }

  // try superpage first.
  if (spe) {
    bool spe_hit = false;
    u64 spe_phys = 0;

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
      spe_hit = true;
      spe_phys = virt & SPE_2_MAP;
    }

    // SPE[1], when set, enables superpage mapping when VA[47:41] = 7E. In
    // this mode, VA[40:13] are mapped directly to PA[40:13] and PA[43:41] are
    // copies of PA[40] (sign extension).
    else if (((virt & SPE_1_MASK) == SPE_1_MATCH) && (spe & 2)) {
      spe_hit = true;
      spe_phys = (virt & SPE_1_MAP) | ((virt & SPE_1_TEST) ? SPE_1_ADD : 0);
    }

    // SPE[0], when set, enables superpage mapping when VA[47:30] = 3FFFE.
    // In this mode, VA[29:13] are mapped directly to PA[29:13] and PA[43:30]
    // are cleared.
    else if (((virt & SPE_0_MASK) == SPE_0_MATCH) && (spe & 1)) {
      spe_hit = true;
      spe_phys = virt & SPE_0_MAP;
    }

    if (spe_hit) {
      if (cm) {
#if defined(DEBUG_TB)
        if (forreal)
#if defined(IDB)
          if (bTB_Debug)
#endif
            printf("SPE-ACV\n");
#endif
        if (!forreal)
          return -1;
        return initiate_acv_fault(virt, flags, ins);
      }

      *phys = spe_phys;
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
    if (!forreal) // debugger-lookup of the address
      return -1;  // report failure, and don't look any further
    if (!state.pal_vms ||
        vmspal_exc_depth >= 3) // unknown PALcode, or the
                               // native VMS fast-path is already nested in
                               // exception delivery: vector through the real
                               // PALcode instead of recursing on the host stack
    {

      // transfer execution to PALcode
      state.exc_addr = state.current_pc;
      if (flags & VPTE) {
        // HRM 5.1.3: VA is NOT written for LD_VPTE misses
        state.va_form_va = virt;
        state.exc_sum = ((u64)REG_1 & 0x1f)
                        << 8; /* HRM 5.2.13: EXC_SUM REG[12:8] is 5 bits */
        /*
         * I_CTL[VA_48] selects the DTB double-miss PAL entry.
         * state.i_ctl_va_mode packs bits [16:15] of I_CTL, so bit 0
         * here is the architectural VA_48 bit.
         */
        set_pc(state.pal_base +
               ((state.i_ctl_va_mode & 1) ? DTBM_DOUBLE_4 : DTBM_DOUBLE_3) + 1);
      } else if (flags & ACCESS_EXEC) {
        set_pc(state.pal_base + ITB_MISS + 1);
      } else {
        state.fault_va = virt;
        state.va_form_va = virt;
        state.exc_sum = ((u64)REG_1 & 0x1f)
                        << 8; /* HRM 5.2.13: EXC_SUM REG[12:8] is 5 bits */

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
        state.exc_sum = ((u64)REG_1 & 0x1f)
                        << 8; /* HRM 5.2.13: EXC_SUM REG[12:8] is 5 bits */

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

    // check if requested access is allowed.
    // WRCHK (HRM 6.4.1: HW_LD type 1012/1112 WrChk variants) requires that
    // BOTH read and write protection pass for the access mode -- fail if
    // the natural-direction access bit is clear OR (when WRCHK is set on a
    // read) the write access bit is also clear.
    if (!state.tb[t][i].access[flags & ACCESS_WRITE][cm] ||
        ((flags & WRCHK) && !state.tb[t][i].access[1][cm])) {
#if defined(DEBUG_TB)
      if (forreal)
#if defined(IDB)
        if (bTB_Debug)
#endif
          printf("acv\n");
#endif
      if (!forreal) // FAKE probe: report failure, don't vector the fault —
                    // caller re-runs it under the interpreter
        return -1;
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
        /* HRM 5.1.5/D.24: VA_FORM is derived from VA, which is written
           on every D-stream fault (incl. DFAULT), not just TB miss.
           Keep the VA_FORM snapshot current so the OS fault handler
           computes the right PTE. */
        state.va_form_va = virt;
        state.exc_sum = ((u64)REG_1 & 0x1f)
                        << 8; /* HRM 5.2.13: EXC_SUM REG[12:8] is 5 bits */

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

    // check if requested access doesn't fault.
    // WRCHK additionally requires that the FOW bit be clear -- HRM 6.4.1
    // WrChk variants check both FOR and FOW.
    if (state.tb[t][i].fault[flags & ACCESS_MODE] ||
        ((flags & WRCHK) && state.tb[t][i].fault[1])) {
#if defined(DEBUG_TB)
      if (forreal)
#if defined(IDB)
        if (bTB_Debug)
#endif
          printf("fault\n");
#endif
      if (!forreal) // FAKE probe: report failure, don't vector the fault —
                    // caller re-runs it under the interpreter
        return -1;
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
        /* HRM 5.1.5/D.24: VA_FORM is derived from VA, which is written
           on every D-stream fault (including FOR/FOW DFAULT), not just TB
           miss. Keep the VA_FORM snapshot current so the OS fault
           handler computes the right PTE address. */
        state.va_form_va = virt;
        state.exc_sum = ((u64)REG_1 & 0x1f)
                        << 8; /* HRM 5.2.13: EXC_SUM REG[12:8] is 5 bits */

        /* HRM 5.3.8 MM_STAT: FOR [bit 2] is set when a fault-on-read
         * error occurs during a read transaction with PTE[FOR] set.
         * FOW [bit 3] is set when a fault-on-write error occurs.
         * For HW_LD WrChk variants the chip is performing both a
         * read AND a write-protection check, so PTE[FOW] (if set)
         * is reportable too
         */
        u32 opcode = I_GETOP(ins);
        int for_bit =
            ((flags & ACCESS_WRITE) == 0) && state.tb[t][i].fault[0] ? 4 : 0;
        int fow_bit = (((flags & ACCESS_WRITE) || (flags & WRCHK)) &&
                       state.tb[t][i].fault[1])
                          ? 8
                          : 0;
        state.mm_stat =
            ((opcode == 0x1b || opcode == 0x1f) ? opcode - 0x18 : opcode) << 4 |
            (flags & ACCESS_WRITE) | for_bit | fow_bit;
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

/*
 * EV68CB/EV68DC HRM:
 *  - 5.3.1, Figure 5-26: DTB_TAG0/1 contain VA[47:13].
 *  - 5.3.2, Figure 5-27: DTB_PTE0/1 contain PA[43:13] and GH[1:0].
 * GH widens the granule by 3 bits per step, so the low 13/16/19/22 bits
 * are kept from the virtual address and excluded from the tag/PFN masks.
 */
#define GH_0_MATCH U64(0x0000ffffffffe000) /* VA <47:13> */
#define GH_0_PHYS U64(0x00000fffffffe000)  /* PA <43:13> */
#define GH_0_KEEP U64(0x0000000000001fff)  /* VA <12:0>  */

#define GH_1_MATCH U64(0x0000ffffffff0000) /* VA <47:16> */
#define GH_1_PHYS U64(0x00000fffffff0000)  /* PA <43:16> */
#define GH_1_KEEP U64(0x000000000000ffff)  /* VA <15:0>  */
#define GH_2_MATCH U64(0x0000fffffff80000) /* VA <47:19> */
#define GH_2_PHYS U64(0x00000ffffff80000)  /* PA <43:19> */
#define GH_2_KEEP U64(0x000000000007ffff)  /* VA <18:0>  */
#define GH_3_MATCH U64(0x0000ffffffc00000) /* VA <47:22> */
#define GH_3_PHYS U64(0x00000fffffc00000)  /* PA <43:22> */
#define GH_3_KEEP U64(0x00000000003fffff)  /* VA <21:0>  */

/**
 * \brief Add translation-buffer entry
 *
 * Add a translation-buffer entry to one of the translation buffers.
 *
 * \param virt    Virtual address.
 * \param pte     Translation in DTB_PTE format (see add_tb_d).
 * \param flags   ACCESS_EXEC determines which translation buffer to use.
 * \param asn     Address space number latched by the PAL fill port.
 **/
void CAlphaCPU::add_tb(u64 virt, u64 pte_phys, u64 pte_flags, int flags,
                       int asn) {
  int t = (flags & ACCESS_EXEC) ? TB_INDEX_ITB : TB_INDEX_DATA;
  int rw = (flags & ACCESS_WRITE) ? 1 : 0;
  u64 match_mask = 0;
  u64 keep_mask = 0;
  u64 phys_mask = 0;
  int i;

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

  i = -1;
  for (int j = 0; j < TB_ENTRIES; j++) {
    if (state.tb[t][j].valid &&
        !((state.tb[t][j].virt ^ virt) & state.tb[t][j].match_mask) &&
        (state.tb[t][j].asm_bit || state.tb[t][j].asn == asn)) {
      i = j;
      break;
    }
  }

#ifdef ES40_JIT
  // A same-(virt,asn) ITB overwrite with a DIFFERENT physical is a code-page
  // remap performed WITHOUT a separate TBIS; chained JIT blocks compiled from
  // the old mapping must re-validate, so bump the generation. A next_tb
  // eviction (i reassigned below) replaces a DIFFERENT vpage (not a remap of
  // this one) and must NOT bump.
  const bool itb_remap = (t == TB_INDEX_ITB) && (i >= 0) &&
                         (state.tb[t][i].phys != (pte_phys & phys_mask));
#endif

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

#ifdef ES40_JIT
  if (itb_remap && m_jit)
    m_jit->note_itb_invalidate(); // code page remapped in place -> chains
                                  // re-validate
#endif

  if (t == TB_INDEX_DATA)
    flush_data_page_cache();

#if defined(DEBUG_TB_)
#if defined(IDB)
  if (bTB_Debug)
#endif
  {
    printf("Add TB---------------------------------------\n");
    printf("Map VIRT    %016" PRIx64 "\n", state.tb[t][i].virt);
    printf("Matching    %016" PRIx64 "\n", state.tb[t][i].match_mask);
    printf("And keeping %016" PRIx64 "\n", state.tb[t][i].keep_mask);
    printf("To PHYS     %016" PRIx64 "\n", state.tb[t][i].phys);
    printf("Read : %c%c%c%c %c\n", state.tb[t][i].access[0][0] ? 'K' : '-',
           state.tb[t][i].access[0][1] ? 'E' : '-',
           state.tb[t][i].access[0][2] ? 'S' : '-',
           state.tb[t][i].access[0][3] ? 'U' : '-',
           state.tb[t][i].fault[0] ? 'F' : '-');
    printf("Write: %c%c%c%c %c\n", state.tb[t][i].access[1][0] ? 'K' : '-',
           state.tb[t][i].access[1][1] ? 'E' : '-',
           state.tb[t][i].access[1][2] ? 'S' : '-',
           state.tb[t][i].access[1][3] ? 'U' : '-',
           state.tb[t][i].fault[1] ? 'F' : '-');
    printf("Exec : %c%c%c%c %c\n", state.tb[t][i].access[1][0] ? 'K' : '-',
           state.tb[t][i].access[1][1] ? 'E' : '-',
           state.tb[t][i].access[1][2] ? 'S' : '-',
           state.tb[t][i].access[1][3] ? 'U' : '-',
           state.tb[t][i].fault[1] ? 'F' : '-');
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
 * \param dtb     DTB fill port number (0 = DTB_PTE0, 1 = DTB_PTE1).
 **/
void CAlphaCPU::add_tb_d(u64 virt, u64 pte, int dtb) {
  add_tb(virt, pte >> (32 - 13), pte, ACCESS_READ,
         dtb ? state.asn1 : state.asn0);
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
  add_tb(virt, pte, pte & 0xf70, ACCESS_EXEC, state.asn);
}

/**
 * \brief Invalidate all translation-buffer entries
 *
 * Invalidate all translation-buffer entries in one of the translation buffers.
 *
 * \param flags   ACCESS_EXEC determines which translation buffer to use.
 **/
void CAlphaCPU::tbia(int flags) {
  int t = (flags & ACCESS_EXEC) ? TB_INDEX_ITB : TB_INDEX_DATA;
  int i;
  for (i = 0; i < TB_ENTRIES; i++)
    state.tb[t][i].valid = false;
  state.last_found_tb[t][0] = 0;
  state.last_found_tb[t][1] = 0;
  state.next_tb[t] = 0;
  if (t == TB_INDEX_DATA)
    flush_data_page_cache();
#ifdef ES40_JIT
  else if (m_jit)
    m_jit->note_itb_invalidate(); // whole ITB cleared -> indirect chains
                                  // re-validate phys
#endif
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
  int t = (flags & ACCESS_EXEC) ? TB_INDEX_ITB : TB_INDEX_DATA;
  int i;
  for (i = 0; i < TB_ENTRIES; i++)
    if (!state.tb[t][i].asm_bit)
      state.tb[t][i].valid = false;

  if (t == TB_INDEX_DATA)
    flush_data_page_cache();
#ifdef ES40_JIT
  else if (m_jit)
    m_jit->note_itb_invalidate(); // process ITB entries cleared -> chains
                                  // re-validate phys
#endif
}

/**
 * \brief Invalidate single translation-buffer entry
 *
 * \param virt    Virtual address for which the entry should be invalidated.
 * \param flags   ACCESS_EXEC determines which translation buffer to use.
 **/
void CAlphaCPU::tbis(u64 virt, int flags) {
  int t = (flags & ACCESS_EXEC) ? TB_INDEX_ITB : TB_INDEX_DATA;

  if (t == TB_INDEX_DATA) {
    tbis_d(virt, state.asn0);
    if (state.asn1 != state.asn0)
      tbis_d(virt, state.asn1);
    return;
  }

  int i = FindTBEntry(virt, flags);
  if (i >= 0)
    state.tb[t][i].valid = false;
#ifdef ES40_JIT
  // A TBIS signals the OS is changing this code page's mapping. Bump the JIT
  // generation even when the entry wasn't currently cached (i<0, already
  // evicted from the TB), a JIT block compiled from this page is still stale
  // and MUST re-validate before being chained.
  if (m_jit)
    m_jit->note_itb_invalidate();
#endif
}

void CAlphaCPU::tbis_d(u64 virt, int asn) {
  int i;

  for (i = 0; i < TB_ENTRIES; i++) {
    if (state.tb[TB_INDEX_DATA][i].valid &&
        !((state.tb[TB_INDEX_DATA][i].virt ^ virt) &
          state.tb[TB_INDEX_DATA][i].match_mask) &&
        (state.tb[TB_INDEX_DATA][i].asm_bit ||
         state.tb[TB_INDEX_DATA][i].asn == asn)) {
      state.tb[TB_INDEX_DATA][i].valid = false;
    }
  }

  flush_data_page_cache();
}

//\}

/**
 * \brief Enable i-cache regardles of config file.
 *
 * Required for SRM-ROM decompression.
 **/
void CAlphaCPU::enable_icache() { icache_enabled = true; }

/**
 * \brief Restore i-cache after temporary ROM decompression setup.
 **/
void CAlphaCPU::restore_icache() { icache_enabled = true; }

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
