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

        // This algorithm comes straight out of Intel TBB. I
        // re-implemented it using std atomic primitives.

        namespace {
            struct Backoff {
                static const ui32 LOOPS_BEFORE_YIELD = 16;
                ui32 count;
                Backoff () : count (1) {}
                void Pause () {
                    if (count <= LOOPS_BEFORE_YIELD) {
                        Thread::Pause (count);
                        // Pause twice as long the next time.
                        count *= 2;
                    }
                    else {
                        // Pause is so long that we might as well
                        // yield CPU to scheduler.
                        Thread::YieldSlice ();
                    }
                }
                void Reset () {
                    count = 1;
                }
            };
        }

        bool SpinRWLock::TryAcquire (bool read) {
        #if defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
            ui32 currState = state.load (std::memory_order_acquire);
        #else // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
            ui32 currState = state.load (boost::memory_order_acquire);
        #endif // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
            if (read) {
                if (!(currState & (WRITER | WRITER_PENDING))) {
                #if defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                    ui32 newState = state.fetch_add (ONE_READER, std::memory_order_release);
                #else // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                    ui32 newState = state.fetch_add (ONE_READER, boost::memory_order_release);
                #endif // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                    if (!(newState & WRITER)) {
                        return true;
                    }
                #if defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                    state.fetch_sub (ONE_READER, std::memory_order_release);
                #else // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                    state.fetch_sub (ONE_READER, boost::memory_order_release);
                #endif // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
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
                for (Backoff backoff;; backoff.Pause ()) {
                #if defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                    ui32 currState = state.load (std::memory_order_acquire);
                #else // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                    ui32 currState = state.load (boost::memory_order_acquire);
                #endif // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                    if (!(currState & (WRITER | WRITER_PENDING))) {
                    #if defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                        ui32 newState = state.fetch_add (ONE_READER, std::memory_order_release);
                    #else // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                        ui32 newState = state.fetch_add (ONE_READER, boost::memory_order_release);
                    #endif // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                        if (!(newState & WRITER)) {
                            break;
                        }
                    #if defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                        state.fetch_sub (ONE_READER, std::memory_order_release);
                    #else // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                        state.fetch_sub (ONE_READER, boost::memory_order_release);
                    #endif // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                    }
                }
            }
            else {
                for (Backoff backoff;; backoff.Pause ()) {
                #if defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                    ui32 currState = state.load (std::memory_order_acquire);
                #else // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                    ui32 currState = state.load (boost::memory_order_acquire);
                #endif // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                    if (!(currState & BUSY)) {
                        if (state.compare_exchange_strong (currState, WRITER)) {
                            break;
                        }
                        backoff.Reset ();
                    }
                    else if (!(currState & WRITER_PENDING)) {
                    #if defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                        state.fetch_or (WRITER_PENDING, std::memory_order_release);
                    #else // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                        state.fetch_or (WRITER_PENDING, boost::memory_order_release);
                    #endif // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                    }
                }
            }
        }

        void SpinRWLock::Release (bool read) {
            if (read) {
                THEKOGANS_UTIL_ASSERT (state & READERS,
                    "Invalid state of a SpinRWLock: no readers");
            #if defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                state.fetch_sub (ONE_READER, std::memory_order_release);
            #else // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                state.fetch_sub (ONE_READER, boost::memory_order_release);
            #endif // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
            }
            else {
                THEKOGANS_UTIL_ASSERT (state & WRITER,
                    "Invalid state of a SpinRWLock: no writer");
            #if defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                state.fetch_and (READERS, std::memory_order_release);
            #else // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
                state.fetch_and (READERS, boost::memory_order_release);
            #endif // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
            }
        }

    } // namespace util
} // namespace thekogans
