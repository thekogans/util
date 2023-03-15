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

#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/operations_lockfree.hpp>
#include <boost/memory_order.hpp>
#include "thekogans/util/Thread.h"
#include "thekogans/util/SpinRWLock.h"

namespace thekogans {
    namespace util {

        namespace {
            typedef boost::atomics::detail::operations<4u, false> operations;
        }

        // This algorithm comes straight out of Intel TBB. I
        // re-implemented it using boost atomic primitives.

        bool StorageSpinRWLock::TryAcquire (bool read) {
            ui32 currState = operations::load (state, boost::memory_order_seq_cst);
            if (read) {
                if (!(currState & (WRITER | WRITER_PENDING))) {
                    ui32 newState = operations::fetch_add (state, ONE_READER, boost::memory_order_seq_cst);
                    if (!(newState & WRITER)) {
                        return true;
                    }
                    operations::fetch_sub (state, ONE_READER, boost::memory_order_seq_cst);
                }
            }
            else {
                if (!(currState & BUSY) &&
                        operations::compare_exchange_strong (
                            state,
                            currState,
                            WRITER,
                            boost::memory_order_seq_cst,
                            boost::memory_order_seq_cst)) {
                    return true;
                }
            }
            return false;
        }

        void StorageSpinRWLock::Acquire (bool read) {
            if (read) {
                for (Thread::Backoff backoff (maxPauseBeforeYield);; backoff.Pause ()) {
                    ui32 currState = operations::load (state, boost::memory_order_seq_cst);
                    if (!(currState & (WRITER | WRITER_PENDING))) {
                        ui32 newState = operations::fetch_add (state, ONE_READER, boost::memory_order_seq_cst);
                        if (!(newState & WRITER)) {
                            break;
                        }
                        operations::fetch_sub (state, ONE_READER, boost::memory_order_seq_cst);
                    }
                }
            }
            else {
                for (Thread::Backoff backoff (maxPauseBeforeYield);; backoff.Pause ()) {
                    ui32 currState = operations::load (state, boost::memory_order_seq_cst);
                    if (!(currState & BUSY)) {
                        if (operations::compare_exchange_strong (
                                state,
                                currState,
                                WRITER,
                                boost::memory_order_seq_cst,
                                boost::memory_order_seq_cst)) {
                            break;
                        }
                        backoff.Reset ();
                    }
                    else if (!(currState & WRITER_PENDING)) {
                        operations::fetch_or (state, WRITER_PENDING, boost::memory_order_seq_cst);
                    }
                }
            }
        }

        void StorageSpinRWLock::Release (bool read) {
            if (read) {
                THEKOGANS_UTIL_ASSERT (state & READERS,
                    "Invalid state of a SpinRWLock: no readers.");
                operations::fetch_sub (state, ONE_READER, boost::memory_order_seq_cst);
            }
            else {
                THEKOGANS_UTIL_ASSERT (state & WRITER,
                    "Invalid state of a SpinRWLock: no writer.");
                operations::fetch_and (state, READERS, boost::memory_order_seq_cst);
            }
        }

    } // namespace util
} // namespace thekogans
