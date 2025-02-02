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

        FileBlockAllocator::FileBlockAllocator (
                const std::string &path,
                ui32 blockSize) :
                file (HostEndian, path, SimpleFile::ReadWrite | SimpleFile::Create),
                header (blockSize) {
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
            }
            else {
                Save ();
            }
        }

        ui64 FileBlockAllocator::GetRootBlock () {
            LockGuard<SpinLock> guard (spinLock);
            return header.rootBlock;
        }

        void FileBlockAllocator::SetRootBlock (ui64 rootBlock) {
            LockGuard<SpinLock> guard (spinLock);
            header.rootBlock = rootBlock;
            Save ();
        }

        ui64 FileBlockAllocator::Alloc () {
            LockGuard<SpinLock> guard (spinLock);
            ui64 offset;
            if (header.freeBlock != NIDX64) {
                offset = header.freeBlock;
                file.Seek (offset, SEEK_SET);
                file >> header.freeBlock;
                Save ();
            }
            else {
                offset = file.GetSize ();
                file.SetSize (offset + header.blockSize);
            }
            return offset;
        }

        void FileBlockAllocator::Free (ui64 offset) {
            LockGuard<SpinLock> guard (spinLock);
            file.Seek (offset, SEEK_SET);
            file << header.freeBlock;
            header.freeBlock = offset;
            Save ();
        }

        void FileBlockAllocator::Read (
                ui64 offset,
                void *data) {
            LockGuard<SpinLock> guard (spinLock);
            file.Seek (offset, SEEK_SET);
            file.Read (data, header.blockSize);
        }

        void FileBlockAllocator::Write (
                ui64 offset,
                const void *data) {
            LockGuard<SpinLock> guard (spinLock);
            file.Seek (offset, SEEK_SET);
            file.Write (data, header.blockSize);
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
