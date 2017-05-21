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

#if !defined (__thekogans_util_Event_h)
#define __thekogans_util_Event_h

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
    #include "thekogans/util/Mutex.h"
    #include "thekogans/util/Condition.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Config.h"
#include "thekogans/util/TimeSpec.h"

namespace thekogans {
    namespace util {

        /// \struct Event Event.h thekogans/util/Event.h
        ///
        /// \brief
        /// Wraps a Windows event synchronization object.
        /// Emulates it on Linux/OS X.

        struct _LIB_THEKOGANS_UTIL_DECL Event {
            /// \brief
            /// Event state.
            typedef enum {
                /// \brief
                /// Not signalled.
                Free,
                /// \brief
                /// Signalled.
                Signalled
            } State;

        private:
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows event handle.
            THEKOGANS_UTIL_HANDLE handle;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// true = manual recet, false = auto recet.
            bool manualReset;
            /// \brief
            /// Event state.
            volatile State state;
            /// \brief
            /// Synchronization mutex.
            Mutex mutex;
            /// \brief
            /// Synchronization conditin variable.
            Condition condition;
        #endif // defined (TOOLCHAIN_OS_Windows)

        public:
            /// \brief
            /// ctor.
            /// \param[in] manualReset_
            /// true = event is to be manually reset entering Signaled state,
            /// false = the event will be reset after the first waiting thread is woken up.
            /// \param[in] initialState initial state of the event.
            Event (
                bool manualReset_ = true,
                State initialState = Free);
            /// \brief
            /// dtor.
            ~Event ();

            /// \brief
            /// Put the event in to Signaled state. If any
            /// threads are waiting for the event to become
            /// signeled, one (or more) will be woken up,
            /// and given a chance to execute.
            void Signal ();

            /// \brief
            /// Put the event in to signaled state. If any threads
            /// are waiting on it, their wait will succeed.
            void SignalAll ();

            /// \brief
            /// Put a manual reset event in to Free state.
            void Reset ();

            /// \brief
            /// Wait for the event to become signaled.
            void Wait ();

            /// \brief
            /// Wait a specified amount of time for the event to become signaled.
            /// IMPORTANT: timeSpec is a relative value.
            /// On POSIX (pthreads) systems it will add
            /// the current time to the value provided
            /// before calling pthread_cond_timedwait.
            /// \param[in] timeSpec Amount of time to wait before returning TimedOut.
            /// \return true = succeeded, false = timed out.
            bool Wait (const TimeSpec &timeSpec);

            /// \brief
            /// Event is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Event)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Event_h)
