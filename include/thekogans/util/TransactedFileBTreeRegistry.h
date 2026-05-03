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
#include "thekogans/util/TransactedFileBTree.h"
#include "thekogans/util/TransactedFileBTreeKeys.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        /// \struct TransactedFileBTreeRegistry TransactedFileBTreeRegistry.h
        /// thekogans/util/TransactedFileBTreeRegistry.h
        ///
        /// \brief
        /// \see{TransactedFileBTreeRegistry} is a \see{TransactedFileBTree}.
        /// It's also a \see{TransactedFile::Allocator::Header::rootObject}.
        /// It provides global ordered, associative storage for \see{TransactedFile}
        /// clients. Use it to store and retrieve practically any value derived
        /// from \see{Serializable}. The key type is std::string.
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileBTreeRegistry : public TransactedFile::Registry {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (TransactedFileBTreeRegistry)

        private:
            bool valueAsObject;
            std::size_t entriesPerNode;
            std::size_t nodesPerPage;
            Allocator::SharedPtr allocator;
            TransactedFileBTree::SharedPtr btree;

        public:
            /// \brief
            /// Default number of entries per \see{TransactedFileBTree::Node}.
            static const std::size_t DEFAULT_BTREE_ENTRIES_PER_NODE = 32;
            /// \brief
            /// Number of \see{TransactedFileBTree::Node}s that will fit in to a \see{BlockAllocator} page.
            static const std::size_t DEFAULT_BTREE_NODES_PER_PAGE = 5;

            /// \brief
            /// ctor.
            /// \param[in] fileAllocator \see{TransactedFileBTree} where the registry lives.
            /// \param[in] entriesPerNode Number of entries per \see{TransactedFileBTree::Node}.
            /// \param[in] nodesPerPage Number of \see{TransactedFileBTree::Node}s per allocator page.
            /// \param[in] allocator Where \see{TransactedFileBTree::Node} pages come from.
            TransactedFileBTreeRegistry (
                bool valueAsObject_ = false,
                std::size_t entriesPerNode_ = DEFAULT_BTREE_ENTRIES_PER_NODE,
                std::size_t nodesPerPage_ = DEFAULT_BTREE_NODES_PER_PAGE,
                util::Allocator::SharedPtr allocator_ = DefaultAllocator::Instance ()) :
                valueAsObject (valueAsObject_),
                entriesPerNode (entriesPerNode_),
                nodesPerPage (nodesPerPage_),
                allocator (allocator_) {}

            virtual void Init (TransactedFile::SharedPtr file) override;

            /// \brief
            /// Given a key, retrieve the associated value. If key is not found,
            /// return nullptr.
            /// \param[in] key Key whose value to retrieve.
            /// \return Value @ key, nullptr if key is not found.
            virtual Serializable::SharedPtr GetValue (const std::string &key) override;
            /// \brief
            /// Given a key, do one of the following three;
            /// 1. If value != nullptr and key is not found, insert new key/value.
            /// 2. If value != nullptr and key is found, replace the old value with new.
            /// 3. if value == nullptr, and key if found, delete the key from the registry.
            /// \param[in] key Key to search/delete.
            /// \param[in] value Value to set/replace/delete.
            virtual void SetValue (
                const std::string &key,
                Serializable::SharedPtr value) override;

            /// \brief
            /// TransactedFileBTreeRegistry is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (TransactedFileBTreeRegistry)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_TransactedFileBTreeRegistry_h)
