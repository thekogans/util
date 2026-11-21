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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include "thekogans/util/Exception.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        namespace {
            bool has_non_zero (
                    const ui8 *begin,
                    const ui8 *end) noexcept {
                const auto *p = begin;
                // Fast path for tiny blocks (e.g., 32 bytes)
                if (end - p < 64) {
                    while (p < end) {
                        if (*p != 0) {
                            return true;
                        }
                        ++p;
                    }
                    return false;
                }
                // 1. Align to UI64_SIZE-byte boundary
                while ((reinterpret_cast<std::uintptr_t> (p) & 7u) != 0) {
                    if (*p != 0) {
                        return true;
                    }
                    ++p;
                }
                // 2. Unrolled 32-byte (4 x ui64) scan
                while (end - p >= 32) {
                    ui64 w0, w1, w2, w3;
                    std::memcpy (&w0, p, UI64_SIZE);
                    std::memcpy (&w1, p + UI64_SIZE, UI64_SIZE);
                    std::memcpy (&w2, p + 16, UI64_SIZE);
                    std::memcpy (&w3, p + 24, UI64_SIZE);
                    if ((w0 | w1 | w2 | w3) != 0) {
                        return true;
                    }
                    p += 32;
                }
                // 3. Remaining UI64_SIZE-byte words
                while (end - p >= UI64_SIZE) {
                    ui64 w;
                    std::memcpy (&w, p, UI64_SIZE);
                    if (w != 0) {
                        return true;
                    }
                    p += UI64_SIZE;
                }
                // 4. Final 0..7 tail bytes
                while (p < end) {
                    if (*p != 0) {
                        return true;
                    }
                    ++p;
                }
                return false;
            }
        }

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
                        // Garbage in the payload of a secure heap.
                        if (has_non_zero (range.GetDataPtr (), range.GetDataPtr () + range.GetDataAvailable ()) &&
                                !callback (next, ERROR_GARBAGE_IN_PAYLOAD)) {
                            return;
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
            std::sort (allocatorFreeList.begin (), allocatorFreeList.end ());
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

        void TransactedFile::HeapWalker::CheckForOrphans (
                TransactedFile *file,
                Callback callback) {
            std::vector<Block> allocatedBlocks;
            Block block (file, TransactedFile::Header::BLOCK_OFFSET);
            // Check block is intact.
            block.Read ();
            // Block 0 cannot be free.
            if (block.IsFree ()) {
                if (!callback (block, ERROR_FREE_BLOCK_0)) {
                    return;
                }
            }
            else {
                allocatedBlocks.push_back (block);
            }
            while (!block.IsLast ()) {
                Block next (file);
                block.Next (next);
                if (!next.IsFree ()) {
                    allocatedBlocks.push_back (next);
                }
                block = next;
            }
            TransactedFile::Object::BlockMap blockMap;
            TransactedFile::Object::BlockMap duplicatesMap;
            file->GetHeader ()->GetAllocatedBlockMap (blockMap, &duplicatesMap);
            // 1. Check for physically allocated blocks that are
            // unreachable from the root object graph
            // (orphans/zombies).
            for (const Block &allocatedBlock : allocatedBlocks) {
                auto it = blockMap.find (allocatedBlock);
                if (it == blockMap.end ()) {
                    if (!callback (allocatedBlock, ERROR_ORPHAN_BLOCK)) {
                        return;
                    }
                }
                else {
                    if (allocatedBlock.GetSize () < it->second.size) {
                        if (!callback (it->first, ERROR_TRUNCATED_BLOCK)) {
                            return;
                        }
                    }
                    blockMap.erase (it);
                }
            }
            for (const auto &[block, header] : duplicatesMap) {
                if (!callback (block, ERROR_DUPLICATE_BLOCK_REFERENCE)) {
                    return;
                }
            }
            for (const auto &[block, header] : blockMap) {
                if (!callback (block, ERROR_DANGLING_BLOCK_REFERENCE)) {
                    return;
                }
            }
        }

        void TransactedFile::HeapWalker::GetStats (
                TransactedFile *file,
                Stats &stats) {
            stats.Clear ();
            Block block (file, TransactedFile::Header::BLOCK_OFFSET);
            // Check block is intact.
            block.Read ();
            // Block 0 cannot be free.
            if (!block.IsFree ()) {
                stats.arenaSize = file->GetSizeEx ();
                stats.totalBlocks = 1;
                stats.allocatedBlocks = 1;
                TransactedFile::SizeType blockSize = block.GetSize ();
                stats.allocatedPayloadBytes = blockSize;
                stats.allocatedBlockBytes = blockSize + TransactedFile::Block::SIZE;
            }
            else {
                // FIXME: throw something
                assert (0);
            }
            while (!block.IsLast ()) {
                Block next (file);
                block.Next (next);
                TransactedFile::SizeType nextSize = next.GetSize ();
                ++stats.totalBlocks;
                if (next.IsStraddlingPage ()) {
                    ++stats.pageStraddlingBlocks;
                    stats.pageStraddlingPayloadBytes += nextSize;
                    stats.pageStraddlingBlockBytes += nextSize + TransactedFile::Block::SIZE;
                }
                if (!next.IsFree ()) {
                    ++stats.allocatedBlocks;
                    stats.allocatedPayloadBytes += nextSize;
                    stats.allocatedBlockBytes += nextSize + TransactedFile::Block::SIZE;
                }
                else  {
                    ++stats.freeBlocks;
                    stats.freePayloadBytes += nextSize;
                    stats.freeBlockBytes += nextSize + TransactedFile::Block::SIZE;
                    if (stats.largestFreePayload < nextSize) {
                        stats.largestFreePayload = nextSize;
                    }
                    if (stats.smallestFreePayload > nextSize) {
                        stats.smallestFreePayload = nextSize;
                    }
                    std::size_t blockIndex =
                        nextSize <= 64 ? 0 :
                        nextSize <= 256 ? 1 :
                        nextSize <= 1024 ? 2 :
                        nextSize <= 4096 ? 3 :
                        nextSize <= 16384 ? 4 : 5;
                    ++stats.freeSizeHistogram[blockIndex].first;
                    stats.freeSizeHistogram[blockIndex].second += nextSize;
                }
                block = next;
            }
            if (stats.freeBlocks == 0) {
                stats.smallestFreePayload = 0;
            }
        }

    } // namespace util
} // namespace thekogans
