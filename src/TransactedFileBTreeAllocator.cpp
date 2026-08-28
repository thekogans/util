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

        TransactedFile::Allocator::PtrType TransactedFileBTreeAllocator::Alloc (std::size_t size) {
            PtrType offset = 0;
            if (size > 0) {
                if (size < MIN_USER_DATA_SIZE) {
                    size = MIN_USER_DATA_SIZE;
                }
                LockGuard<SpinLock> guard (spinLock);
                BTree::Key result = btree->Find (BTree::Key (size, 0));
                // If we got a block see if it's too big.
                if (result.offset != 0) {
                    // Got it!
                    assert (result.size >= size);
                    btree->Remove (result);
                    // If the block we got is bigger than we need, split it.
                    //
                    // we need to go from:
                    //
                    //               |--------------------- result.size ---------------------|
                    // -----+--------+---------------+----------------------------------------+--------+-----
                    //      | header |               |                                        | footer |
                    // -----+--------+---------------+----------------------------------------+--------+-----
                    //               |               |
                    //         result.offset         |
                    //                         page boundary
                    //
                    // to: Where we avoid straddling a page boundary...
                    //
                    //               |--------------------- result.size ---------------------|
                    // -----+--------+------+--------+--------+------+--------+--------+------+--------+-----
                    //      | header | prev | footer | header | size | footer | header | next | footer |
                    // -----+--------+------+--------+--------+------+--------+--------+------+--------+-----
                    //               |-- remainder --|        |                        |
                    //          filler block         | page aligned block         free block
                    //               |               |
                    //          result.offset  page boundary
                    //
                    // ui64 pageOffset = result.offset & file->GetPageMask ();
                    // ui64 remainder = file->GetPageSize () - pageOffset;
                    // prev.offset = result.offset;
                    // prev.size = remainder - Block::HEADER_SIZE;
                    //
                    // or to: ...where we potentially straddle a page boundary.
                    //
                    //               |------------ result.size -----------|
                    // -----+--------+------+----+----+--------+-----------+--------+-----
                    //      | header | size | foo|ter | header | next.size | footer |
                    // -----+--------+------+----+----+--------+-----------+--------+-----
                    //               |           |             |
                    //         result.offset     |        next.offset
                    //                     page boundary
                    //
                    // next.offset = result.offset + size + Block::SIZE;
                    // next.size = result.size - size - Block::SIZE;
                    ui64 remainder = result.size - size;
                    if (remainder >= MIN_BLOCK_SIZE) {
                        // Check to see if the block would straddle a page boundary...
                        ui64 pageOffset = offset & file->GetPageMask ();
                        if (pageOffset + size + Block::HEADER_SIZE > file->GetPageSize ()) {
                            // ...it would. Now check to see if the block would fit aligned...
                            remainder = file->GetPageSize () - pageOffset;
                            if (remainder >= MIN_USER_DATA_SIZE + Block::HEADER_SIZE &&
                                    result.size >= remainder + size + Block::HEADER_SIZE) {
                                // ...it would. Add a filler block.
                                Block prev (
                                    *file,
                                    result.offset,
                                    Block::FLAGS_FREE,
                                    remainder - Block::HEADER_SIZE);
                                prev.Write ();
                                btree->Insert (BTree::Key (prev.GetSize (), prev.GetOffset ()));
                                // Adjust result to account for the filler block so that downstream
                                // calculations have the right values.
                                result.size -= remainder + Block::HEADER_SIZE;
                                result.offset += remainder + Block::HEADER_SIZE;
                            }
                        }
                        // We get our offset here, potentially adjusted by the filler block above.
                        offset = result.offset;
                        // We check to see if there's enough remaining after allocation to split the block...
                        remainder = result.size - size;
                        if (remainder >= MIN_BLOCK_SIZE) {
                            // ...there is. Add a free block.
                            Block next (
                                *file,
                                result.offset + size + Block::SIZE,
                                Block::FLAGS_FREE,
                                remainder - Block::SIZE);
                            next.Write ();
                            btree->Insert (BTree::Key (next.GetSize (), next.GetOffset ()));
                        }
                        else {
                            // ...otherwise adjust size to reflect true free block size.
                            size = result.size;
                        }
                    }
                    else {
                        // Take on the characteristics of result so that block.Write
                        // bolow does it's job.
                        offset = result.offset;
                        size = result.size;
                    }
                }
                else {
                    // If not, we need to grow the file.
                    // Do your best to not straddle a page boundary.
                    // Ranges that straddle page boundaries incur an
                    // allocation/copy/deallocation penalty.
                    // Calculate the remainder left in the last page.
                    ui64 remainder = file->GetPageSize () - (file->GetSizeEx () & file->GetPageMask ());
                    // If we don't fit in to remainder and, if the remainder
                    // can be turned in to another block and we fit into one
                    // page, all is well. Go ahead and create a spacer block
                    // and align us with a page boundary. Otherwise, because
                    // there can be no gaps between blocks, we will straddle
                    // a page boundary.
                    if (remainder < size + Block::SIZE && remainder >= MIN_BLOCK_SIZE &&
                            size <= (file->GetPageSize () - Block::SIZE)) {
                        Block prev (
                            *file,
                            file->Grow (remainder) + Block::HEADER_SIZE,
                            Block::FLAGS_FREE,
                            remainder - Block::SIZE);
                        prev.Write ();
                        btree->Insert (BTree::Key (prev.GetSize (), prev.GetOffset ()));
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
                    // Most file systems will fill the new space with '0' bytes
                    // but we can't take a chance and do it ourselves.
                    if (IsSecure ()) {
                        TransactedFile::Range range (*file, offset, size, false);
                        range.Seek (
                            SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable ()), SEEK_CUR);
                    }
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
                    // Consolidate adjacent free blocks.
                    Block prev (*file);
                    if (block.Prev (prev) && prev.IsFree ()) {
                        btree->Remove (BTree::Key (prev.GetSize (), prev.GetOffset ()));
                        if (IsSecure ()) {
                            // Assume prev body is clear.
                            clearOffset -= Block::HEADER_SIZE;
                            clearLength += Block::HEADER_SIZE;
                        }
                        else {
                            // Since block will grow to occupy prev,
                            // it's offset is no longer valid.
                            Block oldBlock (*file, block.GetOffset ());
                            oldBlock.Clear ();
                        }
                        // Back up to cover the prev.
                        block.SetOffset (block.GetOffset () - Block::SIZE - prev.GetSize ());
                        block.SetSize (block.GetSize () + Block::SIZE + prev.GetSize ());
                    }
                    Block next (*file);
                    if (block.Next (next) && next.IsFree ()) {
                        btree->Remove (BTree::Key (next.GetSize (), next.GetOffset ()));
                        if (IsSecure ()) {
                            // Assume next body is clear.
                            clearLength += Block::HEADER_SIZE;
                        }
                        else {
                            // Since block will grow to occupy next,
                            // next offset is no longer valid.
                            Block oldNext (*file, next.GetOffset ());
                            oldNext.Clear ();
                        }
                        // Expand to swallow the next.
                        block.SetSize (block.GetSize () + Block::SIZE + next.GetSize ());
                    }
                    // If we're not the last block...
                    if (!block.IsLast ()) {
                        // ...add it to the free list.
                        btree->Insert (BTree::Key (block.GetSize (), block.GetOffset ()));
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
            else if (size > 0) {
                Block block (*file, offset);
                block.Read ();
                if (size < MIN_USER_DATA_SIZE) {
                    size = MIN_USER_DATA_SIZE;
                }
                if (block.GetSize () < size) {
                    // Grow the block.
                    offset = Alloc (size);
                    if (moveData) {
                        TransactedFile::Range oldRange (
                            *file, block.GetOffset (), block.GetSize ());
                        TransactedFile::Range range (*file, offset, size, false);
                        range.Seek (
                            oldRange.Read (range.GetDataPtr (), range.GetDataAvailable ()), SEEK_CUR);
                        // Assume that Alloc always gives back clean blocks.
                        // No need to manually clear the rest of range.
                    }
                    Free (block.GetOffset ());
                }
                else {
                    // Shrink the block.
                    // If the new size leaves room for another block, split existing block.
                    ui64 remainder = block.GetSize () - size;
                    if (remainder >= MIN_BLOCK_SIZE) {
                        LockGuard<SpinLock> guard (spinLock);
                        Block next (
                            *file,
                            offset + size + Block::SIZE,
                            Block::FLAGS_FREE,
                            remainder - Block::SIZE);
                        next.Write ();
                        if (IsSecure ()) {
                            TransactedFile::Range range (*file, next.GetOffset (), next.GetSize (), false);
                            range.Seek (
                                SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable ()), SEEK_CUR);
                        }
                        btree->Insert (BTree::Key (next.GetSize (), next.GetOffset ()));
                        block.SetSize (size);
                        block.Write ();
                    }
                }
            }
            else {
                // Realloc (offset, 0, ...) results in block deletion.
                Free (offset);
                offset = 0;
            }
            return offset;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTreeAllocator::Header &header) {
            serializer >> header.btreeOffset;
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
            serializer << header.btreeOffset;
            return serializer;
        }

        void TransactedFileBTreeAllocator::Write (Serializer &serializer) const {
            LockGuard<SpinLock> guard (spinLock);
            Allocator::Write (serializer);
            serializer << MAGIC32 << header;
        }

        void TransactedFileBTreeAllocator::OnTransactedFileTransactionCommit (
                TransactedFile::SharedPtr file,
                int phase) noexcept {
            // We live inside block 1. It's set aside for us by TransactedFile::Init.
            // We don't need to allocate it.
            if (phase == TransactedFile::COMMIT_PHASE_2) {
                // Since allocator block is special (it's first and unresizable)...
                SerializableHeader allocatorHeader;
                {
                    // ...extract the original SerializableHeader...
                    TransactedFile::BlockRange range (*file, Allocator::Block::HEADER_SIZE);
                    range >> allocatorHeader;
                }
                {
                    TransactedFile::BlockRange range (*file, Allocator::Block::HEADER_SIZE, false);
                    // skip over serializable header.
                    range.Seek (allocatorHeader.Size (), SEEK_CUR);
                    // ...and force the allocator to write that particular
                    // version of itself so as not to overflow the first block.
                    Serializer::ContextGuard guard (range, allocatorHeader);
                    range << *this;
                }
            }
        }

        void TransactedFileBTreeAllocator::OnTransactedFileTransactionAbort (
                TransactedFile::SharedPtr file) noexcept {
            TransactedFile::BlockRange range (*file, Allocator::Block::HEADER_SIZE);
            range >> *this;
        }

        void TransactedFileBTreeAllocator::OnTransactedFileObjectAlloc (
                TransactedFile::Object::SharedPtr object) noexcept {
            LockGuard<SpinLock> guard (spinLock);
            header.btreeOffset = object->GetOffset ();
            SetDirty (true);
        }

        void TransactedFileBTreeAllocator::OnTransactedFileObjectFree (
                TransactedFile::Object::SharedPtr /*object*/) noexcept {
            LockGuard<SpinLock> guard (spinLock);
            header.btreeOffset = 0;
            SetDirty (true);
        }

    } // namespace util
} // namespace thekogans
