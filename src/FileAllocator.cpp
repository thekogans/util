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

        FileAllocator::SharedPtr FileAllocator::Pool::GetFileAllocator (
                const std::string &path,
                std::size_t blockSize,
                std::size_t blocksPerPage,
                Allocator::SharedPtr allocator) {
            std::string absolutePath = Path (path).MakeAbsolute ();
            LockGuard<SpinLock> guard (spinLock);
            std::pair<Map::iterator, bool> result =
                map.insert (Map::value_type (absolutePath, nullptr));
            if (result.second) {
                result.first->second.Reset (
                    new FileAllocator (
                        absolutePath, blockSize, blocksPerPage, allocator));
            }
            return result.first->second;
        }

        void FileAllocator::Pool::FlushFileAllocator (const std::string &path) {
            LockGuard<SpinLock> guard (spinLock);
            if (!path.empty ()) {
                Map::iterator it = map.find (Path (path).MakeAbsolute ());
                if (it != map.end ()) {
                    it->second->Flush ();
                }
            }
            else {
                for (Map::iterator it = map.begin (), end = map.end (); it != end; ++it) {
                    it->second->Flush ();
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
                if (IsFree () && IsFixed ()) {
                    file >> nextBlockOffset;
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
            if (IsFree () && IsFixed ()) {
                file << nextBlockOffset;
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
            prev.offset = offset - prev.footer.size - SIZE;
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

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (FileAllocator::BlockBuffer)

        FileAllocator::BlockBuffer::BlockBuffer (
                FileAllocator &fileAllocator,
                PtrType offset,
                std::size_t bufferLength) :
                Buffer (fileAllocator.file.endianness),
                block (offset) {
            block.Read (fileAllocator.file);
            if (!block.IsFree ()) {
                if (bufferLength == 0) {
                    bufferLength = block.GetSize ();
                }
                Resize (
                    bufferLength,
                    block.IsFixed () ?
                        fileAllocator.fixedAllocator :
                        fileAllocator.blockAllocator);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Trying to acquire a FileAllocator::BlockBuffer @"
                    "on an unallocated block: " THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        }

        std::size_t FileAllocator::BlockBuffer::BlockRead (
                File &file,
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
                    file.Seek (block.GetOffset () + blockOffset, SEEK_SET);
                    countRead = AdvanceWriteOffset (
                        file.Read (GetWritePtr (), availableToRead));
                }
            }
            return countRead;
        }

        std::size_t FileAllocator::BlockBuffer::BlockWrite (
                File &file,
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
                    file.Seek (block.GetOffset () + blockOffset, SEEK_SET);
                    countWritten = AdvanceReadOffset (
                        file.Write (GetReadPtr (), availableToWrite));
                }
            }
            return countWritten;
        }

        std::size_t FileAllocator::GetBlockSize (PtrType offset) {
            LockGuard<SpinLock> guard (spinLock);
            BlockInfo block (offset);
            block.Read (file);
            return !block.IsFree () ? block.GetSize () : 0;
        }

        FileAllocator::Registry &FileAllocator::GetRegistry (
                std::size_t entriesPerNode,
                std::size_t nodesPerPage,
                Allocator::SharedPtr allocator) {
            if (!IsFixed ()) {
                LockGuard<SpinLock> guard (registryLock);
                if (registry == nullptr) {
                    // Not a huge fan of this. The implicit call to
                    // FileAllocator::SharedPtr ctor will increment the
                    // reference count on this. Fortunately FileAllocator's
                    // one and only ctor is private and the only way to
                    // create one is to ask the Pool, which will create
                    // it on the heap.
                    registry = new Registry (this, entriesPerNode, nodesPerPage, allocator);
                }
                return *registry;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "FileAllocator is fixed.");
            }
        }

        FileAllocator::PtrType FileAllocator::GetRootOffset () {
            LockGuard<SpinLock> guard (spinLock);
            return header.rootOffset;
        }

        void FileAllocator::SetRootOffset (PtrType rootOffset) {
            LockGuard<SpinLock> guard (spinLock);
            header.rootOffset = rootOffset;
            Save ();
        }

        FileAllocator::PtrType FileAllocator::Alloc (std::size_t size) {
            if (IsFixed ()) {
                LockGuard<SpinLock> guard (spinLock);
                return AllocFixedBlock ();
            }
            else {
                PtrType offset = 0;
                if (size > 0) {
                    if (size < MIN_USER_DATA_SIZE) {
                        size = MIN_USER_DATA_SIZE;
                    }
                    LockGuard<SpinLock> guard (spinLock);
                    // Look for a free block large enough to satisfy the request.
                    BTree::KeyType result = btree->Search (BTree::KeyType (size, 0));
                    if (result.second != 0) {
                        // Got it!
                        assert (result.first >= size);
                        btree->Delete (result);
                        offset = result.second;
                        // If the block we got is bigger than we need, split it.
                        if (result.first - size >= MIN_BLOCK_SIZE) {
                            result.first -= size;
                            PtrType nextBlockOffset = offset + size + BlockInfo::SIZE;
                            ui64 nextBlockSize = result.first - BlockInfo::SIZE;
                            btree->Add (BTree::KeyType (nextBlockSize, nextBlockOffset));
                            BlockInfo block (
                                nextBlockOffset, BlockInfo::FLAGS_FREE, nextBlockSize);
                            block.Write (file);
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
        }

        void FileAllocator::Free (PtrType offset) {
            // To honor the Allocator policy, we ignore NULL pointers.
            if (offset != 0) {
                if (IsFixed ()) {
                    LockGuard<SpinLock> guard (spinLock);
                    FreeFixedBlock (offset);
                }
                else if (offset >= header.heapStart + BlockInfo::HEADER_SIZE) {
                    LockGuard<SpinLock> guard (spinLock);
                    BlockInfo block (offset);
                    block.Read (file);
                    if (!block.IsFree () && !block.IsFixed ()) {
                        if (header.rootOffset == offset) {
                            header.rootOffset = 0;
                            Save ();
                        }
                        // Consolidate adjacent free non fixed blocks.
                        if (!block.IsFirst (header.heapStart)) {
                            BlockInfo prev;
                            block.Prev (file, prev);
                            if (prev.IsFree () && !prev.IsFixed ()) {
                                btree->Delete (BTree::KeyType (prev.GetSize (), prev.GetOffset ()));
                            #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                                // Since block will grow to occupy prev,
                                // it's offset is no longer valid.
                                block.Invalidate (file);
                            #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                                block.SetOffset (
                                    block.GetOffset () - BlockInfo::SIZE - prev.GetSize ());
                                block.SetSize (block.GetSize () + BlockInfo::SIZE + prev.GetSize ());
                            }
                        }
                        if (!block.IsLast (file.GetSize ())) {
                            BlockInfo next;
                            block.Next (file, next);
                            if (next.IsFree () && !next.IsFixed ()) {
                            #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                                // Since block will grow to occupy next,
                                // next offset is no longer valid.
                                next.Invalidate (file);
                            #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                                btree->Delete (BTree::KeyType (next.GetSize (), next.GetOffset ()));
                                block.SetSize (block.GetSize () + BlockInfo::SIZE + next.GetSize ());
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
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }
        }

        void FileAllocator::GetBlockInfo (BlockInfo &block) {
            LockGuard<SpinLock> guard (spinLock);
            block.Read (file);
        }

        bool FileAllocator::GetPrevBlockInfo (
                const BlockInfo &block,
                BlockInfo &prev) {
            LockGuard<SpinLock> guard (spinLock);
            if (!block.IsFirst (header.heapStart)) {
                block.Prev (file, prev);
                return true;
            }
            return false;
        }

        bool FileAllocator::GetNextBlockInfo (
                const BlockInfo &block,
                BlockInfo &next) {
            LockGuard<SpinLock> guard (spinLock);
            if (!block.IsLast (file.GetSize ())) {
                block.Next (file, next);
                return true;
            }
            return false;
        }

        FileAllocator::BlockBuffer::SharedPtr FileAllocator::CreateBlockBuffer (
                PtrType offset,
                std::size_t bufferLength,
                bool read,
                std::size_t blockOffset,
                std::size_t blockLength) {
            LockGuard<SpinLock> guard (spinLock);
            BlockBuffer::SharedPtr buffer (
                new BlockBuffer (*this, offset, bufferLength));
            if (read) {
                buffer->BlockRead (file, blockOffset, blockLength);
            }
            return buffer;
        }

        void FileAllocator::ReadBlockBuffer (
                BlockBuffer &buffer,
                std::size_t blockOffset,
                std::size_t blockLength) {
            LockGuard<SpinLock> guard (spinLock);
            buffer.BlockRead (file, blockOffset, blockLength);
        }

        void FileAllocator::WriteBlockBuffer (
                BlockBuffer &buffer,
                std::size_t blockOffset,
                std::size_t blockLength) {
            LockGuard<SpinLock> guard (spinLock);
            buffer.BlockWrite (file, blockOffset, blockLength);
        }

        void FileAllocator::Flush () {
            {
                LockGuard<SpinLock> guard (spinLock);
                WriteHeader ();
                if (btree != nullptr) {
                    btree->Flush ();
                }
            }
            {
                if (registry != nullptr) {
                    registry->Flush ();
                }
            }
            file.Flush ();
        }

        void FileAllocator::DumpBTree () {
            LockGuard<SpinLock> guard (spinLock);
            if (btree != nullptr) {
                btree->Dump ();
            }
        }

        FileAllocator::FileAllocator (
                const std::string &path,
                std::size_t blockSize,
                std::size_t blocksPerPage,
                Allocator::SharedPtr allocator) :
                file (HostEndian, path, SimpleFile::ReadWrite | SimpleFile::Create),
                header (
                    blockSize > 0 ? Header::FLAGS_FIXED : 0,
                    blockSize > 0 ?
                        MAX (blockSize, MIN_USER_DATA_SIZE) :
                        BTree::Node::FileSize (blocksPerPage)),
                btree (nullptr),
                registry (nullptr),
                dirty (false) {
            if (file.GetSize () > 0) {
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
                        path.c_str ());
                }
                file >> header;
            #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                if (!header.IsBlockInfoUsesMagic ()) {
            #else // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                if (header.IsBlockInfoUsesMagic ()) {
            #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "This FileAllocator file (%s) cannot be opened by this version of %s.",
                        path.c_str (),
                        THEKOGANS_UTIL);
                }
            }
            else {
                file.SetSize (header.heapStart);
                Save ();
            }
            fixedAllocator =
                BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                    header.blockSize,
                    blocksPerPage,
                    allocator);
            blockAllocator = IsFixed () ? fixedAllocator : allocator;
            if (!IsFixed ()) {
                btree = new BTree (
                    *this,
                    header.btreeOffset,
                    blocksPerPage,
                    BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                    allocator);
                if (header.btreeOffset != btree->GetOffset ()) {
                    header.btreeOffset = btree->GetOffset ();
                    Save ();
                }
            }
        }

        FileAllocator::~FileAllocator () {
            WriteHeader ();
            if (btree != nullptr) {
                // This will flush the btree cache to file.
                delete btree;
            }
            if (registry != nullptr) {
                // This will flush the registry cache to file.
                delete registry;
            }
            // The file dtor will call Close which will flush
            // all dirty pages to file.
        }

        FileAllocator::PtrType FileAllocator::AllocFixedBlock () {
            PtrType offset = 0;
            if (header.freeBlockOffset != 0) {
                offset = header.freeBlockOffset;
                BlockInfo block (offset);
                block.Read (file);
                if (block.IsFree () && block.IsFixed ()) {
                    header.freeBlockOffset = block.GetNextBlockOffset ();
                    Save ();
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Heap corruption @ " THEKOGANS_UTIL_UI64_FORMAT
                        ", expecting a free fixed block.",
                        offset);
                }
            }
            else {
                offset = file.GetSize () + BlockInfo::HEADER_SIZE;
                file.SetSize (offset + header.blockSize + BlockInfo::FOOTER_SIZE);
            }
            BlockInfo block (offset, BlockInfo::FLAGS_FIXED, header.blockSize);
            block.Write (file);
            return offset;
        }

        void FileAllocator::FreeFixedBlock (PtrType offset) {
            if (offset >= header.heapStart + BlockInfo::HEADER_SIZE) {
                BlockInfo block (offset);
                block.Read (file);
                if (!block.IsFree () && block.IsFixed ()) {
                    if (header.rootOffset == offset) {
                        header.rootOffset = 0;
                        Save ();
                    }
                    if (!block.IsLast (file.GetSize ())) {
                        block.SetFree (true);
                        block.SetNextBlockOffset (header.freeBlockOffset);
                        block.Write (file);
                        header.freeBlockOffset = offset;
                        Save ();
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

        void FileAllocator::Save () {
            dirty = true;
        }

        void FileAllocator::WriteHeader () {
            if (dirty) {
                file.Seek (0, SEEK_SET);
                file << MAGIC32 << header;
                dirty = false;
            }
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const FileAllocator::Header &header) {
            serializer <<
                header.version <<
                header.flags <<
                header.blockSize <<
                header.heapStart <<
                header.freeBlockOffset <<
                header.btreeOffset <<
                header.registryOffset <<
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
                header.blockSize >>
                header.heapStart >>
                header.freeBlockOffset >>
                header.btreeOffset >>
                header.registryOffset >>
                header.rootOffset;
            // The above are available with every version.
            // Add if statements to test for new versions,
            // and read their fields accordingly.
            return serializer;
        }

    } // namespace util
} // namespace thekogans
