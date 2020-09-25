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

/**
 * \file
 * Contains the definitions for the emulated Serial Port devices.
 *
 * $Id: Serial.h,v 1.21 2008/05/31 15:47:13 iamcamiel Exp $
 *
 * X-1.21       Camiel Vanderhoeven                             31-MAY-2008
 *      Changes to include parts of Poco.
 *
 * X-1.20       Camiel Vanderhoeven                             26-MAR-2008
 *      Fix compiler warnings.
 *
 * X-1.19       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.18       Camiel Vanderhoeven                             13-MAR-2008
 *      Create init(), start_threads() and stop_threads() functions.
 *
 * X-1.17       Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.15       Brian Wheeler                                   29-FEB-2008
 *      Restart serial port connection if lost.
 *
 * X-1.14       Camiel Vanderhoeven                             06-JAN-2008
 *      Proper interrupt handling.
 *
 * X-1.13       Camiel Vanderhoeven                             02-JAN-2008
 *      Comments.
 *
 * X-1.12       Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.11       Camiel Vanderhoeven                             17-DEC-2007
 *      SaveState file format 2.1
 *
 * X-1.10       Camiel Vanderhoeven                             10-DEC-2007
 *      Use configurator.
 *
 * X-1.9        Camiel Vanderhoeven                             30-MAR-2007
 *      Added old changelog comments.
 *
 * X-1.8        Camiel Vanderhoeven                             28-FEB-2007
 *      Added throughSocket to support lockstep debugging.
 *
 * X-1.7        Camiel Vanderhoeven                             16-FEB-2007
 *   a) Changed header guards.
 *   b) DoClock is also used in Win32 environments.
 *
 * X-1.6        Camiel Vanderhoeven                             12-FEB-2007
 *      Added comments.
 *
 * X-1.5        Camiel Vanderhoeven                             9-FEB-2007
 *      Added comments.
 *
 * X-1.4        Camiel Vanderhoeven                             3-FEB-2007
 *      Added bInitialized to support eating the initialization bytes from
 *      the Telnet client.
 *
 * X-1.3        Brian Wheeler                                   3-FEB-2007
 *      Formatting.
 *
 * X-1.2        Brian Wheeler                                   3-FEB-2007
 *      Added Linux support.
 *
 * X-1.1        Camiel Vanderhoeven                             19-JAN-2007
 *      Initial version in CVS.
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#if !defined(INCLUDED_SERIAL_H)
#define INCLUDED_SERIAL_H

#include "SystemComponent.h"
#include "telnet.h"

/**
 * \brief Emulated serial port.
 *
 * The serial port is translated to a telnet port.
 **/
class CSerial : public CSystemComponent, public CRunnable {
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
  virtual void run();
  void execute();

  virtual void init();
  virtual void start_threads();
  virtual void stop_threads();

private:
  void serial_menu();

  CThread *myThread;
  bool StopThread;
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
