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

#if !defined (__thekogans_util_SharedEvent_h)
#define __thekogans_util_SharedEvent_h

#include "thekogans/util/Config.h"
#include "thekogans/util/TimeSpec.h"

namespace thekogans {
    namespace util {

        /// \struct SharedEvent SharedEvent.h thekogans/util/SharedEvent.h
        ///
        /// \brief
        /// SharedEvent implements a cross process event. Use the same name
        /// when creating the event to signal across process boundaries.

        struct _LIB_THEKOGANS_UTIL_DECL SharedEvent {
            /// \brief
            /// SharedEvent state.
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
            struct SharedEventImpl;
            /// \brief
            /// POSIX shared event implementation.
            SharedEventImpl *event;
        #endif // defined (TOOLCHAIN_OS_Windows)

        public:
            /// \brief
            /// ctor.
            /// \param[in] name Shared event name.
            /// \param[in] manualReset
            /// true = event is to be manually reset entering Signaled state,
            /// false = the event will be reset after the first waiting thread is woken up.
            /// \param[in] initialState Initial state of the event.
            SharedEvent (
                const char *name,
                bool manualReset = true,
                State initialState = Free);
            /// \brief
            /// dtor.
            ~SharedEvent ();

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
            /// SharedEvent is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SharedEvent)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SharedEvent_h)
