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
#include "thekogans/util/FileBlockAllocator.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (thekogans::util::FileBlockAllocator)

        FileBlockAllocator::FileBlockAllocator (
                const std::string &path,
                std::size_t blockSize,
                std::size_t blocksPerPage,
                Allocator::SharedPtr allocator) :
                file (HostEndian, path, SimpleFile::ReadWrite | SimpleFile::Create),
                blockAllocator (
                    BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                        blockSize,
                        blocksPerPage,
                        allocator)),
                header ((ui32)blockSize) {
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
                        "Corrupt file block allocator file (%s)",
                        path.c_str ());
                }
                file >> header;
                if (header.blockSize != blockSize) {
                    blockAllocator =
                        BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                            header.blockSize,
                            blocksPerPage,
                            allocator);
                }
            }
            else {
                Save ();
            }
        }

        FileBlockAllocator::PtrType FileBlockAllocator::GetRootBlock () {
            LockGuard<SpinLock> guard (spinLock);
            return header.rootBlock;
        }

        void FileBlockAllocator::SetRootBlock (PtrType rootBlock) {
            LockGuard<SpinLock> guard (spinLock);
            header.rootBlock = rootBlock;
            Save ();
        }

        FileBlockAllocator::PtrType FileBlockAllocator::Alloc (std::size_t size) {
            PtrType offset = nullptr;
            if (size <= header.blockSize) {
                LockGuard<SpinLock> guard (spinLock);
                if (header.freeBlock != nullptr) {
                    offset = header.freeBlock;
                    file.Seek ((ui64)offset, SEEK_SET);
                    file >> header.freeBlock;
                    Save ();
                }
                else {
                    offset = (FileBlockAllocator::PtrType)file.GetSize ();
                    file.SetSize ((ui64)offset + header.blockSize);
                }
            }
            return offset;
        }

        void FileBlockAllocator::Free (
                PtrType offset,
                std::size_t size) {
            if (size <= header.blockSize) {
                bool save = false;
                LockGuard<SpinLock> guard (spinLock);
                if (header.rootBlock == offset) {
                    header.rootBlock = nullptr;
                    save = true;
                }
                if ((ui64)offset + header.blockSize < file.GetSize ()) {
                    file.Seek ((ui64)offset, SEEK_SET);
                    file << header.freeBlock;
                    header.freeBlock = offset;
                    save = true;
                }
                else {
                    while ((ui64)header.freeBlock == (ui64)offset - header.blockSize) {
                        offset = header.freeBlock;
                        file.Seek ((ui64)header.freeBlock, SEEK_SET);
                        file >> header.freeBlock;
                        save = true;
                    }
                    file.SetSize ((ui64)offset);
                }
                if (save) {
                    Save ();
                }
            }
        }

        std::size_t FileBlockAllocator::Read (
                PtrType offset,
                void *data,
                std::size_t length) {
            LockGuard<SpinLock> guard (spinLock);
            file.Seek ((ui64)offset, SEEK_SET);
            return file.Read (data, length);
        }

        std::size_t FileBlockAllocator::Write (
                PtrType offset,
                const void *data,
                std::size_t length) {
            LockGuard<SpinLock> guard (spinLock);
            file.Seek ((ui64)offset, SEEK_SET);
            return file.Write (data, length);
        }

        FileBlockAllocator::SharedPtr FileBlockAllocator::Pool::GetFileBlockAllocator (
                const std::string &path,
                std::size_t blockSize,
                std::size_t blocksPerPage,
                Allocator::SharedPtr allocator) {
            LockGuard<SpinLock> guard (spinLock);
            std::pair<Map::iterator, bool> result =
                map.insert (Map::value_type (path, nullptr));
            if (result.second) {
                result.first->second.Reset (
                    new FileBlockAllocator (path, blockSize, blocksPerPage, allocator));
            }
            return result.first->second;
        }

        void FileBlockAllocator::Save () {
            file.Seek (0, SEEK_SET);
            file << MAGIC32 << header;
        }

        Serializer &operator << (
                Serializer &serializer,
                const FileBlockAllocator::Header &header) {
            serializer << header.blockSize << header.freeBlock << header.rootBlock;
            return serializer;
        }

        Serializer &operator >> (
                Serializer &serializer,
                FileBlockAllocator::Header &header) {
            serializer >> header.blockSize >> header.freeBlock >> header.rootBlock;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
