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

#include <cstring>
#include <regex>
#include "thekogans/util/Heap.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/BTreeKeys.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (thekogans::util::StringKey, 1, BTree::Key::TYPE)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (StringKey)

        i32 StringKey::PrefixCompare (const Key &key_) const {
            return ignoreCase ?
                StringCompareIgnoreCase (key.c_str (), key_.ToString ().c_str (), key.size ()) :
                StringCompare (key.c_str (), key_.ToString ().c_str (), key.size ());
        }

        i32 StringKey::Compare (const Key &key_) const {
            return ignoreCase ?
                StringCompareIgnoreCase (key.c_str (), key_.ToString ().c_str ()) :
                StringCompare (key.c_str (), key_.ToString ().c_str ());
        }

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (thekogans::util::GUIDKey, 1, BTree::Key::TYPE)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (GUIDKey)

        i32 GUIDKey::PrefixCompare (const Key &key_) const {
            return StringCompare (hexString.c_str (), key_.ToString ().c_str (), hexString.size ());
        }

        i32 GUIDKey::Compare (const Key &key_) const {
            return StringCompare (hexString.c_str (), key_.ToString ().c_str ());
        }

    } // namespace util
} // namespace thekogans
