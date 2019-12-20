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

#include <string>
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
        /// Wraps a Windows event synchronization object. Emulates it
        /// on Linux/OS X.
        ///
        /// IMPORTANT: To emulate Windows behavior on POSIX, the following
        /// semantics are observed:
        ///
        /// When calling Signal:
        ///
        /// The state of a manual-reset event object remains signaled
        /// until it is set explicitly to the non-signaled state by the
        /// Reset function. Any number of waiting threads, or threads
        /// that subsequently begin wait operations for the specified
        /// event object by calling Wait, can be released while the
        /// object's state is signaled.
        ///
        /// The state of an auto-reset event object remains signaled
        /// until a single waiting thread is released, at which time
        /// the system automatically sets the state to non-signaled.
        /// If no threads are waiting, the event object's state remains
        /// signaled.
        ///
        /// Setting an event that is already set has no effect.
        ///
        /// When calling SignalAll:
        ///
        /// For a manual-reset event object, all waiting threads that
        /// can be released immediately are released. The event object's
        /// state is then reset to non-signaled state.
        ///
        /// For an auto-reset event object, SignalAll releases at most
        /// one waiting thread, even if multiple threads are waiting.
        /// Regardless of whether a thread was released or not, event
        /// object's state is reset to non-signaled.
        ///
        /// If no threads are waiting, or if no thread can be released
        /// immediately, SignalAll simply sets the event object's state
        /// to non-signaled and returns.

        struct _LIB_THEKOGANS_UTIL_DECL Event {
            /// \brief
            /// Event state.
            typedef enum {
                /// \brief
                /// Not signalled.
                NotSignalled,
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
            struct EventImplConstructor;
            /// \brief
            /// Forward declaration of a private class.
            struct EventImplDestructor;
        #endif // defined (TOOLCHAIN_OS_Windows)

        public:
            /// \brief
            /// ctor.
            /// \param[in] manualReset
            /// true = event is to be manually reset after entering Signaled state,
            /// false = the event will be reset after the first waiting thread is woken up.
            /// VERY IMPORTANT: If your intention is to have multiple waiting threads be
            /// released when you call SignalAll, you must set manualReset == true.
            /// \param[in] initialState Initial state of the event.
            /// \param[in] name Shared event name.
            Event (
                bool manualReset = true,
                State initialState = NotSignalled,
                const std::string &name = std::string ());
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
            /// Put a manual reset event in to NotSignalled state.
            void Reset ();

            /// \brief
            /// Wait for the event to become signaled.
            /// On POSIX (pthreads) systems it will add
            /// the current time to the value provided
            /// before calling pthread_cond_timedwait.
            /// \param[in] timeSpec Amount of time to wait before returning false.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true = succeeded, false = timed out.
            bool Wait (const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Event is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Event)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Event_h)
