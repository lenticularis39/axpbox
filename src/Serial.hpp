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

#if !defined(INCLUDED_SERIAL_H)
#define INCLUDED_SERIAL_H

#include "SystemComponent.hpp"
#include "telnet.hpp"

/**
 * \brief Emulated serial port.
 *
 * The serial port is translated to a telnet port.
 **/
class CSerial : public CSystemComponent {
public:
  void write(const char *s);
  virtual void WriteMem(int index, u64 address, int dsize, u64 data);
  virtual u64 ReadMem(int index, u64 address, int dsize);
  CSerial(CConfigurator *cfg, CSystem *c, u16 number);
  virtual ~CSerial();
  void receive(const char *data);
  virtual void check_state();
  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);
  void eval_interrupts();
  void WaitForConnection();
  void run();
  void execute();

  virtual void init();
  virtual void start_threads();
  virtual void stop_threads();

private:
  void serial_menu();
  std::unique_ptr<std::thread> myThread;
  std::atomic_bool myThreadDead{false};
  bool StopThread = false;
  bool acceptingSocket = false;
  bool breakHit;

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
    bool irq_active;
  } state;
  int listenPort;
  const char *listenAddress;
  int listenSocket;
  int connectSocket;
#if defined(IDB) && defined(LS_MASTER)
  int throughSocket;
#endif
};
#endif // !defined(INCLUDED_SERIAL_H)
