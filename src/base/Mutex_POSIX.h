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
 *
 * Parts of this file based upon the Poco C++ Libraries, which is Copyright (C)
 * 2004-2006, Applied Informatics Software Engineering GmbH. and Contributors.
 */

/**
 * $Id: Mutex_POSIX.h,v 1.1 2008/05/31 15:47:24 iamcamiel Exp $
 *
 * X-1.1        Camiel Vanderhoeven                             31-MAY-2008
 *      Initial version for ES40 emulator.
 **/

//
// Mutex_POSIX.h
//
// $Id: Mutex_POSIX.h,v 1.1 2008/05/31 15:47:24 iamcamiel Exp $
//
// Library: Foundation
// Package: Threading
// Module:  Mutex
//
// Definition of the MutexImpl and FastMutexImpl classes for POSIX Threads.
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

#ifndef Foundation_Mutex_POSIX_INCLUDED
#define Foundation_Mutex_POSIX_INCLUDED

#include "Exception.h"
#include "Foundation.h"
#include <errno.h>
#include <pthread.h>

class CMutexImpl {
protected:
  CMutexImpl();
  CMutexImpl(bool fast);
  ~CMutexImpl();
  void lockImpl();
  bool tryLockImpl();
  bool tryLockImpl(long milliseconds);
  void unlockImpl();

private:
  pthread_mutex_t _mutex;
};

class CFastMutexImpl : public CMutexImpl {
protected:
  CFastMutexImpl();
  ~CFastMutexImpl();
};

//
// inlines
//
inline void CMutexImpl::lockImpl() {
  if (pthread_mutex_lock(&_mutex))
    throw CSystemException("cannot lock mutex");
}

inline bool CMutexImpl::tryLockImpl() {
  int rc = pthread_mutex_trylock(&_mutex);
  if (rc == 0)
    return true;
  else if (rc == EBUSY)
    return false;
  else
    throw CSystemException("cannot lock mutex");
}

inline void CMutexImpl::unlockImpl() {
  if (pthread_mutex_unlock(&_mutex))
    throw CSystemException("cannot unlock mutex");
}

#endif // Foundation_Mutex_POSIX_INCLUDED
