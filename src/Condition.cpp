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

#if defined (TOOLCHAIN_OS_Windows)
    #if defined (__GNUC__)
        #define _WIN32_WINNT 0x0600
        #include <synchapi.h>
    #endif // defined (__GNUC__)
#endif // defined (TOOLCHAIN_OS_Windows)
#include <ctime>
#include "thekogans/util/Exception.h"
#include "thekogans/util/internal.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/Condition.h"

namespace thekogans {
    namespace util {

        Condition::Condition (Mutex &mutex_) :
                mutex (mutex_) {
        #if defined (TOOLCHAIN_OS_Windows)
            InitializeConditionVariable (&cv);
        #else // defined (TOOLCHAIN_OS_Windows)
            if (pthread_cond_init (&condition, 0) != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        Condition::~Condition () {
        #if defined (TOOLCHAIN_OS_Windows)
        #else // defined (TOOLCHAIN_OS_Windows)
            pthread_cond_destroy (&condition);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Condition::Wait () {
        #if defined (TOOLCHAIN_OS_Windows)
            if (!SleepConditionVariableCS (&cv, &mutex.cs, INFINITE)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            if (pthread_cond_wait (&condition, &mutex.mutex) != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        bool Condition::Wait (const TimeSpec &timeSpec) {
        #if defined (TOOLCHAIN_OS_Windows)
            if (!SleepConditionVariableCS (&cv, &mutex.cs, (DWORD)timeSpec.ToMilliseconds ())) {
                if (THEKOGANS_UTIL_OS_ERROR_CODE != ERROR_TIMEOUT) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                return false;
            }
            return true;
        #else // defined (TOOLCHAIN_OS_Windows)
            timespec absolute = (GetCurrentTime () + timeSpec).Totimespec ();
            int errorCode = pthread_cond_timedwait (
                &condition, &mutex.mutex, &absolute);
            if (errorCode != 0) {
                if (errorCode != ETIMEDOUT) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
                return false;
            }
            return true;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Condition::Signal () {
        #if defined (TOOLCHAIN_OS_Windows)
            WakeConditionVariable (&cv);
        #else // defined (TOOLCHAIN_OS_Windows)
            if (pthread_cond_signal (&condition) != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Condition::SignalAll () {
        #if defined (TOOLCHAIN_OS_Windows)
            WakeAllConditionVariable (&cv);
        #else // defined (TOOLCHAIN_OS_Windows)
            if (pthread_cond_broadcast (&condition) != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
