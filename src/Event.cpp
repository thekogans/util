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
    #include "thekogans/util/POSIXUtils.h"
    #include "thekogans/util/LockGuard.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Exception.h"
#include "thekogans/util/Event.h"

namespace thekogans {
    namespace util {

    #if !defined (TOOLCHAIN_OS_Windows)
        struct Event::EventImpl {
            bool manualReset;
            volatile State state;
            bool shared;
            Mutex mutex;
            Condition condition;

            EventImpl (
                    bool manualReset_,
                    State initialState,
                    bool shared_ = false) :
                    manualReset (manualReset_),
                    state (Free),
                    shared (shared_),
                    mutex (shared),
                    condition (mutex, shared) {
                if (initialState == Signalled) {
                    Signal ();
                }
            }

            void Signal () {
                LockGuard<Mutex> guard (mutex);
                state = Signalled;
                if (manualReset) {
                    condition.SignalAll ();
                }
                else {
                    condition.Signal ();
                }
            }

            void SignalAll () {
                LockGuard<Mutex> guard (mutex);
                state = Signalled;
                condition.SignalAll ();
            }

            void Reset () {
                LockGuard<Mutex> guard (mutex);
                state = Free;
            }

            void Wait () {
                LockGuard<Mutex> guard (mutex);
                while (state == Free) {
                    condition.Wait ();
                }
                if (!manualReset) {
                    state = Free;
                }
            }

            bool Wait (const TimeSpec &timeSpec) {
                LockGuard<Mutex> guard (mutex);
                TimeSpec now = GetCurrentTime ();
                TimeSpec deadline = now + timeSpec;
                while (state == Free && deadline > now) {
                    if (!condition.Wait (deadline - now)) {
                        return false;
                    }
                    now = GetCurrentTime ();
                }
                if (!manualReset) {
                    state = Free;
                }
                return true;
            }
        };

        struct Event::SharedEventImpl :
                public SharedObject<SharedEventImpl>,
                public EventImpl {
            SharedEventImpl (
                bool manualReset,
                State initialState,
                const char *name) :
                SharedObject (name),
                EventImpl (manualReset, initialState, true) {}
        };

        struct Event::SharedEventImplConstructor :
                public SharedObject<SharedEventImpl>::Constructor {
            bool manualReset;
            State initialState;

            SharedEventImplConstructor (
                bool manualReset_,
                State initialState_) :
                manualReset (manualReset_),
                initialState (initialState_) {}

            virtual SharedEventImpl *operator () (
                    void *ptr,
                    const char *name) const {
                return new (ptr) SharedEventImpl (manualReset, initialState, name);
            }
        };

        struct Event::SharedEventImplDestructor :
                public SharedObject<SharedEventImpl>::Destructor {
            virtual void operator () (SharedEventImpl *event) const {
                event->~SharedEventImpl ();
            }
        };
    #endif // !defined (TOOLCHAIN_OS_Windows)

        Event::Event (
                bool manualReset,
                State initialState,
                const char *name) :
        #if defined (TOOLCHAIN_OS_Windows)
                handle (CreateEvent (0, manualReset ? TRUE : FALSE,
                    initialState == Signalled ? TRUE : FALSE, name)) {
            if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
                event (name == 0 ?
                    new EventImpl (manualReset, initialState) :
                    SharedEventImpl::Create (
                        name,
                        SharedEventImplConstructor (manualReset, initialState))) {
            if (event == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_ENOMEM);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        Event::~Event () {
        #if defined (TOOLCHAIN_OS_Windows)
            CloseHandle (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            if (!event->shared) {
                delete event;
            }
            else {
                SharedEventImpl::Destroy (
                    (SharedEventImpl *)event,
                    SharedEventImplDestructor ());
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Event::Signal () {
        #if defined (TOOLCHAIN_OS_Windows)
            SetEvent (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            event->Signal ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Event::SignalAll () {
        #if defined (TOOLCHAIN_OS_Windows)
            PulseEvent (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            event->SignalAll ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Event::Reset () {
        #if defined (TOOLCHAIN_OS_Windows)
            ResetEvent (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            event->Reset ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Event::Wait () {
        #if defined (TOOLCHAIN_OS_Windows)
            if (WaitForSingleObject (handle, INFINITE) != WAIT_OBJECT_0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            event->Wait ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        bool Event::Wait (const TimeSpec &timeSpec) {
        #if defined (TOOLCHAIN_OS_Windows)
            DWORD result = WaitForSingleObject (
                handle, (DWORD)timeSpec.ToMilliseconds ());
            if (result != WAIT_OBJECT_0) {
                if (result != WAIT_TIMEOUT) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                return false;
            }
            return true;
        #else // defined (TOOLCHAIN_OS_Windows)
            return event->Wait (timeSpec);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
