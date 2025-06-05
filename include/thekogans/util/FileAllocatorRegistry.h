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

#if !defined (__thekogans_util_FileAllocatorRegistry_h)
#define __thekogans_util_FileAllocatorRegistry_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/BTree.h"
#include "thekogans/util/BTreeKeys.h"
#include "thekogans/util/FileAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct FileAllocatorRegistry FileAllocatorRegistry.h
        /// thekogans/util/FileAllocatorRegistry.h
        ///
        /// \brief
        /// \see{FileAllocatorRegistry} is a \see{BTree}. It's also a
        /// \see{FileAllocator} root object. It provides global ordered,
        /// associative storage for \see{FileAllocator} clients. Use it
        /// to store and retrieve practically any value derived from
        /// \see{BTree::Value}. The key type is any std::string.
        struct _LIB_THEKOGANS_UTIL_DECL FileAllocatorRegistry :
                private BTree,
                public Subscriber<FileAllocator::ObjectEvents> {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (FileAllocatorRegistry)

        public:
            /// \brief
            /// Default number of entries per \see{BTree::Node}.
            static const std::size_t DEFAULT_BTREE_ENTRIES_PER_NODE = 32;
            /// \brief
            /// Number of \see{BTree::Node}s that will fit in to a \see{BlockAllocator} page.
            static const std::size_t DEFAULT_BTREE_NODES_PERP_PAGE = 5;

            /// \brief
            /// ctor.
            /// \param[in] fileAllocator \see{FileAllocator} where the registry lives.
            /// \param[in] entriesPerNode Number of entries per \see{BTree::Node}.
            /// \param[in] nodesPerPage Number of \see{BTree::Node}s per allocator page.
            /// \param[in] allocator Where \see{BTree::Node} pages come from.
            FileAllocatorRegistry (
                FileAllocator::SharedPtr fileAllocator,
                std::size_t entriesPerNode = DEFAULT_BTREE_ENTRIES_PER_NODE,
                std::size_t nodesPerPage = DEFAULT_BTREE_NODES_PERP_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

            /// \brief
            /// Given a key, retrieve the associated value. If key is not found,
            /// return nullptr.
            /// \param[in] key Key whose value to retrieve.
            /// \return Value @ key, nullptr if key is not found.
            BTree::Value::SharedPtr GetValue (const std::string &key);
            /// \brief
            /// Given a key, do one of the following three;
            /// 1. If value != nullptr and key is not found, insert new key/value.
            /// 2. If value != nullptr and key is found, replace the old value with new.
            /// 3. if value == nullptr, delete the key from the registry (if found).
            /// \param[in] key Key to search/delete.
            /// \param[in] value Value to set/replace/delete.
            void SetValue (
                const std::string &key,
                BTree::Value::SharedPtr value);

        protected:
            // FileAllocator::ObjectEvents
            /// \brief
            /// Offset changed.
            /// \param[in] object \see{FileAllocator::Object} whose offset changed.
            virtual void OnFileAllocatorObjectOffsetChanged (
                    FileAllocator::Object::SharedPtr object) noexcept override {
                fileAllocator->SetRootOffset (object->GetOffset ());
            }

            /// \brief
            /// FileAllocatorRegistry is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (FileAllocatorRegistry)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileAllocatorRegistry_h)
