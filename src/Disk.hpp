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
 * Contains definitions for the disk base class.
 *
 * $Id: Disk.h,v 1.16 2008/03/14 15:30:51 iamcamiel Exp $
 *
 * X-1.15       Camiel Vanderhoeven                             26-FEB-2008
 *      Remove private devid_string.
 *
 * X-1.14       David Leonard                                   20-FEB-2008
 *      Return SYSTEM RESOURCE FAILURE sense if dato/dati buffer size is
 *      exceeded.
 *
 * X-1.13       Camiel Vanderhoeven                             17-FEB-2008
 *      Set up sense data when error occurs.
 *
 * X-1.12       Camiel Vanderhoeven                             17-FEB-2008
 *      Added REQUEST_SENSE scsi command.
 *
 * X-1.11       Camiel Vanderhoeven                             13-JAN-2008
 *      Determine best-fitting C/H/S lay-out.
 *
 * X-1.10       Camiel Vanderhoeven                             12-JAN-2008
 *      Include SCSI engine, because this is common to both SCSI and ATAPI
 *      devices.
 *
 * X-1.9        Camiel Vanderhoeven                             09-JAN-2008
 *      Save disk state to state file.
 *
 * X-1.8        Camiel Vanderhoeven                             06-JAN-2008
 *      Support changing the block size (required for SCSI, ATAPI).
 *
 * X-1.7        Camiel Vanderhoeven                             04-JAN-2008
 *      64-bit file I/O.
 *
 * X-1.6        Camiel Vanderhoeven                             02-JAN-2008
 *      Comments.
 *
 * X-1.5        Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.4        Camiel Vanderhoeven                             20-DEC-2007
 *      Close files and free memory when the emulator shuts down.
 *
 * X-1.3        Camiel Vanderhoeven                             19-DEC-2007
 *      Allow for reporting disk-size in bytes.
 *
 * X-1.2        Camiel Vanderhoeven                             18-DEC-2007
 *      Byte-sized transfers for SCSI controller.
 *
 * X-1.1        Camiel Vanderhoeven                             12-DEC-2007
 *      Initial version in CVS.
 **/
#if !defined(__DISK_H__)
#define __DISK_H__

#include "DiskController.hpp"
#include "SCSIBus.hpp"
#include "SCSIDevice.hpp"

#define DATO_BUFSZ 256 * 1024
#define DATI_BUFSZ 256 * 1024

/**
 * \brief Abstract base class for disks (connects to a CDiskController)
 **/
class CDisk : public CSystemComponent, public CSCSIDevice {
public:
  CDisk(CConfigurator *cfg, CSystem *sys, CDiskController *c, int idebus,
        int idedev);
  virtual ~CDisk(void);
  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);

  virtual void scsi_select_me(int bus);
  virtual size_t scsi_expected_xfer_me(int bus);
  virtual void *scsi_xfer_ptr_me(int bus, size_t bytes);
  virtual void scsi_xfer_done_me(int bus);

  void set_atapi_mode() { atapi_mode = true; };

  int do_scsi_command();
  int do_scsi_message();
  void do_scsi_error(int errcode);

  virtual bool seek_byte(off_t_large byte) = 0;
  virtual size_t read_bytes(void *dest, size_t bytes) = 0;
  virtual size_t write_bytes(void *src, size_t bytes) = 0;

  bool seek_block(off_t_large lba) {
    return seek_byte(lba * state.block_size);
  };
  size_t read_blocks(void *dest, size_t blocks) {
    return read_bytes(dest, blocks * state.block_size) / state.block_size;
  };
  size_t write_blocks(void *src, size_t blocks) {
    return write_bytes(src, blocks * state.block_size) / state.block_size;
  };

  size_t get_block_size() { return state.block_size; };
  void set_block_size(size_t bs) {
    state.block_size = bs;
    determine_layout(); /*calc_cylinders();*/
  };

  void determine_layout();

  off_t_large get_lba_size() { return byte_size / state.block_size; };
  off_t_large get_byte_size() { return byte_size; };
  off_t_large get_chs_size() { return cylinders * heads * sectors; };
  off_t_large get_cylinders() { return cylinders; };
  long get_heads() { return heads; };
  long get_sectors() { return sectors; };

  char *get_serial() { return serial_number; };
  char *get_model() { return model_number; };
  char *get_rev() { return revision_number; };

  bool ro() { return read_only; };
  bool rw() { return !read_only; };
  bool cdrom() { return is_cdrom; };

  void calc_cylinders();

protected:
  CConfigurator *myCfg;
  CDiskController *myCtrl;
  int myBus;
  int myDev;

  char *serial_number;
  char *model_number;
  char *revision_number;

  bool read_only;
  bool is_cdrom;

  off_t_large byte_size;
  off_t_large cylinders;
  long heads;
  long sectors;

  bool atapi_mode;

  /// The state structure contains all elements that need to be saved to the
  /// statefile
  struct SDisk_state {
    size_t
        block_size; /**< How many bytes there are in a physical disk block. **/
    off_t_large byte_pos; /**< Current byte position in the disk. **/

    /// SCSI state for SCSI-connected disks
    struct SDisk_scsi {

      // State for Message In Phase (disk -> controller)
      struct SDisk_msgi {
        u8 data[256];           /**< Data buffer. **/
        unsigned int available; /**< Number of bytes available to read. **/
        unsigned int read;      /**< Number of bytes read so far. **/
      } msgi;

      /// State for Message Out Phase (controller -> disk)
      struct SDisk_msgo {
        u8 data[256];         /**< Data buffer. **/
        unsigned int written; /**< Number of bytes in buffer. **/
      } msgo;

      bool lun_selected; /**< A LUN has been selected. CDisk doesn't support
                            LUNs. **/

      /// state for Command phase (controller -> disk)
      struct SDisk_cmd {
        u8 data[256];         /**< Data buffer. **/
        unsigned int written; /**< Number of bytes in buffer. **/
      } cmd;

      /// State for Data In phase (disk -> controller)
      struct SDisk_dati {
        u8 data[DATI_BUFSZ];    /**< Data buffer. **/
        unsigned int available; /**< Number of bytes available to read. **/
        unsigned int read;      /**< Number of bytes read so far. **/
      } dati;

      /// State for Data Out phase (controller -> disk)
      struct SDisk_dato {
        u8 data[DATO_BUFSZ];   /**< Data buffer. **/
        unsigned int expected; /**< Number of bytes the initiator is expected to
                                  write. **/
        unsigned int written;  /**< Number of bytes written sofar. **/
      } dato;

      /// State for Status phase (disk -> controller)
      struct SDisk_stat {
        u8 data[256];           /**< Data buffer. **/
        unsigned int available; /**< Number of bytes available to read. **/
        unsigned int read;      /**< Number of bytes read so far. **/
      } stat;

      /// State for request sense
      struct SDisk_sense {
        u8 data[256];
        unsigned int available;
      } sense;

      bool locked; /**< Media is locked (for CD-ROM type devices). **/

      // bool disconnect_priv;       /**< Initiator has allowed us to
      // disconnect/reconnect. **/ bool will_disconnect;       /**< We intend to
      // disconnect. **/ bool disconnected;          /**< We have disconnected.
      // **/ bool reselected;            /**< We have reselected the initiator.
      // **/ int disconnect_phase;       /**< After we reconnect, we should go
      // to this SCSI phase. **/
    } scsi;
  } state;
};
#endif //! defined(__DISK_H__)
