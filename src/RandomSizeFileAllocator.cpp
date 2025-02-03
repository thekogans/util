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
#include "thekogans/util/RandomSizeFileAllocator.h"

namespace thekogans {
    namespace util {

        FileAllocator::PtrType RandomSizeFileAllocator::AllocBTreeNode (std::size_t size) {
            PtrType offset = nullptr;
            if (size > 0) {
                size +=  BlockHeader::SIZE + BlockHeader::SIZE;
                offset = (PtrType)file.GetSize ();
                file.SetSize ((ui64)offset + size);
                WriteBlockHeaderAndFooter (
                    file,
                    offset,
                    BlockHeader::FLAGS_BTREE_NODE | BlockHeader::FLAGS_IN_USE,
                    size);
                offset = (ui64)offset + BlockHeader::SIZE;
            }
            return offset;
        }

        void RandomSizeFileAllocator::FreeBTreeNode (
                PtrType offset,
                std::size_t size) {
        }

    } // namespace util
} // namespace thekogans
