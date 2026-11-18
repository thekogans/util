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

#include <vector>
#include "thekogans/util/Exception.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        void TransactedFile::HeapWalker::ValidateHeap (
                TransactedFile *file,
                Callback callback) {
            Block block (file, TransactedFile::Header::BLOCK_OFFSET);
            // Check block is intact.
            block.Read ();
            // Block 0 cannot be free.
            if (block.IsFree ()) {
                if (!callback (block, ERROR_FREE_BLOCK_0)) {
                    return;
                }
            }
            std::vector<Block> freeBlockList;
            while (!block.IsLast ()) {
                Block next (file);
                block.Next (next);
                // Two consecutive free blocks.
                if (block.IsFree () && next.IsFree ()) {
                    if (!callback (block, ERROR_TWO_CONSECUTIVE_FREE_BLOCKS)) {
                        return;
                    }
                }
                if (next.IsFree ()) {
                    if (file->IsSecure ()) {
                        Range range (file, next.GetOffset (), next.GetSize ());
                        for (SizeType i = 0, count = next.GetSize (); i < count; ++i) {
                            ui8 byte;
                            range >> byte;
                            if (byte != 0) {
                                // Garbage in the payload of a secure heap.
                                if (!callback (next, ERROR_GARBAGE_IN_PAYLOAD)) {
                                    return;
                                }
                                break;
                            }
                        }
                    }
                    freeBlockList.push_back (next);
                }
                // Check heap contiguity.
                block = next;
            }
            // Last block is free.
            if (block.IsFree ()) {
                if (!callback (block, ERROR_FREE_LAST_BLOCK)) {
                    return;
                }
            }
            // Check that every free block is accounted for in the allocator btree.
            // Collect all {size, offset} pairs from the allocator's free-list B-tree.
            std::vector<std::pair<AddressType, SizeType>> allocatorFreeList;
            file->GetAllocator ()->GetFreeList (allocatorFreeList);
            // 1. Verify exact count match (no leaked free blocks, no phantom B-tree entries).
            if (freeBlockList.size () != allocatorFreeList.size ()) {
                // Report count mismatch
                if (!callback (Block (file), ERROR_FREE_COUNT_MISMATCH)) {
                    return;
                }
            }
            // 2. Verify every physical free block exists in btreeFreeList with exact {size, offset}.
            for (const Block &freeBlock : freeBlockList) {
                std::pair<AddressType, SizeType> expected{freeBlock.GetOffset (), freeBlock.GetSize ()};
                if (!std::binary_search (allocatorFreeList.begin (), allocatorFreeList.end (), expected)) {
                    if (!callback (freeBlock, ERROR_BLOCK_MISSING_FROM_FREE_LIST)) {
                        return;
                    }
                }
            }
        }

        void TransactedFile::HeapWalker::WalkHeap (
                TransactedFile *file,
                Callback callback) {
        }

    } // namespace util
} // namespace thekogans
