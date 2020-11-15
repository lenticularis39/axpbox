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
 * Contains the definitions for the emulated DMA controller.
 *
 * $Id: DMA.h,v 1.6 2008/04/29 09:26:53 iamcamiel Exp $
 *
 * X-1.5        Camiel Vanderhoeven                             29-APR-2008
 *      Removed unused reference to floppy disk image.
 *
 * X-1.4        Brian Wheeler                                   29-APR-2008
 *      Fixed floppy disk implementation.
 *
 * X-1.3        Brian Wheeler                                   18-APR-2008
 *      Rewrote DMA code to make it ready for floppy support.
 *
 * X-1.2        Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.1        Camiel Vanderhoeven                             26-FEB-2008
 *      Created. Contains code previously found in AliM1543C.h
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#if !defined(INCLUDED_DMA_H)
#define INCLUDED_DMA_H

#include "SystemComponent.hpp"

/**
 * \brief Emulated DMA controller.
 **/

class CDMA : public CSystemComponent {
public:
  CDMA(CConfigurator *cfg, CSystem *c);
  virtual ~CDMA();

  virtual int DoClock();
  virtual void WriteMem(int index, u64 address, int dsize, u64 data);
  virtual u64 ReadMem(int index, u64 address, int dsize);
  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);

  void set_request(int index, int channel, int data);
  void send_data(int channel, void *data);
  void recv_data(int channel, void *data);
  int get_count(int channel) { return state.channel[channel].count; };

private:
  void do_dma();

  /// The state structure contains all elements that need to be saved to the
  /// statefile.
  struct SDMA_state {
    /// DMA channel state
    struct SDMA_chan {
      bool a_lobyte; // address lobyte expected
      bool c_lobyte; // count lobyte expected
      u16 current;
      u16 base;
      u16 pagebase;
      u16 count;
      u8 mode;
    } channel[8];

    /// DMA controller state
    struct SDMA_ctrl {
      u8 status;
      u8 command;
      u8 request;
      u8 mask;
    } controller[2];
  } state;
};

#define DMA_IO_BASE 0x1000
#define DMA0_IO_MAIN DMA_IO_BASE + 0
#define DMA1_IO_MAIN DMA_IO_BASE + 1
#define DMA_IO_LPAGE DMA_IO_BASE + 2
#define DMA_IO_HPAGE DMA_IO_BASE + 3
#define DMA0_IO_CHANNEL DMA_IO_BASE + 4
#define DMA1_IO_CHANNEL DMA_IO_BASE + 5
#define DMA0_IO_EXT DMA_IO_BASE + 6
#define DMA1_IO_EXT DMA_IO_BASE + 7

extern CDMA *theDMA;

#endif // !defined(INCLUDED_DMA_H)
