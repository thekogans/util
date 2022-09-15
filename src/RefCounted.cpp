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

#include "thekogans/util/Constants.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/RefCounted.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (RefCounted::References, SpinLock)

        ui32 RefCounted::References::ReleaseWeakRef () {
            ui32 newWeak = --weak;
            if (newWeak == 0) {
                delete this;
            }
            else if (newWeak == UI32_MAX) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "weak counter underflow in an instance of %s.",
                    typeid (*this).name ());
            }
            return newWeak;
        }

        ui32 RefCounted::References::ReleaseSharedRef (RefCounted *object) {
            ui32 newShared = --shared;
            if (newShared == 0) {
                object->Harakiri ();
            }
            else if (newShared == UI32_MAX) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "shared counter underflow in an instance of %s.",
                    typeid (*this).name ());
            }
            return newShared;
        }

        bool RefCounted::References::LockObject () {
            // This is a classical lock-free algorithm for shared access.
            for (ui32 count = shared.load (std::memory_order_acquire);
                    count != 0; count = shared.load (std::memory_order_acquire)) {
                // If compare_exchange_weak failed, it's because between the load
                // above and the exchange below, we were interupted by another thread
                // that modified the value of shared. Reload and try again.
                if (shared.compare_exchange_weak (count, count + 1, std::memory_order_acquire)) {
                    return true;
                }
            }
            return false;
        }

        RefCounted::~RefCounted () {
            // We're going out of scope. If there are still
            // shared references remaining, we have a problem.
            if (references->GetSharedCount () != 0) {
                // Here we both log the problem and assert to give the
                // engineer the best chance of figuring out what happened.
                std::string message =
                    FormatString ("%s : %u\n", typeid (*this).name (), references->GetSharedCount ());
                Log (
                    SubsystemAll,
                    THEKOGANS_UTIL,
                    Error,
                    __FILE__,
                    __FUNCTION__,
                    __LINE__,
                    __DATE__ " " __TIME__,
                    "%s",
                    message.c_str ());
                THEKOGANS_UTIL_ASSERT (references->GetSharedCount () == 0, message);
            }
            references->ReleaseWeakRef ();
        }

    } // namespace util
} // namespace thekogans
