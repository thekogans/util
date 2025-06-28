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
#include "thekogans/util/Heap.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/File.h"
#include "thekogans/util/FileAllocator.h"

namespace thekogans {
    namespace util {

        void FileAllocator::Object::Allocate () {
            if (!IsFixedSize () || offset == 0) {
                ui64 blockSize = 0;
                if (offset != 0) {
                    BlockInfo block (*file, offset);
                    block.Read ();
                    blockSize = block.GetSize ();
                }
                std::size_t size = Size ();
                if (blockSize < size) {
                    if (offset != 0) {
                        fileAllocator->Free (offset);
                    }
                    offset = fileAllocator->Alloc (size);
                    SetDirty (true);
                    Produce (
                        std::bind (
                            &ObjectEvents::OnFileAllocatorObjectOffsetChanged,
                            std::placeholders::_1,
                            this));
                }
            }
        }

        void FileAllocator::BlockInfo::Header::Read (
                File &file,
                PtrType offset) {
            file.Seek (offset, SEEK_SET);
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
            ui32 magic;
            file >> magic;
            if (magic == MAGIC32) {
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                file >> flags >> size;
                if (IsFree () && IsBTreeNode ()) {
                    file >> nextBTreeNodeOffset;
                }
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt FileAllocator::BlockInfo::Header @"
                    THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
        }

        void FileAllocator::BlockInfo::Header::Write (
                File &file,
                PtrType offset) const {
            file.Seek (offset, SEEK_SET);
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
            file << MAGIC32;
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
            file << flags << size;
            if (IsFree () && IsBTreeNode ()) {
                file << nextBTreeNodeOffset;
            }
        }

        void FileAllocator::BlockInfo::Footer::Read (
                File &file,
                PtrType offset) {
            file.Seek (offset, SEEK_SET);
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
            ui32 magic;
            file >> magic;
            if (magic == MAGIC32) {
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                file >> flags >> size;
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt FileAllocator::BlockInfo::Footer @"
                    THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
        }

        void FileAllocator::BlockInfo::Footer::Write (
                File &file,
                PtrType offset) const {
            file.Seek (offset, SEEK_SET);
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
            file << MAGIC32;
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
            file << flags << size;
        }

        inline bool operator != (
                const FileAllocator::BlockInfo::Header &header,
                const FileAllocator::BlockInfo::Footer &footer) {
            return header.flags != footer.flags || header.size != footer.size;
        }

        void FileAllocator::BlockInfo::Prev (BlockInfo &prev) const {
            prev.footer.Read (file, offset - SIZE);
            prev.offset = offset - SIZE - prev.footer.size;
            prev.header.Read (file, prev.offset - HEADER_SIZE);
            if (prev.header != prev.footer) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt FileAllocator::BlockInfo @" THEKOGANS_UTIL_UI64_FORMAT "\n"
                    " prev.header.flags: %u prev.header.size: " THEKOGANS_UTIL_UI64_FORMAT "\n"
                    " prev.footer.flags: %u prev.footer.size: " THEKOGANS_UTIL_UI64_FORMAT,
                    prev.offset,
                    prev.header.flags, prev.header.size,
                    prev.footer.flags, prev.footer.size);
            }
        }

        void FileAllocator:: FileAllocator::BlockInfo::Next (BlockInfo &next) const {
            next.offset = offset + header.size + SIZE;
            next.Read ();
        }

        void FileAllocator::BlockInfo::Read () {
            header.Read (file, offset - HEADER_SIZE);
            footer.Read (file, offset + header.size);
            if (header != footer) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt FileAllocator::BlockInfo @" THEKOGANS_UTIL_UI64_FORMAT "\n"
                    " header.flags: %u header.size: " THEKOGANS_UTIL_UI64_FORMAT "\n"
                    " footer.flags: %u footer.size: " THEKOGANS_UTIL_UI64_FORMAT,
                    offset,
                    header.flags, header.size,
                    footer.flags, footer.size);
            }
        }

        void FileAllocator::BlockInfo::Write () const {
            header.Write (file, offset - HEADER_SIZE);
            footer.Write (file, offset + header.size);
        }

    #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
        void FileAllocator::BlockInfo::Invalidate () const {
            file.Seek (offset - HEADER_SIZE, SEEK_SET);
            // Simply stepping on magic will invalidate
            // this block for all future reads.
            file << (ui32)0;
        }
    #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)

        FileAllocator::BlockBuffer::BlockBuffer (
                File &file,
                PtrType offset,
                std::size_t bufferLength,
                Allocator::SharedPtr allocator) :
                Buffer (file.endianness),
                block (file, offset) {
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
                    block.file.Seek (block.GetOffset () + blockOffset, SEEK_SET);
                    count = read ?
                        AdvanceWriteOffset (block.file.Read (GetWritePtr (), available)) :
                        AdvanceReadOffset (block.file.Write (GetReadPtr (), available));
                }
            }
            return count;
        }

        FileAllocator::FileAllocator (
                BufferedFile::SharedPtr file_,
                bool secure,
                std::size_t btreeEntriesPerNode,
                std::size_t btreeNodesPerPage,
                Allocator::SharedPtr allocator) :
                BufferedFile::TransactionParticipant (file_),
                file (file_),
                header (secure ? Header::FLAGS_SECURE : 0),
                btree (nullptr) {
            file->Seek (0, SEEK_SET);
            if (file->GetSize () > 0) {
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
            #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                if (!header.IsBlockInfoUsesMagic ()) {
            #else // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                if (header.IsBlockInfoUsesMagic ()) {
            #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "This FileAllocator file (%s) cannot be opened by this version of %s.",
                        file->GetPath ().c_str (),
                        THEKOGANS_UTIL);
                }
            }
            else {
                *file << MAGIC32 << header;
            }
            btree.Reset (
                new BTree (
                    *this,
                    header.btreeOffset,
                    btreeEntriesPerNode,
                    btreeNodesPerPage,
                    allocator));
        }

        FileAllocator::PtrType FileAllocator::Alloc (std::size_t size) {
            PtrType offset = 0;
            if (size > 0) {
                if (size < MIN_USER_DATA_SIZE) {
                    size = MIN_USER_DATA_SIZE;
                }
                // Look for a free block large enough to satisfy the request.
                BTree::KeyType result = btree->Search (BTree::KeyType (size, 0));
                if (result.second != 0) {
                    // Got it!
                    assert (result.first >= size);
                    btree->Delete (result);
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
                    // next.offset = result.second + size + BufferInfo::SIZE;
                    // next.size = result.first - size - BufferInfo::SIZE;
                    if (result.first - size >= MIN_BLOCK_SIZE) {
                        BlockInfo next (
                            *file,
                            result.second + size + BlockInfo::SIZE,
                            BlockInfo::FLAGS_FREE,
                            result.first - size - BlockInfo::SIZE);
                        next.Write ();
                        btree->Add (BTree::KeyType (next.GetSize (), next.GetOffset ()));
                    }
                    else {
                        // If not, update the requested size so that block.Write
                        // below can write the proper header/footer.
                        size = result.first;
                    }
                }
                else {
                    // None found? Grow the file.
                    offset = file->GetSize () + BlockInfo::HEADER_SIZE;
                    file->SetSize (offset + size + BlockInfo::FOOTER_SIZE);
                }
                BlockInfo block (*file, offset, 0, size);
                block.Write ();
            }
            return offset;
        }

        void FileAllocator::Free (PtrType offset) {
            // To honor the Allocator policy, we ignore NULL pointers.
            if (offset != 0) {
                BlockInfo block (*file, offset);
                block.Read ();
                if (!block.IsFree ()) {
                    PtrType clearOffset = block.GetOffset ();
                    ui64 clearLength = block.GetSize ();
                    // Consolidate adjacent free non BTree::Node blocks.
                    if (!block.IsFirst (header.heapStart)) {
                        BlockInfo prev (*file);
                        block.Prev (prev);
                        if (prev.IsFree () && !prev.IsBTreeNode ()) {
                            btree->Delete (
                                BTree::KeyType (prev.GetSize (), prev.GetOffset ()));
                            if (IsSecure ()) {
                                // Assume prev body is clear.
                                clearOffset -= BlockInfo::HEADER_SIZE;
                                clearLength += BlockInfo::HEADER_SIZE;
                            }
                        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                            else {
                                // Since block will grow to occupy prev,
                                // it's offset is no longer valid.
                                block.Invalidate ();
                            }
                        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                            // Back up to cover the prev.
                            block.SetOffset (
                                block.GetOffset () - BlockInfo::SIZE - prev.GetSize ());
                            block.SetSize (
                                block.GetSize () + BlockInfo::SIZE + prev.GetSize ());
                        }
                    }
                    if (!block.IsLast (file->GetSize ())) {
                        BlockInfo next (*file);
                        block.Next (next);
                        if (next.IsFree () && !next.IsBTreeNode ()) {
                            btree->Delete (
                                BTree::KeyType (next.GetSize (), next.GetOffset ()));
                            if (IsSecure ()) {
                                // Assume next body is clear.
                                clearLength += BlockInfo::FOOTER_SIZE;
                            }
                        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                            else {
                                // Since block will grow to occupy next,
                                // next offset is no longer valid.
                                next.Invalidate ();
                            }
                        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                            // Expand to swallow the next.
                            block.SetSize (
                                block.GetSize () + BlockInfo::SIZE + next.GetSize ());
                        }
                    }
                    // If we're not the last block...
                    if (!block.IsLast (file->GetSize ())) {
                        // ... add it to the free list.
                        btree->Add (BTree::KeyType (block.GetSize (), block.GetOffset ()));
                        block.SetFree (true);
                        block.Write ();
                        if (IsSecure ()) {
                            BlockBuffer buffer (*file, block.GetOffset (), clearLength);
                            buffer.AdvanceWriteOffset (
                                SecureZeroMemory (
                                    buffer.GetWritePtr (),
                                    buffer.GetDataAvailableForWriting ()));
                            buffer.BlockWrite (clearOffset - block.GetOffset (), clearLength);
                        }
                    }
                    else {
                        // If we are, truncate the heap.
                        file->SetSize (block.GetOffset () - BlockInfo::HEADER_SIZE);
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

        void FileAllocator::DumpBTree () {
            btree->Dump ();
        }

        void FileAllocator::Flush () {
            if (IsDirty ()) {
                file->Seek (0, SEEK_SET);
                *file << MAGIC32 << header;
                SetDirty (false);
            }
        }

        void FileAllocator::Reload () {
            if (IsDirty () && file->GetSize () > 0) {
                file->Seek (0, SEEK_SET);
                ui32 magic;
                *file >> magic >> header;
                SetDirty (false);
            }
        }

        FileAllocator::PtrType FileAllocator::AllocBTreeNode (std::size_t size) {
            PtrType offset = 0;
            if (header.freeBTreeNodeOffset != 0) {
                offset = header.freeBTreeNodeOffset;
                BlockInfo block (*file, offset);
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
                offset = file->GetSize () + BlockInfo::HEADER_SIZE;
                file->SetSize (offset + size + BlockInfo::FOOTER_SIZE);
            }
            BlockInfo block (*file, offset, BlockInfo::FLAGS_BTREE_NODE, size);
            block.Write ();
            return offset;
        }

        void FileAllocator::FreeBTreeNode (PtrType offset) {
            if (offset >= header.heapStart + BlockInfo::HEADER_SIZE) {
                BlockInfo block (*file, offset);
                block.Read ();
                if (!block.IsFree () && block.IsBTreeNode ()) {
                    if (!block.IsLast (file->GetSize ())) {
                        block.SetFree (true);
                        block.SetNextBTreeNodeOffset (header.freeBTreeNodeOffset);
                        block.Write ();
                        header.freeBTreeNodeOffset = offset;
                        SetDirty (true);
                    }
                    else {
                        file->SetSize (block.GetOffset () - BlockInfo::HEADER_SIZE);
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
