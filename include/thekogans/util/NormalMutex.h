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

#if !defined (__thekogans_util_NormalMutex_h)
#define __thekogans_util_NormalMutex_h

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/RWLock.h"
#else // defined (TOOLCHAIN_OS_Windows)
    #include <pthread.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct NormalMutex NormalMutex.h thekogans/util/NormalMutex.h
        ///
        /// \brief
        /// NormalMutex is part of the cross-platform synchronization
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
        ///     LockGuard<NormalMutex> guard (jobsNormalMutex);
        ///     while (jobs.empty ()) {
        ///         notEmpty.Wait (jobsNormalMutex);
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
        /// Thanks to the NormalMutex/Condition pairing, this code is simple,
        /// robust, and easily maintained.
        struct _LIB_THEKOGANS_UTIL_DECL NormalMutex {
        private:
            struct Impl {
            #if defined (TOOLCHAIN_OS_Windows)
                /// \brief
                /// Windows critical section.
                RWLock lock;
            #else // defined (TOOLCHAIN_OS_Windows)
                /// \brief
                /// POSIX mutex.
                pthread_mutex_t mutex;
            #endif // defined (TOOLCHAIN_OS_Windows)

                /// \brief
                /// ctor.
                /// \param[in] shared true == NormalMutex is shared among processes.
                Impl ();
                /// \brief
                /// dtor.
                ~Impl ();
            };
            std::unique_ptr<Impl> impl;

            /// \brief
            /// Condition2 needs access to lock/mutex.
            friend struct Condition2;

        public:
            /// \brief
            /// ctor.
            NormalMutex () :
                impl (std::make_unique<Impl> ()) {}
            /// \brief
            /// Move ctor.
            /// \param[in] other NormalMutex to move.
            NormalMutex (NormalMutex &&other) noexcept :
                impl (std::move (other.impl)) {}

            /// \brief
            /// Move assignment operator.
            /// \param[in] other NormalMutex to move assign.
            /// \return *this.
            NormalMutex &operator = (NormalMutex &&other) noexcept;

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
            /// NormalMutex is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (NormalMutex)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_NormalMutex_h)
