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

#if !defined (__thekogans_util_TransactedFileBTreeRegistry_h)
#define __thekogans_util_TransactedFileBTreeRegistry_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/TransactedFileBTree.h"
#include "thekogans/util/TransactedFileBTreeKeys.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        /// \struct TransactedFileBTreeRegistry TransactedFileBTreeRegistry.h
        /// thekogans/util/TransactedFileBTreeRegistry.h
        ///
        /// \brief
        /// TransactedFileBTreeRegistry is a \see{TransactedFile::Regitry}.
        /// It uses a \see{TransactedFileBree} to provide global ordered,
        /// associative storage for \see{TransactedFile} clients. Use it to
        /// store and retrieve practically any value derived from \see{Serializable}.
        /// The key type is std::string. TransactedFileBTreeRegistry is thread safe.
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileBTreeRegistry :
                public virtual TransactedFile::Registry,
                private TransactedFileBTree {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT (TransactedFileBTreeRegistry)

        private:
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

        public:
            /// \brief
            /// Default number of entries per \see{TransactedFileBTree::Node}.
            static const std::size_t DEFAULT_ENTRIES_PER_NODE = 32;
            /// \brief
            /// Number of \see{TransactedFileBTree::Node}s that will fit in to a \see{BlockAllocator} page.
            static const std::size_t DEFAULT_NODES_PER_PAGE = 5;

            /// \brief
            /// ctor.
            /// \param[in] valueAsObject_ true == \see{TransactedFileBTree::Node} stores values
            /// as \see{TransactedFile::Object}.
            /// \param[in] entriesPerNode_ Number of entries per \see{TransactedFileBTree::Node}.
            /// \param[in] nodesPerPage Number of \see{TransactedFileBTree::Node}s per allocator page.
            TransactedFileBTreeRegistry (
                TransactedFile::SharedPtr file = nullptr,
                TransactedFile::AddressType offset = 0,
                bool valueAsObject = true,
                std::size_t entriesPerNode = DEFAULT_ENTRIES_PER_NODE,
                std::size_t nodesPerPage = DEFAULT_NODES_PER_PAGE) :
                TransactedFileBTree (
                    file,
                    offset,
                    StringKey::GetContext (),
                    SerializableHeader (),
                    valueAsObject,
                    entriesPerNode,
                    nodesPerPage) {}

            /// \brief
            /// Given a key, retrieve the associated value. If key is not found,
            /// return nullptr.
            /// \param[in] key Key whose value to retrieve.
            /// \return Value @ key, nullptr if key is not found.
            virtual TransactedFile::Object::SharedPtr GetValue (const std::string &key) override;
            virtual bool InsertValue (
                const std::string &key,
                TransactedFile::Object::SharedPtr value) override;
            virtual void RemoveValue (const std::string &key) override;

        protected:
            // TransactedFile::Object
            virtual void AllocBlock () override {
                TransactedFileBTree::AllocBlock ();
                file->SetRegistryOffset (offset);
            }

            /// \brief
            /// TransactedFileBTreeRegistry is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (TransactedFileBTreeRegistry)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_TransactedFileBTreeRegistry_h)
