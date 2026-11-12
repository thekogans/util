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
#include "thekogans/util/TransactedFile.h"
#include "thekogans/util/TransactedFileBTreeAllocator.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT (thekogans::util::TransactedFileBTreeAllocator, 1, 0)

        TransactedFile::AddressType TransactedFileBTreeAllocator::Alloc (TransactedFile::SizeType size) {
            if (THEKOGANS_UTIL_UNLIKELY (!file->CheckCanMutate ())) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Structural Logic Error: Blocks can only be allocated during transaction "
                    "in progress and allocation phases (%s).",
                    TransactedFile::Transaction::phaseName[file->GetTransactionPhase ()]);
            }
            TransactedFile::AddressType offset = 0;
            if (size > 0) {
                if (size < TransactedFile::MIN_USER_DATA_SIZE) {
                    size = TransactedFile::MIN_USER_DATA_SIZE;
                }
                LockGuard<SpinLock> guard (spinLock);
                TransactedFileBTreeAllocatorBTree::Key result = Find ({size, 0});
                // If we got a block see if it's too big.
                if (result.offset != 0) {
                    // Got it!
                    assert (result.size >= size);
                    Remove (result);
                    // If the block we got is bigger than we need, split it.
                    //
                    // we need to go from:
                    //
                    //               |---------------------- result.size ---------------------|
                    // -----+--------+---------------+----------------------------------------+--------+-----
                    //      | header |               |                                        | footer |
                    // -----+--------+---------------+----------------------------------------+--------+-----
                    //               |               |
                    //         result.offset         |
                    //                         page boundary
                    //
                    // to: Where we avoid straddling a page boundary...
                    //
                    //               |---------------------- result.size ---------------------|
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
                    //               |------------ result.size ------------|
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
                    if (remainder >= TransactedFile::MIN_BLOCK_SIZE) {
                        // Check to see if the block would straddle a page boundary...
                        ui64 pageOffset = result.offset & file->GetPageMask ();
                        if (pageOffset + size + TransactedFile::Block::HEADER_SIZE > file->GetPageSize ()) {
                            // ...it would. Now check to see if the block would fit aligned...
                            remainder = file->GetPageSize () - pageOffset;
                            if (remainder >= TransactedFile::MIN_USER_DATA_SIZE + TransactedFile::Block::HEADER_SIZE &&
                                    result.size >= remainder + size + TransactedFile::Block::HEADER_SIZE) {
                                // ...it would. Add a filler block.
                                TransactedFile::Block prev (
                                    file,
                                    result.offset,
                                    TransactedFile::Block::FLAGS_FREE,
                                    remainder - TransactedFile::Block::HEADER_SIZE);
                                prev.Write ();
                                Insert ({prev.GetSize (), prev.GetOffset ()});
                                // Adjust result to account for the filler block so that downstream
                                // calculations have the right values.
                                result.size -= remainder + TransactedFile::Block::HEADER_SIZE;
                                result.offset += remainder + TransactedFile::Block::HEADER_SIZE;
                            }
                        }
                        // We get our offset here, potentially adjusted by the filler block above.
                        offset = result.offset;
                        // We check to see if there's enough remaining after allocation to split the block...
                        remainder = result.size - size;
                        if (remainder >= TransactedFile::MIN_BLOCK_SIZE) {
                            // ...there is. Add a free block.
                            TransactedFile::Block next (
                                file,
                                result.offset + size + TransactedFile::Block::SIZE,
                                TransactedFile::Block::FLAGS_FREE,
                                remainder - TransactedFile::Block::SIZE);
                            next.Write ();
                            Insert ({next.GetSize (), next.GetOffset ()});
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
                    if (remainder < size + TransactedFile::Block::SIZE && remainder >= TransactedFile::MIN_BLOCK_SIZE &&
                            size <= (file->GetPageSize () - TransactedFile::Block::SIZE)) {
                        TransactedFile::Block prev (
                            file,
                            file->Grow (remainder) + TransactedFile::Block::HEADER_SIZE,
                            TransactedFile::Block::FLAGS_FREE,
                            remainder - TransactedFile::Block::SIZE);
                        prev.Write ();
                        Insert ({prev.GetSize (), prev.GetOffset ()});
                    }
                    // Otherwise, we fit in the ramainder. Check if what will remain
                    // after our allocation would be too small for a block and cause
                    // the next allocation to straddle the page boundry...
                    else if (remainder - size - TransactedFile::Block::SIZE < TransactedFile::MIN_BLOCK_SIZE) {
                        // ...it is. Round up the size request to align the next
                        // allocation to a page boundary.
                        size = remainder - TransactedFile::Block::SIZE;
                    }
                    // No free block large enough is found? Grow the file.
                    offset = file->Grow (TransactedFile::Block::SIZE + size) + TransactedFile::Block::HEADER_SIZE;
                    // Most file systems will fill the new space with '0' bytes
                    // but we can't take a chance and do it ourselves.
                    if (file->IsSecure ()) {
                        TransactedFile::Range range (file, offset, size, false);
                        range.Seek (
                            SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable ()), SEEK_CUR);
                    }
                }
                // By now we got our block by either reusing a free one or growing the file.
                TransactedFile::Block block (file, offset, 0, size);
                block.Write ();
            }
            return offset;
        }

        void TransactedFileBTreeAllocator::Free (TransactedFile::AddressType offset) {
            if (THEKOGANS_UTIL_UNLIKELY (!file->CheckCanMutate ())) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Structural Logic Error: Blocks can only be freed during transaction "
                    "in progress and allocation phases (%s).",
                    TransactedFile::Transaction::phaseName[file->GetTransactionPhase ()]);
            }
            // To honor the Allocator policy, we ignore NULL pointers.
            if (offset != 0) {
                if (THEKOGANS_UTIL_UNLIKELY (offset == TransactedFile::Block::BLOCK_0_OFFSET)) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Catastrophic Heap Invariance Violation: Attempted to deallocate "
                        "the master metadata bootstrap anchor page at offset " THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
                LockGuard<SpinLock> guard (spinLock);
                TransactedFile::Block block (file, offset);
                block.Read ();
                if (!block.IsFree ()) {
                    TransactedFile::AddressType clearOffset = block.GetOffset ();
                    TransactedFile::SizeType clearLength = block.GetSize ();
                    // Consolidate adjacent free blocks.
                    TransactedFile::Block prev (file);
                    if (block.Prev (prev) && prev.IsFree ()) {
                        Remove ({prev.GetSize (), prev.GetOffset ()});
                        if (file->IsSecure ()) {
                            // Assume prev body is clear.
                            clearOffset -= TransactedFile::Block::HEADER_SIZE;
                            clearLength += TransactedFile::Block::HEADER_SIZE;
                        }
                        else {
                            // Since block will grow to occupy prev,
                            // it's offset is no longer valid.
                            TransactedFile::Block oldBlock (file, block.GetOffset ());
                            oldBlock.Clear ();
                        }
                        // Back up to cover the prev.
                        block.SetOffset (block.GetOffset () - TransactedFile::Block::SIZE - prev.GetSize ());
                        block.SetSize (block.GetSize () + TransactedFile::Block::SIZE + prev.GetSize ());
                    }
                    TransactedFile::Block next (file);
                    if (block.Next (next) && next.IsFree ()) {
                        Remove ({next.GetSize (), next.GetOffset ()});
                        if (file->IsSecure ()) {
                            // Assume next body is clear.
                            clearLength += TransactedFile::Block::HEADER_SIZE;
                        }
                        else {
                            // Since block will grow to occupy next,
                            // next offset is no longer valid.
                            TransactedFile::Block oldNext (file, next.GetOffset ());
                            oldNext.Clear ();
                        }
                        // Expand to swallow the next.
                        block.SetSize (block.GetSize () + TransactedFile::Block::SIZE + next.GetSize ());
                    }
                    // If we're not the last block...
                    if (THEKOGANS_UTIL_LIKELY (!block.IsLast ())) {
                        // ...add it to the free list.
                        Insert ({block.GetSize (), block.GetOffset ()});
                        block.SetFree (true);
                        block.Write ();
                        if (file->IsSecure ()) {
                            TransactedFile::Range range (file, clearOffset, clearLength, false);
                            range.Seek (
                                SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable ()), SEEK_CUR);
                        }
                    }
                    else {
                        // If we are, truncate the heap.
                        file->Shrink (TransactedFile::Block::SIZE + block.GetSize ());
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

        TransactedFile::AddressType TransactedFileBTreeAllocator::Realloc (
                TransactedFile::AddressType offset,
                TransactedFile::SizeType size,
                bool moveData) {
            if (THEKOGANS_UTIL_UNLIKELY (!file->CheckCanMutate ())) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Structural Logic Error: Blocks can only be reallocated during transaction "
                    "in progress and allocation phases (%s).",
                    TransactedFile::Transaction::phaseName[file->GetTransactionPhase ()]);
            }
            if (offset == 0) {
                offset = Alloc (size);
            }
            else if (size > 0) {
                if (THEKOGANS_UTIL_UNLIKELY (offset == TransactedFile::Block::BLOCK_0_OFFSET)) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Catastrophic Heap Invariance Violation: Attempted to reallocate "
                        "the master metadata bootstrap anchor page at offset " THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
                TransactedFile::Block block (file, offset);
                block.Read ();
                if (size < TransactedFile::MIN_USER_DATA_SIZE) {
                    size = TransactedFile::MIN_USER_DATA_SIZE;
                }
                if (block.GetSize () < size) {
                    // Grow the block.
                    offset = Alloc (size);
                    if (moveData) {
                        TransactedFile::Range oldRange (
                            file, block.GetOffset (), block.GetSize ());
                        TransactedFile::Range range (file, offset, size, false);
                        range.Seek (oldRange.Read (range.GetDataPtr (), MIN (block.GetSize (), size)), SEEK_CUR);
                        // Assume that Alloc always gives back clean blocks.
                        // No need to manually clear the rest of range.
                    }
                    Free (block.GetOffset ());
                }
                else if (block.GetSize () > size) {
                    // Shrink the block.
                    // If the new size leaves room for another block, split existing block.
                    ui64 remainder = block.GetSize () - size;
                    if (remainder >= TransactedFile::MIN_BLOCK_SIZE) {
                        LockGuard<SpinLock> guard (spinLock);
                        // If this block happens to be the last physical block on the heap,
                        // do not split it into a ghost free-list chunk—truncate the file tail cleanly instead!
                        if (THEKOGANS_UTIL_UNLIKELY (block.IsLast ())) {
                            block.SetSize (size);
                            block.Write ();
                            file->Shrink (remainder);
                        }
                        else {
                            TransactedFile::Block next (
                                file,
                                offset + size + TransactedFile::Block::SIZE,
                                TransactedFile::Block::FLAGS_FREE,
                                remainder - TransactedFile::Block::SIZE);
                            next.Write ();
                            if (file->IsSecure ()) {
                                TransactedFile::Range range (file, next.GetOffset (), next.GetSize (), false);
                                range.Seek (
                                    SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable ()), SEEK_CUR);
                            }
                            Insert ({next.GetSize (), next.GetOffset ()});
                            block.SetSize (size);
                            block.Write ();
                        }
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

    } // namespace util
} // namespace thekogans
