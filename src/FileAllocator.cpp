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
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/FileAllocatorRegistry.h"
#include "thekogans/util/FileAllocator.h"

namespace thekogans {
    namespace util {

        FileAllocator::Transaction::Transaction (FileAllocator &fileAllocator_) :
                fileAllocator (fileAllocator_) {
            fileAllocator.file.BeginTransaction ();
        }

        FileAllocator::Transaction::~Transaction () {
            if (fileAllocator.file.IsTransactionPending ()) {
                fileAllocator.file.AbortTransaction ();
                fileAllocator.file.Seek (0, SEEK_SET);
                ui32 magic;
                fileAllocator.file >> magic >> fileAllocator.header;
                std::size_t btreeNodesPerPage =
                    fileAllocator.btree->nodeAllocator->GetBlocksPerPage ();
                delete fileAllocator.btree;
                fileAllocator.btree = new BTree (
                    fileAllocator,
                    fileAllocator.header.btreeOffset,
                    DEFAULT_BTREE_ENTRIES_PER_NODE,
                    btreeNodesPerPage,
                    fileAllocator.allocator);
                std::size_t registryNodesPerPage =
                    fileAllocator.registry->nodeAllocator->GetBlocksPerPage ();
                delete fileAllocator.registry;
                fileAllocator.registry = new Registry (
                    fileAllocator,
                    fileAllocator.header.registryOffset,
                    DEFAULT_REGISTRY_ENTRIES_PER_NODE,
                    registryNodesPerPage,
                    fileAllocator.allocator);
                fileAllocator.dirty = false;
            }
        }

        void FileAllocator::Transaction::Commit () {
            fileAllocator.Flush ();
            fileAllocator.file.CommitTransaction ();
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

        void FileAllocator::BlockInfo::Prev (
                File &file,
                BlockInfo &prev) const {
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

        void FileAllocator:: FileAllocator::BlockInfo::Next (
                File &file,
                BlockInfo &next) const {
            next.offset = offset + header.size + SIZE;
            next.Read (file);
        }

        void FileAllocator::BlockInfo::Read (File &file) {
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

        void FileAllocator::BlockInfo::Write (File &file) const {
            header.Write (file, offset - HEADER_SIZE);
            footer.Write (file, offset + header.size);
        }

    #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
        void FileAllocator::BlockInfo::Invalidate (File &file) const {
            file.Seek (offset - HEADER_SIZE, SEEK_SET);
            // Simply stepping on magic will invalidate
            // this block for all future reads.
            file << (ui32)0;
        }
    #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)

        FileAllocator::BlockBuffer::BlockBuffer (
                FileAllocator &fileAllocator_,
                PtrType offset,
                std::size_t bufferLength,
                Allocator::SharedPtr allocator) :
                Buffer (fileAllocator_.file.endianness),
                fileAllocator (fileAllocator_),
                block (offset) {
            block.Read (fileAllocator.file);
            if (!block.IsFree ()) {
                if (bufferLength == 0) {
                    bufferLength = block.GetSize ();
                }
                Resize (bufferLength, allocator);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Trying to acquire a FileAllocator::BlockBuffer "
                    "on an unallocated block @" THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        }

        std::size_t FileAllocator::BlockBuffer::BlockRead (
                std::size_t blockOffset,
                std::size_t blockLength) {
            std::size_t countRead = 0;
            if (blockOffset < block.GetSize ()) {
                if (blockLength == 0 || blockLength > GetDataAvailableForWriting ()) {
                    blockLength = GetDataAvailableForWriting ();
                }
                if (blockLength > 0) {
                    ui64 availableToRead = block.GetSize () - blockOffset;
                    if (availableToRead > blockLength) {
                        availableToRead = blockLength;
                    }
                    fileAllocator.file.Seek (block.GetOffset () + blockOffset, SEEK_SET);
                    countRead = AdvanceWriteOffset (
                        fileAllocator.file.Read (GetWritePtr (), availableToRead));
                }
            }
            return countRead;
        }

        std::size_t FileAllocator::BlockBuffer::BlockWrite (
                std::size_t blockOffset,
                std::size_t blockLength) {
            std::size_t countWritten = 0;
            if (blockOffset < block.GetSize ()) {
                if (blockLength == 0 || blockLength > GetDataAvailableForReading ()) {
                    blockLength = GetDataAvailableForReading ();
                }
                if (blockLength > 0) {
                    ui64 availableToWrite = block.GetSize () - blockOffset;
                    if (availableToWrite > blockLength) {
                        availableToWrite = blockLength;
                    }
                    fileAllocator.file.Seek (block.GetOffset () + blockOffset, SEEK_SET);
                    countWritten = AdvanceReadOffset (
                        fileAllocator.file.Write (GetReadPtr (), availableToWrite));
                }
            }
            return countWritten;
        }

        FileAllocator::FileAllocator (
                const std::string &path,
                std::size_t btreeEntriesPerNode,
                std::size_t btreeNodesPerPage,
                std::size_t registryEntriesPerNode,
                std::size_t registryNodesPerPage,
                Allocator::SharedPtr allocator_) :
                file (HostEndian, path, SimpleFile::ReadWrite | SimpleFile::Create),
                header (),
                allocator (allocator_),
                btree (nullptr),
                registry (nullptr),
                dirty (false) {
            if (file.GetSize () > 0) {
                file.Seek (0, SEEK_SET);
                ui32 magic;
                file >> magic;
                if (magic == MAGIC32) {
                    // File is host endian.
                }
                else if (ByteSwap<GuestEndian, HostEndian> (magic) == MAGIC32) {
                    // File is guest endian.
                    file.endianness = GuestEndian;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt FileAllocator file (%s)",
                        file.GetPath ().c_str ());
                }
                file >> header;
            #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                if (!header.IsBlockInfoUsesMagic ()) {
            #else // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                if (header.IsBlockInfoUsesMagic ()) {
            #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "This FileAllocator file (%s) cannot be opened by this version of %s.",
                        file.GetPath ().c_str (),
                        THEKOGANS_UTIL);
                }
            }
            else {
                dirty = true;
            }
            btree = new BTree (
                *this,
                header.btreeOffset,
                btreeEntriesPerNode,
                btreeNodesPerPage,
                allocator);
            if (header.btreeOffset == 0) {
                assert (dirty);
                header.btreeOffset = btree->GetOffset ();
            }
            registry = new Registry (
                *this,
                header.registryOffset,
                registryEntriesPerNode,
                registryNodesPerPage,
                allocator);
            if (header.registryOffset == 0) {
                assert (dirty);
                header.registryOffset = registry->GetOffset ();
            }
            if (dirty) {
                Flush ();
            }
        }

        FileAllocator::~FileAllocator () {
            delete btree;
            delete registry;
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
                    if (result.first - size >= MIN_BLOCK_SIZE) {
                        BlockInfo next (
                            offset + size + BlockInfo::SIZE,
                            BlockInfo::FLAGS_FREE,
                            result.first - size - BlockInfo::SIZE);
                        next.Write (file);
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
                    offset = file.GetSize () + BlockInfo::HEADER_SIZE;
                    file.SetSize (offset + size + BlockInfo::FOOTER_SIZE);
                }
                BlockInfo block (offset, 0, size);
                block.Write (file);
            }
            return offset;
        }

        void FileAllocator::Free (PtrType offset) {
            // To honor the Allocator policy, we ignore NULL pointers.
            if (offset != 0) {
                BlockInfo block (offset);
                block.Read (file);
                if (!block.IsFree ()) {
                    // Consolidate adjacent free non BTree::Node blocks.
                    if (!block.IsFirst (header.heapStart)) {
                        BlockInfo prev;
                        block.Prev (file, prev);
                        if (prev.IsFree () && !prev.IsBTreeNode ()) {
                            btree->Delete (
                                BTree::KeyType (prev.GetSize (), prev.GetOffset ()));
                        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                            // Since block will grow to occupy prev,
                            // it's offset is no longer valid.
                            block.Invalidate (file);
                        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                            // Back up to cover the prev.
                            block.SetOffset (
                                block.GetOffset () - BlockInfo::SIZE - prev.GetSize ());
                            block.SetSize (
                                block.GetSize () + BlockInfo::SIZE + prev.GetSize ());
                        }
                    }
                    if (!block.IsLast (file.GetSize ())) {
                        BlockInfo next;
                        block.Next (file, next);
                        if (next.IsFree () && !next.IsBTreeNode ()) {
                            btree->Delete (
                                BTree::KeyType (next.GetSize (), next.GetOffset ()));
                        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                            // Since block will grow to occupy next,
                            // next offset is no longer valid.
                            next.Invalidate (file);
                        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                            // Expand to swallow the next.
                            block.SetSize (
                                block.GetSize () + BlockInfo::SIZE + next.GetSize ());
                        }
                    }
                    // If we're not the last block...
                    if (!block.IsLast (file.GetSize ())) {
                        // ... add it to the free list.
                        btree->Add (BTree::KeyType (block.GetSize (), block.GetOffset ()));
                        block.SetFree (true);
                        block.Write (file);
                    }
                    else {
                        // If we are, truncate the heap.
                        file.SetSize (block.GetOffset () - BlockInfo::HEADER_SIZE);
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
            if (dirty) {
                file.Seek (0, SEEK_SET);
                file << MAGIC32 << header;
                dirty = false;
            }
            btree->Flush ();
            registry->Flush ();
        }

        FileAllocator::PtrType FileAllocator::AllocBTreeNode (std::size_t size) {
            PtrType offset = 0;
            if (header.freeBTreeNodeOffset != 0) {
                offset = header.freeBTreeNodeOffset;
                BlockInfo block (offset);
                block.Read (file);
                if (block.IsFree () && block.IsBTreeNode ()) {
                    header.freeBTreeNodeOffset = block.GetNextBTreeNodeOffset ();
                    dirty = true;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Heap corruption @" THEKOGANS_UTIL_UI64_FORMAT
                        ", expecting a free BTree::Node block.",
                        offset);
                }
            }
            else {
                offset = file.GetSize () + BlockInfo::HEADER_SIZE;
                file.SetSize (offset + size + BlockInfo::FOOTER_SIZE);
            }
            BlockInfo block (offset, BlockInfo::FLAGS_BTREE_NODE, size);
            block.Write (file);
            return offset;
        }

        void FileAllocator::FreeBTreeNode (PtrType offset) {
            if (offset >= header.heapStart + BlockInfo::HEADER_SIZE) {
                BlockInfo block (offset);
                block.Read (file);
                if (!block.IsFree () && block.IsBTreeNode ()) {
                    if (!block.IsLast (file.GetSize ())) {
                        block.SetFree (true);
                        block.SetNextBTreeNodeOffset (header.freeBTreeNodeOffset);
                        block.Write (file);
                        header.freeBTreeNodeOffset = offset;
                        dirty = true;
                    }
                    else {
                        file.SetSize (block.GetOffset () - BlockInfo::HEADER_SIZE);
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
                header.registryOffset;
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
                header.registryOffset;
            // The above are available with every version.
            // Add if statements to test for new versions,
            // and read their fields accordingly.
            return serializer;
        }

    } // namespace util
} // namespace thekogans
