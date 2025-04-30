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
#include "thekogans/util/Allocator.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

        /// \struct FileAllocatorRegistry FileAllocator::Registry.h
        /// thekogans/util/FileAllocator::Registry.h
        ///
        /// \brief
        /// \see{FileAllocator}::Registry provides global associative storage for
        /// \see{FileAllocator} clients. It's available only when \see{FileAllocator::IsFixed}
        /// returns false. Use it to store and retrieve practically any value derived
        /// from \see{BTree::Value}. The key type is any std::string.

        struct _LIB_THEKOGANS_UTIL_DECL FileAllocator::Registry {
        private:
            /// \brief
            /// The registry.
            util::BTree btree;
            /// \brief
            /// Registry is a shared resource.
            /// Access to it must be protected.
            SpinLock spinLock;

        public:
            /// \brief
            /// ctor.
            /// See \see{FileAllocator::GetRegistry} for a better description of
            /// these parameters and when you should and should not change them.
            /// \param[in] fileAllocator Registry heap (see \see{FileAllocator}).
            /// \param[in] entriesPerNode Number of entries per btree node.
            /// \param[in] nodesPerPage Number of btree nodes per allocator page.
            /// \param[in] allocator Where allocator node pages come from.
            Registry (
                FileAllocator::SharedPtr fileAllocator,
                std::size_t entriesPerNode = 32,
                std::size_t nodesPerPage = 5,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

            /// \brief
            /// Delete the registry from the heap.
            /// \param[in] fileAllocator Heap where the registry resides.
            static void Delete (FileAllocator &fileAllocator);

            /// \brief
            /// Given a key, retrieve the associated value. If key is not found,
            /// return nullptr.
            /// \param[in] key Key whose value to retrieve.
            /// \return Value @ key, nullptr if key is not found.
            util::BTree::Value::SharedPtr GetValue (const std::string &key);
            /// \brief
            /// Given a key, do one of the following three;
            /// 1. If value != nullptr and key is not found, insert new key/value.
            /// 2. If value != nullptr and key is found, replace the old value with new.
            /// 3. if value == nullptr, delete the key from the registry (if found).
            /// \param[in] key Key to search/delete.
            /// \param[in] value Value to set/replace/delete.
            void SetValue (
                const std::string &key,
                util::BTree::Value::SharedPtr value);

            /// \brief
            /// Registry is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Registry)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileAllocatorRegistry_h)
