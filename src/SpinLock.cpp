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
#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

        bool SpinLock::TryAcquire () {
            return state.exchange (Locked, std::memory_order_acquire) == Unlocked;
        }

        void SpinLock::Acquire () {
            while (state.exchange (Locked, std::memory_order_acquire) == Locked) {
                // Wait for lock to become free with exponential back-off.
                // In a heavily contested lock, this leads to fewer cache
                // line invalidations and better performance.
                // This code was adapted from the ideas found here:
                // https://geidav.wordpress.com/tag/exponential-back-off/
                Thread::Backoff backoff (maxPauseBeforeYield);
                while (state.load (std::memory_order_relaxed) == Locked) {
                    backoff.Pause ();
                }
            }
        }

        void SpinLock::Release () {
            state.store (Unlocked, std::memory_order_release);
        }

    } // namespace util
} // namespace thekogans
