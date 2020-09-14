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
 * Although this is not required, the author would appreciate being notified
 * of, and receiving any modifications you may make to the source code that
 * might serve the general public.
 */

/**
 * \file
 * Contains the definitions for the emulated DecChip 21264CB EV68 Alpha
 *processor.
 *
 * $Id: AlphaCPU.h,v 1.59 2008/06/12 07:29:44 iamcamiel Exp $
 *
 * X-1.59       Camiel Vanderhoeven                             12-JUN-2008
 *   a) Support to keep secondary CPUs waiting until activated from primary.
 *   b) Support for last written and last read memory locations.
 *
 * X-1.58       Camiel Vanderhoeven                             31-MAY-2008
 *      Changes to include parts of Poco.
 *
 * X-1.57       Camiel Vanderhoeven                             24-MAR-2008
 *      Comments.
 *
 * X-1.56       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.55       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.54       Camiel Vanderhoeven                             13-MAR-2008
 *      Create init(), start_threads() and stop_threads() functions.
 *
 * X-1.53       Camiel Vanderhoeven                             11-MAR-2008
 *      Named, debuggable mutexes.
 *
 * X-1.52       Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.51       Brian Wheeler                                   29-FEB-2008
 *      Add BREAKPOINT INSTRUCTION command to IDB.
 *
 * X-1.50       Camiel Vanderhoeven                             08-FEB-2008
 *      Show originating device name on memory errors.
 *
 * X-1.49       Camiel Vanderhoeven                             01-FEB-2008
 *      Avoid unnecessary shift-operations to calculate constant values.
 *
 * X-1.48       Camiel Vanderhoeven                             30-JAN-2008
 *      Always use set_pc or add_pc to change the program counter.
 *
 * X-1.47       Camiel Vanderhoeven                             30-JAN-2008
 *      Remember number of instructions left in current memory page, so
 *      that the translation-buffer doens't need to be consulted on every
 *      instruction fetch when the Icache is disabled.
 *
 * X-1.46       Camiel Vanderhoeven                             29-JAN-2008
 *      Cleanup.
 *
 * X-1.45       Camiel Vanderhoeven                             29-JAN-2008
 *      Remember separate last found translation-buffer entries for read
 *      and write operations. This should help with memory copy operations.
 *
 * X-1.44       Camiel Vanderhoeven                             28-JAN-2008
 *      Better floating-point exception handling.
 *
 * X-1.43       Camiel Vanderhoeven                             27-JAN-2008
 *      Comments.
 *
 * X-1.40       Camiel Vanderhoeven                             27-JAN-2008
 *      Minor floating-point improvements.
 *
 * X-1.39       Camiel Vanderhoeven                             25-JAN-2008
 *      Added option to disable the icache.
 *
 * X-1.37       Camiel Vanderhoeven                             21-JAN-2008
 *      Moved some macro's to cpu_defs.h; implement new floating-point code.
 *
 * X-1.36       Camiel Vanderhoeven                             19-JAN-2008
 *      Run CPU in a separate thread if CPU_THREADS is defined.
 *      NOTA BENE: This is very experimental, and has several problems.
 *
 * X-1.35       Camiel Vanderhoeven                             18-JAN-2008
 *      Comments.
 *
 * X-1.34       Camiel Vanderhoeven                             18-JAN-2008
 *      Process device interrupts after a 100-cpu-cycle delay.
 *
 * X-1.33       Camiel Vanderhoeven                             08-JAN-2008
 *      Removed last references to IDE disk read SRM replacement.
 *
 * X-1.32       Camiel Vanderhoeven                             02-JAN-2008
 *      Endianess fix.
 *
 * X-1.31       Camiel Vanderhoeven                             02-JAN-2008
 *      Comments. Undid part of last change because of performance impact.
 *
 * X-1.30       Camiel Vanderhoeven                             29-DEC-2007
 *      Avoid referencing uninitialized data.
 *
 * X-1.29       Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.28       Camiel Vanderhoeven                             10-DEC-2007
 *      Use configurator.
 *
 * X-1.27       Camiel Vanderhoeven                             2-DEC-2007
 *      Changed the way translation buffers work, the way interrupts work,
 *      added vmspal routines.
 *
 * X-1.26       Brian Wheeler                                   1-DEC-2007
 *      Added support for instruction counting, underlined lines in
 *      listings, corrected some unsigned/signed issues.
 *
 * X-1.25	    Brian Wheeler 22-NOV-2007 Added set_r and set_f for LOADREG
 *and LOADFPREG debugger commands.
 *
 * X-1.24       Camiel Vanderhoeven                             06-NOV-2007
 *      Performance improvements to ICACHE: last result is kept; cache
 *      lines are larger (512 DWORDS in stead of 16 DWORDS), cache size is
 *      configurable (both number of cache lines and size of each cache
 *      line), memcpy is used to move memory into the ICACHE.
 *      CAVEAT: ICACHE can only be filled from memory (not from I/O).
 *
 * X-1.23       Eduardo Marcelo Ferrat                          31-OCT-2007
 *      Disable SRM replacement routines.
 *
 * X-1.22       Camiel Vanderhoeven                             17-APR-2007
 *      Give ASM bit a value (true) for PALmode Icache entries.
 *
 * X-1.21       Camiel Vanderhoeven                             11-APR-2007
 *      Moved all data that should be saved to a state file to a structure
 *      "state".
 *
 * X-1.20	Camiel Vanderhoeven				7-APR-2007
 *	Added get_hwpcb;
 *
 * X-1.19	Camiel Vanderhoeven				5-APR-2007
 *	Fixed X-1.14. The virtual address was returned instead of the
 *	physical one!
 *
 * X-1.18       Camiel Vanderhoeven                             31-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.17	Camiel Vanderhoeven				18-MAR-2007
 *   	Removed pointles comparison (v_prbr > 0).
 *
 * X-1.16	Camiel Vanderhoeven				14-MAR-2007
 *	bListing removed.
 *
 * X-1.15	Camiel Vanderhoeven				12-MAR-2007
 *   a)	Added possibility to retrieve physical address of current instruction.
 *   b) Added member function get_pal_base.
 *
 * X-1.14	Camiel Vanderhoeven				9-MAR-2007
 *	Try to translate a virtual PRBR value to a physical one in get_prbr.
 *
 * X-1.13	Camiel Vanderhoeven				8-MAR-2007
 *	va_form now takes a boolean argument bIBOX to determine which ASN
 *	and VPTB to use.
 *
 * X-1.12	Camiel Vanderhoeven				7-MAR-2007
 *	Added get_tb, get_asn and get_spe functions.
 *
 * X-1.11	Camiel Vanderhoeven				22-FEB-2007
 *	Add ASM bit to the instruction cache & corresponding functions.
 *
 * X-1.10	Camiel Vanderhoeven				18-FEB-2007
 *	Add get_f function.
 *
 * X-1.9        Camiel Vanderhoeven                             16-FEB-2007
 *   a) Added CAlphaCPU::listing.
 *   b) CAlphaCPU::DoClock now returns a value.
 *
 * X-1.8        Camiel Vanderhoeven                             12-FEB-2007
 *	Added get_r and get_prbr functions as inlines.
 *
 * X-1.7        Camiel Vanderhoeven                             12-FEB-2007
 *	Added inline functions to get and update the program counter (pc).
 *
 * X-1.6	Camiel Vanderhoeven				12-FEB-2007
 *	Added comments.
 *
 * X-1.5        Camiel Vanderhoeven                             9-FEB-2007
 *	Added comments.
 *
 * X-1.4        Camiel Vanderhoeven                             9-FEB-2007
 *      Moved debugging flags (booleans) to TraceEngine.
 *
 * X-1.3	Camiel Vanderhoeven				7-FEB-2007
 *	Added comments.
 *
 * X-1.2        Brian Wheeler                                   3-FEB-2007
 *      Formatting.
 *
 * X-1.1        Camiel Vanderhoeven                             19-JAN-2007
 *      Initial version in CVS.
 **/
#if !defined(INCLUDED_ALPHACPU_H)
#define INCLUDED_ALPHACPU_H

#include "System.h"
#include "SystemComponent.h"
#include "cpu_defs.h"

/// Number of entries in the Instruction Cache
#define ICACHE_ENTRIES 1024
// Size of Instruction Cache entries in DWORDS (instructions)
#define ICACHE_LINE_SIZE 512
/** These bits should match to have an Instruction Cache hit.
    This includes bit 0, because it indicates PALmode . */
#define ICACHE_MATCH_MASK (u64)(U64(0x1) - (ICACHE_LINE_SIZE * 4))
/// DWORD (instruction) number of an address in an ICache entry.
#define ICACHE_INDEX_MASK (u64)(ICACHE_LINE_SIZE - U64(0x1))
/// Byte numer of an address in an ICache entry.
#define ICACHE_BYTE_MASK (u64)(ICACHE_INDEX_MASK << 2)
/// Number of entries in each Translation Buffer
#define TB_ENTRIES 16

/**
 * \brief Emulated CPU.
 *
 * The CPU emulated is the DECchip 21264CB Alpha Processor (EV68).
 *
 * Documentation consulted:
 *  - Alpha 21264/EV68CB and 21264/EV68DC Microprocessor Hardware Reference
 *Manual [HRM] (http://download.majix.org/dec/21264ev68cb_ev68dc_hrm.pdf)
 *  - DS-0026A-TE: Alpha 21264B Microprocessor Hardware Reference Manual [HRM]
 *(http://ftp.digital.com/pub/Digital/info/semiconductor/literature/21264hrm.pdf)
 *  - Alpha Architecture Reference Manual, fourth edition [ARM]
 *(http://download.majix.org/dec/alpha_arch_ref.pdf)
 *	.
 **/
class CAlphaCPU : public CSystemComponent, public CRunnable {
public:
  void flush_icache_asm();
  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);
  void irq_h(int number, bool assert, int delay);
  int get_cpuid();
  void flush_icache();

  virtual void run(); // Poco Thread entry point
  void execute();
  void release_threads();

  void set_PAL_BASE(u64 pb);
  virtual void check_state();
  CAlphaCPU(CConfigurator *cfg, CSystem *system);
  virtual ~CAlphaCPU();
  u64 get_r(int i, bool translate);
  u64 get_f(int i);
  void set_r(int reg, u64 val);
  void set_f(int reg, u64 val);
  u64 get_prbr(void);
  u64 get_hwpcb(void);
  u64 get_pc();
  u64 get_pal_base();

  void enable_icache();
  void restore_icache();

  bool get_waiting() { return state.wait_for_start; };
  void stop_waiting() { state.wait_for_start = false; };
#ifdef IDB
  u64 get_current_pc_physical();
  u64 get_instruction_count();
  u32 get_last_instruction();
  u64 get_last_read_loc() { return last_read_loc; }
  u64 get_last_write_loc() { return last_write_loc; }
#endif
  u64 get_clean_pc();
  void next_pc();
  void set_pc(u64 p_pc);
  void add_pc(u64 a_pc);

  u64 get_speed() { return cpu_hz; };

  u64 va_form(u64 address, bool bIBOX);

#if defined(IDB)
  void listing(u64 from, u64 to);
  void listing(u64 from, u64 to, u64 mark);
#endif
  int virt2phys(u64 virt, u64 *phys, int flags, bool *asm_bit, u32 instruction);

  virtual void init();
  virtual void start_threads();
  virtual void stop_threads();

private:
  CThread *myThread;
  CSemaphore mySemaphore;
  bool StopThread;

  int get_icache(u64 address, u32 *data);
  int FindTBEntry(u64 virt, int flags);
  void add_tb(u64 virt, u64 pte_phys, u64 pte_flags, int flags);
  void add_tb_i(u64 virt, u64 pte);
  void add_tb_d(u64 virt, u64 pte);
  void tbia(int flags);
  void tbiap(int flags);
  void tbis(u64 virt, int flags);

  /* Floating Point routines */
  u64 ieee_lds(u32 op);
  u32 ieee_sts(u64 op);
  u64 ieee_cvtst(u64 op, u32 ins);
  u64 ieee_cvtts(u64 op, u32 ins);
  s32 ieee_fcmp(u64 s1, u64 s2, u32 ins, u32 trap_nan);
  u64 ieee_cvtif(u64 val, u32 ins, u32 dp);
  u64 ieee_cvtfi(u64 op, u32 ins);
  u64 ieee_fadd(u64 s1, u64 s2, u32 ins, u32 dp, bool sub);
  u64 ieee_fmul(u64 s1, u64 s2, u32 ins, u32 dp);
  u64 ieee_fdiv(u64 s1, u64 s2, u32 ins, u32 dp);
  u64 ieee_sqrt(u64 op, u32 ins, u32 dp);
  int ieee_unpack(u64 op, UFP *r, u32 ins);
  void ieee_norm(UFP *r);
  u64 ieee_rpack(UFP *r, u32 ins, u32 dp);
  void ieee_trap(u64 trap, u32 instenb, u64 fpcrdsb, u32 ins);
  u64 vax_ldf(u32 op);
  u64 vax_ldg(u64 op);
  u32 vax_stf(u64 op);
  u64 vax_stg(u64 op);
  void vax_trap(u64 mask, u32 ins);
  void vax_unpack(u64 op, UFP *r, u32 ins);
  void vax_unpack_d(u64 op, UFP *r, u32 ins);
  void vax_norm(UFP *r);
  u64 vax_rpack(UFP *r, u32 ins, u32 dp);
  u64 vax_rpack_d(UFP *r, u32 ins);
  int vax_fcmp(u64 s1, u64 s2, u32 ins);
  u64 vax_cvtif(u64 val, u32 ins, u32 dp);
  u64 vax_cvtfi(u64 op, u32 ins);
  u64 vax_fadd(u64 s1, u64 s2, u32 ins, u32 dp, bool sub);
  u64 vax_fmul(u64 s1, u64 s2, u32 ins, u32 dp);
  u64 vax_fdiv(u64 s1, u64 s2, u32 ins, u32 dp);
  u64 vax_sqrt(u64 op, u32 ins, u32 dp);

  /* VMS PALcode call: */
  void vmspal_call_cflush();
  void vmspal_call_draina();
  void vmspal_call_ldqp();
  void vmspal_call_stqp();
  void vmspal_call_swpctx();
  void vmspal_call_mfpr_asn();
  void vmspal_call_mtpr_asten();
  void vmspal_call_mtpr_astsr();
  void vmspal_call_cserve();
  void vmspal_call_mfpr_fen();
  void vmspal_call_mtpr_fen();
  void vmspal_call_mfpr_ipl();
  void vmspal_call_mtpr_ipl();
  void vmspal_call_mfpr_mces();
  void vmspal_call_mtpr_mces();
  void vmspal_call_mfpr_pcbb();
  void vmspal_call_mfpr_prbr();
  void vmspal_call_mtpr_prbr();
  void vmspal_call_mfpr_ptbr();
  void vmspal_call_mfpr_scbb();
  void vmspal_call_mtpr_scbb();
  void vmspal_call_mtpr_sirr();
  void vmspal_call_mfpr_sisr();
  void vmspal_call_mfpr_tbchk();
  void vmspal_call_mtpr_tbia();
  void vmspal_call_mtpr_tbiap();
  void vmspal_call_mtpr_tbis();
  void vmspal_call_mfpr_esp();
  void vmspal_call_mtpr_esp();
  void vmspal_call_mfpr_ssp();
  void vmspal_call_mtpr_ssp();
  void vmspal_call_mfpr_usp();
  void vmspal_call_mtpr_usp();
  void vmspal_call_mtpr_tbisd();
  void vmspal_call_mtpr_tbisi();
  void vmspal_call_mfpr_asten();
  void vmspal_call_mfpr_astsr();
  void vmspal_call_mfpr_vptb();
  void vmspal_call_mtpr_datfx();
  void vmspal_call_mfpr_whami();
  void vmspal_call_imb();
  void vmspal_call_prober();
  void vmspal_call_probew();
  void vmspal_call_rd_ps();
  int vmspal_call_rei();
  void vmspal_call_swasten();
  void vmspal_call_wr_ps_sw();
  void vmspal_call_rscc();
  void vmspal_call_read_unq();
  void vmspal_call_write_unq();

  /* VMS PALcode entry: */
  int vmspal_ent_dtbm_double_3(int flags);
  int vmspal_ent_dtbm_single(int flags);
  int vmspal_ent_itbm(int flags);
  int vmspal_ent_iacv(int flags);
  int vmspal_ent_dfault(int flags);
  int vmspal_ent_ext_int(int ei);
  int vmspal_ent_sw_int(int si);
  int vmspal_ent_ast_int(int ast);

  /* VMS PALcode internal: */
  int vmspal_int_initiate_exception();
  int vmspal_int_initiate_interrupt();

  bool icache_enabled;

  // ... ... ...
  u64 cc_large;
  u64 start_icount;
  u64 start_cc;
  CTimestamp start_time;
  u64 prev_icount;
  u64 prev_cc;
  u64 prev_time;
  u64 cc_per_instruction;
  u64 ins_per_timer_int;
  u64 next_timer_int;
  u64 cpu_hz;

  /// The state structure contains all elements that need to be saved to the
  /// statefile
  struct SCPU_state {
    bool wait_for_start;
    u64 pal_base; /**< IPR PAL_BASE [HRM: p 5-15] */
    u64 pc;       /**< Program counter */
    u64 cc;       /**< IPR CC: Cycle counter [HRM p 5-3] */
    u64 r[64];    /**< Integer registers (0-31 normal, 32-63 shadow) */
    u64 dc_stat;  /**< IPR DC_STAT: Dcache status [HRM p 5-31..32] */
    bool ppcen; /**< IPR PCTX: ppce (proc perf counting enable) [HRM p 5-21..23]
                 */
    u64 i_stat; /**< IPR I_STAT: Ibox status [HRM p 5-18..20] */
    u64 pctr_ctl;  /**< IPR PCTR_CTL [HRM p 5-23..25] */
    bool cc_ena;   /**< IPR CC_CTL: Cycle counter enabled [HRM p 5-3] */
    u32 cc_offset; /**< IPR CC: Cycle counter offset [HRM p 5-3] */
    u64 dc_ctl;    /**< IPR DC_CTL: Dcache control [HRM p 5-30..31] */
    int alt_cm;    /**< IPR DTB_ALTMODE: alternative cm for HW_LD/HW_ST [HRM p
                      5-26..27] */
    int smc; /**< IPR M_CTL: smc (speculative miss control) [HRM p 5-29..30] */
    bool fpen;    /**< IPR PCTX: fpe (floating point enable) [HRM p 5-21..23] */
    bool sde;     /**< IPR I_CTL: sde[1] (PALshadow enable) [HRM p 5-15..18] */
    u64 fault_va; /**< IPR VA: virtual address of last Dstream miss or fault
                     [HRM p 5-4] */
    u64 exc_sum;  /**< IPR EXC_SUM: exception summary [HRM p 5-13..15] */
    int i_ctl_va_mode;  /**< IPR I_CTL: (va_form_32 + va_48) [HRM p 5-15..17] */
    int va_ctl_va_mode; /**< IPR VA_CTL: (va_form_32 + va_48) [HRM p 5-4] */
    u64 i_ctl_vptb;     /**< IPR I_CTL: vptb (virtual page table base) [HRM p
                           5-15..16] */
    u64 va_ctl_vptb; /**< IPR VA_CTL: vptb (virtual page table base) [HRM p 5-4]
                      */
    int cm;          /**< IPR IER_CM: cm (current mode) [HRM p 5-9..10] */
    int asn;   /**< IPR PCTX: asn (address space number) [HRM p 5-21..22] */
    int asn0;  /**< IPR DTB_ASN0: asn (address space number) [HRM p 5-28] */
    int asn1;  /**< IPR DTB_ASN1: asn (address space number) [HRM p 5-28] */
    int eien;  /**< IPR IER_CM: eien (external interrupt enable) [HRM p 5-9..10]
                */
    int slen;  /**< IPR IER_CM: slen (serial line interrupt enable) [HRM p
                  5-9..10] */
    int cren;  /**< IPR IER_CM: cren (corrected read error int enable) [HRM p
                  5-9..10] */
    int pcen;  /**< IPR IER_CM: pcen (perf counter interrupt enable) [HRM p
                  5-9..10] */
    int sien;  /**< IPR IER_CM: sien (software interrupt enable) [HRM p 5-9..10]
                */
    int asten; /**< IPR IER_CM: asten (AST interrupt enable) [HRM p 5-9..10] */
    int sir; /**< IPR SIRR: sir (software interrupt request) [HRM p 5-10..11] */
    int eir; /**< external interrupt request */
    int slr; /**< serial line interrupt request */
    int crr; /**< corrected read error interrupt */
    int pcr; /**< perf counter interrupt */
    int astrr;       /**< IPR PCTX: astrr (AST request) [HRM p 5-21..22] */
    int aster;       /**< IPR PCTX: aster (AST enable) [HRM p 5-21..22] */
    u64 i_ctl_other; /**< various bits in IPR I_CTL that have no meaning to the
                        emulator */
    u64 mm_stat; /**< IPR MM_STAT: memory management status [HRM p 5-28..29] */
    bool hwe;    /**< IPR I_CLT: hwe (allow palmode ins in kernel mode) [HRM p
                    5-15..17] */
    int m_ctl_spe; /**< IPR M_CTL: spe (Super Page mode enabled) [HRM p
                      5-29..30] */
    int i_ctl_spe; /**< IPR I_CTL: spe (Super Page mode enabled) [HRM p
                      5-15..18] */
    u64 exc_addr;  /**< IPR EXC_ADDR: address of last exception [HRM p 5-8] */
    u64 pmpc;
    u64 fpcr; /**< Floating-Point Control Register [HRM p 2-36] */
    bool bIntrFlag;
    u64 current_pc; /**< Virtual address of current instruction */

    /**
     * \brief Instruction cache entry.
     *
     * An instruction cache entry contains the address and address space number
     * (ASN) + 16 32-bit instructions. [HRM 2-11]
     **/
    struct SICache {
      int asn;                    /**< Address Space Number */
      u32 data[ICACHE_LINE_SIZE]; /**< Actual cached instructions  */
      u64 address;                /**< Address of first instruction */
      u64 p_address;              /**< Physical address of first instruction */
      bool asm_bit;               /**< Address Space Match bit */
      bool valid;                 /**< Valid cache entry */
    } icache[ICACHE_ENTRIES];     /**< Instruction cache entries [HRM p 2-11] */
    int next_icache;              /**< Number of next cache entry to use */
    int last_found_icache;        /**< Number of last cache entry found */

    /**
     * \brief Translation Buffer Entry.
     *
     * A translation buffer entry provides the mapping from a page of virtual
     *memory to a page of physical memory.
     **/
    struct STBEntry {
      u64 virt;       /**< Virtual address of page*/
      u64 phys;       /**< Physical address of page*/
      u64 match_mask; /**< The virtual address has to match for these bits to be
                         a hit*/
      u64 keep_mask;  /**< This part of the virtual address is OR-ed with the
                         phys address*/
      int asn;        /**< Address Space Number*/
      int asm_bit;    /**< Address Space Match bit*/
      int access[2][4];  /**< Access permitted [read/write][current mode]*/
      int fault[3];      /**< Fault on access [read/write/execute]*/
      bool valid;        /**< Valid entry*/
    } tb[2][TB_ENTRIES]; /**< Translation buffer entries */

    int next_tb[2]; /**< Number of next translation buffer entry to use */
    int last_found_tb[2]
                     [2]; /**< Number of last translation buffer entry found */
    u32 rem_ins_in_page;  /**< Number of instructions remaining in current page
                           */
    u64 pc_phys;
    u64 f[64];    /**< Floating point registers (0-31 normal, 32-63 shadow) */
    int iProcNum; /**< number of the current processor (0 in a 1-processor
                     system) */
    u64 instruction_count; /**< Number of times doclock has been called */
    u64 last_tb_virt;
    bool pal_vms; /**< True if the PALcode base is 0x8000 (=VMS PALcode base) */
    bool check_int;     /**< True if an interrupt may be pending */
    int irq_h_timer[6]; /**< Timers for delayed IRQ_H[0:5] assertion */
    bool check_timers;
  } state; /**< Determines CPU state that needs to be saved to the state file */

#ifdef IDB
  u64 current_pc_physical; /**< Physical address of current instruction */
  u32 last_instruction;
  u64 last_read_loc;
  u64 last_write_loc;
#endif
};

/** Translate raw register (0..31) number to a number that takes PALshadow
    registers into consideration (0..63). Considers the program counter
    (to determine if we're in PALmode), and the SDE (Shadow Enable) bit. */
#define RREG(a)                                                                \
  (((a)&0x1f) + (((state.pc & 1) && (((a)&0xc) == 0x4) && state.sde) ? 32 : 0))

/**
 * Empty the instruction cache.
 **/
inline void CAlphaCPU::flush_icache() {
  if (icache_enabled) {

    //  memset(state.icache,0,sizeof(state.icache));
    int i;
    for (i = 0; i < ICACHE_ENTRIES; i++) {
      state.icache[i].valid = false;

      //    state.icache[i].asm_bit = true;
    }

    state.next_icache = 0;
    state.last_found_icache = 0;
  }
}

/**
 * Empty the instruction cache of lines with the ASM bit clear.
 **/
inline void CAlphaCPU::flush_icache_asm() {
  if (icache_enabled) {
    int i;
    for (i = 0; i < ICACHE_ENTRIES; i++)
      if (!state.icache[i].asm_bit)
        state.icache[i].valid = false;
  }
}

/**
 * Set the PALcode BASE register, and determine whether we're running VMS
 *PALcode.
 **/
inline void CAlphaCPU::set_PAL_BASE(u64 pb) {
  state.pal_base = pb;
  state.pal_vms = (pb == U64(0x8000));
}

/**
 * Get an instruction from the instruction cache.
 * If necessary, fill a new cache block from memory.
 *
 * get_icache checks all cache entries, to see if there is a
 * cache entry that matches the current address space number,
 * and that contains the address we're looking for. If it
 * exists, the instruction is fetched from this cache,
 * otherwise, the physical address for the instruction is
 * calculated, and the cache block is filled.
 *
 * The last cache entry that was a hit is remembered, so that
 * cache entry is checked first on the next instruction. (very
 * likely to be the same cache block)
 *
 * It would be easiest to do without the instruction cache
 * altogether, but unfortunately SRM uses self-modifying
 * code, that relies on the correct instruction stream to
 * remain in the cache.
 **/
inline int CAlphaCPU::get_icache(u64 address, u32 *data) {
  int i = state.last_found_icache;
  u64 v_a;
  u64 p_a;
  int result;
  bool asm_bit;

  if (icache_enabled) {
    if (state.icache[i].valid &&
        (state.icache[i].asn == state.asn || state.icache[i].asm_bit) &&
        state.icache[i].address == (address & ICACHE_MATCH_MASK)) {
      *data =
          endian_32(state.icache[i].data[(address >> 2) & ICACHE_INDEX_MASK]);
#ifdef IDB
      current_pc_physical =
          state.icache[i].p_address + (address & ICACHE_BYTE_MASK);
#endif
      return 0;
    }

    for (i = 0; i < ICACHE_ENTRIES; i++) {
      if (state.icache[i].valid &&
          (state.icache[i].asn == state.asn || state.icache[i].asm_bit) &&
          state.icache[i].address == (address & ICACHE_MATCH_MASK)) {
        state.last_found_icache = i;
        *data =
            endian_32(state.icache[i].data[(address >> 2) & ICACHE_INDEX_MASK]);

#ifdef IDB
        current_pc_physical =
            state.icache[i].p_address + (address & ICACHE_BYTE_MASK);
#endif
        return 0;
      }
    }

    v_a = address & ICACHE_MATCH_MASK;

    if (address & 1) {
      p_a = v_a & ~U64(0x1);
      asm_bit = true;
    } else {
      result = virt2phys(v_a, &p_a, ACCESS_EXEC, &asm_bit, 0);
      if (result)
        return result;
    }

    memcpy(state.icache[state.next_icache].data, cSystem->PtrToMem(p_a),
           ICACHE_LINE_SIZE * 4);

    state.icache[state.next_icache].valid = true;
    state.icache[state.next_icache].asn = state.asn;
    state.icache[state.next_icache].asm_bit = asm_bit;
    state.icache[state.next_icache].address = address & ICACHE_MATCH_MASK;
    state.icache[state.next_icache].p_address = p_a;

    *data = endian_32(state.icache[state.next_icache]
                          .data[(address >> 2) & ICACHE_INDEX_MASK]);

#ifdef IDB
    current_pc_physical = state.icache[state.next_icache].p_address +
                          (address & ICACHE_BYTE_MASK);
#endif
    state.last_found_icache = state.next_icache;
    state.next_icache++;
    if (state.next_icache == ICACHE_ENTRIES)
      state.next_icache = 0;
    return 0;
  }

  // icache disabled
  if (address & 1) {
    state.pc_phys = address & ~U64(0x3);
    state.rem_ins_in_page = 1;
  } else {
    if (!state.rem_ins_in_page) {
      result = virt2phys(address, &state.pc_phys, ACCESS_EXEC, &asm_bit, 0);
      if (result)
        return result;
      state.rem_ins_in_page = 2048 - ((((u32)address) >> 2) & 2047);
    }
  }

  *data = (u32)cSystem->ReadMem(state.pc_phys, 32, this);
  return 0;
}

/**
 * Convert a virtual address to va_form format.
 * Used for IPR VA_FORM [HRM 5-5..6] and IPR IVA_FORM [HRM 5-9].
 **/
inline u64 CAlphaCPU::va_form(u64 address, bool bIBOX) {
  switch (bIBOX ? state.i_ctl_va_mode : state.va_ctl_va_mode) {
  case 0:
    return ((bIBOX ? state.i_ctl_vptb : state.va_ctl_vptb) &
            U64(0xfffffffe00000000)) |
           ((address >> 10) & U64(0x00000001fffffff8));

  case 1:
    return ((bIBOX ? state.i_ctl_vptb : state.va_ctl_vptb) &
            U64(0xfffff80000000000)) |
           ((address >> 10) & U64(0x0000003ffffffff8)) |
           (((address >> 10) & U64(0x0000002000000000)) * U64(0x3e));

  case 2:
    return ((bIBOX ? state.i_ctl_vptb : state.va_ctl_vptb) &
            U64(0xffffffffc0000000)) |
           ((address >> 10) & U64(0x00000000003ffff8));
  }

  return 0;
}

/**
 * Return processor number.
 **/
inline int CAlphaCPU::get_cpuid() { return state.iProcNum; }

/**
 * Assert or release an external interrupt line to the cpu.
 **/
inline void CAlphaCPU::irq_h(int number, bool assert, int delay) {
  bool active = (state.eir & (U64(0x1) << number)) || state.irq_h_timer[number];
  if (assert && !active) {
    if (delay) {
      state.irq_h_timer[number] = delay;
      state.check_timers = true;
    } else {
      state.eir |= (U64(0x1) << number);
      state.check_int = true;
    }

    return;
  }

  if (!assert && active) {
    state.eir &= ~(U64(0x1) << number);
    state.irq_h_timer[number] = 0;
    state.check_timers = false;
    for (int i = 0; i < 6; i++) {
      if (state.irq_h_timer[i])
        state.check_timers = true;
    }
  }
}

/**
 * Return program counter value.
 **/
inline u64 CAlphaCPU::get_pc() { return state.pc; }

#ifdef IDB

/**
 * Return the physical address the program counter refers to.
 **/
inline u64 CAlphaCPU::get_current_pc_physical() { return state.pc_phys; }
#endif

/**
 * Return program counter value without PALmode bit.
 **/
inline u64 CAlphaCPU::get_clean_pc() { return state.pc & ~U64(0x3); }

/**
 * Jump to next instruction
 **/
inline void CAlphaCPU::next_pc() {
  state.pc += 4;
  state.pc_phys += 4;
  if (state.rem_ins_in_page)
    state.rem_ins_in_page--;
}

/**
 * Set program counter to a certain value.
 **/
inline void CAlphaCPU::set_pc(u64 p_pc) {
  state.pc = p_pc;
  state.rem_ins_in_page = 0;
}

/**
 * Add  value to the program counter.
 **/
inline void CAlphaCPU::add_pc(u64 a_pc) {
  state.pc += a_pc;
  state.rem_ins_in_page = 0;
}

/**
 * Get a register value.
 * If \a translate is true, use shadow registers if currently enabled.
 **/
inline u64 CAlphaCPU::get_r(int i, bool translate) {
  if (translate)
    return state.r[RREG(i)];
  else
    return state.r[i];
}

/**
 * Get a fp register value.
 **/
inline u64 CAlphaCPU::get_f(int i) { return state.f[i]; }

/**
 * Set a register value
 **/
inline void CAlphaCPU::set_r(int reg, u64 value) { state.r[reg] = value; }

/**
 * Set a fp register value
 **/
inline void CAlphaCPU::set_f(int reg, u64 value) { state.f[reg] = value; }

/**
 * Get the PALcode base register.
 **/
inline u64 CAlphaCPU::get_pal_base() { return state.pal_base; }

/**
 * Get the processor base register.
 * A bit fuzzy...
 **/
inline u64 CAlphaCPU::get_prbr(void) {
  u64 v_prbr; // virtual
  u64 p_prbr; // physical
  bool b;
  if (state.r[21 + 32] && ((u64)(state.r[21 + 32] + 0xaf) <
                           (u64)((U64(0x1) << cSystem->get_memory_bits()))))
    v_prbr = cSystem->ReadMem(state.r[21 + 32] + 0xa8, 64, this);
  else
    v_prbr = cSystem->ReadMem(0x70a8 + (0x200 * get_cpuid()), 64, this);
  if (virt2phys(v_prbr, &p_prbr, ACCESS_READ | FAKE | NO_CHECK, &b, 0))
    p_prbr = v_prbr;
  if ((u64)p_prbr > (u64)(U64(0x1) << cSystem->get_memory_bits()))
    p_prbr = 0;
  return p_prbr;
}

/**
 * Get the hardware process control block address.
 **/
inline u64 CAlphaCPU::get_hwpcb(void) {
  u64 v_pcb; // virtual
  u64 p_pcb; // physical
  bool b;
  if (state.r[21 + 32] && ((u64)(state.r[21 + 32] + 0x17) <
                           (u64)((U64(0x1) << cSystem->get_memory_bits()))))
    v_pcb = cSystem->ReadMem(state.r[21 + 32] + 0x10, 64, this);
  else
    v_pcb = cSystem->ReadMem(0x7010 + (0x200 * get_cpuid()), 64, this);
  if (virt2phys(v_pcb, &p_pcb, ACCESS_READ | NO_CHECK | FAKE, &b, 0))
    p_pcb = v_pcb;
  if (p_pcb > (u64)(U64(0x1) << cSystem->get_memory_bits()))
    p_pcb = 0;
  return p_pcb;
}

#if defined(IDB)
/**
 * Return the last instruction executed.
 **/
inline u32 CAlphaCPU::get_last_instruction(void) { return last_instruction; }
#endif
extern bool bTB_Debug;
#endif // !defined(INCLUDED_ALPHACPU_H)
