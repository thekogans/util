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

#if !defined (__thekogans_util_Condition2_h)
#define __thekogans_util_Condition2_h

#include <memory>
#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
    #include <synchapi.h>
#else // defined (TOOLCHAIN_OS_Windows)
    #include <pthread.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Config.h"
#include "thekogans/util/TimeSpec.h"

namespace thekogans {
    namespace util {

        struct NormalMutex;

        /// \struct Condition2 Condition2.h thekogans/util/Condition2.h
        ///
        /// \brief
        /// Wraps a Windows CONDITION_VARIABLE and a POSIX pthread_cond_t in a platform independent api.
        /// VERY VERY IMPORTANT: Please keep in mind that on POSIX spurious wekeups are built in to the
        /// specification of a condition variable:
        /// https://groups.google.com/forum/?hl=de#!topic/comp.programming.threads/MnlYxCfql4w
        /// (read what Dave Butenhof wrote in response to Patrick Doyle).
        /// To properly use condition variables, a predicate loop must be used. Here's an example:
        /// \code{.cpp}
        /// while (!predicate) {
        ///     condition.Wait ();
        /// }
        /// \endcode
        struct _LIB_THEKOGANS_UTIL_DECL Condition2 {
        private:
            /// \struct Condition2::Impl Condition2.h thekogans/util/Condition2.h
            ///
            /// \brief
            /// Isolate system primitives so they stay at the same
            /// memory location facilitating move.
            struct Impl {
            #if defined (TOOLCHAIN_OS_Windows)
                /// \brief
                /// Windows condition variable.
                CONDITION_VARIABLE cv;
            #else // defined (TOOLCHAIN_OS_Windows)
                /// \brief
                /// POSIX condition variable.
                pthread_cond_t cv;
            #endif // defined (TOOLCHAIN_OS_Windows)

                /// \brief
                /// ctor.
                Impl ();
                /// \brief
                /// dtor.
                ~Impl ();
            };
            /// \brief
            /// Isolated system primitives.
            std::unique_ptr<Impl> impl;

        public:
            /// \brief
            /// ctor.
            Condition2 () :
                impl (std::make_unique<Impl> ()) {}
            /// \brief
            /// Move ctor.
            /// \param[in] other Condition2 to move.
            Condition2 (Condition2 &&other) noexcept :
                impl (std::move (other.impl)) {}

            /// \brief
            /// Move assignment operator.
            /// \param[in] other Condition2 to move.
            /// \return *this.
            Condition2 &operator = (Condition2 &&other) noexcept;

            /// \brief
            /// Wait for condition.
            /// \param[in] mutex Mutex to pair with wait.
            /// \param[in] timeSpec How long to wait.
            /// IMPORTANT: timeSpec is a relative value.
            /// On POSIX (pthreads) systems it will add
            /// the current time to the value provided
            /// before calling pthread_cond_timedwait.
            /// \return true = succeeded, false = timed out
            bool Wait (
                NormalMutex &mutex,
                const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Put the condition variable in to signaled state. If
            /// a thread is waiting on it, the wait will succeed.
            void Signal ();

            /// \brief
            /// Put the condition variable in to signaled state. If
            /// any threads are waiting on it, their wait will succeed.
            void SignalAll ();

            /// \brief
            /// Condition2 is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Condition2)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Condition2_h)
