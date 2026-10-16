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
 * Contains the definitions for the emulated Serial Port devices.
 **/
#if !defined(INCLUDED_SERIAL_H)
#define INCLUDED_SERIAL_H

#include "SystemComponent.hpp"
#include "telnet.hpp"

#define STAGE_SIZE 8192

/**
 * \brief Emulated serial port.
 *
 * The serial port is translated to a telnet port.
 **/
class CSerial : public CSystemComponent {
public:
  void write(const char *s, int dsize);
  void write_cstr(const char *s);
  virtual void WriteMem(int index, u64 address, int dsize, u64 data);
  virtual u64 ReadMem(int index, u64 address, int dsize);
  CSerial(CConfigurator *cfg, CSystem *c, u16 number);
  virtual ~CSerial();
  int receive(const char *data, int dsize);
  virtual void check_state();
  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);
  void eval_interrupts();
  void WaitForConnection();
  virtual void run();
  void execute();

  virtual void init();
  virtual void start_threads();
  virtual void stop_threads();

private:
  void serial_menu();
  void drain_staging();
  void drop_client();

  std::unique_ptr<std::thread> myThread;
  std::atomic_bool myThreadDead{false};
  bool StopThread = false;
  bool breakHit = false;
  const char *listenAddress = nullptr;

  /// Set by write() when a send() to the client has failed, and consumed by
  /// the I/O thread in execute().  The guest transmits from the CPU thread and
  /// the socket is owned by this device's I/O thread, so the failure is
  /// recorded here rather than acted on where it is detected: closing the
  /// descriptor from the CPU thread would race the thread that is reading it.
  std::atomic_bool txBroken{false};

  unsigned char
      iac_carry[8];  // Partial telnet sequence carried across recv() calls
  int iac_carry_len; // Number of valid bytes in iac_carry
  bool in_subneg;    // Currently inside IAC SB ... IAC SE subnegotiation

  char stageBuf[STAGE_SIZE]; // Cooked data waiting for baud-rate delivery
  int stageLen;              // Number of valid bytes in stageBuf

  /// The state structure contains all elements that need to be saved to the
  /// statefile.
  struct SSrl_state {
    u8 bTHR; /**< Transmit Hold Register */
    u8 bRDR; /**< Received Data Register */
    u8 bBRB_LSB;
    u8 bBRB_MSB;
    u8 bIER; /**< Interrupt Enable Register */
    u8 bIIR; /**< Interrupt Identification Register */
    u8 bFCR;
    u8 bLCR; /**< Line Control Register (Data Format Register) */
    u8 bMCR; /**< Modem Control Register */
    u8 bLSR; /**< Line Status Register */
    u8 bMSR; /**< Modem Status Register */
    u8 bSPR; /**< Scratch Pad Register */
    int serial_cycles;
    char rcvBuffer[1024];
    int rcvW;
    int rcvR;
    int iNumber;
    bool thre_pending; /**< THRE interrupt latched (THR emptied, not yet acked
                          by an IIR read) */
  } state;
  int listenPort;
  int64_t listenSocket;
  int64_t connectSocket;
  bool disabled =
      false; ///< If true, port is not exposed to guest; reads return 0xff,
             ///< writes ignored. Used to skip KDCOM probe on AXP64 2210 etc.
  bool raw_mode =
      false; ///< If true, skip telnet IAC processing and connect banner. Use
             ///< for windbg/kgdb where the byte stream must be 8-bit clean.
  bool null_attach =
      false; ///< If true, port exists on the bus but no socket is opened and no
             ///< I/O thread runs. Guest sees a healthy idle 16550 (THRE/TSRE,
             ///< CTS/DSR); TX bytes are silently dropped; RX FIFO is
             ///< permanently empty. MCR.LOOP self-test still works (no socket
             ///< touched). Use when the guest expects a UART to exist but you
             ///< don't want a telnet listener — bit-bucket semantics, like
             ///< QEMU's -serial null.
#if defined(IDB) && defined(LS_MASTER)
  int throughSocket;
#endif
};
#endif // !defined(INCLUDED_SERIAL_H)
