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

#include "SCSIDevice.hpp"
#include "SCSIBus.hpp"

/**
 * \brief Constructor.
 *
 * Set this device as unregistered with any SCSI bus.
 **/
CSCSIDevice::CSCSIDevice(void) {
  int i;
  for (i = 0; i < 10; i++) {
    scsi_bus[i] = 0;
    scsi_initiator_id[i] = -1;
  }
}

/**
 * \brief Destructor.
 **/
CSCSIDevice::~CSCSIDevice(void) {}

/**
 * \brief Register this device with a SCSI bus.
 **/
void CSCSIDevice::scsi_register(int busno, class CSCSIBus *with, int target) {
  scsi_bus[busno] = with;
  scsi_initiator_id[busno] = target;
  scsi_bus[busno]->scsi_register(this, busno, target);
}

/**
 * \brief Arbitrate for the SCSI bus.
 *
 * See CSCSIBus::arbitrate for a description.
 **/
bool CSCSIDevice::scsi_arbitrate(int bus) {
  return scsi_bus[bus]->arbitrate(scsi_initiator_id[bus]);
}

/**
 * \brief Select a target on the SCSI bus.
 *
 * See CSCSIBus::select for a description.
 **/
bool CSCSIDevice::scsi_select(int bus, int target) {
  return scsi_bus[bus]->select(scsi_initiator_id[bus], target);
}

/**
 * \brief Called when this device is selected.
 *
 * Override this to allow this device to be selected. Overrided
 * functions should at least call scsi_set_phase to set
 * the SCSI bus phase to a valid phase.
 **/
void CSCSIDevice::scsi_select_me(int bus) {
  FAILURE(NotImplemented, "selected device doesn't implement scsi_select_me");
}

/**
 * \brief Change the SCSI bus phase.
 *
 * See CSCSIBus::set_phase for a description.
 **/
void CSCSIDevice::scsi_set_phase(int bus, int phase) {
  scsi_bus[bus]->set_phase(scsi_initiator_id[bus], phase);
}

/**
 * \brief Get the current SCSI bus phase.
 *
 * See CSCSIBus::get_phase for a description.
 **/
int CSCSIDevice::scsi_get_phase(int bus) { return scsi_bus[bus]->get_phase(); }

/**
 * \brief Release the SCSI bus.
 *
 * See CSCSIBus::free_bus for a description.
 **/
void CSCSIDevice::scsi_free(int bus) {
  return scsi_bus[bus]->free_bus(scsi_initiator_id[bus]);
}

/**
 * \brief Return the number of bytes expected or available.
 *
 * Override this to return the number of bytes the device
 * expects to receive from the initiator, or has available
 * for the initiator, in the current SCSI phase.
 *
 * For an overview of data transfer during a SCSI bus phase,
 * see SCSIDevice::scsi_xfer_ptr.
 **/
size_t CSCSIDevice::scsi_expected_xfer_me(int bus) {
  FAILURE(NotImplemented,
          "selected device doesn't implement scsi_expected_xfer_me");
}

/**
 * \brief Return the number of bytes expected or available.
 *
 * Returns the number of bytes the currently selected target
 * expects to receive from the initiator, or has available
 * for the initiator, in the current SCSI phase.
 *
 * For an overview of data transfer during a SCSI bus phase,
 * see SCSIDevice::scsi_xfer_ptr.
 **/
size_t CSCSIDevice::scsi_expected_xfer(int bus) {
  return scsi_bus[bus]
      ->targets[scsi_bus[bus]->state.target]
      ->scsi_expected_xfer_me(
          scsi_bus[bus]->target_bus_no[scsi_bus[bus]->state.target]);
}

/**
 * \brief Return a pointer where the initiator can read or write data.
 *
 * Override this to return a pointer to where the initiator
 * can read or write data in the current SCSI phase.
 *
 * For an overview of data transfer during a SCSI bus phase,
 * see SCSIDevice::scsi_xfer_ptr.
 **/
void *CSCSIDevice::scsi_xfer_ptr_me(int bus, size_t bytes) {
  FAILURE(NotImplemented, "selected device doesn't implement scsi_xfer_ptr_me");
}

/**
 * \brief Return a pointer where target data can be read or written.
 *
 * Returns a pointer to where the initiator can read or
 * write data from/to the currentlt selected target in
 * the current SCSI phase.
 *
 * The initiator should do the following for each transfer:
 *   - Check the current phase using CSCSIDevice::scsi_get_phase.
 *   - Check the amount of data to transfer using
 *     CSCSIDevice::scsi_expected_xfer.
 *   - Signal intent to transfer data, and obtain a pointer
 *     using SCSIDevice::scsi_xfer_ptr.
 *   - Transfer data using the returned pointer.
 *   - Call CSCSIDevice::scsi_xfer_done to the the target
 *     process the data and/or transfer to a new phase.
 *   .
 **/
void *CSCSIDevice::scsi_xfer_ptr(int bus, size_t bytes) {
  return scsi_bus[bus]->targets[scsi_bus[bus]->state.target]->scsi_xfer_ptr_me(
      scsi_bus[bus]->target_bus_no[scsi_bus[bus]->state.target], bytes);
}

/**
 * \brief Process data written or read.
 *
 * Override this to process data read or written in
 * the current SCSI phase.
 *
 * For an overview of data transfer during a SCSI bus phase,
 * see SCSIDevice::scsi_xfer_ptr.
 **/
void CSCSIDevice::scsi_xfer_done_me(int bus) {
  FAILURE(NotImplemented,
          "selected device doesn't implement scsi_xfer_done_me");
}

/**
 * \brief Mark data transfer done.
 *
 * Let the target know that data has been transfered for the
 * current SCSI phase.
 *
 * For an overview of data transfer during a SCSI bus phase,
 * see SCSIDevice::scsi_xfer_ptr.
 **/
void CSCSIDevice::scsi_xfer_done(int bus) {
  scsi_bus[bus]->targets[scsi_bus[bus]->state.target]->scsi_xfer_done_me(
      scsi_bus[bus]->target_bus_no[scsi_bus[bus]->state.target]);
}
