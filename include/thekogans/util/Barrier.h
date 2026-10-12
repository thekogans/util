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

#if !defined (__thekogans_util_Barrier_h)
#define __thekogans_util_Barrier_h

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
    #if (WINVER > 0x0602)
        #include <synchapi.h>
        #define THEKOGANS_UTIL_USE_WINDOWS_BARRIER
    #endif // WINVER > 0x0602
#else // defined (TOOLCHAIN_OS_Windows)
    #include <pthread.h>
    #if defined (_POSIX_BARRIERS) && (_POSIX_BARRIERS >= 20012L)
        #define THEKOGANS_UTIL_USE_POSIX_BARRIER
    #endif // defined (_POSIX_BARRIERS) && (_POSIX_BARRIERS >= 20012L)
#endif // defined (TOOLCHAIN_OS_Windows)
#include <cstddef>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#if !defined (THEKOGANS_UTIL_USE_WINDOWS_BARRIER) && !defined (THEKOGANS_UTIL_USE_POSIX_BARRIER)
    #include "thekogans/util/Mutex.h"
    #include "thekogans/util/Condition.h"
#endif // !defined (THEKOGANS_UTIL_USE_WINDOWS_BARRIER) && !defined (THEKOGANS_UTIL_USE_POSIX_BARRIER)
#include <dispatch/dispatch.h>
#include <atomic>

namespace thekogans {
    namespace util {

#if 0
        /// \struct Barrier Barrier.h thekogans/util/Barrier.h
        ///
        /// \brief
        /// Wraps a Windows/POSIX barrier synchronization object.
        /// Emulates it if it doesn't exist (OS X).
        struct _LIB_THEKOGANS_UTIL_DECL Barrier {
        private:
        #if defined (THEKOGANS_UTIL_USE_WINDOWS_BARRIER)
            /// \brief
            /// On Windows 8 or higher, use built in OS barrier.
            SYNCHRONIZATION_BARRIER barrier;
        #elif defined (THEKOGANS_UTIL_USE_POSIX_BARRIER)
            /// \brief
            /// On POSIX systems, if provided, use pthread barrier.
            pthread_barrier_t barrier;
        #else // defined (THEKOGANS_UTIL_USE_POSIX_BARRIER)
            /// \brief
            /// Number of threads to wait for.
            const std::size_t count;
            /// \brief
            /// Generation guard.
            std::size_t spaces;
            /// \brief
            /// Generation guard.
            std::size_t generation;
            /// \brief
            /// Synchronization mutex.
            NormalMutex mutex;
            /// \brief
            /// Synchronization condition variable.
            Condition2 condition;
        #endif // defined (THEKOGANS_UTIL_USE_WINDOWS_BARRIER)

        public:
            /// \brief
            /// ctor.
            /// \param[in] count Number of threads to synchronize.
            explicit Barrier (std::size_t count);
            /// \brief
            /// dtor.
            ~Barrier ();

            /// \brief
            /// Wait for all threads to enter the barrier.
            /// \return true = last (signaling) thread, false = waiting thread.
            bool Wait ();

            /// \brief
            /// Barrier is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Barrier)
        };
#else
        class Barrier {
        private:
            std::size_t const count;
            std::atomic<std::size_t> spaces1;
            std::atomic<std::size_t> spaces2;
            dispatch_semaphore_t sem1;
            dispatch_semaphore_t sem2;

        public:
            Barrier(std::size_t count_)
                : count(count_), spaces1(count_), spaces2(count_) {
                sem1 = dispatch_semaphore_create(0);
                sem2 = dispatch_semaphore_create(0);
            }

            ~Barrier() {
                // GCD objects must be explicitly released under MRC rules if using raw C profiles
                dispatch_release(sem1);
                dispatch_release(sem2);
            }

            bool Wait() {
                // Phase 1: Scatter Gate
                if (spaces1.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    spaces1.store(count, std::memory_order_relaxed);
                    for (std::size_t i = 1; i < count; ++i) {
                        dispatch_semaphore_signal(sem1);
                    }
                } else {
                    dispatch_semaphore_wait(sem1, DISPATCH_TIME_FOREVER);
                }

                // Phase 2: Gather Lockout Gate (Prevents phase bleeding completely)
                if (spaces2.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    spaces2.store(count, std::memory_order_relaxed);
                    for (std::size_t i = 1; i < count; ++i) {
                        dispatch_semaphore_signal(sem2);
                    }
                    return true;
                } else {
                    dispatch_semaphore_wait(sem2, DISPATCH_TIME_FOREVER);
                    return false;
                }
            }
        };

#endif

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Barrier_h)
