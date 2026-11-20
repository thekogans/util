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

#include "thekogans/util/LockGuard.h"
#include "thekogans/util/TransactedFileBTreeKeys.h"
#include "thekogans/util/TransactedFileMTBTree.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT (thekogans::util::TransactedFileMTBTree, 1, 0)

        Serializable::SharedPtr TransactedFileMTBTree::Find (const Key &key) {
            LockGuard<Mutex> guard (mutex);
            return TransactedFileBTree::Find (key);
        }

        bool TransactedFileMTBTree::Insert (
                Key::SharedPtr key,
                Serializable::SharedPtr value) {
            LockGuard<Mutex> guard (mutex);
            return TransactedFileBTree::Insert (key, value);
        }

        bool TransactedFileMTBTree::Remove (const Key &key) {
            LockGuard<Mutex> guard (mutex);
            return TransactedFileBTree::Remove (key);
        }

        void TransactedFileMTBTree::Init (
                TransactedFile *file,
                TransactedFile::AddressType offset) {
            LockGuard<Mutex> guard (mutex);
            TransactedFileBTree::Init (file, offset);
        }

    } // namespace util
} // namespace thekogans
