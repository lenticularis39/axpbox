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

#if defined(IDB)
extern const char *PAL_NAME[];
extern const char *IPR_NAME[];

extern char dbg_string[1000];
#if !defined(LS_MASTER) && !defined(LS_SLAVE)
extern char *dbg_strptr;
#endif
void handle_debug_string(char *s);

#define TRC_(down, up, x, y)                                                   \
  if (bTrace)                                                                  \
    trc->trace(this, state.current_pc, state.pc, down, up, x, y);

#define TRC(down, up)                                                          \
  if (bTrace)                                                                  \
    trc->trace(this, state.current_pc, state.pc, down, up, (char *)0, 0);

#define TRC_BR                                                                 \
  if (bTrace)                                                                  \
    trc->trace_br(this, state.current_pc, state.pc);

#define GO_PAL(offset)                                                         \
  {                                                                            \
    if (bDisassemble) {                                                        \
      sprintf(dbg_strptr, " ==> PAL %x!\n", offset);                           \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
    handle_debug_string(dbg_string);                                           \
    state.exc_addr = state.current_pc;                                         \
    set_pc(state.pal_base | offset | 1);                                       \
    if ((offset == DTBM_SINGLE || offset == ITB_MISS) && bTrace)               \
      trc->set_waitfor(this, state.exc_addr & ~U64(0x3));                      \
    else                                                                       \
      TRC_(true, false, "GO_PAL %04x", offset);                                \
  }

#else
#define TRC_(down, up, x, y) ;
#define TRC(down, up) ;
#define TRC_BR ;

#define GO_PAL(offset)                                                         \
  {                                                                            \
    state.exc_addr = state.current_pc;                                         \
    set_pc(state.pal_base | offset | 1);                                       \
  }
#endif
#if defined(IDB)
#define DEBUG_XX                                                               \
  if (trc->get_fnc_name(this, state.current_pc & ~U64(0x3), &funcname)) {      \
    if (bListing && !strcmp(funcname, "")) {                                   \
      printf("%08" PRIx64 ": \"%s\"\n", state.current_pc,                         \
             cSystem->PtrToMem(state.current_pc));                             \
      state.pc = (state.current_pc +                                           \
                  strlen(cSystem->PtrToMem(state.current_pc)) + 4) &           \
                 ~U64(0x3);                                                    \
      while (state.pc < 0x600000 && cSystem->ReadMem(state.pc, 32, this) == 0) \
        state.pc += 4;                                                         \
      return;                                                                  \
    } else if (bListing && !strcmp(funcname, "!SKIP")) {                       \
      while (state.pc < 0x600000 && cSystem->ReadMem(state.pc, 32, this) == 0) \
        state.pc += 4;                                                         \
      return;                                                                  \
    } else if (bListing && !strncmp(funcname, "!CHAR-", 6)) {                  \
      u64 xx_upto;                                                             \
      int xx_result;                                                           \
      xx_result = sscanf(&(funcname[6]), "%" PRIx64 "", &xx_upto);                \
      if (xx_result == 1) {                                                    \
        state.pc = state.current_pc;                                           \
        while (state.pc < xx_upto) {                                           \
          printf("%08" PRIx64 ": \"%s\"\n", state.pc,                             \
                 cSystem->PtrToMem(state.pc));                                 \
          state.pc += strlen(cSystem->PtrToMem(state.pc));                     \
          while (state.pc < xx_upto &&                                         \
                 cSystem->ReadMem(state.pc, 8, this) == 0)                     \
            state.pc++;                                                        \
        }                                                                      \
        return;                                                                \
      }                                                                        \
    } else if (bListing && !strncmp(funcname, "!LCHAR-", 7)) {                 \
      char stringval[300];                                                     \
      int stringlen;                                                           \
      u64 xx_upto;                                                             \
      int xx_result;                                                           \
      xx_result = sscanf(&(funcname[7]), "%" PRIx64 "", &xx_upto);                \
      if (xx_result == 1) {                                                    \
        state.pc = state.current_pc;                                           \
        while (state.pc < xx_upto) {                                           \
          stringlen = (int)cSystem->ReadMem(state.pc++, 8, this);              \
          memset(stringval, 0, 300);                                           \
          strncpy(stringval, cSystem->PtrToMem(state.pc), stringlen);          \
          printf("%08" PRIx64 ": \"%s\"\n", state.pc - 1, stringval);             \
          state.pc += stringlen;                                               \
          while (state.pc < xx_upto &&                                         \
                 cSystem->ReadMem(state.pc, 8, this) == 0)                     \
            state.pc++;                                                        \
        }                                                                      \
        return;                                                                \
      }                                                                        \
    } else if (bListing && !strncmp(funcname, "!X64-", 5)) {                   \
      printf("\n%s:\n", &(funcname[5]));                                       \
      state.pc = state.current_pc;                                             \
      while ((state.pc == state.current_pc) ||                                 \
             !trc->get_fnc_name(this, state.pc, &funcname)) {                  \
        printf("%08" PRIx64 ": %016" PRIx64 "\n", state.pc,                          \
               cSystem->ReadMem(state.pc, 64, this));                          \
        state.pc += 8;                                                         \
      }                                                                        \
      return;                                                                  \
    } else if (bListing && !strncmp(funcname, "!X32-", 5)) {                   \
      printf("\n%s:\n", &(funcname[5]));                                       \
      state.pc = state.current_pc;                                             \
      while ((state.pc == state.current_pc) ||                                 \
             !trc->get_fnc_name(this, state.pc, &funcname)) {                  \
        printf("%08" PRIx64 ": %08" PRIx64 "\n", state.pc,                           \
               cSystem->ReadMem(state.pc, 32, this));                          \
        state.pc += 4;                                                         \
      }                                                                        \
      return;                                                                  \
    } else if (!strncmp(funcname, ":", 1)) {                                   \
      sprintf(dbg_strptr, "%s:\n", funcname);                                  \
      dbg_strptr += strlen(dbg_strptr);                                        \
    } else {                                                                   \
      sprintf(dbg_strptr, "\n%s:\n", funcname);                                \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }                                                                            \
  sprintf(dbg_strptr, bListing ? "%08" PRIx64 ": " : "%016" PRIx64 "",               \
          state.current_pc);                                                   \
  dbg_strptr += strlen(dbg_strptr);                                            \
  if (!bListing)                                                               \
    sprintf(dbg_strptr, "(%08" PRIx64 "): ", current_pc_physical);                \
  else {                                                                       \
    sprintf(dbg_strptr, "%08x %c%c%c%c: ", ins, printable((char)(ins)),        \
            printable((char)(ins >> 8)), printable((char)(ins >> 16)),         \
            printable((char)(ins >> 24)));                                     \
  }                                                                            \
  dbg_strptr += strlen(dbg_strptr);

#define UNKNOWN1                                                               \
  if (bDisassemble) {                                                          \
    DEBUG_XX                                                                   \
  }                                                                            \
  sprintf(dbg_strptr, "Unknown opcode: %02x   ", opcode);                      \
  dbg_strptr += strlen(dbg_strptr);                                            \
  handle_debug_string(dbg_string);                                             \
  return;

#define UNKNOWN2                                                               \
  if (bDisassemble) {                                                          \
    DEBUG_XX                                                                   \
  }                                                                            \
  sprintf(dbg_strptr, "Unknown opcode: %02x.%02x   ", opcode, function);       \
  dbg_strptr += strlen(dbg_strptr);                                            \
  handle_debug_string(dbg_string);                                             \
  return;

#define POST_X64(a)                                                            \
  if (bDisassemble) {                                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, " ==> %" PRIx64 "", a);                                 \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

#define PRE_PAL(mnemonic)                                                      \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    if (function < 0x40 || (function > 0x7f && function < 0xc0))               \
      sprintf(dbg_strptr, #mnemonic " %s", PAL_NAME[function]);                \
    else                                                                       \
      sprintf(dbg_strptr, #mnemonic " ?%x?", function);                        \
    dbg_strptr += strlen(dbg_strptr);                                          \
  }

#define POST_PAL TRC(1, 0);

#define PRE_BR(mnemonic)                                                       \
  if (bDisassemble) {                                                          \
    u64 dbg_x = (state.current_pc + 4 + (DISP_21 * 4)) & ~U64(0x3);            \
    DEBUG_XX sprintf(dbg_strptr, #mnemonic " r%d, ", REG_1 & 31);              \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (trc->get_fnc_name(this, dbg_x, &funcname))                             \
      sprintf(dbg_strptr, "%s", funcname);                                     \
    else                                                                       \
      sprintf(dbg_strptr, "%" PRIx64 "", dbg_x);                                  \
    dbg_strptr += strlen(dbg_strptr);                                          \
  }

#define POST_BR TRC_BR;

#define PRE_COND(mnemonic)                                                     \
  if (bDisassemble) {                                                          \
    u64 dbg_x = (state.current_pc + 4 + (DISP_21 * 4)) & ~U64(0x3);            \
    DEBUG_XX sprintf(dbg_strptr, #mnemonic " r%d, ", REG_1 & 31);              \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (trc->get_fnc_name(this, dbg_x, &funcname))                             \
      sprintf(dbg_strptr, "%s", funcname);                                     \
    else                                                                       \
      sprintf(dbg_strptr, "%" PRIx64 "", dbg_x);                                  \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, ": (%" PRIx64 ")", state.r[REG_1]);                     \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

#define POST_COND TRC_BR;

#define PRE_FCOND(mnemonic)                                                    \
  if (bDisassemble) {                                                          \
    u64 dbg_x = (state.current_pc + 4 + (DISP_21 * 4)) & ~U64(0x3);            \
    DEBUG_XX sprintf(dbg_strptr, #mnemonic " f%d, ", FREG_1);                  \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (trc->get_fnc_name(this, dbg_x, &funcname))                             \
      sprintf(dbg_strptr, "%s", funcname);                                     \
    else                                                                       \
      sprintf(dbg_strptr, "%" PRIx64 "", dbg_x);                                  \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, ": (%" PRIx64 ")", state.f[FREG_1]);                    \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

#define POST_FCOND TRC_BR;

#define PRE_BSR(mnemonic)                                                      \
  if (bDisassemble) {                                                          \
    u64 dbg_x = (state.current_pc + 4 + (DISP_21 * 4)) & ~U64(0x3);            \
    DEBUG_XX sprintf(dbg_strptr, #mnemonic " r%d, ", REG_1 & 31);              \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (trc->get_fnc_name(this, dbg_x, &funcname))                             \
      sprintf(dbg_strptr, "%s", funcname);                                     \
    else                                                                       \
      sprintf(dbg_strptr, "%" PRIx64 "", dbg_x);                                  \
    dbg_strptr += strlen(dbg_strptr);                                          \
  }

#define POST_BSR                                                               \
  if (REG_1 == 31) {                                                           \
    TRC(0, 1);                                                                 \
  } else {                                                                     \
    TRC(1, 1);                                                                 \
  }

#define PRE_JMP(mnemonic)                                                      \
  if (bDisassemble) {                                                          \
    DEBUG_XX sprintf(dbg_strptr, #mnemonic " r%d, r%d", REG_1 & 31,            \
                     REG_2 & 31);                                              \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, ": (%" PRIx64 ")", state.r[REG_2]);                     \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

#define POST_JMP                                                               \
  if (REG_1 == 31) {                                                           \
    TRC(0, 1);                                                                 \
  } else {                                                                     \
    TRC(1, 1);                                                                 \
  }

#define PRE_RET(mnemonic)                                                      \
  if (bDisassemble) {                                                          \
    DEBUG_XX sprintf(dbg_strptr, #mnemonic " r%d", REG_2 & 31);                \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, ": (%" PRIx64 ")", state.r[REG_2]);                     \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

#define POST_RET TRC(0, 1);

#define PRE_MFPR(mnemonic)                                                     \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic " r%d, %s", REG_1 & 31, IPR_NAME[function]); \
    dbg_strptr += strlen(dbg_strptr);                                          \
  }

#define POST_MFPR POST_X64(state.r[REG_1]);

#define PRE_MTPR(mnemonic)                                                     \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic " r%d, %s", REG_2 & 31, IPR_NAME[function]); \
    dbg_strptr += strlen(dbg_strptr);                                          \
  }

#define POST_MTPR POST_X64(state.r[REG_2]);

#define PRE_NOP(mnemonic)                                                      \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic "");                                         \
    dbg_strptr += strlen(dbg_strptr);                                          \
  }

#define POST_NOP ;

#define PRE_MEM(mnemonic)                                                      \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic " r%d, %04xH(r%d)", REG_1 & 31,              \
            (u32)DISP_16, REG_2 & 31);                                         \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, ": (%" PRIx64 ")", state.r[REG_2]);                     \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

#define POST_MEM POST_X64(state.r[REG_1]);

#define PRE_R12_R3(mnemonic)                                                   \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic " r%d, ", REG_1 & 31);                       \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (ins & 0x1000)                                                          \
      sprintf(dbg_strptr, "%02" PRIx64 "H", V_2);                              \
    else                                                                       \
      sprintf(dbg_strptr, "r%d", REG_2 & 31);                                  \
    dbg_strptr += strlen(dbg_strptr);                                          \
    sprintf(dbg_strptr, ", r%d", REG_3 & 31);                                  \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, ": (%" PRIx64 ",%" PRIx64 ")", state.r[REG_1], V_2);       \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

#define POST_R12_R3 POST_X64(state.r[REG_3]);

// Pre-debugging macro for floating-point instructions that have Fa and Fb

// as input and Fc as output.
#define PRE_F12_F3(mnemonic)                                                   \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic " f%d, f%d, f%d", FREG_1, FREG_2, FREG_3);   \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, ": (%" PRIx64 ",%" PRIx64 ")", state.f[FREG_1],            \
              state.f[FREG_2]);                                                \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

// Post-debugging macro for floating-point instructions that have Fa and Fb
// as input and Fc as output.
#define POST_F12_F3 POST_X64(state.f[FREG_3]);

#define PRE_R1_F3(mnemonic)                                                    \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic " r%d, f%d ", REG_1 & 31, FREG_3);           \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, ": (%" PRIx64 ")", state.r[REG_1]);                     \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

#define POST_R1_F3 POST_X64(state.f[FREG_3]);

#define PRE_F1_R3(mnemonic)                                                    \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic " f%d, r%d ", FREG_1, REG_3 & 31);           \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, ": (%" PRIx64 ")", state.f[FREG_1]);                    \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

#define POST_F1_R3 POST_X64(state.r[REG_3]);

#define PRE_X_F1(mnemonic)                                                     \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic " f%d ", FREG_1 & 31);                       \
    dbg_strptr += strlen(dbg_strptr);                                          \
  }

#define POST_X_F1 POST_X64(state.f[FREG_1]);

#define PRE_R2_R3(mnemonic)                                                    \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic " ");                                        \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (ins & 0x1000)                                                          \
      sprintf(dbg_strptr, "%02" PRIx64 "H", V_2);                              \
    else                                                                       \
      sprintf(dbg_strptr, "r%d", REG_2 & 31);                                  \
    dbg_strptr += strlen(dbg_strptr);                                          \
    sprintf(dbg_strptr, ", r%d", REG_3 & 31);                                  \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, ": (%" PRIx64 ")", V_2);                                \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

#define POST_R2_R3 POST_X64(state.r[REG_3]);

#define PRE_X_R1(mnemonic)                                                     \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic " r%d", REG_1 & 31);                         \
    dbg_strptr += strlen(dbg_strptr);                                          \
  }

#define POST_X_R1 POST_X64(state.r[REG_1]);

#define PRE_X_R3(mnemonic)                                                     \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic " r%d", REG_3 & 31);                         \
    dbg_strptr += strlen(dbg_strptr);                                          \
  }

#define POST_X_R3 POST_X64(state.r[REG_3]);

#define PRE_HW_LD(mnemonic)                                                    \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic);                                            \
    dbg_strptr += strlen(dbg_strptr);                                          \
    switch (function & ~1) {                                                   \
    case 0:                                                                    \
      sprintf(dbg_strptr, "/Phys");                                            \
      break;                                                                   \
    case 2:                                                                    \
      sprintf(dbg_strptr, "/Phys/Lock");                                       \
      break;                                                                   \
    case 4:                                                                    \
      sprintf(dbg_strptr, "/Vpte");                                            \
      break;                                                                   \
    case 10:                                                                   \
      sprintf(dbg_strptr, "/Chk");                                             \
      break;                                                                   \
    case 12:                                                                   \
      sprintf(dbg_strptr, "/Alt");                                             \
      break;                                                                   \
    case 14:                                                                   \
      sprintf(dbg_strptr, "/Alt/Chk");                                         \
      break;                                                                   \
    }                                                                          \
    dbg_strptr += strlen(dbg_strptr);                                          \
    sprintf(dbg_strptr, " r%d, %04xH(r%d)", REG_1 & 31, (u32)DISP_12,          \
            REG_2 & 31);                                                       \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, ": (%" PRIx64 ")", state.r[REG_2]);                     \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

#define POST_HW_LD POST_X64(state.r[REG_1]);

#define PRE_HW_ST(mnemonic)                                                    \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic);                                            \
    dbg_strptr += strlen(dbg_strptr);                                          \
    switch (function & ~1) {                                                   \
    case 0:                                                                    \
      sprintf(dbg_strptr, "/Phys");                                            \
      break;                                                                   \
    case 2:                                                                    \
      sprintf(dbg_strptr, "/Phys/Cond");                                       \
      break;                                                                   \
    case 12:                                                                   \
      sprintf(dbg_strptr, "/Alt");                                             \
      break;                                                                   \
    }                                                                          \
    dbg_strptr += strlen(dbg_strptr);                                          \
    sprintf(dbg_strptr, " r%d, %04xH(r%d)", REG_1 & 31, (u32)DISP_12,          \
            REG_2 & 31);                                                       \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, ": (%" PRIx64 ")", state.r[REG_2]);                     \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

#define POST_HW_ST POST_X64(state.r[REG_1]);

// Pre-debugging macro for floating-point load/store instructions.
#define PRE_FMEM(mnemonic)                                                     \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic " f%d, %04xH(r%d)", FREG_1, (u32)DISP_16,    \
            REG_2 & 31);                                                       \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, ": (%" PRIx64 ")", state.r[REG_2]);                     \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

// Post-debugging macro for floating-point load/store instructions.
#define POST_FMEM POST_X64(state.f[FREG_1]);

// Pre-debugging macro for floating-point instructions that have Fb as input

// And Fc as output.
#define PRE_F2_F3(mnemonic)                                                    \
  if (bDisassemble) {                                                          \
    DEBUG_XX;                                                                  \
    sprintf(dbg_strptr, #mnemonic " f%d, f%d", FREG_2, FREG_3);                \
    dbg_strptr += strlen(dbg_strptr);                                          \
    if (!bListing) {                                                           \
      sprintf(dbg_strptr, ": (%" PRIx64 ")", state.f[FREG_2]);                    \
      dbg_strptr += strlen(dbg_strptr);                                        \
    }                                                                          \
  }

// Post-debugging macro for floating-point instructions that have Fb as input
// And Fc as output.
#define POST_F2_F3 POST_X64(state.f[FREG_3]);

#else
#ifndef NDEBUG
#define UNKNOWN1                                                               \
  printf("Unknown opcode: %02x   \n", opcode);                                 \
  return;

#define UNKNOWN2                                                               \
  printf("Unknown opcode: %02x.%02x   \n", opcode, function);                  \
  return;
#else
#define UNKNOWN1 return;
#define UNKNOWN2 return;
#endif
#endif
#if defined(IDB)

// Debugging version of the OP macro:
// Execute the DO_<mnemonic> macro for an instruction, along with disassembling

// the instruction if needed.
#define OP(mnemonic, format)                                                   \
  PRE_##format(mnemonic);                                                      \
  if (!bListing) {                                                             \
    DO_##mnemonic;                                                             \
  }                                                                            \
  POST_##format;                                                               \
  handle_debug_string(dbg_string);                                             \
  return;

// Execute a function rather than a DO_<mnemonic> macro for an instruction
#define OP_FNC(mnemonic, format)                                               \
  PRE_##format(mnemonic);                                                      \
  if (!bListing) {                                                             \
    mnemonic();                                                                \
  }                                                                            \
  POST_##format;                                                               \
  handle_debug_string(dbg_string);                                             \
  return;

#else // defined(IDB)

// Non-debugging version of the OP macro:

// Execute the DO_<mnemonic> macro for an instruction.
#define OP(mnemonic, format)                                                   \
  DO_##mnemonic;                                                               \
  return;

// Execute a function rather than a DO_<mnemonic> macro for an instruction
#define OP_FNC(mnemonic, format)                                               \
  mnemonic();                                                                  \
  return;
#endif // defined(IDB)
