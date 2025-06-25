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

#include <regex>
#include "thekogans/pathfinder/IgnoreList.h"

namespace thekogans {
    namespace pathfinder {

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (
            thekogans::pathfinder::IgnoreList,
            1,
            util::BTree::Value::TYPE)

        bool IgnoreList::AddIgnore (const std::string &ignore) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i] == ignore) {
                    return false;
                }
            }
            value.push_back (ignore);
            return true;
        }

        bool IgnoreList::DeleteIgnore (const std::string &ignore) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i] == ignore) {
                    value.erase (value.begin () + i);
                    return true;
                }
            }
            return false;
        }

        bool IgnoreList::ShouldIgnore (const std::string &path) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                const std::regex regex (value[i]);
                if (std::regex_match (path, regex)) {
                    return true;
                }
            }
            return false;
        }

    } // namespace pathfinder
} // namespace thekogans
