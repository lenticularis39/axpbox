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

#if !defined(__SCSIDEVICE__H__)
#define __SCSIDEVICE__H__

#include "StdAfx.hpp"

/**
 * \brief Base class for emulated SCSI devices.
 *
 * Connects to one or more SCSI busses (class CSCSIBus) together; derived
 * SCSI devices are disks and controllers.
 **/
class CSCSIDevice {
public:
  CSCSIDevice(void);
  virtual ~CSCSIDevice(void);

  void scsi_register(int busno, class CSCSIBus *with, int target);

  bool scsi_arbitrate(int bus);
  bool scsi_select(int bus, int target);
  virtual void scsi_select_me(int bus);
  void scsi_set_phase(int bus, int phase);
  int scsi_get_phase(int bus);
  void scsi_free(int bus);

  virtual size_t scsi_expected_xfer_me(int bus);
  size_t scsi_expected_xfer(int bus);

  virtual void *scsi_xfer_ptr_me(int bus, size_t bytes);
  void *scsi_xfer_ptr(int bus, size_t bytes);

  virtual void scsi_xfer_done_me(int bus);
  void scsi_xfer_done(int bus);

protected:
  class CSCSIBus *scsi_bus[10]; /**< SCSI busses this device connects to. Disks
                        connect to 1 bus only, controllers can have
                        several SCSI busses. **/
  int scsi_initiator_id[10];    /**< Main SCSI id of this device on each of the
                                   busses. **/
};
#endif
