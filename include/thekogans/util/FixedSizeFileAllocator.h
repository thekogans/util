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

#if !defined (__thekogans_util_FixedSizeFileAllocator_h)
#define __thekogans_util_FixedSizeFileAllocator_h

#include "thekogans/util/FileAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct FixedSizeFileAllocator FixedSizeFileAllocator.h thekogans/util/FixedSizeFileAllocator.h
        ///
        /// \brief
        /// FixedSizeFileAllocator

        struct _LIB_THEKOGANS_UTIL_DECL FixedSizeFileAllocator : public FileAllocator {
            FixedSizeFileAllocator (
                const std::string &path,
                std::size_t blockSize = DEFAULT_BLOCK_SIZE,
                std::size_t blocksPerPage = BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                FileAllocator (path, blockSize, blocksPerPage, allocator) {}

            virtual PtrType AllocBTreeNode (std::size_t size) override;
            virtual void FreeBTreeNode (
                PtrType offset,
                std::size_t size) override;

            /// \brief
            /// FixedSizeFileAllocator is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (FixedSizeFileAllocator)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FixedSizeFileAllocator_h)
