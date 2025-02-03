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

#if !defined (__thekogans_util_RandomSizeFileAllocator_h)
#define __thekogans_util_RandomSizeFileAllocator_h

#include "thekogans/util/Types.h"
#include "thekogans/util/BTree.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/FileAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct RandomSizeFileAllocator RandomSizeFileAllocator.h thekogans/util/RandomSizeFileAllocator.h
        ///
        /// \brief
        /// RandomSizeFileAllocator

        struct _LIB_THEKOGANS_UTIL_DECL RandomSizeFileAllocator : public FileAllocator {
        private:
            BTree btree;

        public:
            RandomSizeFileAllocator (
                const std::string &path,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                FileAllocator (path, 0, 0, allocator),
                btree (this, header.freeBlock) {}

            virtual PtrType Alloc (std::size_t size) override;
            virtual void Free (
                PtrType offset,
                std::size_t size) override;

            virtual PtrType AllocBTreeNode (std::size_t size) override;
            virtual void FreeBTreeNode (
                PtrType offset,
                std::size_t size) override;

            /// \brief
            /// RandomSizeFileAllocator is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (RandomSizeFileAllocator)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RandomSizeFileAllocator_h)
