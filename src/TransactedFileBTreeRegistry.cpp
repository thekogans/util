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
#include "thekogans/util/TransactedFileBTreeKeys.h"
#include "thekogans/util/TransactedFileBTreeRegistry.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (
            thekogans::util::TransactedFileBTreeRegistry,
            TransactedFile::Registry::TYPE)

        void TransactedFileBTreeRegistry::Init (TransactedFile::SharedPtr file) {
            btree.Reset (
                new TransactedFileBTree (
                    file,
                    file->GetAllocator ()->GetRegistryOffset (),
                    StringKey::GetContext (),
                    SerializableHeader (),
                    valueAsObject,
                    entriesPerNode,
                    nodesPerPage,
                    allocator));
            Subscriber<TransactedFile::ObjectEvents>::Subscribe (*btree);
        }

        Serializable::SharedPtr TransactedFileBTreeRegistry::GetValue (const std::string &key) {
            TransactedFileBTree::Iterator it;
            return btree->Find (StringKey (key), it) ? it.GetValue () : nullptr;
        }

        void TransactedFileBTreeRegistry::SetValue (
                const std::string &key,
                Serializable::SharedPtr value) {
            if (value == nullptr) {
                btree->Remove (StringKey (key));
            }
            else {
                TransactedFileBTree::Iterator it;
                if (!btree->Insert (new StringKey (key), value, it)) {
                    it.SetValue (value);
                }
            }
        }

    } // namespace util
} // namespace thekogans
