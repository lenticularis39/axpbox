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
 * Contains code for the disk base class.
 *
 * $Id: Disk.cpp,v 1.31 2008/04/02 13:28:29 iamcamiel Exp $
 *
 * X-1.31       Camiel Vanderhoeven                             02-APR-2008
 *      Fixed compiler warnings.
 *
 * X-1.30       Camiel Vanderhoeven                             16-MAR-2008
 *      Always reset dati.read when setting dati.available.
 *
 * X-1.29       Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.28       Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.27       Brian Wheeler                                   27-FEB-2008
 *      Avoid compiler warnings.
 *
 * X-1.26       David Leonard                                   20-FEB-2008
 *      Return SYSTEM RESOURCE FAILURE sense if dato/dati buffer size is
 *      exceeded.
 *
 * X-1.25       Brian Wheeler                                   20-FEB-2008
 *      Support MSF in READ TOC scsi command.
 *
 * X-1.24       Brian Wheeler                                   18-FEB-2008
 *      Added vital product data page 0 (Required for Tru64).
 *
 * X-1.23       Camiel Vanderhoeven                             18-FEB-2008
 *      Removed support for CD-R/RW specific SCSI commands.
 *
 * X-1.22       Camiel Vanderhoeven                             18-FEB-2008
 *      READ CAPACITY returns the number of the last LBA (n-1); not the
 *      number of LBA's (n).
 *
 * X-1.20       Camiel Vanderhoeven                             17-FEB-2008
 *      Set up sense data when error occurs.
 *
 * X-1.19       Camiel Vanderhoeven                             17-FEB-2008
 *      Added REQUEST_SENSE scsi command.
 *
 * X-1.18       Camiel Vanderhoeven                             16-FEB-2008
 *      Added READ_LONG scsi command, and support for MODE_SENSE changeable
 *      parameter pages.
 *
 * X-1.17       Camiel Vanderhoeven                             21-JAN-2008
 *      OpenVMs doesn't like max 255 sectors.
 *
 * X-1.16       Camiel Vanderhoeven                             16-JAN-2008
 *      Less messages without debugging enabled.
 *
 * X-1.15       Brian Wheeler                                   14-JAN-2008
 *      Corrected output from read track info and read toc.
 *
 * X-1.14       Brian Wheeler/Camiel Vanderhoeven               14-JAN-2008
 *      Added read_track_info command, completed read_toc command.
 *
 * X-1.13       Camiel Vanderhoeven                             13-JAN-2008
 *      Determine best-fitting C/H/S lay-out.
 *
 * X-1.11       Brian Wheeler                                   13-JAN-2008
 *      More CD-ROM commands supported.
 *
 * X-1.10       Camiel Vanderhoeven                             12-JAN-2008
 *      Include SCSI engine, because this is common to both SCSI and ATAPI
 *      devices.
 *
 * X-1.9        Camiel Vanderhoeven                             09-JAN-2008
 *      Save disk state to state file.
 *
 * X-1.8        Camiel Vanderhoeven                             06-JAN-2008
 *      Set default blocksize to 2048 for cd-rom devices.
 *
 * X-1.7        Camiel Vanderhoeven                             06-JAN-2008
 *      Support changing the block size (required for SCSI, ATAPI).
 *
 * X-1.6        Camiel Vanderhoeven                             02-JAN-2008
 *      Cleanup.
 *
 * X-1.5        Camiel Vanderhoeven                             29-DEC-2007
 *      Fix memory-leak.
 *
 * X-1.4        Camiel Vanderhoeven                             28-DEC-2007
 *      Throw exceptions rather than just exiting when errors occur.
 *
 * X-1.3        Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.2        Brian Wheeler                                   16-DEC-2007
 *      Fixed case of StdAfx.h.
 *
 * X-1.1        Camiel Vanderhoeven                             12-DEC-2007
 *      Initial version in CVS.
 **/
#include "Disk.h"
#include "StdAfx.h"

/**
 * \brief Constructor.
 **/
CDisk::CDisk(CConfigurator *cfg, CSystem *sys, CDiskController *ctrl,
             int idebus, int idedev)
    : CSystemComponent(cfg, sys) {
  char *a;
  char *b;
  char *c;
  char *d;

  myCfg = cfg;
  myCtrl = ctrl;
  myBus = idebus;
  myDev = idedev;
  atapi_mode = false;

  a = myCfg->get_myName();
  b = myCfg->get_myValue();
  c = myCfg->get_myParent()->get_myName();
  d = myCfg->get_myParent()->get_myValue();

  free(devid_string); // we override the default to include the controller.
  CHECK_ALLOCATION(devid_string = (char *)malloc(strlen(a) + strlen(b) +
                                                 strlen(c) + strlen(d) + 6));
  sprintf(devid_string, "%s(%s).%s(%s)", c, d, a, b);

  serial_number = myCfg->get_text_value("serial_num", "ES40EM00000");
  revision_number = myCfg->get_text_value("rev_num", "0.0");
  read_only = myCfg->get_bool_value("read_only");
  is_cdrom = myCfg->get_bool_value("cdrom");

  state.block_size = is_cdrom ? 2048 : 512;
  state.scsi.sense.available = false;

  myCtrl->register_disk(this, myBus, myDev);
}

/**
 * \brief Destructor.
 **/
CDisk::~CDisk(void) {
  free(devid_string);
  devid_string = nullptr;
}

/**
 * \Calculate the number of cylinders to report.
 **/
void CDisk::calc_cylinders() {
  cylinders = byte_size / state.block_size / sectors / heads;

  off_t_large chs_size = sectors * cylinders * heads * state.block_size;
  if (chs_size < byte_size)
    cylinders++;
}

/**
 * \brief Called when this device is selected.
 *
 * Set status fields up to begin a new SCSI command sequence
 * and set the SCSI bus phase to Message Out.
 **/
void CDisk::scsi_select_me(int bus) {
  state.scsi.msgo.written = 0;
  state.scsi.msgi.available = 0;
  state.scsi.msgi.read = 0;
  state.scsi.cmd.written = 0;
  state.scsi.dati.available = 0;
  state.scsi.dati.read = 0;
  state.scsi.dato.expected = 0;
  state.scsi.dato.written = 0;
  state.scsi.stat.available = 0;
  state.scsi.stat.read = 0;
  state.scsi.lun_selected = false;

  // state.scsi.disconnect_priv = false;
  // state.scsi.will_disconnect = false;
  // state.scsi.disconnected = false;
  if (atapi_mode)
    scsi_set_phase(bus, SCSI_PHASE_COMMAND);
  else
    scsi_set_phase(bus, SCSI_PHASE_MSG_OUT);
}

static u32 disk_magic1 = 0xD15D15D1;
static u32 disk_magic2 = 0x15D15D5;

/**
 * Save state to a Virtual Machine State file.
 **/
int CDisk::SaveState(FILE *f) {
  long ss = sizeof(state);

  fwrite(&disk_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&disk_magic2, sizeof(u32), 1, f);
  printf("%s: %d bytes saved.\n", devid_string, (int)ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CDisk::RestoreState(FILE *f) {
  long ss;
  u32 m1;
  u32 m2;
  size_t r;

  r = fread(&m1, sizeof(u32), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (m1 != disk_magic1) {
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

  if (m2 != disk_magic2) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  // calc_cylinders(); // state.block_size may have changed.
  determine_layout();

  printf("%s: %d bytes restored.\n", devid_string, (int)ss);
  return 0;
}

/**
 * \brief Return the number of bytes expected or available.
 *
 * Return the number of bytes we still expect to receive
 * from the initiator, or still have available for the
 * initiator, in the current SCSI phase.
 *
 * For an overview of data transfer during a SCSI bus phase,
 * see SCSIDevice::scsi_xfer_ptr.
 **/
size_t CDisk::scsi_expected_xfer_me(int bus) {
  switch (scsi_get_phase(0)) {
  case SCSI_PHASE_DATA_OUT:
    return state.scsi.dato.expected - state.scsi.dato.written;

  case SCSI_PHASE_DATA_IN:
    return state.scsi.dati.available - state.scsi.dati.read;

  case SCSI_PHASE_COMMAND:
    return 256 - state.scsi.cmd.written;

  case SCSI_PHASE_STATUS:
    return state.scsi.stat.available - state.scsi.stat.read;

  case SCSI_PHASE_MSG_OUT:
    return 256 - state.scsi.msgo.written;

  case SCSI_PHASE_MSG_IN:
    return state.scsi.msgi.available - state.scsi.msgi.read;

  default:
    FAILURE_2(IllegalState, "%s: transfer requested in phase %d\n",
              devid_string, scsi_get_phase(0));
  }
}

/**
 * \brief Return a pointer where the initiator can read or write data.
 *
 * Return a pointer to where the initiator can read or write
 * (the remainder of) our data in the current SCSI phase.
 *
 * For an overview of data transfer during a SCSI bus phase,
 * see SCSIDevice::scsi_xfer_ptr.
 **/
void *CDisk::scsi_xfer_ptr_me(int bus, size_t bytes) {
  void *res = 0;

  switch (scsi_get_phase(0)) {
  case SCSI_PHASE_DATA_OUT:
    res = &(state.scsi.dato.data[state.scsi.dato.written]);
    state.scsi.dato.written += bytes;
    break;

  case SCSI_PHASE_DATA_IN:
    res = &(state.scsi.dati.data[state.scsi.dati.read]);
    state.scsi.dati.read += bytes;
    break;

  case SCSI_PHASE_COMMAND:
    res = &(state.scsi.cmd.data[state.scsi.cmd.written]);
    state.scsi.cmd.written += bytes;
    break;

  case SCSI_PHASE_STATUS:
    res = &(state.scsi.stat.data[state.scsi.stat.read]);
    state.scsi.stat.read += bytes;
    break;

  case SCSI_PHASE_MSG_OUT:
    res = &(state.scsi.msgo.data[state.scsi.msgo.written]);
    state.scsi.msgo.written += bytes;
    break;

  case SCSI_PHASE_MSG_IN:

    // if (PT.reselected)
    //{
    //  retval = 0x80; // identify
    //  break;
    //}
    // if (PT.disconnected)
    //{
    //  if (!PT.dati_ptr)
    //    retval = 0x04; // disconnect
    //  else
    //  {
    //    if (state.scsi.msgi.read==0)
    //    {
    //      retval = 0x02; // save data pointer
    //      state.scsi.msgi.read=1;
    //    }
    //    else if (state.scsi.msgi.read==1)
    //    {
    //      retval = 0x04; // disconnect
    //      state.scsi.msgi.read=0;
    //    }
    //  }
    //  break;
    //}
    res = &(state.scsi.msgi.data[state.scsi.msgi.read]);
    state.scsi.msgi.read += bytes;
    break;

  default:
    FAILURE_2(IllegalState, "%s: transfer requested in phase %d\n",
              devid_string, scsi_get_phase(0));
  }

  return res;
}

/**
 * \brief Process data written or read.
 *
 * Determine what action (if any) should be taken after a
 * transfer, and what the next SCSI bus phase should be.
 *
 * For an overview of data transfer during a SCSI bus phase,
 * see SCSIDevice::scsi_xfer_ptr.
 *
 * \todo Handle disconnect/reconnect properly.
 **/
void CDisk::scsi_xfer_done_me(int bus) {
  int res;
  int newphase = scsi_get_phase(0);

  switch (scsi_get_phase(0)) {
  case SCSI_PHASE_DATA_OUT:
    if (state.scsi.dato.written < state.scsi.dato.expected)
      break;

    res = do_scsi_command();
    if (res == 2)
      FAILURE(IllegalState, "do_command returned 2 after DATA OUT phase");

    if (state.scsi.dati.available)
      newphase = SCSI_PHASE_DATA_IN;
    else
      newphase = SCSI_PHASE_STATUS;
    break;

  case SCSI_PHASE_DATA_IN:
    if (state.scsi.dati.read < state.scsi.dati.available)
      break;

    newphase = SCSI_PHASE_STATUS;
    break;

  case SCSI_PHASE_COMMAND:
    res = do_scsi_command();
    if (res == 2)
      newphase = SCSI_PHASE_DATA_OUT;
    else if (state.scsi.dati.available)
      newphase = SCSI_PHASE_DATA_IN;
    else
      newphase = SCSI_PHASE_STATUS;
    break;

  case SCSI_PHASE_STATUS:
    if (state.scsi.stat.read < state.scsi.stat.available)
      break;

    if (atapi_mode) {
      scsi_free(0);
      return;
    }

    newphase = SCSI_PHASE_MSG_IN;
    break;

  case SCSI_PHASE_MSG_OUT:
    newphase = do_scsi_message(); // command
    break;

  case SCSI_PHASE_MSG_IN:

    // if (state.scsi.reselected)
    //{
    //  state.scsi.reselected = false;
    //  newphase = state.scsi.disconnect_phase;
    //}
    // else if (state.scsi.disconnected)
    //{
    //  if (!state.scsi.msgi.read)
    //    newphase = -1;
    //}
    if (state.scsi.msgi.read < state.scsi.msgi.available)
      break;

    if (state.scsi.cmd.written) {
      scsi_free(0);
      return;
    } else
      newphase = SCSI_PHASE_COMMAND;
    break;

  default:
    FAILURE_2(IllegalState, "%s: transfer requested in phase %d\n",
              devid_string, scsi_get_phase(0));
  }

  // if data in and can disconnect...
  // if (state.phase!=7 && newphase==1 && PT.will_disconnect &&
  // !PT.disconnected)
  //{
  //  printf("%s: Disconnecting now...\n",devid_string);
  //  PT.disconnected = true;
  //  PT.disconnect_phase = newphase;
  //  newphase = 7; // msg in
  //}
  if (newphase != scsi_get_phase(0)) {

    //    if (newphase==-1)
    //    {
    //      printf("%s: Disconnect. Timer started!\n",devid_string);
    //      // disconnect. generate interrupt?
    //      state.disconnected = 20;
    //    }
    scsi_set_phase(0, newphase);
  }

  // getchar();
}

// SCSI commands:
#define SCSICMD_TEST_UNIT_READY 0x00
#define SCSICMD_REQUEST_SENSE 0x03
#define SCSICMD_INQUIRY 0x12

#define SCSICMD_READ 0x08
#define SCSICMD_READ_10 0x28
#define SCSICMD_READ_12 0xA8
#define SCSICMD_READ_16 0x88
#define SCSICMD_READ_32 0x7F
#define SCSICMD_READ_LONG 0x3E
#define SCSICMD_READ_CD 0xBE

#define SCSICMD_WRITE 0x0A
#define SCSICMD_WRITE_10 0x2A
#define SCSICMD_WRITE_12 0xAA
#define SCSICMD_WRITE_LONG 0x3F

#define SCSICMD_MODE_SELECT 0x15
#define SCSICMD_MODE_SENSE 0x1a
#define SCSICMD_START_STOP_UNIT 0x1b
#define SCSICMD_PREVENT_ALLOW_REMOVE 0x1e
#define SCSICMD_MODE_SENSE_10 0x5a

#define SCSICMD_SYNCHRONIZE_CACHE 0x35

//  SCSI block device commands:
#define SCSIBLOCKCMD_READ_CAPACITY 0x25

//  SCSI CD-ROM commands:
#define SCSICDROM_READ_SUBCHANNEL 0x42
#define SCSICDROM_READ_TOC 0x43

// SCSI CD-R/RW commands:
#define SCSICDRRW_FORMAT 0x04
#define SCSICDRRW_READ_DISC_INFO 0x51
#define SCSICDRRW_READ_TRACK_INFO 0x52
#define SCSICDRRW_RESERVE_TRACK 0x53
#define SCSICDRRW_SEND_OPC_INFO 0x54
#define SCSICDRRW_REPAIR_TRACK 0x58
#define SCSICDRRW_READ_MASTER_CUE 0x59
#define SCSICDRRW_CLOSE_TRACK 0x5b
#define SCSICDRRW_READ_BUFFER_CAP 0x5c
#define SCSICDRRW_SEND_CUE_SHEET 0x5d
#define SCSICDRRW_BLANK 0xa1

//  SCSI tape commands:
#define SCSICMD_REWIND 0x01
#define SCSICMD_READ_BLOCK_LIMITS 0x05
#define SCSICMD_SPACE 0x11

// SCSI mode pages:
#define SCSIMP_VENDOR 0x00
#define SCSIMP_READ_WRITE_ERRREC 0x01
#define SCSIMP_DISCONNECT_RECONNECT 0x02
#define SCSIMP_FORMAT_PARAMS 0x03
#define SCSIMP_RIGID_GEOMETRY 0x04
#define SCSIMP_FLEX_PARAMS 0x05
#define SCSIMP_CACHING 0x08
#define SCSIMP_CDROM_CAP 0x2A

#define SCSI_OK 0
#define SCSI_ILL_CMD -1   /* illegal command */
#define SCSI_LBA_RANGE -2 /* LBA out of range */
#define SCSI_TOO_BIG -3   /* Too big for buffer */

void CDisk::do_scsi_error(int errcode) {
  state.scsi.stat.available = 1;
  state.scsi.stat.data[0] = 0;
  state.scsi.stat.read = 0;
  state.scsi.msgi.available = 1;
  state.scsi.msgi.data[0] = 0;
  state.scsi.msgi.read = 0;

  if (errcode == SCSI_OK) {
#if defined(DEBUG_SCSI)
    printf("%s: Command returns OK status.\n", devid_string);
#endif
    return;
  }

  state.scsi.stat.data[0] = 0x02;  // check sense
  state.scsi.sense.data[0] = 0xf0; // error code
  state.scsi.sense.data[1] = 0x00; // segment number
  state.scsi.sense.data[3] = 0x00; // info
  state.scsi.sense.data[4] = 0x00;
  state.scsi.sense.data[5] = 0x00;
  state.scsi.sense.data[6] = 0x00;
  state.scsi.sense.data[7] = 10;   // additional sense length
  state.scsi.sense.data[8] = 0x00; // command specific
  state.scsi.sense.data[9] = 0x00;
  state.scsi.sense.data[10] = 0x00;
  state.scsi.sense.data[11] = 0x00;
  state.scsi.sense.data[14] = 0x00; // FRU code
  state.scsi.sense.data[15] = 0x00; // sense key specific
  state.scsi.sense.data[16] = 0x00;
  state.scsi.sense.data[17] = 0x00;
  state.scsi.sense.available = 18;

  switch (errcode) {
  case SCSI_ILL_CMD:
    state.scsi.sense.data[2] = 0x05;  // illegal request
    state.scsi.sense.data[12] = 0x20; // invalid command
    state.scsi.sense.data[13] = 0x00;
#if defined(DEBUG_SCSI)
    printf("%s: Command returns check sense status (sense: ILLEGAL COMMAND).\n",
           devid_string);
#endif
    break;

  case SCSI_LBA_RANGE:
    state.scsi.sense.data[2] = 0x05;  // illegal request
    state.scsi.sense.data[12] = 0x21; // LBA out of range
    state.scsi.sense.data[13] = 0x00;
#if defined(DEBUG_SCSI)
    printf(
        "%s: Command returns check sense status (sense: LBA OUT OF RANGE).\n",
        devid_string);
#endif
    break;

  case SCSI_TOO_BIG:
    state.scsi.sense.data[2] = 0x05;  // illegal request
    state.scsi.sense.data[12] = 0x55; // system resource failure
    state.scsi.sense.data[13] = 0x00;
#if defined(DEBUG_SCSI)
    printf("%s: Command returns check sense status (sense: SYSTEM RESOURCE "
           "FAILURE).\n",
           devid_string);
#endif
  }
}

/**
 * Basic algorithm taken from libcdio for converting lba to msf
 **/
static u32 lba2msf(off_t_large lba) {
#define PREGAP_SECTORS 150
#define CD_FRAMES_PER_SEC 75
#define CD_MAX_LSN 450150

#define bin2bcd(x) ((x / 10) << 4) | (x % 10)
  int m;

  int s;

  int f;
  lba -= PREGAP_SECTORS;
  if (lba >= -PREGAP_SECTORS) {
    m = (lba + PREGAP_SECTORS) / (CD_FRAMES_PER_SEC * 60);
    lba -= m * (CD_FRAMES_PER_SEC * 60);
    s = (lba + PREGAP_SECTORS) / CD_FRAMES_PER_SEC;
    lba -= s * CD_FRAMES_PER_SEC;
    f = lba + PREGAP_SECTORS;
  } else {
    m = (lba + CD_MAX_LSN) / (CD_FRAMES_PER_SEC * 60);
    lba -= m * (CD_FRAMES_PER_SEC * 60);
    s = (lba + CD_MAX_LSN) / CD_FRAMES_PER_SEC;
    lba -= s * CD_FRAMES_PER_SEC;
    f = lba + CD_MAX_LSN;
  }

  if (m > 99)
    m = 99;

  // printf("m=%d, s=%d, f=%d == m=%x, s=%x, f=%x\n",
  // m,s,f,bin2bcd(m),bin2bcd(s),bin2bcd(f));
  return bin2bcd(m) << 16 | bin2bcd(s) << 8 | bin2bcd(f);
}

/**
 * \brief Handle a SCSI command.
 *
 * Called when a SCSI command has been received. We parse
 * the command, and set up the state for the data in or
 * data out phases.
 *
 * If a data out phase is required, we return the value 2
 * to indicate this. In that case, do_scsi_command will be
 * called again once the data out has been received from
 * the initiator.
 **/
int CDisk::do_scsi_command() {
  unsigned int retlen = 0;
  int q;
  int pagecode;
  u32 ofs = 0;

#if defined(DEBUG_SCSI)
  printf("%s: %d-byte command ", devid_string, state.scsi.cmd.written);
  for (unsigned int x = 0; x < state.scsi.cmd.written; x++)
    printf("%02x ", state.scsi.cmd.data[x]);
  printf("\n");
#endif
  if (state.scsi.cmd.written < 1)
    return 0;

  if (state.scsi.cmd.data[1] & 0xe0) {
#if defined(DEBUG_SCSI)
    printf("%s: LUN selected...\n", devid_string);
#endif
    state.scsi.lun_selected = true;
  }

  if (state.scsi.lun_selected && state.scsi.cmd.data[0] != SCSICMD_INQUIRY &&
      state.scsi.cmd.data[0] != SCSICMD_REQUEST_SENSE) {
    FAILURE_1(NotImplemented, "%s: LUN not supported!\n", devid_string);
  }

  switch (state.scsi.cmd.data[0]) {
  case SCSICMD_TEST_UNIT_READY:
#if defined(DEBUG_SCSI)
    printf("%s: TEST UNIT READY.\n", devid_string);
#endif

    // unit is always ready...
    do_scsi_error(SCSI_OK);
    break;

  case SCSICMD_REQUEST_SENSE:
#if defined(DEBUG_SCSI)
    printf("%s: REQUEST SENSE.\n", devid_string);
#endif
    retlen = state.scsi.cmd.data[4];

    //    FAILURE("Sense requested");
    if (!state.scsi.sense.available) {
#if defined(DEBUG_SCSI)
      printf("%s: NO SENSE.\n", devid_string);
#endif
      state.scsi.sense.data[0] = 0xf0; // error code
      state.scsi.sense.data[1] = 0x00; // segment number
      state.scsi.sense.data[2] = 0x00; // sense key: no sense
      state.scsi.sense.data[3] = 0x00; // info
      state.scsi.sense.data[4] = 0x00;
      state.scsi.sense.data[5] = 0x00;
      state.scsi.sense.data[6] = 0x00;
      state.scsi.sense.data[7] = 10;   // additional sense length
      state.scsi.sense.data[8] = 0x00; // command specific
      state.scsi.sense.data[9] = 0x00;
      state.scsi.sense.data[10] = 0x00;
      state.scsi.sense.data[11] = 0x00;
      state.scsi.sense.data[12] =
          0x00; // additional sense code: no additional sense
      state.scsi.sense.data[13] = 0x00; // additional qualifier
      state.scsi.sense.data[14] = 0x00; // FRU code
      state.scsi.sense.data[15] = 0x00; // sense key specific
      state.scsi.sense.data[16] = 0x00;
      state.scsi.sense.data[17] = 0x00;
      state.scsi.sense.available = 18;
    }

#if defined(DEBUG_SCSI)
    printf("%s: Returning data: ", devid_string);
    for (unsigned int x1 = 0; x1 < state.scsi.sense.available; x1++)
      printf("%02x ", state.scsi.sense.data[x1]);
    printf("\n");
#endif
    state.scsi.dati.read = 0;
    state.scsi.dati.available = retlen;
    memcpy(state.scsi.dati.data, state.scsi.sense.data,
           state.scsi.sense.available);
    for (unsigned int x2 = state.scsi.sense.available; x2 < retlen; x2++)
      state.scsi.dati.data[x2] = 0;

    do_scsi_error(SCSI_OK);
    break;

  case SCSICMD_INQUIRY: {
#if defined(DEBUG_SCSI)
    printf("%s: INQUIRY.\n", devid_string);
#endif
    if ((state.scsi.cmd.data[1] & 0x1e) != 0x00) {
      FAILURE_2(NotImplemented,
                "%s: Don't know how to handle INQUIRY with cmd[1]=0x%02x.\n",
                devid_string, state.scsi.cmd.data[1]);
      break;
    }

    u8 qual_dev = state.scsi.lun_selected ? 0x7F : (cdrom() ? 0x05 : 0x00);

    retlen = state.scsi.cmd.data[4];
    state.scsi.dati.data[0] = qual_dev; // device type
    if (state.scsi.cmd.data[1] & 0x01) {

      // Vital Product Data
      switch (state.scsi.cmd.data[2]) {
      case 0x00:

        // Page 0 is basically a list of page codes supported, so if
        // any others are added, make sure to insert them in the proper
        // place and increase the page length.
        state.scsi.dati.data[1] = 0x00; // page code 0
        state.scsi.dati.data[2] = 0x00; // reserved
        state.scsi.dati.data[3] = 0x02; // page length
        state.scsi.dati.data[4] = 0x00; // page 0 is supported.
        state.scsi.dati.data[5] = 0x80; // page 0x80 is supported.
        break;

      case 0x80:
        char serial_number[20];
        sprintf(serial_number, "SRL%04x", scsi_initiator_id[0] * 0x0101);

        // unit serial number page
        state.scsi.dati.data[1] = 0x80; // page code: 0x80
        state.scsi.dati.data[2] = 0x00; // reserved
        state.scsi.dati.data[3] = (u8)strlen(serial_number);
        memcpy(&state.scsi.dati.data[4], serial_number, strlen(serial_number));
        break;

      default:
#if 1
        FAILURE_1(NotImplemented,
                  "Don't know format for vital product data page %02x!!\n",
                  state.scsi.cmd.data[2]);
#else
        state.scsi.dati.data[1] = state.scsi.cmd.data[2]; // page code
        state.scsi.dati.data[2] = 0x00;                   // reserved
#endif
      }
    } else {

      //  Return values:
      if (retlen < 36) {
        printf("%s: SCSI inquiry len=%i, <36!\n", devid_string, retlen);
        retlen = 36;
      }

      state.scsi.dati.data[1] = 0;    // not removable;
      state.scsi.dati.data[2] = 0x02; // ANSI scsi 2
      state.scsi.dati.data[3] = 0x02; // response format
      state.scsi.dati.data[4] = 32;   // additional length
      state.scsi.dati.data[5] = 0;    // reserved
      state.scsi.dati.data[6] = 0x04; // reserved
      state.scsi.dati.data[7] = 0x60; // capabilities

      //                        vendor  model           rev.
      memcpy(&(state.scsi.dati.data[8]), "DEC     RZ58     (C) DEC2000", 28);

      //  Some data is different for CD-ROM drives:
      if (cdrom()) {
        state.scsi.dati.data[1] = 0x80; //  0x80 = removable

        //                           vendor  model           rev.
        memcpy(&(state.scsi.dati.data[8]), "DEC     RRD42   (C) DEC 4.5d", 28);
      }
    }

    state.scsi.dati.read = 0;
    state.scsi.dati.available = retlen;

#if defined(DEBUG_SCSI)
    printf("%s: Returning data: ", devid_string);
    for (unsigned int x1 = 0; x1 < 36; x1++)
      printf("%02x ", state.scsi.dati.data[x1]);
    printf("\n");
#endif
    do_scsi_error(SCSI_OK);
  } break;

  case SCSICMD_MODE_SENSE:
  case SCSICMD_MODE_SENSE_10:
#if defined(DEBUG_SCSI)
    printf("%s: MODE SENSE.\n", devid_string);
#endif
    {
      int num_blk_desc = 1;

      if (state.scsi.cmd.data[0] == SCSICMD_MODE_SENSE) {
        q = 4;
        retlen = state.scsi.cmd.data[4];
        state.scsi.dati.data[0] = retlen; // mode data length
        state.scsi.dati.data[1] =
            cdrom() ? 0x01 : 0x00;      // medium type (120 mm data for CD-ROM)
        state.scsi.dati.data[2] = 0x00; // device specific parameter
        state.scsi.dati.data[3] =
            8 * num_blk_desc; // block descriptor length: 1 page (?)
      } else {
        q = 8;
        retlen = state.scsi.cmd.data[7] * 256 + state.scsi.cmd.data[8];
        state.scsi.dati.data[0] = (u8)(retlen >> 8); // mode data length
        state.scsi.dati.data[1] = (u8)retlen;
        state.scsi.dati.data[2] =
            cdrom() ? 0x01 : 0x00;      // medium type (120 mm data for CD-ROM)
        state.scsi.dati.data[3] = 0x00; // device specific parameter
        state.scsi.dati.data[4] = 0x00; // reserved
        state.scsi.dati.data[5] = 0x00; // reserved
        state.scsi.dati.data[6] = (u8)(
            (8 * num_blk_desc) >> 8); //  block descriptor length: 1 page (?)
        state.scsi.dati.data[7] = (u8)(8 * num_blk_desc);
      }

      if ((state.scsi.cmd.data[2] & 0xc0) > 0x40) {
        FAILURE_2(NotImplemented, "%s: mode sense, cmd[2] = 0x%02x.\n",
                  devid_string, state.scsi.cmd.data[2]);
      }

      bool changeable = ((state.scsi.cmd.data[2] & 0xc0) == 0x40);

      //  Return data:
      if (retlen > DATI_BUFSZ) {
        printf("%s: read too big (%d)\n", devid_string, retlen);
        do_scsi_error(SCSI_TOO_BIG);
        break;
      }

      state.scsi.dati.read = 0;
      state.scsi.dati.available = retlen; //  Restore size.
      pagecode = state.scsi.cmd.data[2] & 0x3f;

      // printf("[ MODE SENSE pagecode=%i ]\n", pagecode);
      state.scsi.dati.data[q++] = 0x00; //  density code
      state.scsi.dati.data[q++] =
          0; //  nr of blocks, high (0 = all remaining blocks)
      state.scsi.dati.data[q++] = 0;    //  nr of blocks, mid
      state.scsi.dati.data[q++] = 0;    //  nr of blocks, low
      state.scsi.dati.data[q++] = 0x00; //  reserved
      state.scsi.dati.data[q++] = (u8)(get_block_size() >> 16) & 255;
      state.scsi.dati.data[q++] = (u8)(get_block_size() >> 8) & 255;
      state.scsi.dati.data[q++] = (u8)(get_block_size() >> 0) & 255;

      for (unsigned int x1 = q; x1 < retlen; x1++)
        state.scsi.dati.data[x1] = 0;

      do_scsi_error(SCSI_OK);

      //  descriptors, 8 bytes (each)
      //  page, n bytes (each)
      switch (pagecode) {
      case SCSIMP_VENDOR: // vendor specific
        //  TODO: Nothing here?
        break;

      case SCSIMP_READ_WRITE_ERRREC: //  read-write error recovery page
        state.scsi.dati.data[q + 0] = pagecode;
        state.scsi.dati.data[q + 1] = 10;
        break;

      case SCSIMP_FORMAT_PARAMS: //  format device page
        state.scsi.dati.data[q + 0] = pagecode;
        state.scsi.dati.data[q + 1] = 22;
        if (!changeable) {

          //  10,11 = sectors per track
          state.scsi.dati.data[q + 10] = 0;
          state.scsi.dati.data[q + 11] = (u8)get_sectors();

          //  12,13 = physical sector size
          state.scsi.dati.data[q + 12] = (u8)(get_block_size() >> 8) & 255;
          state.scsi.dati.data[q + 13] = (u8)(get_block_size() >> 0) & 255;
        }
        break;

      case SCSIMP_RIGID_GEOMETRY: //  rigid disk geometry page
        state.scsi.dati.data[q + 0] = pagecode;
        state.scsi.dati.data[q + 1] = 22;
        if (!changeable) {
          state.scsi.dati.data[q + 2] = (u8)(get_cylinders() >> 16) & 255;
          state.scsi.dati.data[q + 3] = (u8)(get_cylinders() >> 8) & 255;
          state.scsi.dati.data[q + 4] = (u8)get_cylinders() & 255;
          state.scsi.dati.data[q + 5] = (u8)get_heads();

          // rpms
          state.scsi.dati.data[q + 20] = (7200 >> 8) & 255;
          state.scsi.dati.data[q + 21] = 7200 & 255;
        }
        break;

      case SCSIMP_FLEX_PARAMS: //  flexible disk page
        if (cdrom()) {
          FAILURE_1(NotImplemented,
                    "%s: CD-ROM write parameter page not implemented.\n",
                    devid_string);
        }

        state.scsi.dati.data[q + 0] = pagecode;
        state.scsi.dati.data[q + 1] = 0x1e; // length
        if (!changeable) {

          //  2,3 = transfer rate
          state.scsi.dati.data[q + 2] = ((5000) >> 8) & 255;
          state.scsi.dati.data[q + 3] = (5000) & 255;

          state.scsi.dati.data[q + 4] = (u8)get_heads();
          state.scsi.dati.data[q + 5] = (u8)get_sectors();

          //  6,7 = data bytes per sector
          state.scsi.dati.data[q + 6] = (u8)(get_block_size() >> 8) & 255;
          state.scsi.dati.data[q + 7] = (u8)(get_block_size() >> 0) & 255;

          state.scsi.dati.data[q + 8] = (u8)(get_cylinders() >> 8) & 255;
          state.scsi.dati.data[q + 9] = (u8)get_cylinders() & 255;

          // rpms
          state.scsi.dati.data[q + 28] = (7200 >> 8) & 255;
          state.scsi.dati.data[q + 29] = 7200 & 255;
        }
        break;

      case SCSIMP_CACHING:                      // Caching page
        state.scsi.dati.data[q + 0] = pagecode; // page code
        state.scsi.dati.data[q + 1] = 0x12;     // page length
        if (!changeable) {

          // 2 = IC,ABPF,CAP,DISC,SIZE,WCE,MF,RCD
          //     |  |    |   |    |    |   |  +- read cache disable (0=no)
          //     |  |    |   |    |    |   +---- multiplication factor (0=block)
          //     |  |    |   |    |    +-------- write cache enable (0=no cache)
          //     |  |    |   |    +------------- use cache segment size (0=no)
          //     |  |    |   +------------------ prefetch across cyls (1=yes)
          //     |  |    +---------------------- cache analysis (0=drive)
          //     |  +--------------------------- abort prefetch (1=abrt on cmd)
          //     +------------------------------ initiator control (0=drive)
          state.scsi.dati.data[q + 2] = 0x0a;

          state.scsi.dati.data[q + 3] = 0;    // read/write cache retention
          state.scsi.dati.data[q + 4] = 0x00; // disable prefetch
          state.scsi.dati.data[q + 5] = 0x00; // for req's greater than this
          state.scsi.dati.data[q + 6] = 0;    // minimum prefetch
          state.scsi.dati.data[q + 7] = 0;

          state.scsi.dati.data[q + 8] = 0; // maximum prefetch
          state.scsi.dati.data[q + 9] = 0;

          state.scsi.dati.data[q + 10] = 0; // maximum prefetch ceiling
          state.scsi.dati.data[q + 11] = 0;

          state.scsi.dati.data[q + 12] = 0;
          state.scsi.dati.data[q + 13] = 0; // # cache segments
          state.scsi.dati.data[q + 14] = 0; // cache segement size
          state.scsi.dati.data[q + 15] = 0;

          state.scsi.dati.data[q + 16] = 0; // reserved
          state.scsi.dati.data[q + 17] = 0; // non-cache segement size
          state.scsi.dati.data[q + 18] = 0;
          state.scsi.dati.data[q + 19] = 0;
        }
        break;

      case SCSIMP_CDROM_CAP: // CD-ROM capabilities
        state.scsi.dati.data[q + 0] = pagecode;
        state.scsi.dati.data[q + 1] = 0x14; // length
        if (!changeable) {
          state.scsi.dati.data[q + 2] = 0x03; // read CD-R/CD-RW
          state.scsi.dati.data[q + 3] = 0x00; // no write
          state.scsi.dati.data[q + 4] = 0x00; // dvd/audio capabilities
          state.scsi.dati.data[q + 5] = 0x00; // cd-da capabilities
          state.scsi.dati.data[q + 6] =
              state.scsi.locked ? 0x23 : 0x21; // tray-loader
          state.scsi.dati.data[q + 7] = 0x00;
          state.scsi.dati.data[q + 8] =
              (u8)(2800 >> 8); // max read speed in kBps (2.8Mbps = 16x)
          state.scsi.dati.data[q + 9] = (u8)(2800 >> 0);
          state.scsi.dati.data[q + 10] =
              (u8)(0 >> 8); // number of volume levels
          state.scsi.dati.data[q + 11] = (u8)(0 >> 0);
          state.scsi.dati.data[q + 12] = (u8)(64 >> 8); // buffer size in KBytes
          state.scsi.dati.data[q + 13] = (u8)(64 >> 0);
          state.scsi.dati.data[q + 14] = (u8)(2800 >> 8); // current read speed
          state.scsi.dati.data[q + 15] = (u8)(2800 >> 0);
          state.scsi.dati.data[q + 16] = 0;            // reserved
          state.scsi.dati.data[q + 17] = 0;            // digital output format
          state.scsi.dati.data[q + 18] = (u8)(0 >> 8); // max write speed
          state.scsi.dati.data[q + 19] = (u8)(0 >> 0);
          state.scsi.dati.data[q + 20] = (u8)(0 >> 8); // current write speed
          state.scsi.dati.data[q + 21] = (u8)(0 >> 0);
        }
        break;

      default:
        FAILURE_2(NotImplemented,
                  "%s: MODE_SENSE for page %i is not yet implemented!\n",
                  devid_string, pagecode);
      }

#if defined(DEBUG_SCSI)
      printf("%s: Returning data: ", devid_string);
      for (unsigned int x1 = 0; x1 < q + 30; x1++)
        printf("%02x ", state.scsi.dati.data[x1]);
      printf("\n");
#endif
    }
    break;

  case SCSICMD_PREVENT_ALLOW_REMOVE:
    if (state.scsi.cmd.data[4] & 1) {
      state.scsi.locked = true;
#if defined(DEBUG_SCSI)
      printf("%s: PREVENT MEDIA REMOVAL.\n", devid_string);
#endif
    } else {
      state.scsi.locked = false;
#if defined(DEBUG_SCSI)
      printf("%s: ALLOW MEDIA REMOVAL.\n", devid_string);
#endif
    }

    do_scsi_error(SCSI_OK);
    break;

  case SCSICMD_MODE_SELECT:

    // get data out first...
    state.scsi.dato.expected = 12;
    if (state.scsi.dato.written < state.scsi.dato.expected)
      return 2;

#if defined(DEBUG_SCSI)
    printf("%s: MODE SELECT.\n", devid_string);
    printf("Data: ");
    for (unsigned int x = 0; x < state.scsi.dato.written; x++)
      printf("%02x ", state.scsi.dato.data[x]);
    printf("\n");
#endif
    if (state.scsi.cmd.written == 6 && state.scsi.dato.written == 12 &&
        state.scsi.dato.data[0] ==
            0x00 // data length
                 //&& state.scsi.dato.data[1] == 0x05 // medium type - ignore
        && state.scsi.dato.data[2] == 0x00  // dev. specific
        && state.scsi.dato.data[3] == 0x08  // block descriptor length
        && state.scsi.dato.data[4] == 0x00  // density code
        && state.scsi.dato.data[5] == 0x00  // all blocks
        && state.scsi.dato.data[6] == 0x00  // all blocks
        && state.scsi.dato.data[7] == 0x00  // all blocks
        && state.scsi.dato.data[8] == 0x00) // reserved
    {
      set_block_size((state.scsi.dato.data[9] << 16) |
                     (state.scsi.dato.data[10] << 8) |
                     state.scsi.dato.data[11]);
#if defined(DEBUG_SCSI)
      printf("%s: Block size set to %d.\n", devid_string, get_block_size());
#endif
    } else {
      unsigned int x;
      printf("%s: MODE SELECT ignored.\nCommand: ", devid_string);
      for (x = 0; x < state.scsi.cmd.written; x++)
        printf("%02x ", state.scsi.cmd.data[x]);
      printf("\nData: ");
      for (x = 0; x < state.scsi.dato.written; x++)
        printf("%02x ", state.scsi.dato.data[x]);
      printf("\nThis might be an attempt to change our blocksize or something "
             "like that...\nPlease check the above data, then press enter.\n>");
      getchar();
    }

    // ignore it...
    do_scsi_error(SCSI_OK);
    break;

  case SCSIBLOCKCMD_READ_CAPACITY:
#if defined(DEBUG_SCSI)
    printf("%s: READ CAPACITY.\n", devid_string);
#endif
    if (state.scsi.cmd.data[8] & 1) {
      FAILURE_1(
          NotImplemented,
          "%s: Don't know how to handle READ CAPACITY with PMI bit set.\n",
          devid_string);
      break;
    }

    // READ CAPACITY returns the number of the last LBA (n-1);
    // not the number of LBA's (n)
    state.scsi.dati.data[0] = (u8)((get_lba_size() - 1) >> 24) & 255;
    state.scsi.dati.data[1] = (u8)((get_lba_size() - 1) >> 16) & 255;
    state.scsi.dati.data[2] = (u8)((get_lba_size() - 1) >> 8) & 255;
    state.scsi.dati.data[3] = (u8)((get_lba_size() - 1) >> 0) & 255;

    state.scsi.dati.data[4] = (u8)(get_block_size() >> 24) & 255;
    state.scsi.dati.data[5] = (u8)(get_block_size() >> 16) & 255;
    state.scsi.dati.data[6] = (u8)(get_block_size() >> 8) & 255;
    state.scsi.dati.data[7] = (u8)(get_block_size() >> 0) & 255;

    state.scsi.dati.read = 0;
    state.scsi.dati.available = 8;

#if defined(DEBUG_SCSI)
    printf("%s: Returning data: ", devid_string);
    for (unsigned int x1 = 0; x1 < 8; x1++)
      printf("%02x ", state.scsi.dati.data[x1]);
    printf("\n");
#endif
    do_scsi_error(SCSI_OK);
    break;

  case SCSICMD_READ:
  case SCSICMD_READ_10:
  case SCSICMD_READ_12:
  case SCSICMD_READ_CD:
#if defined(DEBUG_SCSI)
    printf("%s: READ.\n", devid_string);
#endif

    // if (state.scsi.disconnect_priv)
    //{
    //  //printf("%s: Will disconnect before returning read data.\n",
    //  devid_string); state.scsi.will_disconnect = true;
    //}
    if (state.scsi.cmd.data[0] == SCSICMD_READ) {

      //  bits 4..0 of cmd[1], and cmd[2] and cmd[3]
      //  hold the logical block address.
      //
      //  cmd[4] holds the number of logical blocks
      //  to transfer. (Special case if the value is
      //  0, actually means 256.)
      ofs = ((state.scsi.cmd.data[1] & 0x1f) << 16) +
            (state.scsi.cmd.data[2] << 8) + state.scsi.cmd.data[3];
      retlen = state.scsi.cmd.data[4];
      if (retlen == 0)
        retlen = 256;
    } else if (state.scsi.cmd.data[0] == SCSICMD_READ_10) {

      //  cmd[2..5] hold the logical block address.
      //  cmd[7..8] holds the number of logical
      ofs = (state.scsi.cmd.data[2] << 24) + (state.scsi.cmd.data[3] << 16) +
            (state.scsi.cmd.data[4] << 8) + state.scsi.cmd.data[5];
      retlen = (state.scsi.cmd.data[7] << 8) + state.scsi.cmd.data[8];
    } else if (state.scsi.cmd.data[0] == SCSICMD_READ_12) {

      //  cmd[2..5] hold the logical block address.
      //  cmd[6..9] holds the number of logical
      ofs = (state.scsi.cmd.data[2] << 24) + (state.scsi.cmd.data[3] << 16) +
            (state.scsi.cmd.data[4] << 8) + state.scsi.cmd.data[5];
      retlen = (state.scsi.cmd.data[6] << 24) + (state.scsi.cmd.data[7] << 16) +
               (state.scsi.cmd.data[8] << 8) + state.scsi.cmd.data[9];
    } else if (state.scsi.cmd.data[0] == SCSICMD_READ_CD) {
      if (state.scsi.cmd.data[9] != 0x10) {
        FAILURE_2(NotImplemented, "%s: READ CD issued with data type %02x.\n",
                  devid_string, state.scsi.cmd.data[9]);
      }

      //  cmd[2..5] hold the logical block address.
      //  cmd[6..8] holds the number of logical blocks to transfer.
      ofs = (state.scsi.cmd.data[2] << 24) + (state.scsi.cmd.data[3] << 16) +
            (state.scsi.cmd.data[4] << 8) + state.scsi.cmd.data[5];
      retlen = (state.scsi.cmd.data[6] << 16) + (state.scsi.cmd.data[7] << 8) +
               state.scsi.cmd.data[8];
    }

    // Within bounds?
    if ((ofs + retlen) > get_lba_size()) {
      do_scsi_error(SCSI_LBA_RANGE);
      break;
    }

    // Would exceed buffer?
    if (retlen > DATI_BUFSZ) {
      printf("%s: read too big (%d)\n", devid_string, retlen);
      do_scsi_error(SCSI_TOO_BIG);
      break;
    }

    //  Return data:
    seek_block(ofs);
    read_blocks(state.scsi.dati.data, retlen);
    state.scsi.dati.read = 0;
    state.scsi.dati.available = retlen * get_block_size();

#if defined(DEBUG_SCSI)
    printf("%s: READ  ofs=%d size=%d\n", devid_string, ofs, retlen);
#endif
    do_scsi_error(SCSI_OK);
    break;

  case SCSICMD_READ_LONG:
#if defined(DEBUG_SCSI)
    printf("%s: READ_LONG.\n", devid_string);
#endif

    // The read long command is used to read one block of disk data, including
    // ECC data. OpenVMS uses read long / write long to do host-based shadowing.
    // During driver initialization, OpenVMS will check each disk to see if it
    // supports host-based shadowing, by trying to find the right size for read
    // long commands. The emulated scsi disk sets the size for read long / write
    // long commands to 514 bytes (the first value OpenVMS tries).
    //  cmd[2..5] hold the logical block address.
    //  cmd[7..8] holds the number of bytes to transfer
    ofs = (state.scsi.cmd.data[2] << 24) + (state.scsi.cmd.data[3] << 16) +
          (state.scsi.cmd.data[4] << 8) + state.scsi.cmd.data[5];
    retlen = (state.scsi.cmd.data[7] << 8) + state.scsi.cmd.data[8];

    state.scsi.stat.available = 1;
    state.scsi.stat.data[0] = 0;
    state.scsi.stat.read = 0;
    state.scsi.msgi.available = 1;
    state.scsi.msgi.data[0] = 0;
    state.scsi.msgi.read = 0;

    // If the requested size is not 514 bytes, don't accept it.
    if (retlen != 514) {
      do_scsi_error(SCSI_ILL_CMD);
      break;
    }

    // Within bounds?
    if ((ofs + 1) > get_lba_size()) {
      do_scsi_error(SCSI_LBA_RANGE);
      break;
    }

    // Would exceed buffer?
    if (retlen > DATI_BUFSZ) {
      printf("%s: read too big (%d)\n", devid_string, retlen);
      do_scsi_error(SCSI_TOO_BIG);
      break;
    }

    //  Return data:
    seek_block(ofs);
    read_blocks(state.scsi.dati.data, 1);
    for (unsigned int x1 = get_block_size(); x1 < retlen; x1++)
      state.scsi.dati.data[x1] = 0; // set ECC bytes to 0.
    state.scsi.dati.read = 0;
    state.scsi.dati.available = retlen;
    do_scsi_error(SCSI_OK);
    break;

  case SCSICMD_WRITE:
  case SCSICMD_WRITE_10:
#if defined(DEBUG_SCSI)
    printf("%s: WRITE.\n", devid_string);
#endif
    if (state.scsi.cmd.data[0] == SCSICMD_WRITE) {

      //  bits 4..0 of cmd[1], and cmd[2] and cmd[3]
      //  hold the logical block address.
      //
      //  cmd[4] holds the number of logical blocks
      //  to transfer. (Special case if the value is
      //  0, actually means 256.)
      ofs = ((state.scsi.cmd.data[1] & 0x1f) << 16) +
            (state.scsi.cmd.data[2] << 8) + state.scsi.cmd.data[3];
      retlen = state.scsi.cmd.data[4];
      if (retlen == 0)
        retlen = 256;
    } else {

      //  cmd[2..5] hold the logical block address.
      //  cmd[7..8] holds the number of logical blocks
      //  to transfer.
      ofs = (state.scsi.cmd.data[2] << 24) + (state.scsi.cmd.data[3] << 16) +
            (state.scsi.cmd.data[4] << 8) + state.scsi.cmd.data[5];
      retlen = (state.scsi.cmd.data[7] << 8) + state.scsi.cmd.data[8];
    }

    // Within bounds?
    if (((ofs + retlen)) > get_lba_size()) {
      do_scsi_error(SCSI_LBA_RANGE);
      break;
    }

    // Would exceed buffer?
    if (retlen * get_block_size() > DATO_BUFSZ) {
      printf("%s: write too big (%d)\n", devid_string,
             (int)(retlen * get_block_size()));
      do_scsi_error(SCSI_TOO_BIG);
      break;
    }

    state.scsi.dato.expected = retlen * get_block_size();

    if (state.scsi.dato.written < state.scsi.dato.expected)
      return 2;

    //  Write data
    seek_block(ofs);
    write_blocks(state.scsi.dato.data, retlen);

#if defined(DEBUG_SCSI)
    printf("%s: WRITE  ofs=%d size=%d\n", devid_string, ofs, retlen);
#endif
    do_scsi_error(SCSI_OK);
    break;

  case SCSICMD_SYNCHRONIZE_CACHE:
#if defined(DEBUG_SCSI)
    printf("%s: SYNCHRONIZE CACHE.\n", devid_string);
#endif
    do_scsi_error(SCSI_OK);
    break;

  case SCSICDROM_READ_TOC: {
#if defined(DEBUG_SCSI)
    printf("%s: CDROM READ TOC.\n", devid_string);
#endif
    if (state.scsi.cmd.data[2] & 0x0f) {
      FAILURE_2(NotImplemented,
                "%s: I don't understand READ TOC/PMA/ATIP with format %01x.\n",
                devid_string, state.scsi.cmd.data[2] & 0x0f);
    }

    if (state.scsi.cmd.data[6] > 1 && state.scsi.cmd.data[6] != 0xAA) {
      FAILURE_2(InvalidArgument, "%s: I don't know CD-ROM track 0x%02x.\n",
                devid_string, state.scsi.cmd.data[6]);
    }

    retlen = state.scsi.cmd.data[7] * 256 + state.scsi.cmd.data[8];

    state.scsi.dati.available = retlen;
    state.scsi.dati.read = 0;

    int q = 2;

    /*Here's an actual response from a single-track pressed CD to the
          command  0x43, 00, 00, 00, 00, 00, 00, 00, 0x0c, 0x40, 00, 00

          0000 00 0a 01 01 00 14 01 00 00 00 00 00 00 00 00 00 ................
          0010 00 43 d6 02 00 81 ff ff 19 00 00 00 00 00 00 00 .C..............
          0020 01 00 00 00 00 00 00 00 01 00 00 00 01 00 01 00 ................
          0030 00 00 00 00 00 10 00 00 00 10 00 00 01 00 00 00 ................
    */
    state.scsi.dati.data[q++] = 1; // first track
    state.scsi.dati.data[q++] = 1; // last track
    if (state.scsi.cmd.data[6] <= 1) {
      state.scsi.dati.data[q++] = 0;    // reserved
      state.scsi.dati.data[q++] = 0x14; // adr/control (Q-channel: current
                                        // position, data track, no copy)
      state.scsi.dati.data[q++] = 1;    // track number
      state.scsi.dati.data[q++] = 0;    // reserved
      if (state.scsi.cmd.data[1] & 0x02) {
        u32 x = lba2msf(0);
        state.scsi.dati.data[q++] = 0;
        state.scsi.dati.data[q++] = (x & 0xff0000) >> 16;
        state.scsi.dati.data[q++] = (x & 0xff00) >> 8;
        state.scsi.dati.data[q++] = x & 0xff;
      } else {
        state.scsi.dati.data[q++] = 0 >> 24; // lba
        state.scsi.dati.data[q++] = 0 >> 16;
        state.scsi.dati.data[q++] = 0 >> 8;
        state.scsi.dati.data[q++] = 0;
      }
    }

    state.scsi.dati.data[q++] = 0; // reserved
    state.scsi.dati.data[q++] =
        0x16; // adr/control (Q-channel: current position, data track, copy)
    state.scsi.dati.data[q++] = 0xAA; // track number
    state.scsi.dati.data[q++] = 0;    // reserved
    if (state.scsi.cmd.data[1] & 0x02) {
      u32 x = lba2msf(get_lba_size());
      state.scsi.dati.data[q++] = 0;
      state.scsi.dati.data[q++] = (x & 0xff0000) >> 16;
      state.scsi.dati.data[q++] = (x & 0xff00) >> 8;
      state.scsi.dati.data[q++] = x & 0xff;
    } else {
      state.scsi.dati.data[q++] = (u8)(get_lba_size() >> 24); // lba
      state.scsi.dati.data[q++] = (u8)(get_lba_size() >> 16);
      state.scsi.dati.data[q++] = (u8)(get_lba_size() >> 8);
      state.scsi.dati.data[q++] = (u8)get_lba_size();
    }

    state.scsi.dati.data[0] = (u8)(q >> 8);
    state.scsi.dati.data[1] = (u8)q;

#if defined(DEBUG_SCSI)
    printf("%s: Returning data: ", devid_string);
    for (unsigned int x1 = 0; x1 < q; x1++)
      printf("%02x ", state.scsi.dati.data[x1]);
    printf("\n");
#endif
    do_scsi_error(SCSI_OK);
  } break;

  case SCSICDRRW_FORMAT:
  case SCSICDRRW_READ_DISC_INFO:
  case SCSICDRRW_READ_TRACK_INFO:
  case SCSICDRRW_RESERVE_TRACK:
  case SCSICDRRW_SEND_OPC_INFO:
  case SCSICDRRW_REPAIR_TRACK:
  case SCSICDRRW_READ_MASTER_CUE:
  case SCSICDRRW_CLOSE_TRACK:
  case SCSICDRRW_READ_BUFFER_CAP:
  case SCSICDRRW_SEND_CUE_SHEET:
  case SCSICDRRW_BLANK:

    // These are CD-R/RW specific commands; we pretend to be a simple
    // CD-ROM player, so no support for these commands.
#if defined(DEBUG_SCSI)
    printf("%s: CD-R/RW specific.\n", devid_string);
#endif
    do_scsi_error(SCSI_ILL_CMD);
    break;

  default:
    FAILURE_2(NotImplemented, "%s: Unknown SCSI command 0x%02x.\n",
              devid_string, state.scsi.cmd.data[0]);
  }

  return 0;
}

/**
 * \brief Handle a (series of) SCSI message(s).
 *
 * Called when one or more SCSI messages have been received.
 * We parse the message(s) and return what the next SCSI bus
 * phase should be.
 **/
int CDisk::do_scsi_message() {
  unsigned int msg;
  unsigned int msglen;

  msg = 0;
  while (msg < state.scsi.msgo.written) {
    if (state.scsi.msgo.data[msg] & 0x80) {

      // identify
#if defined(DEBUG_SCSI)
      printf("%s: MSG: identify", devid_string);
#endif
      if (state.scsi.msgo.data[msg] & 0x40) {
#if defined(DEBUG_SCSI)
        printf(" w/disconnect priv");
#endif

        //        state.scsi.disconnect_priv = true;
      }

      if (state.scsi.msgo.data[msg] & 0x07) {

        // LUN...
#if defined(DEBUG_SCSI)
        printf(" for lun %d%", state.scsi.msgo.data[msg] & 0x07);
#endif
        state.scsi.lun_selected = true;
      }

#if defined(DEBUG_SCSI)
      printf("\n");
#endif
      msg++;
    } else {
      switch (state.scsi.msgo.data[msg]) {
      case 0x01:
#if defined(DEBUG_SCSI)
        printf("%s: MSG: extended: ", devid_string);
#endif
        msglen = state.scsi.msgo.data[msg + 1];
        msg += 2;
        switch (state.scsi.msgo.data[msg]) {
        case 0x01: {
#if defined(DEBUG_SCSI)
          printf("SDTR.\n");
#endif
          state.scsi.msgi.available = msglen + 2;
          state.scsi.msgi.data[0] = 0x01;
          state.scsi.msgi.data[1] = msglen;
          for (unsigned int x = 0; x < msglen; x++)
            state.scsi.msgi.data[2 + x] = state.scsi.msgo.data[msg + x];
        } break;

        case 0x03: {
#if defined(DEBUG_SCSI)
          printf("WDTR.\n");
#endif
          state.scsi.msgi.available = msglen + 2;
          state.scsi.msgi.data[0] = 0x01;
          state.scsi.msgi.data[1] = msglen;
          for (unsigned int x = 0; x < msglen; x++)
            state.scsi.msgi.data[2 + x] = state.scsi.msgo.data[msg + x];
        } break;

        default:
          FAILURE_2(NotImplemented,
                    "%s: MSG: don't understand extended message %02x.\n",
                    devid_string, state.scsi.msgo.data[msg]);
        }

        msg += msglen;
        break;

      default:
        FAILURE_2(NotImplemented, "%s: MSG: don't understand message %02x.\n",
                  devid_string, state.scsi.msgo.data[msg]);
      }
    }
  }

  // return next phase
  if (state.scsi.msgi.available)
    return SCSI_PHASE_MSG_IN;
  else
    return SCSI_PHASE_COMMAND;
}

static int primes_54[54] = {
    2,   3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,
    47,  53,  59,  61,  67,  71,  73,  79,  83,  89,  97,  101, 103, 107,
    109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181,
    191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251};

//  2 3 5 7 11 13
static int pri16[16][6] = {{4, 0, 0, 0, 0, 0},  // 16
                           {0, 1, 1, 0, 0, 0},  // 15
                           {1, 0, 0, 1, 0, 0},  // 14
                           {0, 0, 0, 0, 0, 1},  // 13
                           {2, 1, 0, 0, 0, 0},  // 12
                           {0, 0, 0, 0, 1, 0},  // 11
                           {1, 0, 1, 0, 0, 0},  // 10
                           {0, 2, 0, 0, 0, 0},  // 9
                           {3, 0, 0, 0, 0, 0},  // 8
                           {0, 0, 0, 1, 0, 0},  // 7
                           {1, 1, 0, 0, 0, 0},  // 6
                           {0, 0, 1, 0, 0, 0},  // 5
                           {2, 0, 0, 0, 0, 0},  // 4
                           {0, 1, 0, 0, 0, 0},  // 3
                           {1, 0, 0, 0, 0, 0},  // 2
                           {0, 0, 0, 0, 0, 0}}; // 1
static off_t_large get_primes(off_t_large value, int pri[54]) {
  int i;
  for (i = 0; i < 54; i++) {
    pri[i] = 0;
    while (!(value % primes_54[i])) {
      pri[i]++;
      value /= primes_54[i];
    }
  }

  return value;
}

/**
 * Calculate optimal disk layout...
 **/
#define MAX_HD 16
#define MAX_SEC 50

void CDisk::determine_layout() {
  int disk_primes[54];
  int compare_primes[54];

  long heads_sectors = 0;
  long c_heads = 0;
  bool b;
  int prime;

  get_primes(get_lba_size(), disk_primes);

  for (heads_sectors = MAX_SEC * MAX_HD; heads_sectors > 0; heads_sectors--) {
    if (get_primes(heads_sectors, compare_primes) > 1)
      continue;

    for (c_heads = MAX_HD; c_heads > 0; c_heads--) {
      b = true;
      for (prime = 0; prime < 6; prime++) {
        if (pri16[16 - c_heads][prime] > compare_primes[prime]) {
          b = false;
          break;
        }
      }

      if (b)
        break;
    }

    if (heads_sectors / c_heads > MAX_SEC)
      continue;

    b = true;
    for (prime = 0; prime < 54; prime++) {
      if (compare_primes[prime] > disk_primes[prime]) {
        b = false;
        break;
      }
    }

    if (b)
      break;
  }

  heads = c_heads;
  sectors = heads_sectors / c_heads;

  //  sectors = 32;
  //  heads = 8;
  cylinders = get_lba_size() / heads / sectors;
}
