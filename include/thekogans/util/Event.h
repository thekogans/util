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

#include "thekogans/util/Config.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/Types.h"
#else // defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/Mutex.h"
    #include "thekogans/util/Condition.h"
#endif // defined (TOOLCHAIN_OS_Windows)
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
            /// Forward declaration of a private class.
            struct EventImpl;
            /// \brief
            /// POSIX shared event implementation.
            EventImpl *event;
            /// \brief
            /// Forward declaration of a private class.
            struct SharedEventImpl;
            /// \brief
            /// Forward declaration of a private class.
            struct SharedEventImplConstructor;
            /// \brief
            /// Forward declaration of a private class.
            struct SharedEventImplDestructor;
        #endif // defined (TOOLCHAIN_OS_Windows)

        public:
            /// \brief
            /// ctor.
            /// \param[in] manualReset
            /// true = event is to be manually reset entering Signaled state,
            /// false = the event will be reset after the first waiting thread is woken up.
            /// \param[in] initialState Initial state of the event.
            /// \param[in] name Shared event name.
            Event (
                bool manualReset = true,
                State initialState = Free,
                const char *name = 0);
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
