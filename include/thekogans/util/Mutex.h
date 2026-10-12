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

#include <memory>
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
            struct Impl {
                Type type;

            #if defined (TOOLCHAIN_OS_Windows)
                union {
                    SRWLOCK srwLock;
                    CRITICAL_SECTION criticalSection;
                } lock;
            #else // defined (TOOLCHAIN_OS_Windows)
                pthread_mutex_t mutex;
                std::atomic<pthread_t> ownerThread{pthread_t{}};
            #endif // defined (TOOLCHAIN_OS_Windows)

                explicit Impl (Type type_);
                ~Impl ();

                bool TryAcquire ();
                void Acquire ();
                void Release ();
            };
            std::unique_ptr<Impl> impl;

            /// \brief
            /// Needs access to impl.
            friend struct Condition;

        public:
            /// \brief
            /// ctor.
            explicit Mutex (Type type = Normal);
            /// \brief
            /// Move ctor.
            /// \param[in] other Mutex to move.
            Mutex (Mutex &&other) noexcept = default;

            /// \brief
            /// Move assignment operator.
            /// \param[in] other Mutex to move.
            /// \return *this.
            Mutex &operator = (Mutex &&other) noexcept;

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

            /// \brief
            /// Mutex is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Mutex)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Mutex_h)
