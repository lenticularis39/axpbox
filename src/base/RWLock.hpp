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
// RWLock.h
//
// $Id: RWLock.h,v 1.1 2008/05/31 15:47:26 iamcamiel Exp $
//
// Library: Foundation
// Package: Threading
// Module:  RWLock
//
// Definition of the RWLock class.
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

#ifndef Foundation_RWLock_INCLUDED
#define Foundation_RWLock_INCLUDED

#include "../es40_debug.hpp"
#include "Exception.hpp"
#include "Foundation.hpp"
#include "Mutex.hpp"
#include "Timestamp.hpp"

#include <thread>

#if defined(POCO_OS_FAMILY_WINDOWS)
#include "RWLock_WIN32.hpp"
#else
#include "RWLock_POSIX.hpp"
#endif

class CScopedRWLock;

class CRWLock : private CRWLockImpl
/// A reader writer lock allows multiple concurrent
/// readers or one exclusive writer.
{
public:
  typedef CScopedRWLock CScopedLock;

  CRWLock(const char *lName);

  void readLock(long milliseconds);
  bool tryReadLock(long milliseconds);
  void writeLock(long milliseconds);
  bool tryWriteLock(long milliseconds);

  char *lockName;

  CRWLock();
  /// Creates the Reader/Writer lock.

  ~CRWLock();
  /// Destroys the Reader/Writer lock.

  void readLock();
  /// Acquires a read lock. If another thread currently holds a write lock,
  /// waits until the write lock is released.

  bool tryReadLock();
  /// Tries to acquire a read lock. Immediately returns true if successful, or
  /// false if another thread currently holds a write lock.

  void writeLock();
  /// Acquires a write lock. If one or more other threads currently hold
  /// locks, waits until all locks are released. The results are undefined
  /// if the same thread already holds a read or write lock

  bool tryWriteLock();
  /// Tries to acquire a write lock. Immediately returns true if successful,
  /// or false if one or more other threads currently hold
  /// locks. The result is undefined if the same thread already
  /// holds a read or write lock.

  void unlock();
  /// Releases the read or write lock.

private:
  CRWLock(const CRWLock &);
  CRWLock &operator=(const CRWLock &);
};

class CScopedRWLock
/// A variant of ScopedLock for reader/writer locks.
{
public:
  CScopedRWLock(CRWLock *rwl, bool write = false);
  ~CScopedRWLock();

private:
  CRWLock *_rwl;

  CScopedRWLock();
  CScopedRWLock(const CScopedRWLock &);
  CScopedRWLock &operator=(const CScopedRWLock &);
};

//
// inlines
//
inline void CRWLock::readLock() {
#if defined(DEBUG_LOCKS)
  printf("   READ LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    readLockImpl();
  }

  catch (CException &e) {
    FAILURE_3(
        Thread,
        "Locking error (%s) trying to read-lock mutex %s from thread %s.\n",
        e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }

#if defined(DEBUG_LOCKS)
  printf(" READ LOCKED mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
}

inline bool CRWLock::tryReadLock() {
  bool res;
#if defined(DEBUG_LOCKS)
  printf(" TRY RD LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    res = tryReadLockImpl();
  } catch (CException &e) {
    FAILURE_3(
        Thread,
        "Locking error (%s) trying to read-lock mutex %s from thread %s.\n",
        e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }

#if defined(DEBUG_LOCKS)
  printf("  %s mutex %s from thread %s.   \n",
         res ? "    LOCKED" : "CAN'T LOCK", lockName, CURRENT_THREAD_NAME);
#endif
  return res;
}

inline bool CRWLock::tryWriteLock(long milliseconds) {
#if defined(DEBUG_LOCKS)
  printf(" TRY WR LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    CTimestamp now;
    CTimestamp::TimeDiff diff(CTimestamp::TimeDiff(milliseconds) * 1000);
    do {
      if (tryWriteLock()) {
#if defined(DEBUG_LOCKS)
        printf("   WR LOCKED mutex %s from thread %s.   \n", lockName,
               CURRENT_THREAD_NAME);
#endif
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (!now.isElapsed(diff));
#if defined(DEBUG_LOCKS)
    printf("CAN'T W LOCK mutex %s from thread %s.   \n", lockName,
           CURRENT_THREAD_NAME);
#endif
    return false;
  } catch (CException &e) {
    FAILURE_3(
        Thread,
        "Locking error (%s) trying to write-lock mutex %s from thread %s.\n",
        e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }
}

inline bool CRWLock::tryReadLock(long milliseconds) {
#if defined(DEBUG_LOCKS)
  printf(" TRY RD LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    CTimestamp now;
    CTimestamp::TimeDiff diff(CTimestamp::TimeDiff(milliseconds) * 1000);
    do {
      if (tryReadLock()) {
#if defined(DEBUG_LOCKS)
        printf("   RD LOCKED mutex %s from thread %s.   \n", lockName,
               CURRENT_THREAD_NAME);
#endif
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (!now.isElapsed(diff));
#if defined(DEBUG_LOCKS)
    printf("CAN'T R LOCK mutex %s from thread %s.   \n", lockName,
           CURRENT_THREAD_NAME);
#endif
    return false;
  } catch (CException &e) {
    FAILURE_3(
        Thread,
        "Locking error (%s) trying to read-lock mutex %s from thread %s.\n",
        e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }
}

inline void CRWLock::writeLock(long milliseconds) {
#if defined(DEBUG_LOCKS)
  printf("  WRITE LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    CTimestamp now;
    CTimestamp::TimeDiff diff(CTimestamp::TimeDiff(milliseconds) * 1000);
    do {
      if (tryWriteLock()) {
#if defined(DEBUG_LOCKS)
        printf("   WR LOCKED mutex %s from thread %s.   \n", lockName,
               CURRENT_THREAD_NAME);
#endif
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    } while (!now.isElapsed(diff));
    FAILURE(Timeout, "Timeout");
  } catch (CException &e) {
    FAILURE_3(
        Thread,
        "Locking error (%s) trying to write-lock mutex %s from thread %s.\n",
        e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }
}

inline void CRWLock::readLock(long milliseconds) {
#if defined(DEBUG_LOCKS)
  printf("   READ LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    CTimestamp now;
    CTimestamp::TimeDiff diff(CTimestamp::TimeDiff(milliseconds) * 1000);
    do {
      if (tryReadLock()) {
#if defined(DEBUG_LOCKS)
        printf("   RD LOCKED mutex %s from thread %s.   \n", lockName,
               CURRENT_THREAD_NAME);
#endif
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (!now.isElapsed(diff));
    FAILURE(Timeout, "Timeout");
  } catch (CException &e) {
    FAILURE_3(
        Thread,
        "Locking error (%s) trying to read-lock mutex %s from thread %s.\n",
        e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }
}

inline void CRWLock::writeLock() {
#if defined(DEBUG_LOCKS)
  printf("  WRITE LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    writeLockImpl();
  }

  catch (CException &e) {
    FAILURE_3(
        Thread,
        "Locking error (%s) trying to write-lock mutex %s from thread %s.\n",
        e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }

#if defined(DEBUG_LOCKS)
  printf("WRITE LOCKED mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
}

inline bool CRWLock::tryWriteLock() {
  bool res;
#if defined(DEBUG_LOCKS)
  printf(" TRY WR LOCK mutex %s from thread %s.   \n", lockName,
         CURRENT_THREAD_NAME);
#endif
  try {
    res = tryWriteLockImpl();
  }

  catch (CException &e) {
    FAILURE_3(
        Thread,
        "Locking error (%s) trying to write-lock mutex %s from thread %s.\n",
        e.message().c_str(), lockName, CURRENT_THREAD_NAME);
  }

#if defined(DEBUG_LOCKS)
  printf("  %s mutex %s from thread %s.   \n",
         res ? "    LOCKED" : "CAN'T LOCK", lockName, CURRENT_THREAD_NAME);
#endif
  return res;
}

inline void CRWLock::unlock() {
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

inline CScopedRWLock::CScopedRWLock(CRWLock *rwl, bool write) : _rwl(rwl) {
  _rwl = rwl;
  if (write)
    _rwl->writeLock(LOCK_TIMEOUT_MS);
  else
    _rwl->readLock(LOCK_TIMEOUT_MS);
}

inline CScopedRWLock::~CScopedRWLock() { _rwl->unlock(); }

#define SCOPED_READ_LOCK(mutex) CRWLock::CScopedLock L_##__LINE__(mutex, false)
#define SCOPED_WRITE_LOCK(mutex) CRWLock::CScopedLock L_##__LINE__(mutex, true)

#endif // Foundation_RWLock_INCLUDED
