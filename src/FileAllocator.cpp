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
            struct BlockInfo {
                File &file;
                ui64 offset;
                enum {
                    FLAGS_FREE = 1,
                    FLAGS_FIXED = 2
                };
                struct Header {
                    ui32 flags;
                    ui64 size;
                    ui64 nextOffset;

                    enum {
                        SIZE =
                            UI32_SIZE +
                            UI32_SIZE +
                            UI64_SIZE +
                            UI64_SIZE
                    };

                    Header (
                        ui32 flags_ = 0,
                        ui64 size_ = 0,
                        ui64 nextOffset_ = 0) :
                        flags (flags_),
                        size (size_),
                        nextOffset (nextOffset_) {}

                    void Read (
                        File &file,
                        ui64 offset);
                    void Write (
                        File &file,
                        ui64 offset);
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
                        ui64 offset);
                    void Write (
                        File &file,
                        ui64 offset);
                } footer;

                enum {
                    SIZE = Header::SIZE + Footer::SIZE
                };

                BlockInfo (
                    File &file_,
                    ui64 offset_ = 0,
                    ui32 flags = 0,
                    ui64 size = 0,
                    ui64 nextOffset = 0) :
                    file (file_),
                    offset (offset_),
                    header (flags, size, nextOffset),
                    footer (flags, size) {}

                inline ui64 GetOffset () const {
                    return offset;
                }
                inline void SetOffset (ui64 offset_) {
                    offset = offset_;
                }
                inline bool IsFirst () const {
                    return offset == Header::SIZE;
                }
                inline bool IsLast () const {
                    return offset + GetSize () == file.GetSize ();
                }

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

                inline ui64 GetSize () const {
                    return header.size;
                }
                inline void SetSize (ui64 size) {
                    header.size = size;
                    footer.size = size;
                }

                inline ui64 GetNextOffset () const {
                    return header.nextOffset;
                }
                inline void SetNextOffset (ui64 nextOffset) {
                    header.nextOffset = nextOffset;
                }

                BlockInfo Prev ();
                BlockInfo Next ();

                void Read ();
                void Write ();

                friend bool operator != (
                    const Header &header,
                    const Footer &footer);
            };

            void BlockInfo::Header::Read (
                    File &file,
                    ui64 offset) {
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
                    ui64 offset) {
                file.Seek (offset, SEEK_SET);
                file << MAGIC32 << flags << size << nextOffset;
            }

            void BlockInfo::Footer::Read (
                    File &file,
                    ui64 offset) {
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
                    ui64 offset) {
                file.Seek (offset, SEEK_SET);
                file << MAGIC32 << flags << size;
            }

            inline bool operator != (
                    const BlockInfo::Header &header,
                    const BlockInfo::Footer &footer) {
                return header.flags != footer.flags || header.size != footer.size;
            }

            BlockInfo BlockInfo::Prev () {
                BlockInfo prev (file);
                prev.footer.Read (file, offset - Footer::SIZE);
                prev.offset = offset - prev.footer.size;
                prev.header.Read (file, prev.offset);
                if (prev.header != prev.footer) {
                    // FIXME: throw
                    assert (0);
                }
                return prev;
            }

            BlockInfo BlockInfo::Next () {
                BlockInfo next (file, offset + header.size);
                next.Read ();
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
        }

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (FileAllocator::BlockBuffer)

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
                            allocator) :
                        allocator),
                header (
                    blockSize > 0 ? Header::FLAGS_FIXED : 0,
                    blockSize > 0 ?
                        (ui32)blockSize :
                    (ui32)BTree::Node::FileSize (blocksPerPage)),
                minUserBlockSize (16),
                minUserBlockOffset (Header::SIZE + BlockInfo::Header::SIZE) {
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
                if (header.blockSize != blockSize) {
                    blockAllocator = header.blockSize > 0 ?
                        BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                            header.blockSize,
                            blocksPerPage,
                            allocator) :
                        allocator;
                }
            }
            else {
                Save ();
            }
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
            return header.rootOffset;
        }

        void FileAllocator::SetRootOffset (ui64 rootOffset) {
            LockGuard<SpinLock> guard (spinLock);
            header.rootOffset = rootOffset;
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
                    if (size < minUserBlockSize) {
                        size = minUserBlockSize;
                    }
                    size += BlockInfo::SIZE;
                    LockedFilePtr file (*this);
                    BTree::Key result = btree->Search (BTree::Key (size, 0));
                    if (result.second != 0) {
                        assert (result.first >= size);
                        btree->Delete (result);
                        offset = result.second;
                        result.first -= size;
                        if (result.first > BlockInfo::SIZE + minUserBlockSize) {
                            btree->Add (BTree::Key (result.first, offset + size));
                            BlockInfo block (
                                *file,
                                offset + size,
                                BlockInfo::FLAGS_FREE,
                                result.first);
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
                    offset += BlockInfo::Header::SIZE;
                }
                return offset;
            }
        }

        void FileAllocator::Free (ui64 offset) {
            if (IsFixed ()) {
                LockGuard<SpinLock> guard (spinLock);
                FreeFixedBlock (offset);
            }
            else if (offset >= minUserBlockOffset) {
                offset -= BlockInfo::Header::SIZE;
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
                        BlockInfo prev = block.Prev ();
                        if (prev.IsFree () && !prev.IsFixed ()) {
                            btree->Delete (BTree::Key (prev.GetSize (), prev.GetOffset ()));
                            block.SetOffset (block.GetOffset () - prev.GetSize ());
                            block.SetSize (block.GetSize () + prev.GetSize ());
                        }
                    }
                    if (!block.IsLast ()) {
                        BlockInfo next = block.Next ();
                        if (next.IsFree () && !next.IsFixed ()) {
                            btree->Delete (BTree::Key (next.GetSize (), next.GetOffset ()));
                            block.SetSize (block.GetSize () + next.GetSize ());
                        }
                    }
                    if (!block.IsLast ()) {
                        btree->Add (BTree::Key (block.GetSize (), block.GetOffset ()));
                        block.SetIsFree (true);
                        block.Write ();
                    }
                    else {
                        file->SetSize (block.GetOffset ());
                    }
                }
            }
        }

        FileAllocator::BlockBuffer::SharedPtr FileAllocator::CreateBlockBuffer (
                ui64 offset,
                std::size_t size,
                bool read,
                std::size_t offset_) {
            BlockBuffer::SharedPtr buffer;
            if (offset >= minUserBlockOffset) {
                if (size == 0) {
                    LockedFilePtr file (*this);
                    BlockInfo block (*file, offset - BlockInfo::Header::SIZE);
                    block.Read ();
                    size = block.GetSize () - BlockInfo::SIZE;
                }
                buffer.Reset (
                    new BlockBuffer (
                        *this,
                        offset,
                        GetFileEndianness (),
                        size,
                        0,
                        0,
                        blockAllocator));
                if (read) {
                    buffer->Read (offset_);
                }
            }
            return buffer;
        }

        std::size_t FileAllocator::GetBlockSize (ui64 offset) {
            if (offset >= minUserBlockOffset) {
                offset -= BlockInfo::Header::SIZE;
                LockedFilePtr file (*this);
                BlockInfo block (*file, offset);
                block.Read ();
                return block.GetSize () - BlockInfo::SIZE;
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
                    block.SetIsFree (false);
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
            offset += BlockInfo::Header::SIZE;
            return offset;
        }

        void FileAllocator::FreeFixedBlock (ui64 offset) {
            if (offset >= minUserBlockOffset) {
                offset -= BlockInfo::Header::SIZE;
                if (header.rootOffset == offset) {
                    header.rootOffset = 0;
                    Save ();
                }
                BlockInfo block (file, offset);
                block.Read ();
                if (!block.IsFree () && block.IsFixed ()) {
                    if (!block.IsLast ()) {
                        block.SetIsFree (true);
                        block.SetNextOffset (header.fixedFreeListHeadOffset);
                        block.Write ();
                        header.fixedFreeListHeadOffset = offset;
                        Save ();
                    }
                    else {
                        // FIXME: see if we can consolidate and
                        // remove previous free blocks.
                        file.SetSize (block.GetOffset ());
                    }
                }
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
