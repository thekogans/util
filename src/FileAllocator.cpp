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


#include "thekogans/util/LockGuard.h"
#include "thekogans/util/FixedSizeFileAllocator.h"
#include "thekogans/util/RandomSizeFileAllocator.h"
#include "thekogans/util/FileAllocator.h"

namespace thekogans {
    namespace util {

        namespace {
        }

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (thekogans::util::FileAllocator)

        FileAllocator::FileAllocator (
                const std::string &path,
                std::size_t blockSize,
                std::size_t blocksPerPage,
                Allocator::SharedPtr allocator) :
                file (HostEndian, path, SimpleFile::ReadWrite | SimpleFile::Create),
                blockAllocator (
                    blockSize > 0 ?
                        BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                            blockSize,
                            blocksPerPage,
                            allocator) : allocator),
                header ((ui32)blockSize) {
            if (file.GetSize () >= UI32_SIZE + Header::SIZE) {
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
                        "Corrupt file block allocator file (%s)",
                        path.c_str ());
                }
                file >> header;
                if (header.blockSize != blockSize) {
                    blockAllocator =
                        header.blockSize > 0 ?
                            BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                                header.blockSize,
                                blocksPerPage,
                                allocator) : allocator;
                }
            }
            else {
                Save ();
            }
        }

        FileAllocator::PtrType FileAllocator::GetRootBlock () {
            LockGuard<SpinLock> guard (spinLock);
            return header.rootBlockOffset;
        }

        void FileAllocator::SetRootBlock (PtrType rootBlockOffset) {
            LockGuard<SpinLock> guard (spinLock);
            header.rootBlockOffset = rootBlockOffset;
            Save ();
        }

        FileAllocator::PtrType FileAllocator::Alloc (std::size_t size) {
            PtrType offset = nullptr;
            if (size > 0) {
                size += BlockHeader::SIZE + BlockFooter::SIZE;
                LockGuard<SpinLock> guard (spinLock);
                BTree::Key result = btree.Search (BTree::Key (size, nullptr));
                if (result.first >= size && result.second != nullptr) {
                    btree.Delete (result);
                    offset = result.second;
                    if (result.first > size) {
                        ui64 remainder = result.first - size;
                        if (remainder > BlockHeader::SIZE + BlockFooter::SIZE) {
                            PtrType nextBlock = (ui64)offset + size;
                            btree.Add (BTree::Key (remainder, nextBlock));
                            BlockInfo block (file, nextBlock);
                            block.SetFlags (BlockInfo::FLAGS_FREE);
                            block.SetSize (remainder);
                            block.Write ();
                        }
                    }
                }
                else {
                    offset = (PtrType)file.GetSize ();
                    file.SetSize ((ui64)offset + size);
                }
                BlockInfo block (file, offset);
                block.SetFlags (BlockInfo::FLAGS_IN_USE);
                block.SetSize (size);
                block.Write ();
                offset = (ui64)offset + BlockHeader::SIZE;
            }
            return offset;
        }

        void FileAllocator::Free (
                PtrType offset,
                std::size_t size) {
            if (offset != nullptr && size > 0) {
                LockGuard<SpinLock> guard (spinLock);
                if (header.rootBlock == offset) {
                    header.rootBlock = nullptr;
                    Save ();
                }
                BlockInfo block ((ui64)offset - BlockHeader::SIZE);
                block.Read (file);
                if (block.Validate (BlockHeader::FLAGS_IN_USE, size)) {
                    if (!block.IsLast (file)) {
                        BlockInfo::SharedPtr prev = block.Prev (file);
                        if (prev->IsFree () && !prev->IsFixed ()) {
                            btree.Delete (BTree::Key (prev->header.size, prev->offset));
                            block.offset -= prev->header.size;
                            block.header.size += prev->header.size;
                        }
                        BlockInfo::SharedPtr next = block.Next (file);
                        if (next->IsFree () && !next->IsFixed ()) {
                            btree.Delete (BTree::Key (next->header.size, next->offset));
                            block.header.size += next->header.size;
                        }
                    }
                    if (!block.IsLast (file)) {
                        btree.Add (BTree::Key (block.header.size, block.offset));
                        block.SetFlags (BlockHeader::FLAGS_FREE);
                        block.Write ();
                    }
                    else {
                        file.SetSize ((ui64)headerOffset);
                    }
                }
            }
        }

        FileAllocator::PtrType FileAllocator::AllocFixedBlock (std::size_t size) {
            PtrType offset = nullptr;
            if (size <= header.blockSize) {
                LockGuard<SpinLock> guard (spinLock);
                if (header.headFreeFixedBlockOffset != nullptr) {
                    offset = header.headFreeFixedBlockOffset;
                    BlockHeader blockHeader;
                    blockHeader.Read (file, offset);
                    header.headFreeFixedBlockOffset = blockHeader.nextOffset;
                    Save ();
                }
                else {
                    offset = (PtrType)file.GetSize ();
                    file.SetSize ((ui64)offset + header.blockSize);
                }

            }
            return offset;
        }

        void FileAllocator::FreeFixedBlock (
                PtrType offset,
                std::size_t size) {
            if (size <= header.blockSize) {
                bool save = false;
                LockGuard<SpinLock> guard (spinLock);
                if (header.rootBlockOffset == offset) {
                    header.rootBlockOffset = nullptr;
                    save = true;
                }
                if ((ui64)offset + header.blockSize < file.GetSize ()) {
                    file.Seek ((ui64)offset, SEEK_SET);
                    file << header.headFreeFixedBlockOffset;
                    header.headFreeFixedBlockOffset = offset;
                    save = true;
                }
                else {
                    while ((ui64)header.headFreeFixedBlockOffset == (ui64)offset - header.blockSize) {
                        offset = header.headFreeFixedBlockOffset;
                        file.Seek ((ui64)header.headFreeFixedBlockOffset, SEEK_SET);
                        file >> header.headFreeFixedBlockOffset;
                        save = true;
                    }
                    file.SetSize ((ui64)offset);
                }
                if (save) {
                    Save ();
                }
            }
        }

        std::size_t FileAllocator::Read (
                PtrType offset,
                void *data,
                std::size_t length) {
            LockGuard<SpinLock> guard (spinLock);
            file.Seek ((ui64)offset, SEEK_SET);
            return file.Read (data, length);
        }

        std::size_t FileAllocator::Write (
                PtrType offset,
                const void *data,
                std::size_t length) {
            LockGuard<SpinLock> guard (spinLock);
            file.Seek ((ui64)offset, SEEK_SET);
            return file.Write (data, length);
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
                    blockSize > 0 ?
                        (FileAllocator *)new FixedSizeFileAllocator (path, blockSize, blocksPerPage, allocator) :
                        (FileAllocator *)new RandomSizeFileAllocator (path, allocator));
            }
            return result.first->second;
        }

        void FileAllocator::Save () {
            file.Seek (0, SEEK_SET);
            file << MAGIC32 << header;
        }

        Serializer &operator << (
                Serializer &serializer,
                const FileAllocator::Header &header) {
            serializer <<
                header.flags <<
                header.blockSize <<
                header.headFreeFixedBlockOffset <<
                header.btreeOffset <<
                header.rootBlockOffset;
            return serializer;
        }

        Serializer &operator >> (
                Serializer &serializer,
                FileAllocator::Header &header) {
            serializer >>
                header.flags >>
                header.blockSize >>
                header.headFreeFixedBlockOffset >>
                header.btreeOffset >>
                header.rootBlockOffset;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
