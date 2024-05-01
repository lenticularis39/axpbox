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
#pragma once

#include "es40_debug.hpp"
#include <chrono>
#include <mutex>
#include <condition_variable>

class CSemaphore {
public:
  explicit CSemaphore(int n) : m_count(n), m_max(n) {}
  CSemaphore(int n, int max) : m_count(n), m_max(max) {
    if (n < 0 || n > max) {
      FAILURE(InvalidArgument, "Invalid initial count for semaphore");
    }
  }

  ~CSemaphore() = default;
  CSemaphore() = delete;
  CSemaphore(const CSemaphore &) = delete;
  CSemaphore &operator=(const CSemaphore &) = delete;
  CSemaphore(const CSemaphore &&) = delete;
  CSemaphore &operator=(const CSemaphore &&) = delete;

  void set() {
    std::unique_lock<std::mutex> const lock(m_mutex);
    if (m_count >= m_max) {
      FAILURE(System, "cannot signal semaphore: count would exceed maximum");
    }
    ++m_count;
    m_cv.notify_one();
  }

  void wait() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return m_count > 0; });
    --m_count;
  }

  void wait(long milliseconds) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_cv.wait_for(lock, std::chrono::milliseconds(milliseconds), [this] { return m_count > 0; })) {
      FAILURE(Timeout, "Timeout");
    }
    --m_count;
  }

  bool tryWait(long milliseconds) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_cv.wait_for(lock, std::chrono::milliseconds(milliseconds), [this] { return m_count > 0; })) {
      --m_count;
      return true;
    }
    return false;
  }

private:
  int m_count;
  int m_max;
  std::mutex m_mutex;
  std::condition_variable m_cv;
};