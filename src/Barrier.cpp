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
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Barrier.h"

namespace thekogans {
    namespace util {

    #if defined (THEKOGANS_UTIL_USE_WINDOWS_BARRIER)
        Barrier::Barrier (std::size_t count) {
            if (count != 0) {
                if (!InitializeSynchronizationBarrier (&barrier, count, -1)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }
    #elif defined (THEKOGANS_UTIL_USE_POSIX_BARRIER)
        Barrier::Barrier (std::size_t count) {
            if (count != 0) {
                THEKOGANS_UTIL_ERROR_CODE errorCode =
                    pthread_barrier_init (&barrier, 0, count);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }
    #else // defined (THEKOGANS_UTIL_USE_POSIX_BARRIER)
        Barrier::Barrier (std::size_t count_) :
                count (count_),
                entered (0),
                condition (mutex) {
            if (count == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }
    #endif // defined (THEKOGANS_UTIL_USE_WINDOWS_BARRIER)

    #if !defined (TOOLCHAIN_OS_Windows)
        namespace {
            struct DisableCancelState {
                int cancel;
                DisableCancelState () {
                    THEKOGANS_UTIL_ERROR_CODE errorCode =
                        pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &cancel);
                    if (errorCode != 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
                ~DisableCancelState () {
                    THEKOGANS_UTIL_TRY {
                        THEKOGANS_UTIL_ERROR_CODE errorCode =
                            pthread_setcancelstate (cancel, &cancel);
                        if (errorCode != 0) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                        }
                    }
                    THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
                }
            };
        }
    #endif // !defined (TOOLCHAIN_OS_Windows)

        #define THEKOGANS_UTIL_BARRIER_FLAG (1 << 30)

        Barrier::~Barrier () {
        #if defined (THEKOGANS_UTIL_USE_WINDOWS_BARRIER)
            if (DeleteSynchronizationBarrier (&barrier) != TRUE) {
                THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (
                    THEKOGANS_UTIL,
                    "%s\n", "Unable to delete the barrier.");
            }
        #elif defined (THEKOGANS_UTIL_USE_POSIX_BARRIER)
            if (pthread_barrier_destroy (&barrier) != 0) {
                THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (
                    THEKOGANS_UTIL,
                    "%s\n", "Unable to destroy the barrier.");
            }
        #else // defined (THEKOGANS_UTIL_USE_POSIX_BARRIER)
            THEKOGANS_UTIL_TRY {
                LockGuard<Mutex> guard (mutex);
                while (entered > THEKOGANS_UTIL_BARRIER_FLAG) {
                #if !defined (TOOLCHAIN_OS_Windows)
                    DisableCancelState disableCancelState;
                #endif // !defined (TOOLCHAIN_OS_Windows)
                    condition.Wait ();
                }
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        #endif // defined (THEKOGANS_UTIL_USE_WINDOWS_BARRIER)
        }

        bool Barrier::Wait () {
        #if defined (THEKOGANS_UTIL_USE_WINDOWS_BARRIER)
            return EnterSynchronizationBarrier (&barrier, 0) == TRUE;
        #elif defined (THEKOGANS_UTIL_USE_POSIX_BARRIER)
            DisableCancelState disableCancelState;
            THEKOGANS_UTIL_ERROR_CODE errorCode =
                pthread_barrier_wait (&barrier);
            if (errorCode != PTHREAD_BARRIER_SERIAL_THREAD && errorCode != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
            }
            return errorCode == PTHREAD_BARRIER_SERIAL_THREAD;
        #else // defined (THEKOGANS_UTIL_USE_POSIX_BARRIER)
            LockGuard<Mutex> guard (mutex);
            while (entered > THEKOGANS_UTIL_BARRIER_FLAG) {
            #if !defined (TOOLCHAIN_OS_Windows)
                DisableCancelState disableCancelState;
            #endif // !defined (TOOLCHAIN_OS_Windows)
                condition.Wait ();
            }
            if (entered == THEKOGANS_UTIL_BARRIER_FLAG) {
                entered = 0;
            }
            ++entered;
            if (entered == count) {
                entered += THEKOGANS_UTIL_BARRIER_FLAG - 1;
                condition.SignalAll ();
                return true;
            }
            else {
                while (entered < THEKOGANS_UTIL_BARRIER_FLAG) {
                #if !defined (TOOLCHAIN_OS_Windows)
                    DisableCancelState disableCancelState;
                #endif // !defined (TOOLCHAIN_OS_Windows)
                    condition.Wait ();
                }
                --entered;
                if (entered == THEKOGANS_UTIL_BARRIER_FLAG) {
                    condition.SignalAll ();
                }
                return false;
            }
        #endif // defined (THEKOGANS_UTIL_USE_WINDOWS_BARRIER)
        }

    } // namespace util
} // namespace thekogans
