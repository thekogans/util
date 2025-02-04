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


#include "thekogans/util/Config.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/File.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/FileAllocator.h"

namespace thekogans {
    namespace util {

        namespace {
            struct BlockInfo : public RefCounted {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (BlockInfo)

                /// \brief
                /// Declare \see{DynamicCreatable} boilerplate.
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                File &file;
                FileAllocator::PtrType offset;
                enum {
                    FLAGS_FREE = 1,
                    FLAGS_FIXED = 2
                };
                struct Header {
                    ui32 flags;
                    ui64 size;
                    FileAllocator::PtrType nextOffset;

                    enum {
                        SIZE =
                            UI32_SIZE +
                            UI32_SIZE +
                            UI64_SIZE +
                            FileAllocator::PtrTypeSize +
                            FileAllocator::PtrTypeSize
                    };

                    Header (
                        ui32 flags_ = 0,
                        ui64 size_ = 0,
                        FileAllocator::PtrType nextOffset_ = 0) :
                        flags (flags_),
                        size (size_),
                        nextOffset (nextOffset_) {}

                    void Read (
                        File &file,
                        FileAllocator::PtrType offset);
                    void Write (
                        File &file,
                        FileAllocator::PtrType offset);
                } header;
                struct Footer {
                    ui32 flags;
                    ui64 size;

                    enum {
                        SIZE = UI32_SIZE + UI32_SIZE + UI64_SIZE
                    };

                    Footer (
                        ui32 flags_ = 0,
                        ui64 size_ = 0) :
                        flags (flags_),
                        size (size_) {}

                    void Read (
                        File &file,
                        FileAllocator::PtrType offset);
                    void Write (
                        File &file,
                        FileAllocator::PtrType offset);
                } footer;

                enum {
                    SIZE = Header::SIZE + Footer::SIZE
                };

                BlockInfo (
                    File &file_,
                    FileAllocator::PtrType offset_ = 0,
                    ui32 flags = 0,
                    ui64 size = 0,
                    FileAllocator::PtrType nextOffset = 0) :
                    file (file_),
                    offset (offset_),
                    header (flags, size, nextOffset),
                    footer (flags, size) {}

                inline bool IsFree () const {
                    return Flags32 (header.flags).Test (FLAGS_FREE);
                }
                inline void SetIsFree (bool isFree) {
                    Flags32 (header.flags).Set (FLAGS_FREE, isFree);
                    Flags32 (footer.flags).Set (FLAGS_FREE, isFree);
                }
                inline bool IsFixed () const {
                    return Flags32 (header.flags).Test (FLAGS_FIXED);
                }
                inline void SetFixed (bool fixed) {
                    Flags32 (header.flags).Set (FLAGS_FIXED, fixed);
                    Flags32 (footer.flags).Set (FLAGS_FIXED, fixed);
                }
                inline bool IsFirst () const {
                    return offset == Header::SIZE;
                }
                inline bool IsLast () const {
                    return offset + header.size == file.GetSize ();
                }

                SharedPtr Prev ();
                SharedPtr Next ();

                void Read ();
                void Write ();
            };

            void BlockInfo::Header::Read (
                    File &file,
                    FileAllocator::PtrType offset) {
                file.Seek (offset, SEEK_SET);
                ui32 magic;
                file >> magic;
                if (magic == MAGIC32) {
                    file >> flags >> size >> nextOffset;
                }
                else {
                    // FIXME: throw
                    assert (0);
                }
            }

            void BlockInfo::Header::Write (
                    File &file,
                    FileAllocator::PtrType offset) {
                file.Seek (offset, SEEK_SET);
                file << MAGIC32 << flags << size << nextOffset;
            }

            void BlockInfo::Footer::Read (
                    File &file,
                    FileAllocator::PtrType offset) {
                file.Seek (offset, SEEK_SET);
                ui32 magic;
                file >> magic;
                if (magic == MAGIC32) {
                    file >> flags >> size;
                }
                else {
                    // FIXME: throw
                    assert (0);
                }
            }

            void BlockInfo::Footer::Write (
                File &file,
                FileAllocator::PtrType offset) {
                file.Seek (offset, SEEK_SET);
                file << MAGIC32 << flags << size;
            }

            bool operator != (
                    const BlockInfo::Header &header,
                    const BlockInfo::Footer &footer) {
                return header.flags != footer.flags || header.size != footer.size;
            }

            BlockInfo::SharedPtr BlockInfo::Prev () {
                SharedPtr prev (new BlockInfo (file));
                prev->footer.Read (file, offset - Footer::SIZE);
                prev->offset -= prev->footer.size;
                prev->header.Read (file, prev->offset);
                if (prev->header != prev->footer) {
                    // FIXME: throw
                    assert (0);
                }
                return prev;
            }

            BlockInfo::SharedPtr BlockInfo::Next () {
                SharedPtr next (new BlockInfo (file, offset + header.size));
                next->Read ();
                return next;
            }

            void BlockInfo::Read () {
                header.Read (file, offset);
                footer.Read (file, offset + header.size - Footer::SIZE);
                if (header != footer) {
                    // FIXME: throw
                    assert (0);
                }
            }

            void BlockInfo::Write () {
                header.Write (file, offset);
                footer.Write (file, offset + header.size - Footer::SIZE);
            }

            THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (BlockInfo)
        }

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (FileAllocator::BlockData)

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
                header (
                    blockSize > 0 ? Header::FLAGS_FIXED : 0,
                    blockSize > 0 ? (ui32)blockSize : (ui32)BTree::Node::FileSize (blocksPerPage)) {
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
            if (!IsFixed ()) {
                freeList.Reset (
                    new BTree (*this, header.freeListOffset, blocksPerPage, allocator));
                if (header.freeListOffset == 0) {
                    header.freeListOffset = freeList->offset;
                    Save ();
                }
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
            if (IsFixed ()) {
                return AllocFixedBlock ();
            }
            PtrType offset = 0;
            if (size > 0) {
                size += BlockInfo::SIZE;
                LockGuard<SpinLock> guard (spinLock);
                BTree::Key result = freeList->Search (BTree::Key (size, 0));
                if (result.first >= size && result.second != 0) {
                    freeList->Delete (result);
                    offset = result.second;
                    if (result.first > size) {
                        ui64 remainder = result.first - size;
                        if (remainder > BlockInfo::SIZE) {
                            PtrType nextOffset = offset + size;
                            freeList->Add (BTree::Key (remainder, nextOffset));
                            BlockInfo block (file, nextOffset, BlockInfo::FLAGS_FREE, remainder);
                            block.Write ();
                        }
                    }
                }
                else {
                    offset = (PtrType)file.GetSize ();
                    file.SetSize (offset + size);
                }
                BlockInfo block (file, offset, 0, size);
                block.Write ();
                offset += BlockInfo::Header::SIZE;
            }
            return offset;
        }

        void FileAllocator::Free (
                PtrType offset,
                std::size_t size) {
            if (IsFixed ()) {
                FreeFixedBlock (offset);
            }
            else if (offset != 0 && size > 0) {
                LockGuard<SpinLock> guard (spinLock);
                offset -= BlockInfo::Header::SIZE;
                if (header.rootBlockOffset == offset) {
                    header.rootBlockOffset = 0;
                    Save ();
                }
                BlockInfo block (file, offset);
                block.Read ();
                if (!block.IsFree () && !block.IsFixed ()) {
                    if (!block.IsLast ()) {
                        BlockInfo::SharedPtr prev = block.Prev ();
                        if (prev->IsFree () && !prev->IsFixed ()) {
                            freeList->Delete (BTree::Key (prev->header.size, prev->offset));
                            block.offset = block.offset - prev->header.size;
                            block.header.size += prev->header.size;
                        }
                        BlockInfo::SharedPtr next = block.Next ();
                        if (next->IsFree () && !next->IsFixed ()) {
                            freeList->Delete (BTree::Key (next->header.size, next->offset));
                            block.header.size += next->header.size;
                        }
                    }
                    if (!block.IsLast ()) {
                        freeList->Add (BTree::Key (block.header.size, block.offset));
                        block.SetIsFree (true);
                        block.Write ();
                    }
                    else {
                        file.SetSize (block.offset);
                    }
                }
            }
        }

        std::size_t FileAllocator::Read (
                PtrType offset,
                void *data,
                std::size_t length) {
            LockGuard<SpinLock> guard (spinLock);
            file.Seek (offset, SEEK_SET);
            return file.Read (data, length);
        }

        std::size_t FileAllocator::Write (
                PtrType offset,
                const void *data,
                std::size_t length) {
            LockGuard<SpinLock> guard (spinLock);
            file.Seek (offset, SEEK_SET);
            return file.Write (data, length);
        }

        FileAllocator::BlockData::SharedPtr FileAllocator::CreateBlockData (
                PtrType offset,
                std::size_t size,
                bool read) {
            BlockData::SharedPtr blockData (
                new BlockData (
                    *this,
                    offset,
                    GetFileEndianness (),
                    size > 0 ? size : header.blockSize,
                    0,
                    0,
                    blockAllocator));
            if (read) {
                blockData->Read ();
            }
            return blockData;
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

        void FileAllocator::Save () {
            file.Seek (0, SEEK_SET);
            file << MAGIC32 << header;
        }

        FileAllocator::PtrType FileAllocator::AllocFixedBlock () {
            PtrType offset = 0;
            if (header.headFreeFixedBlockOffset != 0) {
                offset = header.headFreeFixedBlockOffset;
                BlockInfo block (file, offset);
                block.Read ();
                if (block.IsFree () && block.IsFixed ()) {
                    header.headFreeFixedBlockOffset = block.header.nextOffset;
                    Save ();
                    block.SetIsFree (false);
                    block.Write ();
                }
            }
            else {
                offset = (PtrType)file.GetSize ();
                file.SetSize (offset + header.blockSize);
                BlockInfo block (file, offset, BlockInfo::FLAGS_FIXED, header.blockSize);
                block.Write ();
            }
            offset += BlockInfo::Header::SIZE;
            return offset;
        }

        void FileAllocator::FreeFixedBlock (PtrType offset) {
            offset -= BlockInfo::Header::SIZE;
            if (header.rootBlockOffset == offset) {
                header.rootBlockOffset = 0;
                Save ();
            }
            BlockInfo block (file, offset);
            block.Read ();
            if (!block.IsFree () && block.IsFixed ()) {
                if (!block.IsLast ()) {
                    block.header.nextOffset = header.headFreeFixedBlockOffset;
                    header.headFreeFixedBlockOffset = offset;
                    Save ();
                }
                else {
                    file.SetSize (block.offset);
                }
            }
        }

        Serializer &operator << (
                Serializer &serializer,
                const FileAllocator::Header &header) {
            serializer <<
                header.flags <<
                header.blockSize <<
                header.headFreeFixedBlockOffset <<
                header.freeListOffset <<
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
                header.freeListOffset >>
                header.rootBlockOffset;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
