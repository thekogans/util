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

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
    #include "thekogans/util/os/windows/WindowsUtils.h"
#else // defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/Heap.h"
    #include "thekogans/util/SpinLock.h"
    #include "thekogans/util/Mutex.h"
    #include "thekogans/util/Condition.h"
    #include "thekogans/util/SharedObject.h"
    #include "thekogans/util/LockGuard.h"
    #include "thekogans/util/HRTimer.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Exception.h"
#include "thekogans/util/Event.h"

namespace thekogans {
    namespace util {

    #if !defined (TOOLCHAIN_OS_Windows)
        struct Event::EventImpl {
            THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

            bool manualReset;
            volatile State state;
            bool shared;
            ui32 count;
            volatile bool pulse;
            volatile ui64 pulseTime;
            ui32 pulseCount;
            Mutex mutex;
            Condition condition;

            EventImpl (
                bool manualReset_,
                State state_,
                bool shared_ = false) :
                manualReset (manualReset_),
                state (state_),
                shared (shared_),
                count (0),
                pulse (false),
                pulseTime (0),
                pulseCount (0),
                mutex (shared),
                condition (mutex, shared) {}

            // On Windows Signal uses SetEvent and SignalAll uses PulseEvent.
            // The following logic is necessary to emulate their behavior on
            // POSIX.

            void Signal () {
                LockGuard<Mutex> guard (mutex);
                if (state == NotSignalled) {
                    state = Signalled;
                    pulse = false;
                    // If in manual reset mode, release all waiting threads
                    // to simulate the behavior of Windows event.
                    if (manualReset) {
                        condition.SignalAll ();
                    }
                    else {
                        condition.Signal ();
                    }
                }
            }

            void SignalAll () {
                LockGuard<Mutex> guard (mutex);
                if (state == NotSignalled) {
                    if (count > 0) {
                        state = Signalled;
                        pulse = true;
                        pulseTime = HRTimer::Click ();
                        if (manualReset) {
                            pulseCount = count;
                            condition.SignalAll ();
                        }
                        else {
                            pulseCount = 1;
                            condition.Signal ();
                        }
                    }
                }
                else {
                    state = NotSignalled;
                }
            }

            void Reset () {
                LockGuard<Mutex> guard (mutex);
                state = NotSignalled;
            }

            bool Wait (const TimeSpec &timeSpec) {
                LockGuard<Mutex> guard (mutex);
                ui64 time = HRTimer::Click ();
                struct CountMgr {
                    ui32 &count;
                    CountMgr (ui32 &count_) :
                            count (count_) {
                        ++count;
                    }
                    ~CountMgr () {
                        --count;
                    }
                } countMgr (count);
                if (timeSpec == TimeSpec::Infinite) {
                    while (state == NotSignalled || (pulse && time > pulseTime)) {
                        condition.Wait ();
                    }
                }
                else {
                    TimeSpec now = GetCurrentTime ();
                    TimeSpec deadline = now + timeSpec;
                    while ((state == NotSignalled || (pulse && time > pulseTime)) &&
                            deadline > now) {
                        condition.Wait (deadline - now);
                        now = GetCurrentTime ();
                    }
                    if (state == NotSignalled) {
                        return false;
                    }
                }
                if (!manualReset || (pulse && --pulseCount == 0)) {
                    state = NotSignalled;
                }
                return true;
            }
        };

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (Event::EventImpl)

        struct Event::EventImplConstructor : public SharedObject::Constructor {
            bool manualReset;
            State initialState;

            EventImplConstructor (
                bool manualReset_,
                State initialState_) :
                manualReset (manualReset_),
                initialState (initialState_) {}

            virtual void *operator () (void *ptr) const {
                return new (ptr) EventImpl (manualReset, initialState, true);
            }
        };

        struct Event::EventImplDestructor : public SharedObject::Destructor {
            virtual void operator () (void *ptr) const {
                ((EventImpl *)ptr)->~EventImpl ();
            }
        };
    #endif // !defined (TOOLCHAIN_OS_Windows)

        Event::Event (
                bool manualReset,
                State initialState,
                const std::string &name) :
        #if defined (TOOLCHAIN_OS_Windows)
                handle (
                    CreateEventW (
                        0,
                        manualReset ? TRUE : FALSE,
                        initialState == Signalled ? TRUE : FALSE,
                        !name.empty () ? os::windows::UTF8ToUTF16 (name).c_str () : 0)) {
            if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
                event (name.empty () ?
                    new EventImpl (manualReset, initialState) :
                    (EventImpl *)SharedObject::Create (
                        name.c_str (),
                        sizeof (EventImpl),
                        false,
                        EventImplConstructor (manualReset, initialState))) {
            if (event == nullptr) {
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
                SharedObject::Destroy (event, EventImplDestructor ());
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

        bool Event::Wait (const TimeSpec &timeSpec) {
        #if defined (TOOLCHAIN_OS_Windows)
            if (timeSpec == TimeSpec::Infinite) {
                if (WaitForSingleObject (handle, INFINITE) != WAIT_OBJECT_0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
            else {
                DWORD result = WaitForSingleObject (
                    handle, (DWORD)timeSpec.ToMilliseconds ());
                if (result != WAIT_OBJECT_0) {
                    if (result != WAIT_TIMEOUT) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                    return false;
                }
            }
            return true;
        #else // defined (TOOLCHAIN_OS_Windows)
            return event->Wait (timeSpec);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
