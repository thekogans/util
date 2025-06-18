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

#if !defined (__thekogans_pathfinder_IgnoreList_h)
#define __thekogans_pathfinder_IgnoreList_h

#include <string>
#include "thekogans/util/Serializable.h"
#include "thekogans/util/BTreeValues.h"

namespace thekogans {
    namespace pathfinder {

        struct IgnoreList : public util::StringArrayValue {
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (IgnoreList)

            bool AddIgnore (const std::string &ignore);
            bool DeleteIgnore (const std::string &ignore);
            bool ShouldIgnore (const std::string &ignore);
        };

    } // namespace pathfinder
} // namespace thekogans

#endif // !defined (__thekogans_pathfinder_IgnoreList_h)
