/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * WWW    : http://sourceforge.net/projects/es40
 * E-mail : camiel@camicom.com
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
 * $Id: SCSIBus.cpp,v 1.5 2008/03/14 15:30:51 iamcamiel Exp $
 *
 * X-1.4        Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.3        Brian Wheeler                                   27-FEB-2008
 *      Avoid compiler warnings.
 *
 * X-1.2        Camiel Vanderhoeven                             16-FEB-2008
 *      Owner of the SCSI bus is allowed to re-arbitrate for it.
 *
 * X-1.1        Camiel Vanderhoeven                             12-JAN-2008
 *      Initial version in CVS.
 **/
#include "SCSIBus.h"
#include "StdAfx.h"

/**
 * \brief Constructor.
 *
 * Initialize all disks to non-existing.
 **/
CSCSIBus::CSCSIBus(CConfigurator *cfg, CSystem *c) : CSystemComponent(cfg, c) {
  int i;
  for (i = 0; i < 16; i++)
    targets[i] = 0;

  state.phase = SCSI_PHASE_FREE;
  state.target = -1;
  state.initiator = -1;
}

/**
 * \brief Destructor.
 **/
CSCSIBus::~CSCSIBus(void) {}

/**
 * \brief Register a SCSI device.
 *
 * Register a SCSI device as responding to a specific SCSI id.
 **/
void CSCSIBus::scsi_register(CSCSIDevice *dev, int bus, int target) {
  if (targets[target] && targets[target] != dev)
    FAILURE(IllegalState, "More than one SCSI device at the same ID");
  targets[target] = dev;
  target_bus_no[target] = bus;
}

/**
 * \brief Unregister a SCSI device.
 *
 * Unregister a SCSI device as responding to a specific SCSI id. Needed
 * for some controllers if software changes it's SCSI id.
 **/
void CSCSIBus::scsi_unregister(CSCSIDevice *dev, int target) {
  if (targets[target] != dev)
    FAILURE(IllegalState, "Attempt to unregister other SCSI device");
  targets[target] = 0;
}

/**
 * \brief Arbitrate for the SCSI bus.
 *
 * Returns true on succesful arbitration.
 * Arbitration succeeds if the bus is free.
 **/
bool CSCSIBus::arbitrate(int initiator) {
  if (state.phase != SCSI_PHASE_FREE && state.initiator != initiator) {
    printf("Could not arbitrate for the SCSI bus.\n");
    return false;
  }

  state.initiator = initiator;
  state.phase = SCSI_PHASE_ARBITRATION;
  return true;
}

/**
 * \brief Select a device as a target.
 *
 * Returns true on succesful selection (= if the device responds to the select
 * by changing the SCSI phase.
 * The initiator calling this function should have succesfully arbitrated
 * for the SCSI bus first.
 **/
bool CSCSIBus::select(int initiator, int target) {
  if (state.phase != SCSI_PHASE_ARBITRATION || state.initiator != initiator)
    FAILURE(IllegalState,
            "Attempt to select while the device has not won SCSI arbitration");

  if (!targets[target])
    return false;

  state.target = target;
  targets[target]->scsi_select_me(target_bus_no[target]);
  return (state.phase >= 0);
}

/**
 * \brief Change the SCSI phase.
 *
 * Only the selected target can do this.
 **/
void CSCSIBus::set_phase(int target, int phase) {
  if (targets[target] != targets[state.target])
    FAILURE(IllegalState,
            "Attempt to set phase while the device has not been selected");

  state.phase = phase;
}

/**
 * \brief Release the SCSI bus.
 *
 * In the Arbitration phase, only the initiator can do this; otherwise,
 * only the selected target can do this.
 **/
void CSCSIBus::free_bus(int initiator) {

  // nothing to do
  if (state.phase == SCSI_PHASE_FREE)
    return;

  if (state.phase == SCSI_PHASE_ARBITRATION) {
    if (initiator != state.initiator)
      FAILURE(IllegalState, "Attempt to free the scsi bus");
  } else {
    if (targets[initiator] != targets[state.target])
      FAILURE(IllegalState, "Attempt to free the scsi bus");
  }

  state.phase = SCSI_PHASE_FREE;
}

static u32 scsi_magic1 = 0x5C510123;
static u32 scsi_magic2 = 0x32105c51;

/**
 * Save state to a Virtual Machine State file.
 **/
int CSCSIBus::SaveState(FILE *f) {
  long ss = sizeof(state);

  fwrite(&scsi_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&scsi_magic2, sizeof(u32), 1, f);
  printf("%s: %d bytes saved.\n", devid_string, (int)ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CSCSIBus::RestoreState(FILE *f) {
  long ss;
  u32 m1;
  u32 m2;
  size_t r;

  r = fread(&m1, sizeof(u32), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (m1 != scsi_magic1) {
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

  if (m2 != scsi_magic2) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  printf("%s: %d bytes restored.\n", devid_string, (int)ss);
  return 0;
}
