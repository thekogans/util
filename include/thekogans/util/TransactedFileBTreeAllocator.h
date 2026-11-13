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

#if !defined (__thekogans_util_TransactedFileBTreeAllocator_h)
#define __thekogans_util_TransactedFileBTreeAllocator_h

#include "thekogans/util/Config.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/TransactedFile.h"
#include "thekogans/util/TransactedFileBTreeAllocatorBTree.h"

namespace thekogans {
    namespace util {

        /// \struct TransactedFileBTreeAllocator TransactedFileBTreeAllocator.h
        /// thekogans/util/TransactedFileBTreeAllocator.h
        ///
        /// \brief
        /// TransactedFileBTreeAllocator is a general purpose \see{TransactedFile::Allocator}.
        /// It uses a hand tuned \see{TransactedFileBTreeAllocator::BTree} to manage the free list.
        /// It is specifically tuned and is very sesitive to the fact that blocks that straddle
        /// page boundaries (see \see{PageMap::pageSize} incur a heavy performance penalty
        /// (see \see{TransactedFile::Range}). Every attempt is made to avoid that.
        /// TransactedFileBTreeAllocator is thread safe.
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileBTreeAllocator :
                public TransactedFile::Allocator,
                // As much as I'd like to hide from the rest of the world that an Allocator is
                // an Object, TransactedFile assumes it (as it should) during initiatiation.
                // Unfortunately private inheritance prevents it from cross-typing (dynamic_cast)
                // the Allocator interface to Object through TransactedFileBTreeAllocatorBTree.
                // So here sits an announcement to the rest of the world that yes, we are indeed
                // an Object.
                public virtual TransactedFile::Object,
                private TransactedFileBTreeAllocatorBTree {
            /// \brief
            /// TransactedFileBTreeAllocator is an \see{TransactedFile::Object}.
            THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT (TransactedFileBTreeAllocator)

        private:
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

        public:
            /// \brief
            /// ctor.
            /// \param[in] file
            /// \param[in] offset
            /// \param[in] entriesPerNode Number of entries per \see{BTree::Node}.
            /// \param[in] nodesPerPage Number of \see{TransactedFileBTreeAllocatorBTree::Node}s
            /// that will fit in to a \see{SlabAllocatorImpl} page.
            TransactedFileBTreeAllocator (
                TransactedFile::SharedPtr file = nullptr,
                TransactedFile::AddressType offset = 0,
                std::size_t entriesPerNode = TransactedFileBTreeAllocatorBTree::DEFAULT_ENTRIES_PER_NODE,
                std::size_t nodesPerPage = TransactedFileBTreeAllocatorBTree::DEFAULT_NODES_PER_PAGE) :
                TransactedFileBTreeAllocatorBTree (file, offset, entriesPerNode, nodesPerPage) {}

            // Allocator
            /// \brief
            /// Allocate a block.
            /// \param[in] size Size of block to allocate.
            /// \return Offset to the allocated block.
            virtual TransactedFile::AddressType Alloc (TransactedFile::SizeType size) override;
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// \param[in] offset Offset of block to free.
            virtual void Free (TransactedFile::AddressType offset) override;
            /// \brief
            /// Resize a block. Make it bigger or smaller.
            /// \param[in] offset Offset of \see{Block} to resize.
            /// \param[in] newSize New \see{Block} size.
            /// \param[in] moveData true == Copy the data to the new block.
            /// \return If newSize is greater than current size, return the
            /// new \see{Block} offset. If not, return the old \see{Block}
            /// offset.
            virtual TransactedFile::AddressType Realloc (
                TransactedFile::AddressType offset,
                TransactedFile::SizeType newSize,
                bool moveData = true) override;

            /// \brief
            /// TransactedFileBTreeAllocator is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (TransactedFileBTreeAllocator)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_TransactedFileBTreeAllocator_h)
