/*  ES40 emulator.
 *
 *  This file is based upon GXemul.
 *
 *  Copyright (C) 2004-2007  Anders Gavare.  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */

/**
 * \file
 * Contains code for debugging.
 *
 * X-1.4        Camiel Vanderhoeven                             29-DEC-2007
 *      Compileable with older compilers (VC 6.0).
 *
 * X-1.3        Camiel Vanderhoeven                             28-DEC-2007
 *      Throw exceptions rather than just exiting when errors occur.
 *
 * X-1.2        Camiel Vanderhoeven                             15-NOV-2007
 *      Added newline at end to avoid warnings.
 *
 * X-1.1        Camiel Vanderhoeven                             14-NOV-2007
 *      Initial creation.
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/

/*****************************************************************************
 *
 *  NOTE:  debug(), fatal(), and debug_indentation() are not re-entrant.
 *         The global variable quiet_mode can be used to suppress the output
 *         of debug(), but not the output of fatal().
 *
 *****************************************************************************/
#include "StdAfx.h"

// int verbose = 0;
int quiet_mode = 0;

static int debug_indent = 0;
static int debug_currently_at_start_of_line = 1;

/*
 *  va_debug():
 *
 *  Used internally by debug() and fatal().
 */
static void va_debug(va_list argp, char *fmt) {
  char buf[DEBUG_BUFSIZE + 1];
  char *s;
  int i;

  buf[0] = buf[DEBUG_BUFSIZE] = 0;

  // vsnprintf(buf, DEBUG_BUFSIZE, fmt, argp);
  sprintf(buf, fmt, argp);

  s = buf;
  while (*s) {
    if (debug_currently_at_start_of_line) {
      for (i = 0; i < debug_indent; i++)
        printf(" ");
    }

    printf("%c", *s);

    debug_currently_at_start_of_line = 0;
    if (*s == '\n' || *s == '\r')
      debug_currently_at_start_of_line = 1;
    s++;
  }
}

/*
 *  debug_indentation():
 *
 *  Modify the debug indentation.
 */
void debug_indentation(int diff) {
  debug_indent += diff;
  if (debug_indent < 0)
    fprintf(stderr, "WARNING: debug_indent less than 0!\n");
}

/*
 *  debug():
 *
 *  Debug output (ignored if quiet_mode is set).
 */
void debug(char *fmt, ...) {
  va_list argp;

  if (quiet_mode)
    return;

  va_start(argp, fmt);
  va_debug(argp, fmt);
  va_end(argp);
}

/*
 *  fatal():
 *
 *  Fatal works like debug(), but doesn't care about the quiet_mode
 *  setting.
 */
void fatal(char *fmt, ...) {
  va_list argp;

  va_start(argp, fmt);
  va_debug(argp, fmt);
  va_end(argp);
}
