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

#include "thekogans/util/RefCounted.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (RefCounted::References, SpinLock)

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
