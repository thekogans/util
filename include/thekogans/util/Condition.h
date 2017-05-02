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

#if !defined (__thekogans_util_Condition_h)
#define __thekogans_util_Condition_h

#if defined (TOOLCHAIN_OS_Windows)
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
        #include <windows.h>
    #endif // !defined (_WINDOWS_)
#else // defined (TOOLCHAIN_OS_Windows)
    #include <pthread.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Config.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/TimeSpec.h"

namespace thekogans {
    namespace util {

        /// \struct Condition Condition.h thekogans/util/Condition.h
        ///
        /// \brief
        /// Wraps a Windows CONDITION_VARIABLE and a
        /// POSIX pthread_cond_t in a platform independent api.

        struct _LIB_THEKOGANS_UTIL_DECL Condition {
        private:
            /// \brief
            /// The mutex this condition variable is paired with.x
            Mutex &mutex;
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows condition variable.
            CONDITION_VARIABLE cv;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// POSIX condition variable.
            pthread_cond_t condition;
        #endif // defined (TOOLCHAIN_OS_Windows)

        public:
            /// \brief
            /// ctor.
            /// \param[in] mutex_ The mutex to pair this condition variable with.
            explicit Condition (Mutex &mutex_);
            /// \brief
            /// dtor.
            ~Condition ();

            /// \brief
            /// Unconditional wait.
            /// NOTE: This wait will either succeed, or throw an exception.
            void Wait ();

            /// \brief
            /// Conditional wait.
            /// \param[in] timeSpec How long to wait.
            /// IMPORTANT: timeSpec is a relative value.
            /// On POSIX (pthreads) systems it will add
            /// the current time to the value provided
            /// before calling pthread_cond_timedwait.
            /// \return true = succeeded, false = timed out
            bool Wait (const TimeSpec &timeSpec);

            /// \brief
            /// Put the condition variable in to signaled state. If
            /// a thread is waiting on it, the wait will succeed.
            void Signal ();

            /// \brief
            /// Put the condition variable in to signaled state. If
            /// any threads are waiting on it, their wait will succeed.
            void SignalAll ();

            /// \brief
            /// Condition is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Condition)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Condition_h)
