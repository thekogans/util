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

        /// \struct IgnoreList IgnoreList.h thekogans/pathfinder/IgnoreList.h
        ///
        /// \brief
        /// IgnoreList stores a list of regular expressions in the \see{Database::registry}.
        /// \see{Root} uses this list to exclude paths that match patterns in the list.
        struct IgnoreList : public util::StringArrayValue {
            /// \brief
            /// IgnoreList is a \see{util::Serializable}
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (IgnoreList)

            /// \brief
            /// Add a new pattern to the list.
            /// \param[in] ignore Regular expression pattern to add.
            /// \return true == pattern was added. false == duplicate.
            bool AddIgnore (const std::string &ignore);
            /// \brief
            /// Delete a pattern from the list.
            /// \param[in] ignore Regular expression pattern to delete.
            /// \return true == found and deleted, false == not found.
            bool DeleteIgnore (const std::string &ignore);
            /// \brief
            /// Return true if the given path should be excluded (i.e. it matched
            /// one of the ignore list entries).
            /// \param[in] path Path componenet to check if it matches one of the
            /// ignore patterns.
            /// \return true == should be ignored. false == did not match any of
            /// the patterns in the list.
            bool ShouldIgnore (const std::string &path);
        };

    } // namespace pathfinder
} // namespace thekogans

#endif // !defined (__thekogans_pathfinder_IgnoreList_h)
