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
 *
 * Parts of this file based upon the Poco C++ Libraries, which is Copyright (C)
 * 2004-2006, Applied Informatics Software Engineering GmbH. and Contributors.
 */

//
// Timestamp.cpp
//
// $Id: Timestamp.cpp,v 1.1 2008/05/31 15:47:29 iamcamiel Exp $
//
// Library: Foundation
// Package: DateTime
// Module:  Timestamp
//
// Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

#include "Timestamp.hpp"
#include "Exception.hpp"
#include <algorithm>
#if defined(POCO_OS_FAMILY_UNIX)
#include <sys/time.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>
#elif defined(POCO_OS_FAMILY_WINDOWS)
#include "UnWindows.hpp"
#endif

CTimestamp::CTimestamp() { update(); }

CTimestamp::CTimestamp(TimeVal tv) { _ts = tv; }

CTimestamp::CTimestamp(const CTimestamp &other) { _ts = other._ts; }

CTimestamp::~CTimestamp() {}

CTimestamp &CTimestamp::operator=(const CTimestamp &other) {
  _ts = other._ts;
  return *this;
}

CTimestamp &CTimestamp::operator=(TimeVal tv) {
  _ts = tv;
  return *this;
}

void CTimestamp::swap(CTimestamp &timestamp) { std::swap(_ts, timestamp._ts); }

CTimestamp CTimestamp::fromEpochTime(std::time_t t) {
  return CTimestamp(TimeVal(t) * resolution());
}

CTimestamp CTimestamp::fromUtcTime(UtcTimeVal val) {
  val -= (TimeDiff(0x01b21dd2) << 32) + 0x13814000;
  val /= 10;
  return CTimestamp(val);
}

void CTimestamp::update() {
#if defined(POCO_OS_FAMILY_WINDOWS)

  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  ULARGE_INTEGER epoch; // UNIX epoch (1970-01-01 00:00:00) expressed in Windows
                        // NT FILETIME
  epoch.LowPart = 0xD53E8000;
  epoch.HighPart = 0x019DB1DE;

  ULARGE_INTEGER ts;
  ts.LowPart = ft.dwLowDateTime;
  ts.HighPart = ft.dwHighDateTime;
  ts.QuadPart -= epoch.QuadPart;
  _ts = ts.QuadPart / 10;

#else

  struct timeval tv;
  if (gettimeofday(&tv, NULL))
    throw CSystemException("cannot get time of day");
  _ts = TimeVal(tv.tv_sec) * resolution() + tv.tv_usec;

#endif
}

#if defined(_WIN32)

CTimestamp CTimestamp::fromFileTimeNP(UInt32 fileTimeLow, UInt32 fileTimeHigh) {
  ULARGE_INTEGER epoch; // UNIX epoch (1970-01-01 00:00:00) expressed in Windows
                        // NT FILETIME
  epoch.LowPart = 0xD53E8000;
  epoch.HighPart = 0x019DB1DE;

  ULARGE_INTEGER ts;
  ts.LowPart = fileTimeLow;
  ts.HighPart = fileTimeHigh;
  ts.QuadPart -= epoch.QuadPart;

  return CTimestamp(ts.QuadPart / 10);
}

void CTimestamp::toFileTimeNP(UInt32 &fileTimeLow, UInt32 &fileTimeHigh) const {
  ULARGE_INTEGER epoch; // UNIX epoch (1970-01-01 00:00:00) expressed in Windows
                        // NT FILETIME
  epoch.LowPart = 0xD53E8000;
  epoch.HighPart = 0x019DB1DE;

  ULARGE_INTEGER ts;
  ts.QuadPart = _ts * 10;
  ts.QuadPart += epoch.QuadPart;
  fileTimeLow = ts.LowPart;
  fileTimeHigh = ts.HighPart;
}

#endif
