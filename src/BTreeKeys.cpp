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
#include "thekogans/util/BTreeKeys.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (thekogans::util::StringKey, 1, BTree::Key::TYPE)

        i32 StringKey::PrefixCompare (const Key &prefix) const {
            const StringKey *stringKey = static_cast<const StringKey *> (&prefix);
            return key.compare (0, stringKey->key.size (), stringKey->key);
        }

        i32 StringKey::Compare (const Key &key_) const {
            return key.compare (static_cast<const StringKey *> (&key_)->key);
        }

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (thekogans::util::GUIDKey, 1, BTree::Key::TYPE)

        i32 GUIDKey::PrefixCompare (const Key &prefix) const {
            return memcmp (
                key.data,
                static_cast<const GUIDKey *> (&prefix)->key.data,
                GUID::SIZE);
        }

        i32 GUIDKey::Compare (const Key &key_) const {
            return memcmp (
                key.data,
                static_cast<const GUIDKey *> (&key_)->key.data,
                GUID::SIZE);
        }

    } // namespace util
} // namespace thekogans
