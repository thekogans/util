// Copyright 2011 Boris Kogan (boris@thekogans.net)
//
// This file is part of libthekogans_util.
//
// libthekogans_util is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libthekogans_util is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libthekogans_util. If not, see <http://www.gnu.org/licenses/>.

#include "thekogans/util/Semaphore.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        Semaphore::Semaphore (
                ui32 maxCount_,
                ui32 initialCount) :
        #if defined (TOOLCHAIN_OS_Windows)
                handle (CreateSemaphore (0, initialCount, maxCount_, 0)) {
        #else // defined (TOOLCHAIN_OS_Windows)
                maxCount (maxCount_),
                count (0),
                condition (mutex) {
        #endif // defined (TOOLCHAIN_OS_Windows)
            if (maxCount_ < initialCount) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        #if defined (TOOLCHAIN_OS_Windows)
            if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            if (initialCount != 0) {
                Release (initialCount);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        Semaphore::~Semaphore () {
        #if defined (TOOLCHAIN_OS_Windows)
            CloseHandle (handle);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Semaphore::Acquire () {
        #if defined (TOOLCHAIN_OS_Windows)
            if (WaitForSingleObject (handle, INFINITE) != WAIT_OBJECT_0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            LockGuard<Mutex> guard (mutex);
            while (count == 0) {
                condition.Wait ();
            }
            if (count > 0) {
                --count;
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Semaphore::Release (ui32 count_) {
            if (count_ != 0) {
            #if defined (TOOLCHAIN_OS_Windows)
                if (!ReleaseSemaphore (handle, count_, 0)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                LockGuard<Mutex> guard (mutex);
                if (count + count_ <= maxCount) {
                    count += count_;
                    condition.SignalAll ();
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "count_ (%u) > available (%u)",
                        count_, maxCount - count);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        }

    } // namespace util
} // namespace thekogans
