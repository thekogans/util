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
#include "thekogans/util/FileAllocator.h"

namespace thekogans {
    namespace util {

        void FileAllocator::Object::Alloc () {
            if (!IsFixedSize () || offset == 0) {
                ui64 blockSize = 0;
                if (offset != 0) {
                    Block block (*fileAllocator, offset);
                    block.Read ();
                    blockSize = block.GetSize ();
                }
                std::size_t size = Size ();
                if (blockSize < size) {
                    FileAllocator::PtrType oldOffset = offset;
                    offset = fileAllocator->Alloc (size);
                    if (oldOffset != 0) {
                        fileAllocator->Free (oldOffset);
                    }
                    Produce (
                        std::bind (
                            &ObjectEvents::OnFileAllocatorObjectAlloc,
                            std::placeholders::_1,
                            this));
                }
            }
        }

        void FileAllocator::Object::Free () {
            if (offset != 0) {
                fileAllocator->Free (offset);
                Produce (
                    std::bind (
                        &ObjectEvents::OnFileAllocatorObjectFree,
                        std::placeholders::_1,
                        this));
                offset = 0;
            }
        }

        void FileAllocator::Object::Flush () {
            assert (IsDirty ());
            assert (GetOffset () != 0);
            FileAllocator::BlockBuffer buffer (*fileAllocator, GetOffset ());
            Serializable::Header header;
            WriteHeader (buffer);
            Write (header, buffer);
            if (fileAllocator->IsSecure ()) {
                buffer.AdvanceWriteOffset (
                    SecureZeroMemory (
                        buffer.GetWritePtr (),
                        buffer.GetDataAvailableForWriting ()));
            }
            buffer.BlockWrite ();
        }

        void FileAllocator::Object::Reload () {
            Reset ();
            if (GetOffset () != 0) {
                FileAllocator::BlockBuffer buffer (*fileAllocator, GetOffset ());
                buffer.Read ();
                Serializable::Header header;
                ReadHeader (buffer);
                Read (header, buffer);
            }
        }

        void FileAllocator::Block::Header::Read (
                File &file,
                PtrType offset) {
            file.Seek (offset, SEEK_SET);
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            ui32 magic;
            file >> magic;
            if (magic == MAGIC32) {
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                file >> flags >> size;
                if (IsFree () && IsBTreeNode ()) {
                    file >> nextBTreeNodeOffset;
                }
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt FileAllocator::Block::Header @"
                    THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
        }

        void FileAllocator::Block::Header::Write (
                File &file,
                PtrType offset) const {
            file.Seek (offset, SEEK_SET);
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            file << MAGIC32;
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            file << flags << size;
            if (IsFree () && IsBTreeNode ()) {
                file << nextBTreeNodeOffset;
            }
        }

        void FileAllocator::Block::Footer::Read (
                File &file,
                PtrType offset) {
            file.Seek (offset, SEEK_SET);
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            ui32 magic;
            file >> magic;
            if (magic == MAGIC32) {
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                file >> flags >> size;
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt FileAllocator::Block::Footer @"
                    THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
        }

        void FileAllocator::Block::Footer::Write (
                File &file,
                PtrType offset) const {
            file.Seek (offset, SEEK_SET);
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            file << MAGIC32;
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            file << flags << size;
        }

        inline bool operator != (
                const FileAllocator::Block::Header &header,
                const FileAllocator::Block::Footer &footer) {
            return header.flags != footer.flags || header.size != footer.size;
        }


        bool FileAllocator::Block::Prev (Block &prev) const {
            if (!IsFirst ()) {
                prev.footer.Read (*fileAllocator.file, offset - SIZE);
                prev.offset = offset - SIZE - prev.footer.size;
                prev.header.Read (*fileAllocator.file, prev.offset - HEADER_SIZE);
                if (prev.header != prev.footer) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt FileAllocator::Block @" THEKOGANS_UTIL_UI64_FORMAT "\n"
                        " prev.header.flags: %u prev.header.size: " THEKOGANS_UTIL_UI64_FORMAT "\n"
                        " prev.footer.flags: %u prev.footer.size: " THEKOGANS_UTIL_UI64_FORMAT,
                        prev.offset,
                        prev.header.flags, prev.header.size,
                        prev.footer.flags, prev.footer.size);
                }
                return true;
            }
            return false;
        }

        bool FileAllocator:: FileAllocator::Block::Next (Block &next) const {
            if (!IsLast ()) {
                next.offset = offset + header.size + SIZE;
                next.Read ();
                return true;
            }
            return false;
        }

        void FileAllocator::Block::Read () {
            header.Read (*fileAllocator.file, offset - HEADER_SIZE);
            footer.Read (*fileAllocator.file, offset + header.size);
            if (header != footer) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt FileAllocator::Block @" THEKOGANS_UTIL_UI64_FORMAT "\n"
                    " header.flags: %u header.size: " THEKOGANS_UTIL_UI64_FORMAT "\n"
                    " footer.flags: %u footer.size: " THEKOGANS_UTIL_UI64_FORMAT,
                    offset,
                    header.flags, header.size,
                    footer.flags, footer.size);
            }
        }

        void FileAllocator::Block::Write () const {
            header.Write (*fileAllocator.file, offset - HEADER_SIZE);
            footer.Write (*fileAllocator.file, offset + header.size);
        }

    #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
        void FileAllocator::Block::Invalidate () const {
            fileAllocator.file->Seek (offset - HEADER_SIZE, SEEK_SET);
            // Simply stepping on magic will invalidate
            // this block for all future reads.
            *fileAllocator.file << (ui32)0;
        }
    #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)

        FileAllocator::BlockBuffer::BlockBuffer (
                FileAllocator &fileAllocator,
                PtrType offset,
                std::size_t bufferLength,
                Allocator::SharedPtr allocator) :
                Buffer (fileAllocator.file->endianness),
                block (fileAllocator, offset) {
            block.Read ();
            if (bufferLength == 0) {
                bufferLength = block.GetSize ();
            }
            Resize (bufferLength, allocator);
        }

        std::size_t FileAllocator::BlockBuffer::BlockIO (
                std::size_t blockOffset,
                std::size_t blockLength,
                bool read) {
            std::size_t count = 0;
            if (blockOffset < block.GetSize ()) {
                std::size_t availableLength = read ?
                    GetDataAvailableForWriting () :
                    GetDataAvailableForReading ();
                if (blockLength == 0 || blockLength > availableLength) {
                    blockLength = availableLength;
                }
                if (blockLength > 0) {
                    ui64 available = block.GetSize () - blockOffset;
                    if (available > blockLength) {
                        available = blockLength;
                    }
                    block.fileAllocator.file->Seek (block.GetOffset () + blockOffset, SEEK_SET);
                    count = read ?
                        AdvanceWriteOffset (
                            block.fileAllocator.file->Read (GetWritePtr (), available)) :
                        AdvanceReadOffset (
                            block.fileAllocator.file->Write (GetReadPtr (), available));
                }
            }
            return count;
        }

        FileAllocator::FileAllocator (
                BufferedFile::SharedPtr file,
                bool secure,
                std::size_t btreeEntriesPerNode,
                std::size_t btreeNodesPerPage,
                Allocator::SharedPtr allocator) :
                BufferedFile::TransactionParticipant (file),
                header (secure ? Header::FLAGS_SECURE : 0),
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

        FileAllocator::PtrType FileAllocator::Alloc (std::size_t size) {
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
                    Block block (*this, header.freeBTreeNodeOffset);
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
                            *this,
                            result.second + size + Block::SIZE,
                            Block::FLAGS_FREE,
                            result.first - size - Block::SIZE);
                        next.Write ();
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
                Block block (*this, offset, 0, size);
                block.Write ();
            }
            return offset;
        }

        void FileAllocator::Free (PtrType offset) {
            // To honor the Allocator policy, we ignore NULL pointers.
            if (offset != 0) {
                Block block (*this, offset);
                block.Read ();
                if (!block.IsFree ()) {
                    PtrType clearOffset = block.GetOffset ();
                    ui64 clearLength = block.GetSize ();
                    // Consolidate adjacent free non BTree::Node blocks.
                    Block prev (*this);
                    if (block.Prev (prev) && prev.IsFree () && !prev.IsBTreeNode ()) {
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
                            block.Invalidate ();
                        }
                    #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        // Back up to cover the prev.
                        block.SetOffset (
                            block.GetOffset () - Block::SIZE - prev.GetSize ());
                        block.SetSize (
                            block.GetSize () + Block::SIZE + prev.GetSize ());
                    }
                    Block next (*this);
                    if (block.Next (next) && next.IsFree () && !next.IsBTreeNode ()) {
                        btree->Remove (BTree::KeyType (next.GetSize (), next.GetOffset ()));
                        if (IsSecure ()) {
                            // Assume next body is clear.
                            clearLength += Block::FOOTER_SIZE;
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
                        block.Write ();
                        if (IsSecure ()) {
                            BlockBuffer buffer (*this, block.GetOffset (), clearLength);
                            buffer.AdvanceWriteOffset (
                                SecureZeroMemory (
                                    buffer.GetWritePtr (),
                                    buffer.GetDataAvailableForWriting ()));
                            buffer.BlockWrite (clearOffset - block.GetOffset (), clearLength);
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

        void FileAllocator::Flush () {
            file->Seek (0, SEEK_SET);
            *file << MAGIC32 << header;
        }

        void FileAllocator::Reload () {
            if (file->GetSize () > 0) {
                Load ();
            }
            else {
                Reset ();
            }
        }

        void FileAllocator::Reset () {
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

        void FileAllocator::Load () {
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
                    "Corrupt FileAllocator file (%s)",
                    file->GetPath ().c_str ());
            }
            *file >> header;
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            if (!header.IsBlockUsesMagic ()) {
        #else // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            if (header.IsBlockUsesMagic ()) {
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "This FileAllocator file (%s) cannot be opened by this version of %s.",
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

        FileAllocator::PtrType FileAllocator::AllocBTreeNode (std::size_t size) {
            PtrType offset = 0;
            if (header.freeBTreeNodeOffset != 0) {
                offset = header.freeBTreeNodeOffset;
                Block block (*this, offset);
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
                offset = file->GetSize () + Block::HEADER_SIZE;
            }
            Block block (*this, offset, Block::FLAGS_BTREE_NODE, size);
            block.Write ();
            return offset;
        }

        void FileAllocator::FreeBTreeNode (PtrType offset) {
            if (offset >= header.heapStart + Block::HEADER_SIZE) {
                Block block (*this, offset);
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
                const FileAllocator::Header &header) {
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
                FileAllocator::Header &header) {
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
