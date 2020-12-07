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
 * Contains definitions for the SCSI bus class.
 *
 * $Id: SCSIBus.h,v 1.3 2008/03/14 15:30:52 iamcamiel Exp $
 *
 * X-1.2        Camiel Vanderhoeven                             20-JAN-2008
 *      Avoid compiler warnings.
 *
 * X-1.1        Camiel Vanderhoeven                             12-JAN-2008
 *      Initial version in CVS.
 **/
#if !defined(__SCSIBUS__H__)
#define __SCSIBUS__H__

#include "SCSIDevice.hpp"
#include "StdAfx.hpp"
#include "SystemComponent.hpp"

/**
 * \brief Emulated SCSI bus.
 *
 * connects SCSI Devices (class CSCSIDevice) together; SCSI devices are
 * disks and controllers.
 **/
class CSCSIBus : public CSystemComponent {
public:
  CSCSIBus(CConfigurator *cfg, CSystem *c);
  ~CSCSIBus(void);
  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);

  void scsi_register(CSCSIDevice *dev, int bus, int target);
  void scsi_unregister(CSCSIDevice *dev, int target);

  bool arbitrate(int initiator);
  bool select(int initiator, int target);
  void set_phase(int target, int phase);
  int get_phase() { return state.phase; };

  /**< Get current SCSI bus phase **/
  void free_bus(int initiator);

  CSCSIDevice *targets[16]; /**< pointers to the SCSI devices that respond to
                               the 15 possible target id's. **/
  int target_bus_no[16]; /**< indicates what bus this is for each connected SCSI
                     device. always 0 for disks, but controllers could have
                     multiple SCSI busses. **/

  /// The state structure contains all elements that need to be saved to the
  /// statefile
  struct SSCSI_state {
    int initiator; /**< SCSI id of the initiator. **/
    int target;    /**< SCSI id of the target. **/
    int phase;     /**< SCSI bus phase. **/
  } state;
};

#define SCSI_PHASE_FREE -2
#define SCSI_PHASE_ARBITRATION -1
#define SCSI_PHASE_DATA_OUT 0
#define SCSI_PHASE_DATA_IN 1
#define SCSI_PHASE_COMMAND 2
#define SCSI_PHASE_STATUS 3
#define SCSI_PHASE_MSG_OUT 6
#define SCSI_PHASE_MSG_IN 7
#endif
