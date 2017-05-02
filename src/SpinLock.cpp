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

#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

    #if defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
        bool SpinLock::TryAcquire () {
            return state.exchange (Locked, std::memory_order_acquire) == Unlocked;
        }

        void SpinLock::Acquire () {
            while (state.exchange (Locked, std::memory_order_acquire) == Locked) {
                // busy-wait
            }
        }

        void SpinLock::Release () {
            state.store (Unlocked, std::memory_order_release);
        }
    #else // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
        bool SpinLock::TryAcquire () {
            return state.exchange (Locked, boost::memory_order_acquire) == Unlocked;
        }

        void SpinLock::Acquire () {
            while (state.exchange (Locked, boost::memory_order_acquire) == Locked) {
                // busy-wait
            }
        }

        void SpinLock::Release () {
            state.store (Unlocked, boost::memory_order_release);
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)

    } // namespace util
} // namespace thekogans
