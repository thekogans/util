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

#if !defined (__thekogans_util_TransactedFileMTBTree_h)
#define __thekogans_util_TransactedFileMTBTree_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/TransactedFileBTree.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        /// \struct TransactedFileMTBTree TransactedFileMTBTree.h thekogans/util/TransactedFileMTBTree.h
        ///
        /// \brief
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileMTBTree :
                public virtual TransactedFile::Object,
                private TransactedFileBTree {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT (TransactedFileMTBTree)

        private:
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

        public:
            static const std::size_t DEFAULT_ENTRIES_PER_NODE = 256;
            static const std::size_t DEFAULT_NODES_PER_PAGE = 16;

            TransactedFileMTBTree (
                const SerializableHeader &keyContext = SerializableHeader (),
                const SerializableHeader &valueContext = SerializableHeader (),
                bool valueIsObject = false,
                std::size_t entriesPerNode = DEFAULT_ENTRIES_PER_NODE,
                std::size_t nodesPerPage = DEFAULT_NODES_PER_PAGE) :
                TransactedFileBTree (keyContext, valueContext, valueIsObject, entriesPerNode, nodesPerPage) {}

            /// \brief
            /// Search for the given key in the btree.
            /// \param[in] key \see{Key} to search for.
            /// \return true == found.
            inline Serializable::SharedPtr Find (const Key &key) {
                LockGuard<SpinLock> guard (spinLock);
                return TransactedFileBTree::Find (key);
            }
            /// \brief
            /// Insert the given key in to the btree.
            /// \param[in] key Key to insert.
            /// \param[in] value Value associated with the given key.
            /// \return true == added. false == duplicate. If false, return
            /// the current entry in it.
            inline bool Insert (
                    Key::SharedPtr key,
                    Serializable::SharedPtr value) {
                LockGuard<SpinLock> guard (spinLock);
                return TransactedFileBTree::Insert (key, value);
            }
            /// \brief
            /// Delete the given key from the btree.
            /// \param[in] key Key to delete.
            /// \return true == entry deleted. false == entry not found.
            inline bool Remove (const Key &key) {
                LockGuard<SpinLock> guard (spinLock);
                return TransactedFileBTree::Remove (key);
            }

            virtual void Init (
                    TransactedFile *file,
                    TransactedFile::AddressType offset) override {
                LockGuard<SpinLock> guard (spinLock);
                TransactedFileBTree::Init (file, offset);
            }

            /// \brief
            /// TransactedFileMTBTree is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (TransactedFileMTBTree)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_TransactedFileMTBTree_h)
