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
// Mutex.cpp
//
// $Id: Mutex.cpp,v 1.2 2008/06/12 06:52:33 iamcamiel Exp $
//
// Library: Foundation
// Package: Threading
// Module:  Mutex
//
// Copyright (c) 2004-2008, Applied Informatics Software Engineering GmbH.
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

#include "Mutex.hpp"
#include <string.h>

#if defined(POCO_OS_FAMILY_WINDOWS)
#include "Mutex_WIN32.cpp"
#else
#include "Mutex_POSIX.cpp"
#endif

CMutex::CMutex() { lockName = strdup("anon"); }

CMutex::CMutex(const char *lName) { lockName = strdup(lName); }

CMutex::~CMutex() { free(lockName); }

CFastMutex::CFastMutex() { lockName = strdup("anon"); }

CFastMutex::CFastMutex(const char *lName) { lockName = strdup(lName); }

CFastMutex::~CFastMutex() { free(lockName); }

void CMutex::lock() {
#if defined(DEBUG_LOCKS)
  printf("        LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    lockImpl();
  }

  catch (CException &e) {
    FAILURE_3(Thread,
              "Locking error (%s) trying to lock mutex %s from thread %s.\n",
              e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }

#if defined(DEBUG_LOCKS)
  printf("      LOCKED mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
}

void CMutex::lock(long milliseconds) {
#if defined(DEBUG_LOCKS)
  printf("        LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    if (!tryLockImpl(milliseconds))
      FAILURE(Timeout, "Timeout");
  }

  catch (CException &e) {
    FAILURE_3(Thread,
              "Locking error (%s) trying to lock mutex %s from thread %s.\n",
              e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }

#if defined(DEBUG_LOCKS)
  printf("      LOCKED mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
}

bool CMutex::tryLock() {
  bool res;
#if defined(DEBUG_LOCKS)
  printf("    TRY LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    res = tryLockImpl();
  }

  catch (CException &e) {
    FAILURE_3(Thread,
              "Locking error (%s) trying to lock mutex %s from thread %s.\n",
              e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }

#if defined(DEBUG_LOCKS)
  printf("  %s mutex %s from thread %s.   \n",
         res ? "    LOCKED" : "CAN'T LOCK", lockName, CURRENT_THREAD_NAME);
#endif
  return res;
}

bool CMutex::tryLock(long milliseconds) {
  bool res;
#if defined(DEBUG_LOCKS)
  printf("    TRY LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    res = tryLockImpl(milliseconds);
  }

  catch (CException &e) {
    FAILURE_3(Thread,
              "Locking error (%s) trying to lock mutex %s from thread %s.\n",
              e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }

#if defined(DEBUG_LOCKS)
  printf("  %s mutex %s from thread %s.   \n",
         res ? "    LOCKED" : "CAN'T LOCK", lockName, CURRENT_THREAD_NAME);
#endif
  return res;
}

void CMutex::unlock() {
#if defined(DEBUG_LOCKS)
  printf("      UNLOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    unlockImpl();
  }

  catch (CException &e) {
    FAILURE_3(Thread,
              "Locking error (%s) trying to unlock mutex %s from thread %s.\n",
              e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }

#if defined(DEBUG_LOCKS)
  printf("    UNLOCKED mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
}

void CFastMutex::lock() {
#if defined(DEBUG_LOCKS)
  printf("        LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    lockImpl();
  }

  catch (CException &e) {
    FAILURE_3(Thread,
              "Locking error (%s) trying to lock mutex %s from thread %s.\n",
              e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }

#if defined(DEBUG_LOCKS)
  printf("      LOCKED mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
}

void CFastMutex::lock(long milliseconds) {
#if defined(DEBUG_LOCKS)
  printf("        LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    if (!tryLockImpl(milliseconds))
      FAILURE(Timeout, "Timeout");
  }

  catch (CException &e) {
    FAILURE_3(Thread,
              "Locking error (%s) trying to lock mutex %s from thread %s.\n",
              e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }

#if defined(DEBUG_LOCKS)
  printf("      LOCKED mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
}

bool CFastMutex::tryLock() {
  bool res;
#if defined(DEBUG_LOCKS)
  printf("    TRY LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    res = tryLockImpl();
  }

  catch (CException &e) {
    FAILURE_3(Thread,
              "Locking error (%s) trying to lock mutex %s from thread %s.\n",
              e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }

#if defined(DEBUG_LOCKS)
  printf("  %s mutex %s from thread %s.   \n",
         res ? "    LOCKED" : "CAN'T LOCK", lockName, CURRENT_THREAD_NAME);
#endif
  return res;
}

bool CFastMutex::tryLock(long milliseconds) {
  bool res;
#if defined(DEBUG_LOCKS)
  printf("    TRY LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    res = tryLockImpl(milliseconds);
  }

  catch (CException &e) {
    FAILURE_3(Thread,
              "Locking error (%s) trying to lock mutex %s from thread %s.\n",
              e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }

#if defined(DEBUG_LOCKS)
  printf("  %s mutex %s from thread %s.   \n",
         res ? "    LOCKED" : "CAN'T LOCK", lockName, CURRENT_THREAD_NAME);
#endif
  return res;
}

void CFastMutex::unlock() {
#if defined(DEBUG_LOCKS)
  printf("      UNLOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    unlockImpl();
  }

  catch (CException &e) {
    FAILURE_3(Thread,
              "Locking error (%s) trying to unlock mutex %s from thread %s.\n",
              e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }

#if defined(DEBUG_LOCKS)
  printf("    UNLOCKED mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
}