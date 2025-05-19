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
#include "thekogans/util/BTreeKeys.h"
#include "thekogans/util/FileAllocatorRegistry.h"

namespace thekogans {
    namespace util {

        util::BTree::Value::SharedPtr FileAllocator::Registry::GetValue (const std::string &key) {
            util::BTree::Iterator it;
            return Find (StringKey (key), it) ? it.GetValue () : nullptr;
        }

        void FileAllocator::Registry::SetValue (
                const std::string &key,
                util::BTree::Value::SharedPtr value) {
            if (value == nullptr) {
                Delete (StringKey (key));
            }
            else {
                util::BTree::Iterator it;
                if (!Insert (new StringKey (key), value, it)) {
                    it.SetValue (value);
                }
            }
        }

    } // namespace util
} // namespace thekogans
