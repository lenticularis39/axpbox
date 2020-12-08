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
 * Contains the definitions for the emulated Keyboard and mouse devices and
 *controller.
 *
 * $Id: Keyboard.h,v 1.6 2008/05/31 15:47:10 iamcamiel Exp $
 *
 * X-1.6        Camiel Vanderhoeven                             31-MAY-2008
 *      Changes to include parts of Poco.
 *
 * X-1.5        Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.4        Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.3        Camiel Vanderhoeven                             13-MAR-2008
 *      Create init(), start_threads() and stop_threads() functions.
 *
 * X-1.2        Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.1        Camiel Vanderhoeven                             12-FEB-2008
 *      Created. Contains code previously found in AliM1543C.h
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#if !defined(INCLUDED_KEYBOARD_H)
#define INCLUDED_KEYBOARD_H

#include "SystemComponent.hpp"
#include "gui/gui.hpp"

#define BX_KBD_ELEMENTS 16
#define BX_MOUSE_BUFF_SIZE 48

#define MOUSE_MODE_RESET 10
#define MOUSE_MODE_STREAM 11
#define MOUSE_MODE_REMOTE 12
#define MOUSE_MODE_WRAP 13

/**
 * \brief Emulated keyboard controller, keyboard and mouse.
 **/
class CKeyboard : public CSystemComponent {
public:
  CKeyboard(CConfigurator *cfg, CSystem *c);
  virtual ~CKeyboard();

  virtual void check_state();
  virtual void WriteMem(int index, u64 address, int dsize, u64 data);
  virtual u64 ReadMem(int index, u64 address, int dsize);
  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);
  void run();
  void execute();

  void gen_scancode(u32 key);

  virtual void init();
  virtual void start_threads();
  virtual void stop_threads();

private:
  std::unique_ptr<std::thread> myThread;
  std::atomic_bool myThreadDead{false};
  bool StopThread;

  u8 read_60();
  void write_60(u8 data);
  u8 read_64();
  void write_64(u8 data);
  void resetinternals(bool powerup);
  void enQ(u8 scancode);
  void controller_enQ(u8 data, unsigned source);
  void set_kbd_clock_enable(u8 value);
  void set_aux_clock_enable(u8 value);
  void ctrl_to_kbd(u8 value);
  void enQ_imm(u8 val);
  void ctrl_to_mouse(u8 value);
  bool mouse_enQ_packet(u8 b1, u8 b2, u8 b3, u8 b4);
  void mouse_enQ(u8 mouse_data);
  unsigned periodic();

  //  void kbd_clock();
  void create_mouse_packet(bool force_enq);

  /// The state structure contains all elements that need to be saved to the
  /// statefile.
  struct SKb_state {

    /// status bits matching the status port
    struct SAli_kbdc_status {
      bool pare; /**< Bit7, 1= parity error from keyboard/mouse - ignored. */
      bool tim;  /**< Bit6, 1= timeout from keyboard - ignored. */
      bool auxb; /**< Bit5, 1= mouse data waiting for CPU to read. */
      bool keyl; /**< Bit4, 1= keyswitch in lock position - ignored. */
      bool c_d;  /**< Bit3, 1=command to port 64h, 0=data to port 60h. */
      bool sysf; /**< Bit2 */
      bool inpb; /**< Bit1 */
      bool outb; /**< Bit0, 1= keyboard data or mouse data ready for CPU. Check
                    aux to see which. */
    } status;

    /* internal to our version of the keyboard controller */
    bool kbd_clock_enabled;
    bool aux_clock_enabled;
    bool allow_irq1;
    bool allow_irq12;
    u8 kbd_output_buffer;
    u8 aux_output_buffer;
    u8 last_comm;
    u8 expecting_port60h;
    u8 expecting_mouse_parameter;
    u8 last_mouse_command;
    u32 timer_pending;
    bool irq1_requested;
    bool irq12_requested;
    bool scancodes_translate;
    bool expecting_scancodes_set;
    u8 current_scancodes_set;
    bool bat_in_progress;

    /// mouse status
    struct SAli_mouse {
      bool captured; // host mouse capture enabled

      //      u8   type;
      u8 sample_rate;
      u8 resolution_cpmm; // resolution in counts per mm
      u8 scaling;
      u8 mode;
      u8 saved_mode; // the mode prior to entering wrap mode
      bool enable;

      u8 get_status_byte() {

        // top bit is 0 , bit 6 is 1 if remote mode.
        u8 ret = (u8)((mode == MOUSE_MODE_REMOTE) ? 0x40 : 0);
        ret |= (enable << 5);
        ret |= (scaling == 1) ? 0 : (1 << 4);
        ret |= ((button_status & 0x1) << 2);
        ret |= ((button_status & 0x2) << 0);
        return ret;
      }

      u8 get_resolution_byte() {
        u8 ret = 0;

        switch (resolution_cpmm) {
        case 1:
          ret = 0;
          break;
        case 2:
          ret = 1;
          break;
        case 4:
          ret = 2;
          break;
        case 8:
          ret = 3;
          break;
        default:
          FAILURE(NotImplemented, "mouse: invalid resolution_cpmm");
        };
        return ret;
      }

      u8 button_status;
      s16 delayed_dx;
      s16 delayed_dy;
      s16 delayed_dz;
      u8 im_request;
      bool im_mode;
    } mouse;

    /// internal keyboard buffer
    struct SAli_kbdib {
      int num_elements;
      u8 buffer[BX_KBD_ELEMENTS];
      int head;
      bool expecting_typematic;
      bool expecting_led_write;
      bool expecting_make_break;
      u8 delay;
      u8 repeat_rate;
      u8 led_status;
      bool scanning_enabled;
    } kbd_internal_buffer;

    /// internal mouse buffer
    struct SAli_mib {
      int num_elements;
      u8 buffer[BX_MOUSE_BUFF_SIZE];
      int head;
    } mouse_internal_buffer;

#define BX_KBD_CONTROLLER_QSIZE 5
    u8 kbd_controller_Q[BX_KBD_CONTROLLER_QSIZE];
    unsigned kbd_controller_Qsize;
    unsigned kbd_controller_Qsource; /**< 0=keyboard, 1=mouse */
  } state;
};

extern CKeyboard *theKeyboard;
#endif // !defined(INCLUDED_KEYBOARD_H)
