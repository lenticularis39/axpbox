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
 * Contains the code for the emulated Keyboard and mouse devices and controller.
 *
 * $Id: Keyboard.cpp,v 1.10 2008/05/31 15:47:09 iamcamiel Exp $
 *
 * X-1.10       Camiel Vanderhoeven                             31-MAY-2008
 *      Changes to include parts of Poco.
 *
 * X-1.9        Camiel Vanderhoeven                             16-MAR-2008
 *      Fixed threading problems with SDL (I hope).
 *
 * X-1.8        Camiel Vanderhoeven                             14-MAR-2008
 *      Formatting.
 *
 * X-1.7        Camiel Vanderhoeven                             14-MAR-2008
 *   1. More meaningful exceptions replace throwing (int) 1.
 *   2. U64 macro replaces X64 macro.
 *
 * X-1.6        Camiel Vanderhoeven                             13-MAR-2008
 *      Create init(), start_threads() and stop_threads() functions.
 *
 * X-1.5        Camiel Vanderhoeven                             05-MAR-2008
 *      Multi-threading version.
 *
 * X-1.4        Brian Wheeler                                   29-FEB-2008
 *      ACK recognized, but unhandled, keyboard commands.
 *
 * X-1.3        Brian Wheeler                                   27-FEB-2008
 *      Avoid compiler warnings.
 *
 * X-1.2        David Leonard                                   20-FEB-2008
 *      Avoid 'Xlib: unexpected async reply' errors on Linux/Unix/BSD's by
 *      adding some thread interlocking.
 *
 * X-1.1        Camiel Vanderhoeven                             12-FEB-2008
 *      Created. Contains code previously found in AliM1543C.cpp
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#include "Keyboard.h"
#include "AliM1543C.h"
#include "StdAfx.h"
#include "System.h"
#include <math.h>

#include "gui/keymap.h"
#include "gui/scancodes.h"

/**
 * Constructor.
 **/
CKeyboard::CKeyboard(CConfigurator *cfg, CSystem *c)
    : CSystemComponent(cfg, c) {
  if (theKeyboard != 0)
    FAILURE(Configuration, "More than one Keyboard controller");
  theKeyboard = this;
}

/**
 * Initialize the Keyboard device.
 **/
void CKeyboard::init() {
  int i;

  cSystem->RegisterMemory(this, 0, U64(0x00000801fc000060), 1);
  cSystem->RegisterMemory(this, 1, U64(0x00000801fc000064), 1);

  resetinternals(1);

  state.kbd_internal_buffer.led_status = 0;
  state.kbd_internal_buffer.scanning_enabled = 1;

  state.mouse_internal_buffer.num_elements = 0;
  for (i = 0; i < BX_MOUSE_BUFF_SIZE; i++)
    state.mouse_internal_buffer.buffer[i] = 0;
  state.mouse_internal_buffer.head = 0;

  state.status.pare = 0;
  state.status.tim = 0;
  state.status.auxb = 0;
  state.status.keyl = 1;
  state.status.c_d = 1;
  state.status.sysf = 0;
  state.status.inpb = 0;
  state.status.outb = 0;

  state.kbd_clock_enabled = 1;
  state.aux_clock_enabled = 0;
  state.allow_irq1 = 1;
  state.allow_irq12 = 1;
  state.kbd_output_buffer = 0;
  state.aux_output_buffer = 0;
  state.last_comm = 0;
  state.expecting_port60h = 0;
  state.irq1_requested = 0;
  state.irq12_requested = 0;
  state.expecting_mouse_parameter = 0;
  state.bat_in_progress = 0;
  state.scancodes_translate = 1;

  state.timer_pending = 0;

  // Mouse initialization stuff
  state.mouse.captured = myCfg->get_bool_value("mouse.enabled", true);
  state.mouse.sample_rate = 100;   // reports per second
  state.mouse.resolution_cpmm = 4; // 4 counts per millimeter
  state.mouse.scaling = 1;         /* 1:1 (default) */
  state.mouse.mode = MOUSE_MODE_RESET;
  state.mouse.enable = 0;
  state.mouse.delayed_dx = 0;
  state.mouse.delayed_dy = 0;
  state.mouse.delayed_dz = 0;
  state.mouse.im_request = 0; // wheel mouse mode request
  state.mouse.im_mode = 0;    // wheel mouse mode
  for (i = 0; i < BX_KBD_CONTROLLER_QSIZE; i++)
    state.kbd_controller_Q[i] = 0;
  state.kbd_controller_Qsize = 0;
  state.kbd_controller_Qsource = 0;

  printf("kbc: $Id: Keyboard.cpp,v 1.10 2008/05/31 15:47:09 iamcamiel Exp $\n");
}

void CKeyboard::start_threads() {
  if (!myThread) {
    printf(" kbd");
    StopThread = false;
    myThread = std::make_unique<std::thread>([this](){ this->run(); });
  }
}

void CKeyboard::stop_threads() {
  StopThread = true;
  if (myThread) {
    printf(" kbd");
    myThread->join();
    myThread = nullptr;
  }
}

/**
 * Destructor.
 **/
CKeyboard::~CKeyboard() { stop_threads(); }

u64 CKeyboard::ReadMem(int index, u64 address, int dsize) {
  switch (index) {
  case 0:
    return read_60();
    break;
  case 1:
    return read_64();
    break;
  default:
    FAILURE(InvalidArgument, "kbc: ReadMem index out of range");
  }
}

void CKeyboard::WriteMem(int index, u64 address, int dsize, u64 data) {
  switch (index) {
  case 0:
    write_60((u8)data);
    break;
  case 1:
    write_64((u8)data);
    break;
  default:
    FAILURE(InvalidArgument, "kbc: ReadMem index out of range");
  }
}

/**
 * Enqueue scancode for a keypress or key-release. Used by the GUI
 *implementation to send keypresses to the keyboard controller.
 **/
void CKeyboard::gen_scancode(u32 key) {
  unsigned char *scancode;
  u8 i;

#if defined(DEBUG_KBD)
  printf("gen_scancode(): %s %s  \n", bx_keymap->getBXKeyName(key),
         (key >> 31) ? "released" : "pressed");
  if (!state.scancodes_translate)
    BX_DEBUG(("keyboard: gen_scancode with scancode_translate cleared"));
#endif

  // Ignore scancode if keyboard clock is driven low
  if (state.kbd_clock_enabled == 0)
    return;

  // Ignore scancode if scanning is disabled
  if (state.kbd_internal_buffer.scanning_enabled == 0)
    return;

  // Source: http://www.win.tue.nl/~aeb/linux/kbd/scancodes-10.html
  //
  // Three scancode sets
  //
  // The usual PC keyboards are capable of producing three sets
  // of scancodes. Writing 0xf0 followed by 1, 2 or 3 to port
  // 0x60 will put the keyboard in scancode mode 1, 2 or 3.
  // Writing 0xf0 followed by 0 queries the mode, resulting in
  // a scancode byte 43, 41 or 3f from the keyboard.
  //
  // Set 1 contains the values that the XT keyboard (with only
  // one set of scancodes) produced, with extensions for new
  // keys. Someone decided that another numbering was more
  // logical and invented scancode Set 2. However, it was
  // realized that new scancodes would break old programs, so
  // the keyboard output was fed to a 8042 microprocessor on
  // the motherboard that could translate Set 2 back into Set
  // 1. Indeed a smart construction. This is the default today.
  // Finally there is the PS/2 version, Set 3, more regular,
  // but used by almost nobody.
  //
  // Sets 2 and 3 are designed to be translated by the 8042.
  // Set 1 should not be translated.
  //
  // Make and Break Codes
  //
  // The key press / key release is coded as follows:
  //
  // For Set 1, if the make code of a key is c, the break
  // code will be c+0x80. If the make code is e0 c, the
  // break code will be e0 c+0x80. The Pause key has make
  // code e1 1d 45 e1 9d c5 and does not generate a break code.
  //
  // For Set 2, if the make code of a key is c, the break code
  // will be f0 c. If the make code is e0 c, the break code
  // will be e0 f0 c. The Pause key has the 8-byte make code
  // e1 14 77 e1 f0 14 f0 77.
  //
  // For Set 3, by default most keys do not generate a break
  // code - only CapsLock, LShift, RShift, LCtrl and LAlt do.
  // However, by default all non-traditional keys do generate
  // a break code - thus, LWin, RWin, Menu do, and for example
  // on the Microsoft Internet keyboard, so do Back, Forward,
  // Stop, Mail, Search, Favorites, Web/Home, MyComputer,
  // Calculator, Sleep. On my BTC keyboard, also the Macro key
  // does.
  //
  // In Scancode Mode 3 it is possible to enable or disable
  // key repeat and the production of break codes either on a
  // key-by-key basis or for all keys at once. And just like
  // for Set 2, key release is indicated by a f0 prefix in
  // those cases where it is indicated. There is nothing
  // special with the Pause key in scancode mode 3.
  if (key & BX_KEY_RELEASED)
    scancode =
        (unsigned char *)scancodes[(key & 0xFF)][state.current_scancodes_set]
            .brek;
  else
    scancode =
        (unsigned char *)scancodes[(key & 0xFF)][state.current_scancodes_set]
            .make;

  // Translation
  //
  // The 8042 microprocessor translates the incoming byte stream
  // produced by the keyboard, and turns an f0 prefix into an OR
  // with 80 for the next byte.
  //
  // Unless told not to translate, the keyboard controller translates
  // keyboard scancodes into the scancodes it returns to the CPU using
  // the following table (in hex):
  //
  // +----+-------------------------------------------------+
  // |    | 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f |
  // +----+-------------------------------------------------+
  // | 00 | ff 43 41 3f 3d 3b 3c 58 64 44 42 40 3e 0f 29 59 |
  // | 10 |     65 38 2a 70 1d 10 02 5a 66 71 2c 1f 1e 11 03 5b |
  // | 20 |     67 2e 2d 20 12 05 04 5c 68 39 2f 21 14 13 06 5d |
  // | 30 |     69 31 30 23 22 15 07 5e 6a 72 32 24 16 08 09 5f |
  // | 40 |     6b 33 25 17 18 0b 0a 60 6c 34 35 26 27 19 0c 61 |
  // | 50 |     6d 73 28 74 1a 0d 62 6e 3a 36 1c 1b 75 2b 63 76 |
  // | 60 |     55 56 77 78 79 7a 0e 7b 7c 4f 7d 4b 47 7e 7f 6f |
  // | 70 |     52 53 50 4c 4d 48 01 45 57 4e 51 4a 37 49 46 54 |
  // | 80 |     80 81 82 41 54 85 86 87 88 89 8a 8b 8c 8d 8e 8f |
  // | 90 |     90 91 92 93 94 95 96 97 98 99 9a 9b 9c 9d 9e 9f |
  // | a0 |     a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 aa ab ac ad ae af |
  // | b0 |     b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 ba bb bc bd be bf |
  // | c0 |     c0 c1 c2 c3 c4 c5 c6 c7 c8 c9 ca cb cc cd ce cf |
  // | d0 |     d0 d1 d2 d3 d4 d5 d6 d7 d8 d9 da db dc dd de df |
  // | e0 |     e0 e1 e2 e3 e4 e5 e6 e7 e8 e9 ea eb ec ed ee ef |
  // | f0 |     -  f1 f2 f3     f4 f5 f6 f7 f8 f9 fa fb fc fd fe ff |
  // +----+-------------------------------------------------+
  if (state.scancodes_translate) {

    // Translate before send
    u8 escaped = 0x00;

    for (i = 0; i < strlen((const char *)scancode); i++) {
      if (scancode[i] == 0xF0) {
        escaped = 0x80;
      } else {
#if defined(DEBUG_KBD)
        printf("gen_scancode(): writing translated %02x   \n",
               translation8042[scancode[i]] | escaped);
#endif
        enQ(translation8042[scancode[i]] | escaped);
        escaped = 0x00;
      }
    }
  } else {

    // Send raw data
    for (i = 0; i < strlen((const char *)scancode); i++) {
#if defined(DEBUG_KBD)
      printf("gen_scancode(): writing raw %02x   \n", scancode[i]);
#endif
      enQ(scancode[i]);
    }
  }
}

/**
 * Reset keyboard internals.
 **/
void CKeyboard::resetinternals(bool powerup) {
  state.kbd_internal_buffer.num_elements = 0;
  for (int i = 0; i < BX_KBD_ELEMENTS; i++)
    state.kbd_internal_buffer.buffer[i] = 0;
  state.kbd_internal_buffer.head = 0;

  state.kbd_internal_buffer.expecting_typematic = 0;
  state.kbd_internal_buffer.expecting_make_break = 0;

  // Default scancode set is mf2 (translation is controlled by the 8042)
  state.expecting_scancodes_set = 0;

  // state.current_scancodes_set = 1;
  state.current_scancodes_set = 2;

  // state.scancodes_translate = 1;
  if (powerup) {
    state.kbd_internal_buffer.expecting_led_write = 0;
    state.kbd_internal_buffer.delay = 1;          // 500 mS
    state.kbd_internal_buffer.repeat_rate = 0x0b; // 10.9 chars/sec
  }
}

/**
 * Enqueue a byte (scancode) into the keyboard buffer.
 **/
void CKeyboard::enQ(u8 scancode) {
  int tail;

#if defined(DEBUG_KBD)
  printf("enQ(0x%02x)", (unsigned)scancode);
#endif
  if (state.kbd_internal_buffer.num_elements >= BX_KBD_ELEMENTS) {
    printf("internal keyboard buffer full, ignoring scancode.(%02x)  \n",
           (unsigned)scancode);
    return;
  }

  /* enqueue scancode in multibyte internal keyboard buffer */
#if defined(DEBUG_KBD)
  BX_DEBUG(
      ("enQ: putting scancode 0x%02x in internal buffer", (unsigned)scancode));
#endif
  tail = (state.kbd_internal_buffer.head +
          state.kbd_internal_buffer.num_elements) %
         BX_KBD_ELEMENTS;
  state.kbd_internal_buffer.buffer[tail] = scancode;
  state.kbd_internal_buffer.num_elements++;

  if (!state.status.outb && state.kbd_clock_enabled) {
    state.timer_pending = 1;
    return;
  }
}

/**
 * Read a byte from keyboard port 60.
 **/
u8 CKeyboard::read_60() {
  u8 val;

  /* output buffer */
  if (state.status.auxb) { /* mouse byte available */
    val = state.aux_output_buffer;
    state.aux_output_buffer = 0;
    state.status.outb = 0;
    state.status.auxb = 0;
    state.irq12_requested = 0;

    if (state.kbd_controller_Qsize) {
      unsigned i;
      state.aux_output_buffer = state.kbd_controller_Q[0];
      state.status.outb = 1;
      state.status.auxb = 1;
      if (state.allow_irq12)
        state.irq12_requested = 1;
      for (i = 0; i < state.kbd_controller_Qsize - 1; i++) {

        // move Q elements towards head of queue by one
        state.kbd_controller_Q[i] = state.kbd_controller_Q[i + 1];
      }

      state.kbd_controller_Qsize--;
    }

    // DEV_pic_lower_irq(12);
    state.timer_pending = 1;
    execute();
#if defined(DEBUG_KBD)
    BX_DEBUG(("[mouse] read from 0x60 returns 0x%02x", val));
#endif
    return val;
  } else if (state.status.outb) { /* kbd byte available */
    val = state.kbd_output_buffer;
    state.status.outb = 0;
    state.status.auxb = 0;
    state.irq1_requested = 0;
    state.bat_in_progress = 0;

    if (state.kbd_controller_Qsize) {
      unsigned i;
      state.aux_output_buffer = state.kbd_controller_Q[0];
      state.status.outb = 1;
      state.status.auxb = 1;
      if (state.allow_irq1)
        state.irq1_requested = 1;
      for (i = 0; i < state.kbd_controller_Qsize - 1; i++) {

        // move Q elements towards head of queue by one
        state.kbd_controller_Q[i] = state.kbd_controller_Q[i + 1];
      }

#if defined(DEBUG_KBD)
      BX_DEBUG(("s.controller_Qsize: %02X", state.kbd_controller_Qsize));
#endif
      state.kbd_controller_Qsize--;
    }

    //      DEV_pic_lower_irq(1);
    state.timer_pending = 1;
    execute();
#if defined(DEBUG_KBD)
    BX_DEBUG(("READ(60) = %02x", (unsigned)val));
#endif
    return val;
  } else {
#if defined(DEBUG_KBD)
    BX_DEBUG(("num_elements = %d", state.kbd_internal_buffer.num_elements));
    BX_DEBUG(("read from port 60h with outb empty"));
    BX_DEBUG(("READ(60) = %02x", state.kbd_output_buffer));
#endif
    return state.kbd_output_buffer;
  }
}

/**
 * Read a byte from keyboard port 64
 *
 * The keyboard controller status register
 *
 * The keyboard controller has an 8-bit status register. It can be inspected by
 *the CPU by reading port 0x64. (Typically, it has the value 0x14: keyboard not
 *locked, self-test completed.)
 *
 * \code
 * +------+-----+------+------+-----+------+------+------+
 * | PARE |	TIM | AUXB | KEYL | C/D | SYSF | INPB | OUTB |
 * +------+-----+------+------+-----+------+------+------+
 * \endcode
 *
 * Bit 7: Parity error
 *    0: OK.
 *    1: Parity error with last byte.
 *
 * Bit 6: Timeout
 *    0: OK.
 *    1: General timeout.
 *
 * Bit 5: Auxiliary output buffer full
 *    Bit 0 tells whether a read from port 0x60 will be valid. If it is valid,
 *this bit 5 tells what data will be read from port 0x60. 0: Keyboard data. 1:
 *Mouse data.
 *
 * Bit 4: Keyboard lock
 *    0: Locked.
 *    1: Not locked.
 *
 * Bit 3: Command/Data
 *    0: Last write to input buffer was data (written via port 0x60).
 *    1: Last write to input buffer was a command (written via port 0x64). (This
 *bit is also referred to as Address Line A2.)
 *
 * Bit 2: System flag
 *    Set to 0 after power on reset. Set to 1 after successful completion of the
 *keyboard controller self-test (Basic Assurance Test, BAT). Can also be set by
 *command (see below).
 *
 * Bit 1: Input buffer status
 *    0: Input buffer empty, can be written.
 *    1: Input buffer full, don't write yet.
 *
 * Bit 0: Output buffer status
 *    0: Output buffer empty, don't read yet.
 *    1: Output buffer full, can be read. (Bit 5 tells whether the available
 *data is from keyboard or mouse.) This bit is cleared when port 0x60 is read.
 **/
u8 CKeyboard::read_64() {
  u8 val;

  /* status register */
  val = (state.status.pare << 7) | (state.status.tim << 6) |
        (state.status.auxb << 5) | (state.status.keyl << 4) |
        (state.status.c_d << 3) | (state.status.sysf << 2) |
        (state.status.inpb << 1) | (state.status.outb << 0);
  state.status.tim = 0;
#if defined(DEBUG_KBD)
  BX_DEBUG(("read from 0x64 returns 0x%02x", val));
#endif
  return val;
}

/**
 * Write a byte to keyboard port 60.
 **/
void CKeyboard::write_60(u8 value) {
#if defined(DEBUG_KBD)
  printf("kbd: port 60 write: %02x.   \n", value);
#endif

  // data byte written last to 0x60
  state.status.c_d = 0;

  // if expecting data byte from command last sent to port 64h
  if (state.expecting_port60h) {
    state.expecting_port60h = 0;
#if defined(DEBUG_KBD)
    if (state.status.inpb)
      printf("write to port 60h, not ready for write   \n");
#endif
    switch (state.last_comm) {
    case 0x60: // write command byte
    {

      // The keyboard controller is provided with some RAM, for example
      // 32 bytes, that can be accessed by the CPU. The most important
      // part of this RAM is byte 0, the Controller Command Byte (CCB).
      // It can be read/written by writing 0x20/0x60 to port 0x64 and
      // then reading/writing a data byte from/to port 0x60.
      //
      // This byte has the following layout.
      //
      // +---+-------+----+----+---+------+-----+-----+
      // | 0 | XLATE | ME | KE | 0 | SYSF | MIE | KIE |
      // +---+-------+----+----+---+------+-----+-----+
      //
      // Bit 6: Translate
      //    0: No translation.
      //    1: Translate keyboard scancodes, using the translation table
      //       given above. MCA type 2 controllers cannot set this bit
      //       to 1. In this case scan code conversion is set using
      //       keyboard command 0xf0 to port 0x60.
      //
      // Bit 5: Mouse enable
      //    0: Enable mouse.
      //    1: Disable mouse by driving the clock line low.
      //
      // Bit 4: Keyboard enable
      //    0: Enable keyboard.
      //    1: Disable keyboard by driving the clock line low.
      //
      // Bit 2: System flag
      //    This bit is shown in bit 2 of the status register. A
      //    "cold reboot" is one with this bit set to zero. A
      //    "warm reboot" is one with this bit set to one (BAT
      //    already completed). This will influence the tests and
      //    initializations done by the POST.
      //
      // Bit 1: Mouse interrupt enable
      //    0: Do not use mouse interrupts.
      //    1: Send interrupt request IRQ12 when the mouse output
      //       buffer is full.
      //
      // Bit 0: Keyboard interrupt enable
      //    0: Do not use keyboard interrupts.
      //    1: Send interrupt request IRQ1 when the keyboard output
      //       buffer is full.
      //
      //    When no interrupts are used, the CPU has to poll bits 0
      //    (and 5) of the status register.
      bool scan_convert;

      // The keyboard controller is provided with some RAM, for example
      bool disable_keyboard;

      // The keyboard controller is provided with some RAM, for example
      bool disable_aux;

      scan_convert = (value >> 6) & 0x01;
      disable_aux = (value >> 5) & 0x01;
      disable_keyboard = (value >> 4) & 0x01;
      state.status.sysf = (value >> 2) & 0x01;
      state.allow_irq1 = (value >> 0) & 0x01;
      state.allow_irq12 = (value >> 1) & 0x01;
      set_kbd_clock_enable(!disable_keyboard);
      set_aux_clock_enable(!disable_aux);
      if (state.allow_irq12 && state.status.auxb)
        state.irq12_requested = 1;
      else if (state.allow_irq1 && state.status.outb)
        state.irq1_requested = 1;

#if defined(DEBUG_KBD)
      BX_DEBUG((" allow_irq12 set to %u", (unsigned)state.allow_irq12));
      if (!scan_convert)
        BX_INFO(("keyboard: scan convert turned off"));
#endif

      // (mch) NT needs this
      state.scancodes_translate = scan_convert;
    } break;

    case 0xd1: // write output port
#if defined(DEBUG_KBD)
      BX_DEBUG(("write output port with value %02xh", (unsigned)value));
#endif
      break;

    case 0xd4: // Write to mouse
      // I don't think this enables the AUX clock
      // set_aux_clock_enable(1); // enable aux clock line
      ctrl_to_mouse(value);

      // ??? should I reset to previous value of aux enable?
      break;

    case 0xd3: // write mouse output buffer
      // Queue in mouse output buffer
      controller_enQ(value, 1);
      break;

    case 0xd2:

      // Queue in keyboard output buffer
      controller_enQ(value, 0);
      break;

    default:
      printf("=== unsupported write to port 60h(lastcomm=%02x): %02x   \n",
             (unsigned)state.last_comm, (unsigned)value);
    }
  } else {

    // data byte written last to 0x60
    state.status.c_d = 0;
    state.expecting_port60h = 0;

    /* pass byte to keyboard */

    /* ??? should conditionally pass to mouse device here ??? */
    if (state.kbd_clock_enabled == 0)
      set_kbd_clock_enable(1);
    ctrl_to_kbd(value);
  }

  execute();
}

/**
 * Write a byte to keyboard port 64.
 **/
void CKeyboard::write_64(u8 value) {
#if defined(DEBUG_KBD)
  printf("kbd: port 64 write: %02x.   \n", value);
#endif

  static int kbd_initialized = 0;
  u8 command_byte;

  // command byte written last to 0x64
  state.status.c_d = 1;
  state.last_comm = value;

  // most commands NOT expecting port60 write next
  state.expecting_port60h = 0;

  switch (value) {
  case 0x20: // get keyboard command byte
#if defined(DEBUG_KBD)
    BX_DEBUG(("get keyboard command byte"));
#endif

    // controller output buffer must be empty
    if (state.status.outb) {
#if defined(DEBUG_KBD)
      BX_ERROR(("kbd: OUTB set and command 0x%02x encountered", value));
#endif
      break;
    }

    command_byte = (state.scancodes_translate << 6) |
                   ((!state.aux_clock_enabled) << 5) |
                   ((!state.kbd_clock_enabled) << 4) | (0 << 3) |
                   (state.status.sysf << 2) | (state.allow_irq12 << 1) |
                   (state.allow_irq1 << 0);
    controller_enQ(command_byte, 0);
    break;

  case 0x60: // write command byte
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command 60: write command byte.   \n");
#endif

    // following byte written to port 60h is command byte
    state.expecting_port60h = 1;
    break;

  case 0xa0:
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command a0: BIOS name (not supported).   \n");
#endif
    break;

  case 0xa1:
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command a0: BIOS version (not supported).   \n");
#endif
    break;

  case 0xa7: // disable the aux device
    set_aux_clock_enable(0);
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command a7: aux i/f disable.   \n");
#endif
    break;

  case 0xa8: // enable the aux device
    set_aux_clock_enable(1);
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command a7: aux i/f enable.   \n");
#endif
    break;

  case 0xa9: // Test Mouse Port
             // controller output buffer must be empty
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command a9: aux i/f test.   \n");
#endif
    if (state.status.outb) {
      printf("kbd: OUTB set and command 0x%02x encountered", value);
      break;
    }

    controller_enQ(0x00, 0); // no errors detected
    break;

  case 0xaa: // motherboard controller self test
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command aa: self test.   \n");
#endif
    if (kbd_initialized == 0) {
      state.kbd_controller_Qsize = 0;
      state.status.outb = 0;
      kbd_initialized = 1;
    }

    // controller output buffer must be empty
    if (state.status.outb) {
      printf("kbd: OUTB set and command 0x%02x encountered", value);

      // break;
      // drain the queue?
      state.kbd_internal_buffer.head = 0;
      state.kbd_internal_buffer.num_elements = 0;
      state.status.outb = 0;
    }

    state.status.sysf = 1;   // self test complete
    controller_enQ(0x55, 0); // controller OK
    break;

  case 0xab: // Interface Test
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command ab: kbd i/f test.   \n");
#endif

    // controller output buffer must be empty
    if (state.status.outb) {
      printf("kbd: OUTB set and command 0x%02x encountered", value);
      break;
    }

    controller_enQ(0x00, 0);
    break;

  case 0xad: // disable keyboard
    set_kbd_clock_enable(0);
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command ad: kbd i/f disable.   \n");
#endif
    break;

  case 0xae: // enable keyboard
    set_kbd_clock_enable(1);
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command ae: kbd i/f enable.   \n");
#endif
    break;

  case 0xaf: // get controller version
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command af: controller version (not supported).   \n");
#endif
    break;

  case 0xc0: // read input port
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command c0: read input port.   \n");
#endif

    // controller output buffer must be empty
    if (state.status.outb) {
      BX_PANIC(("kbd: OUTB set and command 0x%02x encountered", value));
      break;
    }

    // keyboard not inhibited
    controller_enQ(0x80, 0);
    break;

  case 0xd0: // read output port: next byte read from port 60h
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command d0: read output port. (partial)   \n");
#endif

    // controller output buffer must be empty
    if (state.status.outb) {
      BX_PANIC(("kbd: OUTB set and command 0x%02x encountered", value));
      break;
    }

    controller_enQ((state.irq12_requested << 5) | (state.irq1_requested << 4) |
                       //              (BX_GET_ENABLE_A20() << 1) |
                       0x01,
                   0);
    break;

  case 0xd1: // write output port: next byte written to port 60h
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command d1: write output port.   \n");
#endif

    // following byte to port 60h written to output port
    state.expecting_port60h = 1;
    break;

  case 0xd3: // write mouse output buffer
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command d3: write aux output buffer.   \n");
#endif

    // following byte to port 60h written to output port as mouse write.
    state.expecting_port60h = 1;
    break;

  case 0xd4: // write to mouse
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command d4: write to aux.   \n");
#endif

    // following byte written to port 60h
    state.expecting_port60h = 1;
    break;

  case 0xd2: // write keyboard output buffer
#if defined(DEBUG_KBD)
    printf("kbd_ctrl: command d2: write kbd output buffer.   \n");
#endif
    state.expecting_port60h = 1;
    break;

  case 0xc1: // Continuous Input Port Poll, Low
  case 0xc2: // Continuous Input Port Poll, High
  case 0xe0: // Read Test Inputs
    BX_PANIC(("io write 0x64: command = %02xh", (unsigned)value));
    break;

  default:
    if (value == 0xff || (value >= 0xf0 && value <= 0xfd)) {

      /* useless pulse output bit commands ??? */
#if defined(DEBUG_KBD)
      BX_DEBUG(("io write to port 64h, useless command %02x", (unsigned)value));
#endif
      return;
    }

    BX_ERROR(("unsupported io write to keyboard port 64, value = %x",
              (unsigned)value));
    break;
  }

  execute();
}

/**
 * Enqueue a byte into one of the keyboard controller's output buffers.
 **/
void CKeyboard::controller_enQ(u8 data, unsigned source) {

  // source is 0 for keyboard, 1 for mouse
#if defined(DEBUG_KBD)
  BX_DEBUG(("controller_enQ(%02x) source=%02x", (unsigned)data, source));
#endif

  // see if we need to Q this byte from the controller
  // remember this includes mouse bytes.
  if (state.status.outb) {
    if (state.kbd_controller_Qsize >= BX_KBD_CONTROLLER_QSIZE)
      FAILURE(Runtime, "controller_enq(): controller_Q full!");
    state.kbd_controller_Q[state.kbd_controller_Qsize++] = data;
    state.kbd_controller_Qsource = source;
    return;
  }

  // the Q is empty
  if (source == 0) { // keyboard
    state.kbd_output_buffer = data;
    state.status.outb = 1;
    state.status.auxb = 0;
    state.status.inpb = 0;
    if (state.allow_irq1)
      state.irq1_requested = 1;
  } else { // mouse
    state.aux_output_buffer = data;
    state.status.outb = 1;
    state.status.auxb = 1;
    state.status.inpb = 0;
    if (state.allow_irq12)
      state.irq12_requested = 1;
  }
}

/**
 * Enable or disable the keyboard clock.
 **/
void CKeyboard::set_kbd_clock_enable(u8 value) {
  bool prev_kbd_clock_enabled;

  if (value == 0) {
    state.kbd_clock_enabled = 0;
  } else {

    /* is another byte waiting to be sent from the keyboard ? */
    prev_kbd_clock_enabled = state.kbd_clock_enabled;
    state.kbd_clock_enabled = 1;

    if (prev_kbd_clock_enabled == 0 && state.status.outb == 0)
      state.timer_pending = 1;
  }
}

/**
 * Enable or disable the mouse clock.
 **/
void CKeyboard::set_aux_clock_enable(u8 value) {
  bool prev_aux_clock_enabled;

#if defined(DEBUG_KBD)
  BX_DEBUG(("set_aux_clock_enable(%u)", (unsigned)value));
#endif
  if (value == 0) {
    state.aux_clock_enabled = 0;
  } else {

    /* is another byte waiting to be sent from the keyboard ? */
    prev_aux_clock_enabled = state.aux_clock_enabled;
    state.aux_clock_enabled = 1;
    if (prev_aux_clock_enabled == 0 && state.status.outb == 0)
      state.timer_pending = 1;
  }
}

/**
 * Send a byte from controller to keyboard
 **/
void CKeyboard::ctrl_to_kbd(u8 value) {
#if defined(DEBUG_KBD)
  BX_DEBUG(("controller passed byte %02xh to keyboard", value));
#endif
  if (state.kbd_internal_buffer.expecting_make_break) {
    state.kbd_internal_buffer.expecting_make_break = 0;
#if defined(DEBUG_KBD)
    printf("setting key %x to make/break mode (unused)   \n", value);
#endif
    enQ(0xFA); // send ACK
    return;
  }

  if (state.kbd_internal_buffer.expecting_typematic) {
    state.kbd_internal_buffer.expecting_typematic = 0;
    state.kbd_internal_buffer.delay = (value >> 5) & 0x03;
#if defined(DEBUG_KBD)
    switch (state.kbd_internal_buffer.delay) {
    case 0:
      BX_INFO(("setting delay to 250 mS (unused)"));
      break;
    case 1:
      BX_INFO(("setting delay to 500 mS (unused)"));
      break;
    case 2:
      BX_INFO(("setting delay to 750 mS (unused)"));
      break;
    case 3:
      BX_INFO(("setting delay to 1000 mS (unused)"));
      break;
    }
#endif
    state.kbd_internal_buffer.repeat_rate = value & 0x1f;
#if defined(DEBUG_KBD)
    double cps =
        1 /
        ((double)(8 + (value & 0x07)) *
         (double)exp(log((double)2) * (double)((value >> 3) & 0x03)) * 0.00417);
    BX_INFO(("setting repeat rate to %.1f cps (unused)", cps));
#endif
    enQ(0xFA); // send ACK
    return;
  }

  if (state.kbd_internal_buffer.expecting_led_write) {
    state.kbd_internal_buffer.expecting_led_write = 0;
    state.kbd_internal_buffer.led_status = value;
#if defined(DEBUG_KBD)
    BX_DEBUG(("LED status set to %02x",
              (unsigned)state.kbd_internal_buffer.led_status));
#endif
    enQ(0xFA); // send ACK %%%
    return;
  }

  if (state.expecting_scancodes_set) {
    state.expecting_scancodes_set = 0;
    if (value != 0) {
      if (value < 4) {
        state.current_scancodes_set = (value - 1);
#if defined(DEBUG_KBD)
        BX_INFO(("Switched to scancode set %d",
                 (unsigned)state.current_scancodes_set + 1));
#endif
        enQ(0xFA);
      } else {
        BX_ERROR(("Received scancodes set out of range: %d", value));
        enQ(0xFF); // send ERROR
      }
    } else {

      // Send ACK (SF patch #1159626)
      enQ(0xFA);

      // Send current scancodes set to port 0x60
      if (state.scancodes_translate)
        enQ(translation8042[1 + state.current_scancodes_set]);
      else
        enQ(1 + state.current_scancodes_set);
    }

    return;
  }

  switch (value) {

  //    case 0x00: // ??? ignore and let OS timeout with no response
  //#if defined(DEBUG_KBD)
  //      printf("kbd: command 00: ignored.   \n");
  //#endif
  //      enQ(0xFA); // send ACK %%%
  //      break;
  //
  //    case 0x05: // ???
  //#if defined(DEBUG_KBD)
  //      printf("kbd: command 05:  unknown.   \n");
  //#endif
  //      // (mch) trying to get this to work...
  //      state.status.sysf = 1;
  //      enQ_imm(0xfe);
  //      break;
  case 0xed: // LED Write
    state.kbd_internal_buffer.expecting_led_write = 1;
#if defined(DEBUG_KBD)
    printf("kbd: Expecting led write info.   \n");
#endif
    enQ_imm(0xFA); // send ACK %%%
    break;

  case 0xee: // echo
#if defined(DEBUG_KBD)
    printf("kbd: command ee: echo.   \n");
#endif
    enQ(0xEE); // return same byte (EEh) as echo diagnostic
    break;

  case 0xf0: // Select alternate scan code set
    state.expecting_scancodes_set = 1;
#if defined(DEBUG_KBD)
    printf("kbd: Expecting scancode set info.   \n");
#endif
    enQ(0xFA); // send ACK
    break;

  case 0xf2: // identify keyboard
#if defined(DEBUG_KBD)
    printf("kbd: command f2: identify keyboard.   \n");
#endif

    //  Keyboard IDs
    //
    // Keyboards do report an ID as a reply to the command f2. An MF2 AT
    // keyboard reports ID ab 83. Translation turns this into ab 41.
    enQ(0xFA);
    enQ(0xAB);

    if (state.scancodes_translate)
      enQ(0x41);
    else
      enQ(0x83);
    break;

  case 0xf3: // typematic info
    state.kbd_internal_buffer.expecting_typematic = 1;
#if defined(DEBUG_KBD)
    printf("kbd: Expecting typematic info.   \n");
#endif
    enQ(0xFA); // send ACK
    break;

  case 0xf4: // enable keyboard
    state.kbd_internal_buffer.scanning_enabled = 1;
#if defined(DEBUG_KBD)
    printf("kbd: command f4: enable keyboard.   \n");
#endif
    enQ(0xFA); // send ACK
    break;

  case 0xf5: // reset keyboard to power-up settings and disable scanning
    resetinternals(1);
    enQ(0xFA); // send ACK
    state.kbd_internal_buffer.scanning_enabled = 0;
#if defined(DEBUG_KBD)
    printf("kbd: command f5: reset and disable keyboard.   \n");
#endif
    break;

  case 0xf6: // reset keyboard to power-up settings and enable scanning
    resetinternals(1);
    enQ(0xFA); // send ACK
    state.kbd_internal_buffer.scanning_enabled = 1;
#if defined(DEBUG_KBD)
    printf("kbd: command f6: reset and enable keyboard.   \n");
#endif
    break;

  case 0xfc: // PS/2 Set Key Type to Make/Break
    state.kbd_internal_buffer.expecting_make_break = 1;
#if defined(DEBUG_KBD)
    printf("kbd: Expecting make/break info.   \n");
#endif
    enQ(0xFA); /* send ACK */
    break;

  case 0xfe: // resend. aiiee.
    printf("kbd: resend command received.   \n");
    break;

  case 0xff: // reset: internal keyboard reset and afterwards the BAT
#if defined(DEBUG_KBD)
    printf("kbd: command ff: reset keyboard w/BAT.   \n");
#endif
    resetinternals(1);
    enQ(0xFA); // send ACK
    state.bat_in_progress = 1;
    enQ(0xAA); // BAT test passed
    break;

  // case 0xd3:
  //  enQ(0xfa);
  //  break;
  case 0xf7: // PS/2 Set All Keys To Typematic
  case 0xf8: // PS/2 Set All Keys to Make/Break
  case 0xf9: // PS/2 PS/2 Set All Keys to Make
  case 0xfa: // PS/2 Set All Keys to Typematic Make/Break
  case 0xfb: // PS/2 Set Key Type to Typematic
  case 0xfd: // PS/2 Set Key Type to Make
    printf("kbd: unhandled command: %02x, ACKing     \n", value);
    enQ(0xFA);
    break;

  default:
    printf("kbd: command %02x: not recognized!   \n", value);
    enQ(0xFE); /* send NACK */
    break;
  }
}

/**
 * enqueue scancode in multibyte internal keyboard buffer
 **/
void CKeyboard::enQ_imm(u8 val) {
  int tail;

  if (state.kbd_internal_buffer.num_elements >= BX_KBD_ELEMENTS) {
    BX_PANIC(("internal keyboard buffer full (imm)"));
    return;
  }

  tail = (state.kbd_internal_buffer.head +
          state.kbd_internal_buffer.num_elements) %
         BX_KBD_ELEMENTS;

  state.kbd_output_buffer = val;
  state.status.outb = 1;

  if (state.allow_irq1)
    state.irq1_requested = 1;
}

/**
 * Send a byte from controller to mouse
 **/
void CKeyboard::ctrl_to_mouse(u8 value) {
#if defined(DEBUG_KBD)
  BX_DEBUG(("MOUSE: ctrl_to_mouse(%02xh)", (unsigned)value));
  BX_DEBUG(("  enable = %u", (unsigned)state.mouse.enable));
  BX_DEBUG(("  allow_irq12 = %u", (unsigned)state.allow_irq12));
  BX_DEBUG(("  aux_clock_enabled = %u", (unsigned)state.aux_clock_enabled));
#endif

  // an ACK (0xFA) is always the first response to any valid input
  // received from the system other than Set-Wrap-Mode & Resend-Command
  if (state.expecting_mouse_parameter) {
    state.expecting_mouse_parameter = 0;
    switch (state.last_mouse_command) {
    case 0xf3: // Set Mouse Sample Rate
      state.mouse.sample_rate = value;
#if defined(DEBUG_KBD)
      BX_DEBUG(("[mouse] Sampling rate set: %d Hz", value));
#endif
      if ((value == 200) && (!state.mouse.im_request)) {
        state.mouse.im_request = 1;
      } else if ((value == 100) && (state.mouse.im_request == 1)) {
        state.mouse.im_request = 2;
      } else if ((value == 80) && (state.mouse.im_request == 2)) {
#if defined(DEBUG_KBD)
        BX_INFO(("wheel mouse mode enabled"));
#endif
        state.mouse.im_mode = 1;
        state.mouse.im_request = 0;
      } else {
        state.mouse.im_request = 0;
      }

      controller_enQ(0xFA, 1); // ack
      break;

    case 0xe8: // Set Mouse Resolution
      switch (value) {
      case 0:
        state.mouse.resolution_cpmm = 1;
        break;
      case 1:
        state.mouse.resolution_cpmm = 2;
        break;
      case 2:
        state.mouse.resolution_cpmm = 4;
        break;
      case 3:
        state.mouse.resolution_cpmm = 8;
        break;
      default:
        BX_PANIC(("[mouse] Unknown resolution %d", value));
        break;
      }

#if defined(DEBUG_KBD)
      BX_DEBUG(("[mouse] Resolution set to %d counts per mm",
                state.mouse.resolution_cpmm));
#endif
      controller_enQ(0xFA, 1); // ack
      break;

    default:
      BX_PANIC(("MOUSE: unknown last command (%02xh)",
                (unsigned)state.last_mouse_command));
    }
  } else {
    state.expecting_mouse_parameter = 0;
    state.last_mouse_command = value;

    // test for wrap mode first
    if (state.mouse.mode == MOUSE_MODE_WRAP) {

      // if not a reset command or reset wrap mode
      // then just echo the byte.
      if ((value != 0xff) && (value != 0xec)) {

        //        if (bx_dbg.mouse)
#if defined(DEBUG_KBD)
        BX_INFO(("[mouse] wrap mode: Ignoring command %0X02.", value));
#endif
        controller_enQ(value, 1);

        // bail out
        return;
      }
    }

    switch (value) {
    case 0xe6:                 // Set Mouse Scaling to 1:1
      controller_enQ(0xFA, 1); // ACK
      state.mouse.scaling = 2;
#if defined(DEBUG_KBD)
      BX_DEBUG(("[mouse] Scaling set to 1:1"));
#endif
      break;

    case 0xe7:                 // Set Mouse Scaling to 2:1
      controller_enQ(0xFA, 1); // ACK
      state.mouse.scaling = 2;
#if defined(DEBUG_KBD)
      BX_DEBUG(("[mouse] Scaling set to 2:1"));
#endif
      break;

    case 0xe8:                 // Set Mouse Resolution
      controller_enQ(0xFA, 1); // ACK
      state.expecting_mouse_parameter = 1;
      break;

    case 0xea: // Set Stream Mode
               //        if (bx_dbg.mouse)
#if defined(DEBUG_KBD)
      BX_INFO(("[mouse] Mouse stream mode on."));
#endif
      state.mouse.mode = MOUSE_MODE_STREAM;
      controller_enQ(0xFA, 1); // ACK
      break;

    case 0xec: // Reset Wrap Mode
      // unless we are in wrap mode ignore the command
      if (state.mouse.mode == MOUSE_MODE_WRAP) {

        //          if (bx_dbg.mouse)
#if defined(DEBUG_KBD)
        BX_INFO(("[mouse] Mouse wrap mode off."));
#endif

        // restore previous mode except disable stream mode reporting.
        // ### TODO disabling reporting in stream mode
        state.mouse.mode = state.mouse.saved_mode;
        controller_enQ(0xFA, 1); // ACK
      }
      break;

    case 0xee: // Set Wrap Mode
               // ### TODO flush output queue.
               // ### TODO disable interrupts if in stream mode.
               //        if (bx_dbg.mouse)
#if defined(DEBUG_KBD)
      BX_INFO(("[mouse] Mouse wrap mode on."));
#endif
      state.mouse.saved_mode = state.mouse.mode;
      state.mouse.mode = MOUSE_MODE_WRAP;
      controller_enQ(0xFA, 1); // ACK
      break;

    case 0xf0: // Set Remote Mode (polling mode, i.e. not stream mode.)
               //        if (bx_dbg.mouse)
#if defined(DEBUG_KBD)
      BX_INFO(("[mouse] Mouse remote mode on."));
#endif

      // ### TODO should we flush/discard/ignore any already queued packets?
      state.mouse.mode = MOUSE_MODE_REMOTE;
      controller_enQ(0xFA, 1); // ACK
      break;

    case 0xf2:                 // Read Device Type
      controller_enQ(0xFA, 1); // ACK
      if (state.mouse.im_mode)
        controller_enQ(0x03, 1); // Device ID (wheel z-mouse)
      else
        controller_enQ(0x00, 1); // Device ID (standard)
#if defined(DEBUG_KBD)
      BX_DEBUG(("[mouse] Read mouse ID"));
#endif
      break;

    case 0xf3: // Set Mouse Sample Rate (sample rate written to port 60h)
      controller_enQ(0xFA, 1); // ACK
      state.expecting_mouse_parameter = 1;
      break;

    case 0xf4: // Enable (in stream mode)
      state.mouse.enable = 1;
      controller_enQ(0xFA, 1); // ACK
#if defined(DEBUG_KBD)
      BX_DEBUG(("[mouse] Mouse enabled (stream mode)"));
#endif
      break;

    case 0xf5: // Disable (in stream mode)
      state.mouse.enable = 0;
      controller_enQ(0xFA, 1); // ACK
#if defined(DEBUG_KBD)
      BX_DEBUG(("[mouse] Mouse disabled (stream mode)"));
#endif
      break;

    case 0xf6:                         // Set Defaults
      state.mouse.sample_rate = 100;   /* reports per second (default) */
      state.mouse.resolution_cpmm = 4; /* 4 counts per millimeter (default) */
      state.mouse.scaling = 1;         /* 1:1 (default) */
      state.mouse.enable = 0;
      state.mouse.mode = MOUSE_MODE_STREAM;
      controller_enQ(0xFA, 1); // ACK
#if defined(DEBUG_KBD)
      BX_DEBUG(("[mouse] Set Defaults"));
#endif
      break;

    case 0xff:                         // Reset
      state.mouse.sample_rate = 100;   /* reports per second (default) */
      state.mouse.resolution_cpmm = 4; /* 4 counts per millimeter (default) */
      state.mouse.scaling = 1;         /* 1:1 (default) */
      state.mouse.mode = MOUSE_MODE_RESET;
      state.mouse.enable = 0;
#if defined(DEBUG_KBD)
      if (state.mouse.im_mode)
        BX_INFO(("wheel mouse mode disabled"));
#endif
      state.mouse.im_mode = 0;
      controller_enQ(0xFA, 1); // ACK
      controller_enQ(0xAA, 1); // completion code
      controller_enQ(0x00, 1); // ID code (standard after reset)
#if defined(DEBUG_KBD)
      BX_DEBUG(("[mouse] Mouse reset"));
#endif
      break;

    case 0xe9: // Get mouse information
      // should we ack here? (mch): Yes
      controller_enQ(0xFA, 1);                              // ACK
      controller_enQ(state.mouse.get_status_byte(), 1);     // status
      controller_enQ(state.mouse.get_resolution_byte(), 1); // resolution
      controller_enQ(state.mouse.sample_rate, 1);           // sample rate
#if defined(DEBUG_KBD)
      BX_DEBUG(("[mouse] Get mouse information"));
#endif
      break;

    case 0xeb:                 // Read Data (send a packet when in Remote Mode)
      controller_enQ(0xFA, 1); // ACK

      // perhaps we should be adding some movement here.
      mouse_enQ_packet(((state.mouse.button_status & 0x0f) | 0x08), 0x00, 0x00,
                       0x00); // bit3 of first byte always set

      // assumed we really aren't in polling mode, a rather odd assumption.
#if defined(DEBUG_KBD)
      BX_ERROR(("[mouse] Warning: Read Data command partially supported."));
#endif
      break;

    case 0xbb: // OS/2 Warp 3 uses this command
#if defined(DEBUG_KBD)
      BX_ERROR(("[mouse] ignoring 0xbb command"));
#endif
      break;

    default:
      BX_ERROR(("[mouse] ctrl_to_mouse(): got value of 0x%02x", value));
      controller_enQ(0xFE, 1); /* send NACK */
    }
  }
}

/**
 * Put a (3/4 byte) mouse packet into the mouse buffer.
 **/
bool CKeyboard::mouse_enQ_packet(u8 b1, u8 b2, u8 b3, u8 b4) {
  int bytes = 3;
  if (state.mouse.im_mode)
    bytes = 4;

  if ((state.mouse_internal_buffer.num_elements + bytes) >=
      BX_MOUSE_BUFF_SIZE) {
    return (0); /* buffer doesn't have the space */
  }

  mouse_enQ(b1);
  mouse_enQ(b2);
  mouse_enQ(b3);
  if (state.mouse.im_mode)
    mouse_enQ(b4);

  return (1);
}

/**
 * Put a byte into the mouse buffer.
 **/
void CKeyboard::mouse_enQ(u8 mouse_data) {
  int tail;

#if defined(DEBUG_KBD)
  BX_DEBUG(("mouse_enQ(%02x)", (unsigned)mouse_data));
#endif
  if (state.mouse_internal_buffer.num_elements >= BX_MOUSE_BUFF_SIZE) {
    BX_ERROR(("[mouse] internal mouse buffer full, ignoring mouse data.(%02x)",
              (unsigned)mouse_data));
    return;
  }

  /* enqueue mouse data in multibyte internal mouse buffer */
  tail = (state.mouse_internal_buffer.head +
          state.mouse_internal_buffer.num_elements) %
         BX_MOUSE_BUFF_SIZE;
  state.mouse_internal_buffer.buffer[tail] = mouse_data;
  state.mouse_internal_buffer.num_elements++;

  if (!state.status.outb && state.aux_clock_enabled) {
    state.timer_pending = 1;
    return;
  }
}

/**
 * Determine what IRQ's need to be asserted.
 **/
unsigned CKeyboard::periodic() {
  u8 retval;

  retval = (state.irq1_requested << 0) | (state.irq12_requested << 1);
  state.irq1_requested = 0;
  state.irq12_requested = 0;

  if (state.timer_pending == 0) {
    return (retval);
  }

  if (1 >= state.timer_pending) {
    state.timer_pending = 0;
  } else {
    state.timer_pending--;
    return (retval);
  }

  if (state.status.outb) {
    return (retval);
  }

  /* nothing in outb, look for possible data xfer from keyboard or mouse */
  if (state.kbd_internal_buffer.num_elements &&
      (state.kbd_clock_enabled || state.bat_in_progress)) {
#if defined(DEBUG_KBD)
    BX_DEBUG(("service_keyboard: key in internal buffer waiting"));
#endif
    state.kbd_output_buffer =
        state.kbd_internal_buffer.buffer[state.kbd_internal_buffer.head];
    state.status.outb = 1;

    // commented out since this would override the current state of the
    // mouse buffer flag - no bug seen - just seems wrong (das)
    //    state.auxb = 0;
    state.kbd_internal_buffer.head =
        (state.kbd_internal_buffer.head + 1) % BX_KBD_ELEMENTS;
    state.kbd_internal_buffer.num_elements--;
    if (state.allow_irq1)
      state.irq1_requested = 1;
  } else {
    create_mouse_packet(0);
    if (state.aux_clock_enabled && state.mouse_internal_buffer.num_elements) {
#if defined(DEBUG_KBD)
      BX_DEBUG(
          ("service_keyboard: key(from mouse) in internal buffer waiting"));
#endif
      state.aux_output_buffer =
          state.mouse_internal_buffer.buffer[state.mouse_internal_buffer.head];

      state.status.outb = 1;
      state.status.auxb = 1;
      state.mouse_internal_buffer.head =
          (state.mouse_internal_buffer.head + 1) % BX_MOUSE_BUFF_SIZE;
      state.mouse_internal_buffer.num_elements--;
      if (state.allow_irq12)
        state.irq12_requested = 1;
    }

#if defined(DEBUG_KBD)
    else {
      BX_DEBUG(("service_keyboard(): no keys waiting"));
    }
#endif
  }

  return (retval);
}

/**
 * Create a mouse packet and send it to the controller if needed.
 **/
void CKeyboard::create_mouse_packet(bool force_enq) {
  u8 b1;

  u8 b2;

  u8 b3;

  u8 b4;

  if (state.mouse_internal_buffer.num_elements && !force_enq)
    return;

  s16 delta_x = state.mouse.delayed_dx;
  s16 delta_y = state.mouse.delayed_dy;
  u8 button_state = state.mouse.button_status | 0x08;

  if (!force_enq && !delta_x && !delta_y) {
    return;
  }

  if (delta_x > 254)
    delta_x = 254;
  if (delta_x < -254)
    delta_x = -254;
  if (delta_y > 254)
    delta_y = 254;
  if (delta_y < -254)
    delta_y = -254;

  b1 = (button_state & 0x0f) | 0x08; // bit3 always set
  if ((delta_x >= 0) && (delta_x <= 255)) {
    b2 = (u8)delta_x;
    state.mouse.delayed_dx -= delta_x;
  } else if (delta_x > 255) {
    b2 = (u8)0xff;
    state.mouse.delayed_dx -= 255;
  } else if (delta_x >= -256) {
    b2 = (u8)delta_x;
    b1 |= 0x10;
    state.mouse.delayed_dx -= delta_x;
  } else {
    b2 = (u8)0x00;
    b1 |= 0x10;
    state.mouse.delayed_dx += 256;
  }

  if ((delta_y >= 0) && (delta_y <= 255)) {
    b3 = (u8)delta_y;
    state.mouse.delayed_dy -= delta_y;
  } else if (delta_y > 255) {
    b3 = (u8)0xff;
    state.mouse.delayed_dy -= 255;
  } else if (delta_y >= -256) {
    b3 = (u8)delta_y;
    b1 |= 0x20;
    state.mouse.delayed_dy -= delta_y;
  } else {
    b3 = (u8)0x00;
    b1 |= 0x20;
    state.mouse.delayed_dy += 256;
  }

  b4 = (u8)-state.mouse.delayed_dz;

  mouse_enQ_packet(b1, b2, b3, b4);
}

/**
 * Keyboard clock. Handle events on a clocked basis.
 *
 * Do the following:
 *  - Let the GUI (if available) handle any pending events.
 *  - Check if interrupts need to be asserted.
 *  - Assert interrupts as needed.
 *  .
 **/
void CKeyboard::execute() {
  unsigned retval;

  /* -- moved to VGA card --
    if(bx_gui)
    {
      bx_gui->lock();
      bx_gui->handle_events();
      bx_gui->unlock();
    }
  */
  retval = periodic();

  if (retval & 0x01)
    theAli->pic_interrupt(0, 1);
  if (retval & 0x02)
    theAli->pic_interrupt(1, 4);
}

/**
 * Check if threads are still running.
 **/
void CKeyboard::check_state() {
  if (myThreadDead.load())
    FAILURE(Thread, "KBD thread has died");
}

/**
 * Thread entry point.
 **/
void CKeyboard::run() {
  try {
    for (;;) {
      if (StopThread)
        return;
      execute();
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }

  catch (CException &e) {
    printf("Exception in kbd thread: %s.\n", e.displayText().c_str());
    myThreadDead.store(true);
    // Let the thread die...
  }
}

static u32 kb_magic1 = 0x65481687;
static u32 kb_magic2 = 0x24895375;

/**
 * Save state to a Virtual Machine State file.
 **/
int CKeyboard::SaveState(FILE *f) {
  long ss = sizeof(state);

  fwrite(&kb_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&kb_magic2, sizeof(u32), 1, f);
  printf("kbc: %d bytes saved.\n", (int)ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CKeyboard::RestoreState(FILE *f) {
  long ss;
  u32 m1;
  u32 m2;
  size_t r;

  r = fread(&m1, sizeof(u32), 1, f);
  if (r != 1) {
    printf("kbc: unexpected end of file!\n");
    return -1;
  }

  if (m1 != kb_magic1) {
    printf("kbc: MAGIC 1 does not match!\n");
    return -1;
  }

  fread(&ss, sizeof(long), 1, f);
  if (r != 1) {
    printf("kbc: unexpected end of file!\n");
    return -1;
  }

  if (ss != sizeof(state)) {
    printf("kbc: STRUCT SIZE does not match!\n");
    return -1;
  }

  fread(&state, sizeof(state), 1, f);
  if (r != 1) {
    printf("kbc: unexpected end of file!\n");
    return -1;
  }

  r = fread(&m2, sizeof(u32), 1, f);
  if (r != 1) {
    printf("kbc: unexpected end of file!\n");
    return -1;
  }

  if (m2 != kb_magic2) {
    printf("kbc: MAGIC 1 does not match!\n");
    return -1;
  }

  printf("kbc: %d bytes restored.\n", (int)ss);
  return 0;
}

CKeyboard *theKeyboard = 0;
