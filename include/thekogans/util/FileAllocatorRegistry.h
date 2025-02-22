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

#include "thekogans/util/Config.h"
#include "thekogans/util/BTree.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/BlockAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct FileAllocatorRegistry FileAllocatorRegistry.h
        /// thekogans/util/FileAllocatorRegistry.h
        ///
        /// \brief
        /// \see{FileAllocator} exposes a global heap value (rootOffset) that
        /// can be used to access a known object location. It provides an api
        /// for getting and setting it (GetRootOffset/SetRootOffset). If a single
        /// heap will be used by more than one client use the FileAllocatorRegistry
        /// to register an offset for each client.

        struct FileAllocatorRegistry : public BTree {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (FileAllocatorRegistry)

        public:
            /// \brief
            /// ctor.
            /// \param[in] fileAllocator Registry heap (see \see{FileAllocator}).
            /// \param[in] entriesPerNode If we're creating the registry, contains entries per
            /// \see{Node}. If we're reading an existing registry, this value will come from the
            /// \see{BTree::Header}.
            /// \param[in] nodesPerPage \see{BTree::Node}s are allocated using a \see{BlockAllocator}.
            /// This value sets the number of nodes that will fit on it's page. It's a subtle
            /// tunning parameter that might result in slight performance gains (depending on
            /// your situation). For the most part leaving it alone is the most sensible thing
            /// to do.
            /// \param[in] allocator This is the \see{Allocator} used to allocate pages
            /// for the \see{BlockAllocator}. As with the previous parameter, the same
            /// advice aplies.
            FileAllocatorRegistry (
                FileAllocator::SharedPtr fileAllocator,
                std::size_t entriesPerNode = DEFAULT_ENTRIES_PER_NODE,
                std::size_t nodesPerPage = BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

            /// \brief
            /// Delete the registry from the heap.
            /// \param[in] fileAllocator Heap where the registry resides.
            static void Delete (FileAllocator &fileAllocator);

            /// \brief
            /// FileAllocatorRegistry is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (FileAllocatorRegistry)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileAllocatorRegistry_h)
