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
            return header.rootBlock;
        }

        void FileAllocator::SetRootBlock (PtrType rootBlock) {
            LockGuard<SpinLock> guard (spinLock);
            header.rootBlock = rootBlock;
            Save ();
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
                        new FixedSizeFileAllocator (path, blockSize, blocksPerPage, allocator) :
                        new RandomSizeFileAllocator (path, allocator));
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
            serializer << header.blockSize << header.freeBlock << header.rootBlock;
            return serializer;
        }

        Serializer &operator >> (
                Serializer &serializer,
                FileAllocator::Header &header) {
            serializer >> header.blockSize >> header.freeBlock >> header.rootBlock;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
