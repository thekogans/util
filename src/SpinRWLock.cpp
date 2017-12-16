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

#include "thekogans/util/Thread.h"
#include "thekogans/util/SpinRWLock.h"

namespace thekogans {
    namespace util {

    #if defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
        #define memory_order_acquire std::memory_order_acquire
        #define memory_order_release std::memory_order_release
    #else // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
        #define memory_order_acquire boost::memory_order_acquire
        #define memory_order_release boost::memory_order_release
    #endif // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)

        // This algorithm comes straight out of Intel TBB. I
        // re-implemented it using std atomic primitives.

        bool SpinRWLock::TryAcquire (bool read) {
            ui32 currState = state.load (memory_order_acquire);
            if (read) {
                if (!(currState & (WRITER | WRITER_PENDING))) {
                    ui32 newState = state.fetch_add (ONE_READER, memory_order_release);
                    if (!(newState & WRITER)) {
                        return true;
                    }
                    state.fetch_sub (ONE_READER, memory_order_release);
                }
            }
            else {
                if (!(currState & BUSY) &&
                        state.compare_exchange_strong (currState, WRITER)) {
                    return true;
                }
            }
            return false;
        }

        void SpinRWLock::Acquire (bool read) {
            if (read) {
                for (Thread::Backoff backoff;; backoff.Pause ()) {
                    ui32 currState = state.load (memory_order_acquire);
                    if (!(currState & (WRITER | WRITER_PENDING))) {
                        ui32 newState = state.fetch_add (ONE_READER, memory_order_release);
                        if (!(newState & WRITER)) {
                            break;
                        }
                        state.fetch_sub (ONE_READER, memory_order_release);
                    }
                }
            }
            else {
                for (Thread::Backoff backoff;; backoff.Pause ()) {
                    ui32 currState = state.load (memory_order_acquire);
                    if (!(currState & BUSY)) {
                        if (state.compare_exchange_strong (currState, WRITER)) {
                            break;
                        }
                        backoff.Reset ();
                    }
                    else if (!(currState & WRITER_PENDING)) {
                        state.fetch_or (WRITER_PENDING, memory_order_release);
                    }
                }
            }
        }

        void SpinRWLock::Release (bool read) {
            if (read) {
                THEKOGANS_UTIL_ASSERT (state & READERS,
                    "Invalid state of a SpinRWLock: no readers.");
                state.fetch_sub (ONE_READER, memory_order_release);
            }
            else {
                THEKOGANS_UTIL_ASSERT (state & WRITER,
                    "Invalid state of a SpinRWLock: no writer.");
                state.fetch_and (READERS, memory_order_release);
            }
        }

    } // namespace util
} // namespace thekogans
