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
#include "thekogans/util/BTreeTransactedFileAllocator.h"

namespace thekogans {
    namespace util {

        void BTreeTransactedFileAllocator::Block::Read (TransactedFile &file) {
            Allocator::Block::Read (file);
            if (header.IsFree () && header.IsBTreeNode ()) {
                file.Seek (offset, SEEK_SET);
                file >> nextBTreeNodeOffset;
            }
        }

        void BTreeTransactedFileAllocator::Block::Write (TransactedFile &file) const {
            Allocator::Block::Write (file);
            if (header.IsFree () && header.IsBTreeNode ()) {
                file.Seek (offset, SEEK_SET);
                file << nextBTreeNodeOffset;
            }
        }

        BTreeTransactedFileAllocator::BTreeTransactedFileAllocator (
                TransactedFile::SharedPtr file,
                bool secure,
                std::size_t btreeEntriesPerNode,
                std::size_t btreeNodesPerPage,
                Allocator::SharedPtr allocator) :
                TransactedFile::Allocator (file, secure),
                btree (new BTree (*this, btreeEntriesPerNode, btreeNodesPerPage, allocator)),
                btreeNodeFileSize (BTree::Node::FileSize (btree->header.entriesPerNode)) {
            if (file->GetSize () > 0) {
                Load ();
            }
            else {
                // Write an empty header to make sure that when Alloc
                // calls file->GetSize it will get a correct value.
                Reset ();
            }
        }

        BTreeTransactedFileAllocator::PtrType BTreeTransactedFileAllocator::Alloc (std::size_t size) {
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

        void BTreeTransactedFileAllocator::Free (PtrType offset) {
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
                    #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        else {
                            // Since block will grow to occupy prev,
                            // it's offset is no longer valid.
                            block.Invalidate (*file);
                        }
                    #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
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
                    #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        else {
                            // Since block will grow to occupy next,
                            // next offset is no longer valid.
                            next.Invalidate ();
                        }
                    #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        // Expand to swallow the next.
                        block.SetSize (
                            block.GetSize () + Block::SIZE + next.GetSize ());
                    }
                    // If we're not the last block...
                    if (!block.IsLast ()) {
                        // ... add it to the free list.
                        btree->Insert (BTree::KeyType (block.GetSize (), block.GetOffset ()));
                        block.SetFree (true);
                        block.Write (*file);
                        if (IsSecure ()) {
                            Block::Buffer buffer (*file, block.GetOffset (), clearLength);
                            buffer.AdvanceWriteOffset (
                                SecureZeroMemory (
                                    buffer.GetWritePtr (),
                                    buffer.GetDataAvailableForWriting ()));
                            buffer.BlockWrite (*file, clearOffset - block.GetOffset (), clearLength);
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

        BTreeTransactedFileAllocator::PtrType BTreeTransactedFileAllocator::Realloc (
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
                    BTreeTransactedFileAllocator::PtrType oldOffset = offset;
                    offset = Alloc (size);
                    if (moveData) {
                        Block::Buffer oldBuffer (*file, oldOffset);
                        oldBuffer.BlockRead (*file);
                        Block::Buffer buffer (*file, offset);
                        buffer.AdvanceWriteOffset (
                            oldBuffer.Read (
                                *file,
                                buffer.GetWritePtr (),
                                buffer.GetDataAvailableForWriting ()));
                        if (IsSecure ()) {
                            buffer.AdvanceWriteOffset (
                                SecureZeroMemory (
                                    buffer.GetWritePtr (),
                                    buffer.GetDataAvailableForWriting ()));
                        }
                        buffer.BlockWrite (*file);
                    }
                    Free (oldOffset);
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

        void BTreeTransactedFileAllocator::Flush () {
            file->Seek (0, SEEK_SET);
            *file << MAGIC32 << header;
        }

        void BTreeTransactedFileAllocator::Reload () {
            if (file->GetSize () > 0) {
                Load ();
            }
            else {
                Reset ();
            }
        }

        void BTreeTransactedFileAllocator::Reset () {
            header.btreeOffset = 0;
            header.freeBTreeNodeOffset = 0;
            header.rootOffset = 0;
            file->Seek (0, SEEK_SET);
            *file << MAGIC32 << header;
            // Shrink the file.
            file->SetSize (file->Tell ());
            btree.Reset (
                new BTree (
                    *this,
                    btree->header.entriesPerNode,
                    btree->nodeAllocator->GetBlocksPerPage (),
                    btree->nodeAllocator->GetAllocator ()));
            btreeNodeFileSize = BTree::Node::FileSize (btree->header.entriesPerNode);
        }

        void BTreeTransactedFileAllocator::Load () {
            file->Seek (0, SEEK_SET);
            ui32 magic;
            *file >> magic;
            if (magic == MAGIC32) {
                // File is host endian.
            }
            else if (ByteSwap<GuestEndian, HostEndian> (magic) == MAGIC32) {
                // File is guest endian.
                file->endianness = GuestEndian;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt BTreeTransactedFileAllocator file (%s)",
                    file->GetPath ().c_str ());
            }
            *file >> header;
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            if (!header.IsBlockUsesMagic ()) {
        #else // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            if (header.IsBlockUsesMagic ()) {
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "This BTreeTransactedFileAllocator file (%s) cannot be opened by this version of %s.",
                    file->GetPath ().c_str (),
                    THEKOGANS_UTIL);
            }
            btree.Reset (
                new BTree (
                    *this,
                    btree->header.entriesPerNode,
                    btree->nodeAllocator->GetBlocksPerPage (),
                    btree->nodeAllocator->GetAllocator ()));
            btreeNodeFileSize = BTree::Node::FileSize (btree->header.entriesPerNode);
        }

        BTreeTransactedFileAllocator::PtrType BTreeTransactedFileAllocator::AllocBTreeNode (std::size_t size) {
            PtrType offset = 0;
            if (header.freeBTreeNodeOffset != 0) {
                offset = header.freeBTreeNodeOffset;
                Block block ( offset);
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

        void BTreeTransactedFileAllocator::FreeBTreeNode (PtrType offset) {
            if (offset >= header.heapStart + Block::HEADER_SIZE) {
                Block block (offset);
                block.Read (*file);
                if (!block.IsFree () && block.IsBTreeNode ()) {
                    if (!block.IsLast ()) {
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

        inline Serializer &operator << (
                Serializer &serializer,
                const BTreeTransactedFileAllocator::Header &header) {
            serializer <<
                header.version <<
                header.flags <<
                header.heapStart <<
                header.btreeOffset <<
                header.freeBTreeNodeOffset <<
                header.rootOffset;
            // The above are available with every version.
            // Add if statements to test for new versions,
            // and write their fields accordingly.
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                BTreeTransactedFileAllocator::Header &header) {
            serializer >>
                header.version >>
                header.flags >>
                header.heapStart >>
                header.btreeOffset >>
                header.freeBTreeNodeOffset >>
                header.rootOffset;
            // The above are available with every version.
            // Add if statements to test for new versions,
            // and read their fields accordingly.
            return serializer;
        }

    } // namespace util
} // namespace thekogans
