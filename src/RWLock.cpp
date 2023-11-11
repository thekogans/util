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

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows) && defined (__GNUC__)
    #define _WIN32_WINNT 0x0600
    #include <synchapi.h>
#endif // defined (TOOLCHAIN_OS_Windows) && defined (__GNUC__)
#include "thekogans/util/Exception.h"
#include "thekogans/util/RWLock.h"

namespace thekogans {
    namespace util {

        RWLock::RWLock () {
        #if defined (TOOLCHAIN_OS_Windows)
            InitializeSRWLock (&rwlock);
        #else // defined (TOOLCHAIN_OS_Windows)
            THEKOGANS_UTIL_ERROR_CODE errorCode =
                pthread_rwlock_init (&rwlock, 0);
            if (errorCode != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        RWLock::~RWLock () {
        #if !defined (TOOLCHAIN_OS_Windows)
            pthread_rwlock_destroy (&rwlock);
        #endif // !defined (TOOLCHAIN_OS_Windows)
        }

        bool RWLock::TryAcquire (bool read) {
        #if defined (TOOLCHAIN_OS_Windows)
            return (read ?
                TryAcquireSRWLockShared (&rwlock) :
                TryAcquireSRWLockExclusive (&rwlock)) == TRUE;
        #else // defined (TOOLCHAIN_OS_Windows)
            return (read ? pthread_rwlock_tryrdlock (&rwlock) :
                pthread_rwlock_trywrlock (&rwlock)) == 0;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void RWLock::Acquire (bool read) {
        #if defined (TOOLCHAIN_OS_Windows)
            if (read) {
                AcquireSRWLockShared (&rwlock);
            }
            else {
                AcquireSRWLockExclusive (&rwlock);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            if (read) {
                THEKOGANS_UTIL_ERROR_CODE errorCode =
                    pthread_rwlock_rdlock (&rwlock);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            }
            else {
                THEKOGANS_UTIL_ERROR_CODE errorCode =
                    pthread_rwlock_wrlock (&rwlock);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void RWLock::Release (bool read) {
        #if defined (TOOLCHAIN_OS_Windows)
            if (read) {
                ReleaseSRWLockShared (&rwlock);
            }
            else {
                ReleaseSRWLockExclusive (&rwlock);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            THEKOGANS_UTIL_ERROR_CODE errorCode =
                pthread_rwlock_unlock (&rwlock);
            if (errorCode != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
