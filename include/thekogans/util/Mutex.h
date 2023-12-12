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

#if !defined (__thekogans_util_Mutex_h)
#define __thekogans_util_Mutex_h

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
#else // defined (TOOLCHAIN_OS_Windows)
    #include <pthread.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct Mutex Mutex.h thekogans/util/Mutex.h
        ///
        /// \brief
        /// Mutex is part of the cross-platform synchronization
        /// primitives. Note, on Windows it is defined as a critical
        /// section. This is on purpose. The POSIX model of pairing a
        /// mutex with a condition variable is very valuable, and is
        /// maintained by having the Windows condition variable be linked
        /// to this critical section (See Condition.h). With that in
        /// mind, you can write the following code, and it will work on
        /// all platforms:
        ///
        /// \code{.cpp}
        /// RunLoop::Job::SharedPtr JobQueue::Deq () {
        ///     LockGuard<Mutex> guard (jobsMutex);
        ///     while (jobs.empty ()) {
        ///         notEmpty.Wait ();
        ///     }
        ///     Job::SharedPtr job;
        ///     assert (!jobs.empty ());
        ///     if (!jobs.empty ()) {
        ///         job.reset (jobs.front ());
        ///         jobs.pop_front ();
        ///     }
        ///     return job;
        /// }
        /// \endcode
        ///
        /// Thanks to the Mutex/Condition pairing, this code is simple,
        /// robust, and easily maintained.
        ///
        /// VERY IMPORTANT: There is a slight semantec difference between
        /// CRITICAL_SECTION on Windows and pthread_mutex_t on POSIX.
        /// On Windows, a single thread can acquire a CRITICAL_SECTION
        /// as many times as it needs, while on POSIX, a single thread
        /// HAS to release the pthread_mutex_t between every acquisition.

        struct _LIB_THEKOGANS_UTIL_DECL Mutex {
        private:
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows critical section.
            CRITICAL_SECTION cs;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// POSIX mutex.
            pthread_mutex_t mutex;
        #endif // defined (TOOLCHAIN_OS_Windows)

            /// \brief
            /// Condition needs access to cs/mutex.
            friend struct Condition;

        public:
            /// \brief
            /// ctor.
            Mutex ();
            /// \brief
            /// dtor.
            ~Mutex ();

            /// \brief
            /// Try to acquire the mutex without blocking.
            /// \return true = acquired, false = failed to acquire
            bool TryAcquire ();

            /// \brief
            /// Acquire the mutex.
            void Acquire ();

            /// \brief
            /// Release the mutex.
            void Release ();

    #if !defined (TOOLCHAIN_OS_Windows)
        private:
            /// \brief
            /// ctor.
            /// \param[in] shared For pthread_mutex_t. Initialize with
            /// PTHREAD_PROCESS_SHARED attribute.
            Mutex (bool shared) {
                Init (shared);
            }

            /// \brief
            /// Initialize the mutex.
            /// \param[in] shared For pthread_mutex_t. Initialize with
            /// PTHREAD_PROCESS_SHARED attribute.
            void Init (bool shared);

            /// \brief
            /// Event needs access to the private ctor.
            friend struct Event;
            /// \brief
            /// Semaphore needs access to the private ctor.
            friend struct Semaphore;
    #endif // !defined (TOOLCHAIN_OS_Windows)

            /// \brief
            /// Mutex is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Mutex)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Mutex_h)
