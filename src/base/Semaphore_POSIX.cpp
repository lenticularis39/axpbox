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
// Semaphore_POSIX.cpp
//
// Library: Foundation
// Package: Threading
// Module:  Semaphore
//
// Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//

#include "Semaphore_POSIX.hpp"
#include <sys/time.h>

//
// Note: pthread_cond_timedwait() with CLOCK_MONOTONIC is supported
// on Linux and QNX, as well as on Android >= 5.0 (API level 21).
// On Android < 5.0, HAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC is defined
// to indicate availability of non-standard pthread_cond_timedwait_monotonic().
//
#ifndef POCO_HAVE_MONOTONIC_PTHREAD_COND_TIMEDWAIT
#if (defined(__linux__) || defined(__QNX__)) &&                                \
    !(defined(__ANDROID__) &&                                                  \
      (defined(HAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC) ||                       \
       __ANDROID_API__ <= 21))
#define POCO_HAVE_MONOTONIC_PTHREAD_COND_TIMEDWAIT 1
#endif
#endif

#ifndef POCO_HAVE_CLOCK_GETTIME
#if (defined(_POSIX_TIMERS) && defined(CLOCK_REALTIME)) ||                     \
    defined(POCO_VXWORKS) || defined(__QNX__)
#ifndef __APPLE__ // See GitHub issue #1453 - not available before Mac
                  // OS 10.12/iOS 10
#define POCO_HAVE_CLOCK_GETTIME
#endif
#endif
#endif

CSemaphoreImpl::CSemaphoreImpl(int n, int max) : _n(n), _max(max) {
  poco_assert(n >= 0 && max > 0 && n <= max);

#if defined(POCO_VXWORKS)
  // This workaround is for VxWorks 5.x where
  // pthread_mutex_init() won't properly initialize the mutex
  // resulting in a subsequent freeze in pthread_mutex_destroy()
  // if the mutex has never been used.
  std::memset(&_mutex, 0, sizeof(_mutex));
#endif
  if (pthread_mutex_init(&_mutex, NULL))
    throw CSystemException("cannot create semaphore (mutex)");

#if defined(POCO_HAVE_MONOTONIC_PTHREAD_COND_TIMEDWAIT)
  pthread_condattr_t attr;
  if (pthread_condattr_init(&attr)) {
    pthread_mutex_destroy(&_mutex);
    throw CSystemException("cannot create semaphore (condition attribute)");
  }
  if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)) {
    pthread_condattr_destroy(&attr);
    pthread_mutex_destroy(&_mutex);
    throw CSystemException(
        "cannot create semaphore (condition attribute clock)");
  }
  if (pthread_cond_init(&_cond, &attr)) {
    pthread_condattr_destroy(&attr);
    pthread_mutex_destroy(&_mutex);
    throw CSystemException("cannot create semaphore (condition)");
  }
  pthread_condattr_destroy(&attr);
#else
  if (pthread_cond_init(&_cond, NULL)) {
    pthread_mutex_destroy(&_mutex);
    throw CSystemException("cannot create semaphore (condition)");
  }
#endif
}

CSemaphoreImpl::~CSemaphoreImpl() {
  pthread_cond_destroy(&_cond);
  pthread_mutex_destroy(&_mutex);
}

void CSemaphoreImpl::waitImpl() {
  if (pthread_mutex_lock(&_mutex))
    throw CSystemException("wait for semaphore failed (lock)");
  while (_n < 1) {
    if (pthread_cond_wait(&_cond, &_mutex)) {
      pthread_mutex_unlock(&_mutex);
      throw CSystemException("wait for semaphore failed");
    }
  }
  --_n;
  pthread_mutex_unlock(&_mutex);
}

bool CSemaphoreImpl::waitImpl(long milliseconds) {
  int rc = 0;
  struct timespec abstime;

#if defined(POCO_HAVE_MONOTONIC_PTHREAD_COND_TIMEDWAIT)
  clock_gettime(CLOCK_MONOTONIC, &abstime);
  abstime.tv_sec += milliseconds / 1000;
  abstime.tv_nsec += (milliseconds % 1000) * 1000000;
  if (abstime.tv_nsec >= 1000000000) {
    abstime.tv_nsec -= 1000000000;
    abstime.tv_sec++;
  }
#elif defined(POCO_HAVE_CLOCK_GETTIME)
  clock_gettime(CLOCK_REALTIME, &abstime);
  abstime.tv_sec += milliseconds / 1000;
  abstime.tv_nsec += (milliseconds % 1000) * 1000000;
  if (abstime.tv_nsec >= 1000000000) {
    abstime.tv_nsec -= 1000000000;
    abstime.tv_sec++;
  }
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  abstime.tv_sec = tv.tv_sec + milliseconds / 1000;
  abstime.tv_nsec = tv.tv_usec * 1000 + (milliseconds % 1000) * 1000000;
  if (abstime.tv_nsec >= 1000000000) {
    abstime.tv_nsec -= 1000000000;
    abstime.tv_sec++;
  }
#endif

  if (pthread_mutex_lock(&_mutex) != 0)
    throw CSystemException("wait for semaphore failed (lock)");
  while (_n < 1) {
    if ((rc = pthread_cond_timedwait(&_cond, &_mutex, &abstime))) {
      if (rc == ETIMEDOUT)
        break;
      pthread_mutex_unlock(&_mutex);
      throw CSystemException("cannot wait for semaphore");
    }
  }
  if (rc == 0)
    --_n;
  pthread_mutex_unlock(&_mutex);
  return rc == 0;
}
