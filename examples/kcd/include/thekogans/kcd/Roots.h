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

#if !defined (__thekogans_kcd_Roots_h)
#define __thekogans_kcd_Roots_h

#include <string>
#include <vector>
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/Serializable.h"
#include "thekogans/util/BTreeValues.h"
#include "thekogans/kcd/Root.h"

namespace thekogans {
    namespace kcd {

        struct Roots : public util::ArrayValue<Root::SharedPtr> {
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (Roots)

            void ScanRoot (
                const std::string &path,
                util::FileAllocator::SharedPtr fileAllocator);
            void EnableRoot (
                const std::string &path,
                util::FileAllocator::SharedPtr fileAllocator);
            void DisableRoot (
                const std::string &path,
                util::FileAllocator::SharedPtr fileAllocator);
            void DeleteRoot (
                const std::string &path,
                util::FileAllocator::SharedPtr fileAllocator);

            void Find (
                util::FileAllocator::SharedPtr fileAllocator,
                const std::string &pattern,
                bool ignoreCase,
                bool ordered,
                std::vector<std::string> &results);
        };

    } // namespace kcd
} // namespace thekogans

#endif // !defined (__thekogans_kcd_Roots_h)
