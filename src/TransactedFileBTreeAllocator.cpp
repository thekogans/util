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

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (
            thekogans::util::TransactedFileBTreeAllocator,
            TransactedFile::Allocator::TYPE)

        inline Serializer &operator << (
                Serializer &serializer,
                const TransactedFileBTreeAllocator::Header &header) {
            serializer << header.btreeOffset << header.freeBTreeNodeOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTreeAllocator::Header &header) {
            serializer >> header.btreeOffset >> header.freeBTreeNodeOffset;
            return serializer;
        }

        void TransactedFileBTreeAllocator::Block::Read (TransactedFile &file) {
            Allocator::Block::Read (file);
            if (IsFree () && IsBTreeNode ()) {
                file.Seek (offset, SEEK_SET);
                file >> nextBTreeNodeOffset;
            }
        }

        void TransactedFileBTreeAllocator::Block::Write (TransactedFile &file) const {
            Allocator::Block::Write (file);
            if (IsFree () && IsBTreeNode ()) {
                file.Seek (offset, SEEK_SET);
                file << nextBTreeNodeOffset;
            }
        }

        void TransactedFileBTreeAllocator::Init (TransactedFile::SharedPtr file_) (
            file = file_;
            if (file->GetSize () > 0) {
                Read ();
            }
            else {
                // Write an empty header to make sure that when Alloc
                // calls file->GetSize it will get a correct value.
                Write ();
            }
        }

        TransactedFile::Allocator::PtrType TransactedFileBTreeAllocator::Alloc (std::size_t size) {
            PtrType offset = 0;
            if (size > 0) {
                if (size < MIN_USER_DATA_SIZE) {
                    size = MIN_USER_DATA_SIZE;
                }
                BTree::KeyType result;
                // If allocation request is <= BTree::Node::FileSize and the BTree::Node
                // cache is not empty, reuse the first free block. This optimization allows
                // us to reclaim the node blocks to be used for general purpose allocations.
                if (size <= btreeNodeFileSize && header.freeBTreeNodeOffset != 0) {
                    result.first = btreeNodeFileSize;
                    result.second = header.freeBTreeNodeOffset;
                    Block block (header.freeBTreeNodeOffset);
                    block.Read (*file);
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
                    offset = result.second;
                    // If the block we got is bigger than we need, split it.
                    //
                    // we need to go from:
                    //
                    //               |----------- result.first -----------|
                    // -----+--------+------------------------------------+--------+-----
                    //      | header |                                    | footer |
                    // -----+--------+------------------------------------+--------+-----
                    //               |
                    //         result.second
                    //
                    // to:
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
                    if (result.first - size >= MIN_BLOCK_SIZE) {
                        Block next (
                            result.second + size + Block::SIZE,
                            Block::FLAGS_FREE,
                            result.first - size - Block::SIZE);
                        next.Write (*file);
                        btree->Insert (BTree::KeyType (next.GetSize (), next.GetOffset ()));
                    }
                    else {
                        // If not, update the requested size so that block.Write
                        // below can write the proper header/footer.
                        size = result.first;
                    }
                }
                else {
                    // No free block large enough is found? Grow the file.
                    offset = file->GetSize () + Block::HEADER_SIZE;
                }
                Block block (offset, 0, size);
                block.Write (*file);
            }
            return offset;
        }

        void TransactedFileBTreeAllocator::Free (PtrType offset) {
            // To honor the Allocator policy, we ignore NULL pointers.
            if (offset != 0) {
                Block block (offset);
                block.Read (*file);
                if (!block.IsFree ()) {
                    PtrType clearOffset = block.GetOffset ();
                    ui64 clearLength = block.GetSize ();
                    // Consolidate adjacent free non BTree::Node blocks.
                    Block prev;
                    if (block.Prev (*this, prev) && prev.IsFree () && !prev.IsBTreeNode ()) {
                        btree->Remove (
                            BTree::KeyType (prev.GetSize (), prev.GetOffset ()));
                        if (IsSecure ()) {
                            // Assume prev body is clear.
                            clearOffset -= Block::HEADER_SIZE;
                            clearLength += Block::HEADER_SIZE;
                        }
                    #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        else {
                            // Since block will grow to occupy prev,
                            // it's offset is no longer valid.
                            block.Invalidate (*file);
                        }
                    #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        // Back up to cover the prev.
                        block.SetOffset (
                            block.GetOffset () - Block::SIZE - prev.GetSize ());
                        block.SetSize (
                            block.GetSize () + Block::SIZE + prev.GetSize ());
                    }
                    Block next;
                    if (block.Next (*this, next) && next.IsFree () && !next.IsBTreeNode ()) {
                        btree->Remove (BTree::KeyType (next.GetSize (), next.GetOffset ()));
                        if (IsSecure ()) {
                            // Assume next body is clear.
                            clearLength += Block::HEADER_SIZE;
                        }
                    #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        else {
                            // Since block will grow to occupy next,
                            // next offset is no longer valid.
                            next.Invalidate (*file);
                        }
                    #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        // Expand to swallow the next.
                        block.SetSize (
                            block.GetSize () + Block::SIZE + next.GetSize ());
                    }
                    // If we're not the last block...
                    if (!block.IsLast (*this)) {
                        // ... add it to the free list.
                        btree->Insert (BTree::KeyType (block.GetSize (), block.GetOffset ()));
                        block.SetFree (true);
                        block.Write (*file);
                        if (IsSecure ()) {
                            TransactedFile::WriteOnlyRange buffer (*file, clearOffset, clearLength);
                            buffer.AdvanceOffset (
                                SecureZeroMemory (buffer.GetDataPtr (), buffer.GetDataAvailable ()));
                        }
                    }
                    else {
                        // If we are, truncate the heap.
                        file->SetSize (block.GetOffset () - Block::HEADER_SIZE);
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
                Block block (offset);
                block.Read (*file);
                if (block.GetSize () < size) {
                    offset = Alloc (size);
                    if (moveData) {
                        TransactedFile::ReadOnlyRange oldBuffer (
                            *file, block.GetOffset (), block.GetSize ());
                        TransactedFile::WriteOnlyRange buffer (*file, offset, size);
                        buffer.AdvanceOffset (
                            oldBuffer.Read (
                                buffer.GetDataPtr (),
                                buffer.GetDataAvailable ()));
                        if (IsSecure ()) {
                            buffer.AdvanceOffset (
                                SecureZeroMemory (
                                    buffer.GetDataPtr (),
                                    buffer.GetDataAvailable ()));
                        }
                    }
                    Free (block.GetOffset ());
                }
                // If the new size leaves room for another block, split existing block.
                else if (block.GetSize () - size >= MIN_BLOCK_SIZE) {
                    Block next (
                        offset + size + Block::SIZE,
                        Block::FLAGS_FREE,
                        block.GetSize () - size - Block::SIZE);
                    next.Write (*file);
                    btree->Insert (BTree::KeyType (next.GetSize (), next.GetOffset ()));
                    block.SetSize (size);
                    block.Write (*file);
                }
            }
            return offset;
        }

        void TransactedFileBTreeAllocator::Read () {
            Allocator::Read ();
            TransactedFile::ReadOnlyRange buffer (
                *file, Allocator::Header::SIZE, Header::SIZE);
            buffer >> header;
            btree.Reset (
                new BTree (
                    *this,
                    btree->header.entriesPerNode,
                    btree->nodeAllocator->GetBlocksPerPage (),
                    btree->nodeAllocator->GetAllocator ()));
            btreeNodeFileSize = BTree::Node::FileSize (btree->header.entriesPerNode);
        }

        void TransactedFileBTreeAllocator::Write () {
            Allocator::Write ();
            TransactedFile::WriteOnlyRange buffer (
                *file, Allocator::Header::SIZE, Header::SIZE);
            buffer << header;
        }

        TransactedFile::Allocator::PtrType TransactedFileBTreeAllocator::AllocBTreeNode (std::size_t size) {
            PtrType offset = 0;
            if (header.freeBTreeNodeOffset != 0) {
                offset = header.freeBTreeNodeOffset;
                Block block (offset);
                block.Read (*file);
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
                offset = file->GetSize () + Block::HEADER_SIZE;
            }
            Block block (offset, Block::FLAGS_BTREE_NODE, size);
            block.Write (*file);
            return offset;
        }

        void TransactedFileBTreeAllocator::FreeBTreeNode (PtrType offset) {
            if (offset >= Allocator::header.heapStart + Block::HEADER_SIZE) {
                Block block (offset);
                block.Read (*file);
                if (!block.IsFree () && block.IsBTreeNode ()) {
                    if (!block.IsLast (*this)) {
                        block.SetFree (true);
                        block.SetNextBTreeNodeOffset (header.freeBTreeNodeOffset);
                        block.Write (*file);
                        header.freeBTreeNodeOffset = offset;
                        SetDirty (true);
                    }
                    else {
                        file->SetSize (block.GetOffset () - Block::HEADER_SIZE);
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
