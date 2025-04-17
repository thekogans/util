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

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (
            thekogans::util::StringKey,
            1,
            BTree::Key::TYPE)

        i32 StringKey::PrefixCompare (const Key &prefix) const {
            if (prefix.Type () == StringKey::TYPE || prefix.IsDerivedFrom (StringKey::TYPE)) {
                const StringKey *stringKey = static_cast<const StringKey *> (&prefix);
                return key.compare (0, stringKey->key.size (), stringKey->key);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        i32 StringKey::Compare (const Key &key_) const {
            if (key_.Type () == StringKey::TYPE || key_.IsDerivedFrom (StringKey::TYPE)) {
                const StringKey *stringKey = static_cast<const StringKey *> (&key_);
                return key.compare (stringKey->key);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (
            thekogans::util::GUIDKey,
            1,
            BTree::Key::TYPE)

        i32 GUIDKey::PrefixCompare (const Key &prefix) const {
            if (prefix.Type () == GUIDKey::TYPE || prefix.IsDerivedFrom (GUIDKey::TYPE)) {
                const GUIDKey *guidKey = static_cast<const GUIDKey *> (&prefix);
                return key == guidKey->key;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        i32 GUIDKey::Compare (const Key &key) const {
            if (key.Type () == GUIDKey::TYPE || key.IsDerivedFrom (GUIDKey::TYPE)) {
                const GUIDKey *guidKey = static_cast<const GUIDKey *> (&key);
                return this->key == guidKey->key;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    } // namespace util
} // namespace thekogans
