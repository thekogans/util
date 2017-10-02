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

#include "thekogans/util/Exception.h"
#if !defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/Mutex.h"
    #include "thekogans/util/LockGuard.h"
#endif // !defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Event.h"

namespace thekogans {
    namespace util {

        Event::Event (
                bool manualReset_,
                State initialState) :
        #if defined (TOOLCHAIN_OS_Windows)
                handle (CreateEvent (0, manualReset_ ? TRUE : FALSE,
                    initialState == Signalled ? TRUE : FALSE, 0)) {
            if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
                manualReset (manualReset_),
                state (Free),
                condition (mutex) {
            if (initialState == Signalled) {
                Signal ();
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        Event::~Event () {
        #if defined (TOOLCHAIN_OS_Windows)
            CloseHandle (handle);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Event::Signal () {
        #if defined (TOOLCHAIN_OS_Windows)
            SetEvent (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            LockGuard<Mutex> guard (mutex);
            state = Signalled;
            if (manualReset) {
                condition.SignalAll ();
            }
            else {
                condition.Signal ();
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Event::SignalAll () {
        #if defined (TOOLCHAIN_OS_Windows)
            PulseEvent (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            LockGuard<Mutex> guard (mutex);
            state = Signalled;
            condition.SignalAll ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Event::Reset () {
        #if defined (TOOLCHAIN_OS_Windows)
            ResetEvent (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            LockGuard<Mutex> guard (mutex);
            state = Free;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Event::Wait () {
        #if defined (TOOLCHAIN_OS_Windows)
            if (WaitForSingleObject (handle, INFINITE) != WAIT_OBJECT_0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            LockGuard<Mutex> guard (mutex);
            while (state == Free) {
                condition.Wait ();
            }
            if (!manualReset) {
                state = Free;
            }
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
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
