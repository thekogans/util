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

#include "thekogans/util/Config.h"
#include "thekogans/util/ImplPtr.h"

namespace thekogans {
    namespace util {

        /// \struct Mutex Mutex.h thekogans/util/Mutex.h
        ///
        /// \brief
        /// Mutex is part of the cross-platform synchronization primitives.
        /// Depending on the type you need (Normal or Recursive), on Windows
        /// it is based on either SRWLOCK or CRITICAL_SECTION. Mutex is movable
        /// so it can be stored in movable containers or transferred during setup.
        /// It is not copyable or copy-assignable. Do not move it while it is
        /// locked, while a thread may be waiting to acquire it, or while a
        /// LockGuard or Condition::Wait refers to it.
        ///
        /// Condition::Wait requires the mutex to be owned exactly once. Windows
        /// requires this for SleepConditionVariableCS; on POSIX, waiting on a
        /// recursively held mutex does not fully unlock it. Although a Recursive
        /// mutex may work if every wait occurs at recursion depth one, Mutex does
        /// not track that depth. For portable use, pair Condition only with a
        /// Normal mutex. This is why the type parameter defaults to Normal.
        ///
        /// \code{.cpp}
        /// RunLoop::Job::SharedPtr JobQueue::Deq () {
        ///     LockGuard<Mutex> guard (jobsMutex);
        ///     while (jobs.empty ()) {
        ///         notEmpty.Wait (jobsMutex);
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
        struct _LIB_THEKOGANS_UTIL_DECL Mutex {
            enum Type {
                /// \brief
                /// Fast, zero-overhead, non-recursive (SRWLock / Standard POSIX)
                Normal,
                /// \brief
                /// Supports re-entrant ownership (Critical Section / Recursive POSIX)
                Recursive
            };

        private:
            /// \brief
            /// Forward declaration on Impl.
            struct Impl;
            /// \brief
            /// Implementation.
            ImplPtr<Impl> impl;

            /// \brief
            /// Needs access to impl.
            friend struct Condition;

        public:
            /// \brief
            /// ctor.
            explicit Mutex (Type type = Normal);
            /// \brief
            /// dtor.
            ~Mutex ();

            /// \brief
            /// Try to acquire the mutex without blocking.
            /// On POSIX, errors other than EBUSY are reported as exceptions.
            /// \return true if acquired, false if currently unavailable.
            bool TryAcquire ();

            /// \brief
            /// Acquire the mutex.
            void Acquire ();

            /// \brief
            /// Release the mutex.
            void Release ();

            /// \brief
            /// Mutex is not copy constructable or copy assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Mutex)
            /// \brief
            /// Mutex is movable.
            THEKOGANS_UTIL_DEFAULT_MOVE_AND_ASSIGN (Mutex)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Mutex_h)
