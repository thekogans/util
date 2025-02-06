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
#include "thekogans/util/File.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/FileAllocator.h"

namespace thekogans {
    namespace util {

        void FileAllocator::BlockInfo::Header::Read (
                File &file,
                ui64 offset) {
            file.Seek (offset, SEEK_SET);
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
            ui32 magic;
            file >> magic;
            if (magic == MAGIC32) {
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                file >> flags >> size;
                if (IsFree () && IsFixed ()) {
                    file >> nextOffset;
                }
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt FileAllocator FileAllocator::BlockInfo::Header: "
                    THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
        }

        void FileAllocator::BlockInfo::Header::Write (
                File &file,
                ui64 offset) {
            file.Seek (offset, SEEK_SET);
        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
            file << MAGIC32;
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
            file << flags << size;
            if (IsFree () && IsFixed ()) {
                file << nextOffset;
            }
        }

        void FileAllocator::BlockInfo::Footer::Read (
                File &file,
                ui64 offset) {
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
                    "Corrupt FileAllocator::BlockInfo::Footer: "
                    THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
        }

        void FileAllocator::BlockInfo::Footer::Write (
                File &file,
                ui64 offset) {
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

        void FileAllocator::BlockInfo::Prev (BlockInfo &prev) {
            prev.footer.Read (file, offset - Footer::SIZE);
            prev.offset = offset - prev.footer.size;
            prev.header.Read (file, prev.offset);
            if (prev.header != prev.footer) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt FileAllocator::BlockInfo: " THEKOGANS_UTIL_UI64_FORMAT
                    " prev.header.flags: %u prev.header.size: " THEKOGANS_UTIL_UI64_FORMAT
                    " prev.footer.flags: %u prev.footer.size: " THEKOGANS_UTIL_UI64_FORMAT,
                    offset,
                    header.flags, header.size,
                    footer.flags, footer.size);
            }
        }

        void FileAllocator:: FileAllocator::BlockInfo::Next (BlockInfo &next) {
            next.offset = offset + header.size;
            next.Read ();
        }

        void FileAllocator::BlockInfo::Read () {
            header.Read (file, offset);
            footer.Read (file, offset + header.size - Footer::SIZE);
            if (header != footer) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt FileAllocator::BlockInfo: " THEKOGANS_UTIL_UI64_FORMAT
                    " prev.header.flags: %u prev.header.size: " THEKOGANS_UTIL_UI64_FORMAT
                    " prev.footer.flags: %u prev.footer.size: " THEKOGANS_UTIL_UI64_FORMAT,
                    offset,
                    header.flags, header.size,
                    footer.flags, footer.size);
            }
        }

        void FileAllocator::BlockInfo::Write () {
            header.Write (file, offset);
            footer.Write (file, offset + header.size - Footer::SIZE);
        }

        FileAllocator::BlockBuffer::BlockBuffer (
                FileAllocator &allocator_,
                ui64 offset_,
                std::size_t length) :
                Buffer (allocator_.file.endianness),
                allocator (allocator_),
                offset (offset_),
                block (allocator.file, offset - BlockInfo::HEADER_SIZE) {
            {
                LockedFilePtr file (allocator);
                block.Read ();
            }
            if (length == 0) {
                length = block.GetUserSize ();
            }
            Resize (
                length,
                allocator.IsFixed () || !block.IsFixed () ?
                    allocator.blockAllocator : allocator.fixedAllocator);
        }

        std::size_t FileAllocator::BlockBuffer::Read (
                std::size_t blockOffset,
                std::size_t length) {
            LockedFilePtr file (allocator);
            BlockInfo block (*file, offset - BlockInfo::HEADER_SIZE);
            block.Read ();
            if (blockOffset < block.GetUserSize ()) {
                if (length == 0) {
                    length = GetDataAvailableForWriting ();
                }
                ui64 availableToRead = block.GetUserSize () - blockOffset;
                if (availableToRead > length) {
                    availableToRead = length;
                }
                file->Seek (offset + blockOffset, SEEK_SET);
                return AdvanceWriteOffset (file->Read (GetWritePtr (), availableToRead));
            }
            return 0;
        }

        std::size_t FileAllocator::BlockBuffer::Write (
                std::size_t blockOffset,
                std::size_t length) {
            LockedFilePtr file (allocator);
            BlockInfo block (*file, offset - BlockInfo::HEADER_SIZE);
            block.Read ();
            if (blockOffset < block.GetUserSize ()) {
                if (length == 0) {
                    length = GetDataAvailableForReading ();
                }
                ui64 availableToWrite = block.GetUserSize () - blockOffset;
                if (availableToWrite > length) {
                    availableToWrite = length;
                }
                file->Seek (offset + blockOffset, SEEK_SET);
                return AdvanceReadOffset (file->Write (GetReadPtr (), availableToWrite));
            }
            return 0;
        }

        FileAllocator::SharedPtr FileAllocator::Pool::GetFileAllocator (
                const std::string &path,
                std::size_t blockSize,
                std::size_t blocksPerPage,
                Allocator::SharedPtr allocator) {
            LockGuard<SpinLock> guard (spinLock);
            std::pair<Map::iterator, bool> result =
                map.insert (Map::value_type (path, nullptr));
            if (result.second) {
                result.first->second.Reset (
                    new FileAllocator (path, blockSize, blocksPerPage, allocator));
            }
            return result.first->second;
        }

        FileAllocator::FileAllocator (
                const std::string &path,
                std::size_t blockSize,
                std::size_t blocksPerPage,
                Allocator::SharedPtr allocator) :
                file (HostEndian, path, SimpleFile::ReadWrite | SimpleFile::Create),
                header (
                    blockSize > 0 ? Header::FLAGS_FIXED : 0,
                    (ui32)(blockSize > 0 ? blockSize : BTree::Node::FileSize (blocksPerPage))) {
            if (file.GetSize () >= Header::SIZE) {
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
                        path.c_str ());
                }
                file >> header;
            }
            else {
                Save ();
            }
            blockAllocator = header.blockSize > 0 ?
                BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                    header.blockSize,
                    blocksPerPage,
                    allocator) :
                allocator;
            fixedAllocator =
                BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                    header.blockSize > 0 ?
                    header.blockSize :
                    BTree::Node::FileSize (blocksPerPage),
                    blocksPerPage,
                    allocator);
            if (!IsFixed ()) {
                btree.Reset (
                    new BTree (
                        *this,
                        header.btreeHeaderOffset,
                        blocksPerPage,
                        BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                        allocator));
                if (header.btreeHeaderOffset != btree->GetOffset ()) {
                    header.btreeHeaderOffset = btree->GetOffset ();
                    Save ();
                }
            }
        }

        ui64 FileAllocator::GetRootOffset () {
            LockGuard<SpinLock> guard (spinLock);
            return header.rootOffset + BlockInfo::HEADER_SIZE;
        }

        void FileAllocator::SetRootOffset (ui64 rootOffset) {
            LockGuard<SpinLock> guard (spinLock);
            header.rootOffset = rootOffset - BlockInfo::HEADER_SIZE;
            Save ();
        }

        ui64 FileAllocator::Alloc (std::size_t size) {
            if (IsFixed ()) {
                LockGuard<SpinLock> guard (spinLock);
                return AllocFixedBlock ();
            }
            else {
                ui64 offset = 0;
                if (size > 0) {
                    if (size < MIN_USER_DATA_SIZE) {
                        size = MIN_USER_DATA_SIZE;
                    }
                    size += BlockInfo::SIZE;
                    LockedFilePtr file (*this);
                    BTree::Key result = btree->Search (BTree::Key (size, 0));
                    if (result.second != 0) {
                        assert (result.first >= size);
                        btree->Delete (result);
                        offset = result.second;
                        result.first -= size;
                        if (result.first >= MIN_USER_DATA_SIZE) {
                            btree->Add (BTree::Key (result.first, offset + size));
                            BlockInfo block (*file, offset + size,
                                BlockInfo::FLAGS_FREE, result.first);
                            block.Write ();
                        }
                        else {
                            size += result.first;
                        }
                    }
                    else {
                        offset = file->GetSize ();
                        file->SetSize (offset + size);
                    }
                    BlockInfo block (*file, offset, 0, size);
                    block.Write ();
                    offset += BlockInfo::HEADER_SIZE;
                }
                return offset;
            }
        }

        void FileAllocator::Free (ui64 offset) {
            if (IsFixed ()) {
                LockGuard<SpinLock> guard (spinLock);
                FreeFixedBlock (offset);
            }
            else if (offset >= MIN_USER_DATA_OFFSET) {
                offset -= BlockInfo::HEADER_SIZE;
                LockedFilePtr file (*this);
                if (header.rootOffset == offset) {
                    header.rootOffset = 0;
                    Save ();
                }
                BlockInfo block (*file, offset);
                block.Read ();
                if (!block.IsFree () && !block.IsFixed ()) {
                    // Consolidate adjacent free non fixed blocks.
                    if (!block.IsFirst ()) {
                        BlockInfo prev (*file);
                        block.Prev (prev);
                        if (prev.IsFree () && !prev.IsFixed ()) {
                            btree->Delete (BTree::Key (prev.GetSize (), prev.GetOffset ()));
                            block.SetOffset (block.GetOffset () - prev.GetSize ());
                            block.SetSize (block.GetSize () + prev.GetSize ());
                        }
                    }
                    if (!block.IsLast ()) {
                        BlockInfo next (*file);
                        block.Next (next);
                        if (next.IsFree () && !next.IsFixed ()) {
                            btree->Delete (BTree::Key (next.GetSize (), next.GetOffset ()));
                            block.SetSize (block.GetSize () + next.GetSize ());
                        }
                    }
                    if (!block.IsLast ()) {
                        btree->Add (BTree::Key (block.GetSize (), block.GetOffset ()));
                        block.SetFree (true);
                        block.Write ();
                    }
                    else {
                        file->SetSize (block.GetOffset ());
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void FileAllocator::Save () {
            file.Seek (0, SEEK_SET);
            file << MAGIC32 << header;
        }

        ui64 FileAllocator::AllocFixedBlock () {
            ui64 offset = 0;
            if (header.fixedFreeListHeadOffset != 0) {
                offset = header.fixedFreeListHeadOffset;
                BlockInfo block (file, offset);
                block.Read ();
                if (block.IsFree () && block.IsFixed ()) {
                    header.fixedFreeListHeadOffset = block.GetNextOffset ();
                    Save ();
                    block.SetFree (false);
                    block.SetNextOffset (0);
                    block.Write ();
                }
            }
            else {
                offset = file.GetSize ();
                std::size_t size = BlockInfo::SIZE + header.blockSize;
                file.SetSize (offset + size);
                BlockInfo block (file, offset, BlockInfo::FLAGS_FIXED, size);
                block.Write ();
            }
            offset += BlockInfo::HEADER_SIZE;
            return offset;
        }

        void FileAllocator::FreeFixedBlock (ui64 offset) {
            if (offset >= MIN_USER_DATA_OFFSET) {
                offset -= BlockInfo::HEADER_SIZE;
                if (header.rootOffset == offset) {
                    header.rootOffset = 0;
                    Save ();
                }
                BlockInfo block (file, offset);
                block.Read ();
                if (!block.IsFree () && block.IsFixed ()) {
                    if (!block.IsLast ()) {
                        block.SetFree (true);
                        block.SetNextOffset (header.fixedFreeListHeadOffset);
                        block.Write ();
                        header.fixedFreeListHeadOffset = offset;
                        Save ();
                    }
                    else {
                        file.SetSize (block.GetOffset ());
                    }
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
                header.flags <<
                header.blockSize <<
                header.fixedFreeListHeadOffset <<
                header.btreeHeaderOffset <<
                header.rootOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                FileAllocator::Header &header) {
            serializer >>
                header.flags >>
                header.blockSize >>
                header.fixedFreeListHeadOffset >>
                header.btreeHeaderOffset >>
                header.rootOffset;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
