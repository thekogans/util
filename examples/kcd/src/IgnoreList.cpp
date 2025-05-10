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

#include <string>
#include <list>
#include <vector>
#include <unordered_set>
#include <iostream>
#include "thekogans/util/Environment.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/FileAllocatorRegistry.h"
#include "thekogans/util/BTreeValues.h"
#include "thekogans/kcd/IgnoreList.h"

namespace thekogans {
    namespace kcd {

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (IgnoreList, 1, util::BTree::Value::TYPE)

        bool IgnoreList::AddIgnore (
                const std::string &ignore,
                util::FileAllocator::SharedPtr fileAllocator) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i] == ignore) {
                    return false;
                }
            }
            {
                util::FileAllocator::Transaction transaction (fileAllocator);
                value.push_back (ignore);
                fileAllocator->GetRegistry ().SetValue ("ignore_list", this);
                transaction.Commit ();
                return true;
            }
        }

        bool IgnoreList::DeleteIgnore (
                const std::string &ignore,
                util::FileAllocator::SharedPtr fileAllocator) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i] == ignore) {
                    util::FileAllocator::Transaction transaction (fileAllocator);
                    value.erase (value.begin () + i);
                    fileAllocator->GetRegistry ().SetValue ("ignore_list", this);
                    transaction.Commit ();
                    return true;
                }
            }
            return false;
        }

        bool IgnoreList::ShouldIgnore (const std::string &ignore) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i] == ignore) {
                    return true;
                }
            }
            return false;
        }

    } // namespace kcd
} // namespace thekogans
