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

#include "thekogans/util/Exception.h"
#include "thekogans/util/File.h"
#include "thekogans/util/TransactedFileBTreeAllocator.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (
            thekogans::util::TransactedFileBTreeAllocator,
            1,
            TransactedFileBTreeAllocator::SIZE,
            TransactedFile::Allocator::TYPE)

        void TransactedFileBTreeAllocator::Block::Read () {
            Allocator::Block::Read ();
            if (IsFree () && IsBTreeNode ()) {
                TransactedFile::Range range (file, offset, PTR_TYPE_SIZE);
                range >> nextBTreeNodeOffset;
            }
        }

        void TransactedFileBTreeAllocator::Block::Write () const {
            Allocator::Block::Write ();
            if (IsFree () && IsBTreeNode ()) {
                TransactedFile::Range range (file, offset, PTR_TYPE_SIZE, false);
                range << nextBTreeNodeOffset;
            }
        }

        TransactedFile::Allocator::PtrType TransactedFileBTreeAllocator::Alloc (std::size_t size) {
            PtrType offset = 0;
            if (size > 0) {
                if (size < MIN_USER_DATA_SIZE) {
                    size = MIN_USER_DATA_SIZE;
                }
                LockGuard<SpinLock> guard (spinLock);
                BTree::KeyType result;
                // If allocation request is <= BTree::Node::FileSize and the BTree::Node
                // cache is not empty, reuse the first free block. This optimization allows
                // us to reclaim the node blocks to be used for general purpose allocations.
                if (size <= btreeNodeFileSize && header.freeBTreeNodeOffset != 0) {
                    result.first = btreeNodeFileSize;
                    result.second = header.freeBTreeNodeOffset;
                    Block block (*file, header.freeBTreeNodeOffset);
                    block.Read ();
                    if (block.IsFree () && block.IsBTreeNode ()) {
                        header.freeBTreeNodeOffset = block.GetNextBTreeNodeOffset ();
                        SetDirty (true);
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Heap corruption @" THEKOGANS_UTIL_UI64_FORMAT
                            ", expecting a free BTree::Node block.",
                            header.freeBTreeNodeOffset);
                    }
                }
                else {
                    // Else look for a free block large enough to satisfy the request.
                    result = btree->Find (BTree::KeyType (size, 0));
                    if (result.second != 0) {
                        // Got it!
                        assert (result.first >= size);
                        btree->Remove (result);
                    }
                }
                // If we got a block by the above two methods, see if it's too big.
                if (result.second != 0) {
                    // If the block we got is bigger than we need, split it.
                    //
                    // we need o go from:
                    //
                    //               |--------------------- result.first ---------------------|
                    // -----+--------+---------------+----------------------------------------+--------+-----
                    //      | header |               |                                        | footer |
                    // -----+--------+---------------+----------------------------------------+--------+-----
                    //               |               |
                    //         result.second    page boundary
                    //
                    // to: Where we do our best to not straddle a page boundary...
                    //
                    //               |--------------------- result.first ---------------------|
                    // -----+--------+------+--------+--------+------+--------+--------+------+--------+-----
                    //      | header | prev | footer | header | size | footer | header | next | footer |
                    // -----+--------+------+--------+--------+------+--------+--------+------+--------+-----
                    //               |-- remainder --|                                 |
                    //         result.second    page boundary                 optional free block
                    //           pageOffset
                    //
                    // ui64 pageOffset = result.second & (TransactedFile::Page::SIZE - 1);
                    // ui64 remainder = TransactedFile::Page::SIZE - pageOffset;
                    // prev.offset = result.second;
                    // prev.size = remainder - Block::HEADER_SIZE;
                    // offset = prev.offset + remainder + Block::HEADER_SIZE;
                    // size = size or result.first - remainder;
                    // next.offset = offset + Block::SIZE;
                    // next.size = result.first - Block::SIZE - prev.size - Block::SIZE - size;
                    //
                    // or to: ...where we potentially straddle a page boundary.
                    //
                    //               |----------- result.first -----------|
                    // -----+--------+------+--------+--------+-----------+--------+-----
                    //      | header | size | footer | header | next.size | footer |
                    // -----+--------+------+--------+--------+-----------+--------+-----
                    //               |                        |
                    //         result.second             next.offset
                    //
                    // next.offset = result.second + size + Block::SIZE;
                    // next.size = result.first - size - Block::SIZE;
                    ui64 remainder = result.first - size;
                    if (remainder >= MIN_BLOCK_SIZE) {
                        // Check to see if the block would straddle a page boundary...
                        ui64 pageOffset = offset & (TransactedFile::Page::SIZE - 1);
                        if (pageOffset + size + Block::HEADER_SIZE > TransactedFile::Page::SIZE) {
                            // ...it does. Now check to see if the block would fit aligned...
                            remainder = TransactedFile::Page::SIZE - pageOffset;
                            if (remainder >= MIN_USER_DATA_SIZE + Block::HEADER_SIZE &&
                                result.first > remainder &&
                                result.first - remainder >= size + Block::HEADER_SIZE) {
                                // ...it would. Add a filler block.
                                Block prev (
                                    *file,
                                    result.second,
                                    Block::FLAGS_FREE,
                                    remainder - Block::HEADER_SIZE);
                                prev.Write ();
                                btree->Insert (BTree::KeyType (prev.GetSize (), prev.GetOffset ()));
                                // Adjust result to account for the filler block so that downstream
                                // calculations have the right values.
                                result.first -= remainder + Block::HEADER_SIZE;
                                result.second += remainder + Block::HEADER_SIZE;
                            }
                        }
                        offset = result.second;
                        remainder = result.first - size;
                        if (remainder >= MIN_BLOCK_SIZE) {
                            Block next (
                                *file,
                                result.second + size + Block::SIZE,
                                Block::FLAGS_FREE,
                                remainder - Block::SIZE);
                            next.Write ();
                            btree->Insert (BTree::KeyType (next.GetSize (), next.GetOffset ()));
                        }
                        else {
                            size = result.first;
                        }
                    }
                    else {
                        offset = result.second;
                        size = result.first;
                    }
                }
                else {
                    // If not, we need to grow the file.
                    // Do your best to not straddle a page boundary.
                    // Ranges that straddle page boundaries incur an
                    // allocation/copy/deallocation penalty.
                    // Calculate the remainder left in the last page.
                    ui64 remainder =
                        TransactedFile::Page::SIZE -
                        (file->GetSize () & (TransactedFile::Page::SIZE - 1));
                    // If we don't fit in to remainder and, if the remainder
                    // can be turned in to another block and we fit into one
                    // page, all is well. Go ahead and create a spacer block
                    // and align us with a page boundary. Otherwise, because
                    // there can be no gaps between blocks, we will straddle
                    // a page boundary.
                    if (remainder < size + Block::SIZE && remainder >= MIN_BLOCK_SIZE &&
                            size <= (TransactedFile::Page::SIZE - Block::SIZE)) {
                        Block prev (
                            *file,
                            file->Grow (remainder) + Block::HEADER_SIZE,
                            Block::FLAGS_FREE,
                            remainder - Block::SIZE);
                        prev.Write ();
                        btree->Insert (BTree::KeyType (prev.GetSize (), prev.GetOffset ()));
                    }
                    // Otherwise, we fit in the ramainder. Check if what will remain
                    // after our allocation would be too small for a block and cause
                    // the next allocation to straddle the page boundry...
                    else if (remainder - size - Block::SIZE < MIN_BLOCK_SIZE) {
                        // ...it is. Round up the size request to align the next
                        // allocation to a page boundary.
                        size = remainder - Block::SIZE;
                    }
                    // No free block large enough is found? Grow the file.
                    offset = file->Grow (Block::SIZE + size) + Block::HEADER_SIZE;
                }
                // By now we got our block by either reusing a free one or growing the file.
                Block block (*file, offset, 0, size);
                block.Write ();
            }
            return offset;
        }

        void TransactedFileBTreeAllocator::Free (PtrType offset) {
            // To honor the Allocator policy, we ignore NULL pointers.
            if (offset != 0) {
                LockGuard<SpinLock> guard (spinLock);
                Block block (*file, offset);
                block.Read ();
                if (!block.IsFree ()) {
                    PtrType clearOffset = block.GetOffset ();
                    ui64 clearLength = block.GetSize ();
                    // Consolidate adjacent free non BTree::Node blocks.
                    Block prev (*file);
                    if (block.Prev (prev) && prev.IsFree () && !prev.IsBTreeNode ()) {
                        btree->Remove (BTree::KeyType (prev.GetSize (), prev.GetOffset ()));
                        if (IsSecure ()) {
                            // Assume prev body is clear.
                            clearOffset -= Block::HEADER_SIZE;
                            clearLength += Block::HEADER_SIZE;
                        }
                    #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        else {
                            // Since block will grow to occupy prev,
                            // it's offset is no longer valid.
                            block.Invalidate ();
                        }
                    #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        // Back up to cover the prev.
                        block.SetOffset (block.GetOffset () - Block::SIZE - prev.GetSize ());
                        block.SetSize (block.GetSize () + Block::SIZE + prev.GetSize ());
                    }
                    Block next (*file);
                    if (block.Next (next) && next.IsFree () && !next.IsBTreeNode ()) {
                        btree->Remove (BTree::KeyType (next.GetSize (), next.GetOffset ()));
                        if (IsSecure ()) {
                            // Assume next body is clear.
                            clearLength += Block::HEADER_SIZE;
                        }
                    #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        else {
                            // Since block will grow to occupy next,
                            // next offset is no longer valid.
                            next.Invalidate ();
                        }
                    #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        // Expand to swallow the next.
                        block.SetSize (block.GetSize () + Block::SIZE + next.GetSize ());
                    }
                    // If we're not the last block...
                    if (!block.IsLast ()) {
                        // ... add it to the free list.
                        btree->Insert (BTree::KeyType (block.GetSize (), block.GetOffset ()));
                        block.SetFree (true);
                        block.Write ();
                        if (IsSecure ()) {
                            TransactedFile::Range range (*file, clearOffset, clearLength, false);
                            range.Seek (
                                SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable ()), SEEK_CUR);
                        }
                    }
                    else {
                        // If we are, truncate the heap.
                        file->Shrink (Block::SIZE + block.GetSize ());
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Attempting to free an unallocated block @"
                        THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
            }
        }

        TransactedFile::Allocator::PtrType TransactedFileBTreeAllocator::Realloc (
                PtrType offset,
                std::size_t size,
                bool moveData) {
            if (offset == 0) {
                offset = Alloc (size);
            }
            else {
                Block block (*file, offset);
                block.Read ();
                if (block.GetSize () < size) {
                    offset = Alloc (size);
                    if (moveData) {
                        TransactedFile::Range oldRange (
                            *file, block.GetOffset (), block.GetSize ());
                        TransactedFile::Range range (*file, offset, size, false);
                        range.Seek (
                            oldRange.Read (range.GetDataPtr (), range.GetDataAvailable ()), SEEK_CUR);
                        if (IsSecure ()) {
                            range.Seek (
                                SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable ()), SEEK_CUR);
                        }
                    }
                    Free (block.GetOffset ());
                }
                else {
                    // If the new size leaves room for another block, split existing block.
                    ui64 remainder = block.GetSize () - size;
                    if (remainder >= MIN_BLOCK_SIZE) {
                        Block next (
                            *file,
                            offset + size + Block::SIZE,
                            Block::FLAGS_FREE,
                            remainder - Block::SIZE);
                        next.Write ();
                        {
                            LockGuard<SpinLock> guard (spinLock);
                            btree->Insert (BTree::KeyType (next.GetSize (), next.GetOffset ()));
                        }
                        block.SetSize (size);
                        block.Write ();
                    }
                }
            }
            return offset;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTreeAllocator::Header &header) {
            serializer >> header.btreeOffset >> header.freeBTreeNodeOffset;
            return serializer;
        }

        void TransactedFileBTreeAllocator::Read (
                const SerializableHeader &header_,
                Serializer &serializer) {
            LockGuard<SpinLock> guard (spinLock);
            Allocator::Read (header_, serializer);
            ui32 magic;
            serializer >> magic;
            if (magic == MAGIC32) {
                serializer >> header;
                btree.Reset (
                    new BTree (
                        *this,
                        header.btreeOffset,
                        btreeEntriesPerNode,
                        btreeNodesPerPage,
                        allocator));
                Subscriber<TransactedFile::ObjectEvents>::Subscribe (*btree);
                btreeNodeFileSize = BTree::Node::FileSize (btree->header.entriesPerNode);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFileBTreeAllocator file (%s).",
                    file->GetPath ().c_str ());
            }
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const TransactedFileBTreeAllocator::Header &header) {
            serializer << header.btreeOffset << header.freeBTreeNodeOffset;
            return serializer;
        }

        void TransactedFileBTreeAllocator::Write (Serializer &serializer) const {
            LockGuard<SpinLock> guard (spinLock);
            Allocator::Write (serializer);
            serializer << MAGIC32 << header;
        }

        TransactedFile::Allocator::PtrType TransactedFileBTreeAllocator::AllocBTreeNode (std::size_t size) {
            PtrType offset = 0;
            if (header.freeBTreeNodeOffset != 0) {
                offset = header.freeBTreeNodeOffset;
                Block block (*file, offset);
                block.Read ();
                if (block.IsFree () && block.IsBTreeNode ()) {
                    header.freeBTreeNodeOffset = block.GetNextBTreeNodeOffset ();
                    SetDirty (true);
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Heap corruption @" THEKOGANS_UTIL_UI64_FORMAT
                        ", expecting a free BTree::Node block.",
                        offset);
                }
            }
            else {
                offset = file->Grow (Block::SIZE + size) + Block::HEADER_SIZE;
            }
            Block block (*file, offset, Block::FLAGS_BTREE_NODE, size);
            block.Write ();
            return offset;
        }

        void TransactedFileBTreeAllocator::FreeBTreeNode (PtrType offset) {
            if (offset != 0) {
                Block block (*file, offset);
                block.Read ();
                if (!block.IsFree () && block.IsBTreeNode ()) {
                    if (!block.IsLast ()) {
                        block.SetFree (true);
                        block.SetNextBTreeNodeOffset (header.freeBTreeNodeOffset);
                        block.Write ();
                        header.freeBTreeNodeOffset = offset;
                        SetDirty (true);
                    }
                    else {
                        file->Shrink (Block::SIZE + block.GetSize ());
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    } // namespace util
} // namespace thekogans
