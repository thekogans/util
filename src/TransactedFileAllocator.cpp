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
#include "thekogans/util/File.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        void TransactedFile::Allocator::Block::Header::Read (
                TransactedFile &file,
                PtrType offset) {
            file.Seek (offset, SEEK_SET);
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            ui32 magic;
            file >> magic;
            if (magic == MAGIC32) {
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                file >> flags >> size;
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFileAllocator::Block::Header @"
                    THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
        }

        void TransactedFile::Allocator::Block::Header::Write (
                TransactedFile &file,
                PtrType offset) const {
            file.Seek (offset, SEEK_SET);
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            file << MAGIC32;
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            file << flags << size;
        }

        inline bool operator != (
                const TransactedFile::Allocator::Block::Header &header,
                const TransactedFile::Allocator::Block::Header &footer) {
            return header.flags != footer.flags || header.size != footer.size;
        }


        bool TransactedFileAllocator::Block::Prev (
                Allocator &allocator,
                Block &prev) const {
            if (!IsFirst (allocator)) {
                Header footer;
                footer.Read (*allocator.GetFile (), offset - SIZE);
                prev.offset = offset - SIZE - footer.size;
                prev.header.Read (*allocator.GetFile (), prev.offset - HEADER_SIZE);
                if (prev.header != footer) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt TransactedFileAllocator::Block @" THEKOGANS_UTIL_UI64_FORMAT "\n"
                        " prev.header.flags: %u prev.header.size: " THEKOGANS_UTIL_UI64_FORMAT "\n"
                        " prev.footer.flags: %u prev.footer.size: " THEKOGANS_UTIL_UI64_FORMAT,
                        prev.offset,
                        prev.header.flags, prev.header.size,
                        footer.flags, footer.size);
                }
                return true;
            }
            return false;
        }

        bool TransactedFile::Allocator::Block::Next (
                Allocator &allocator,
                Block &next) const {
            if (!IsLast (allocator)) {
                next.offset = offset + header.size + SIZE;
                next.Read (*allocator.GetFile ());
                return true;
            }
            return false;
        }

        void TransactedFile::Allocator::Block::Read (TransactedFile &file) {
            header.Read (file, offset - HEADER_SIZE);
            Header footer;
            footer.Read (file, offset + header.size);
            if (header != footer) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFile::Allocator::Block @" THEKOGANS_UTIL_UI64_FORMAT "\n"
                    " header.flags: %u header.size: " THEKOGANS_UTIL_UI64_FORMAT "\n"
                    " footer.flags: %u footer.size: " THEKOGANS_UTIL_UI64_FORMAT,
                    offset,
                    header.flags, header.size,
                    footer.flags, footer.size);
            }
        }

        void TransactedFile::Allocator::Block::Write (TransactedFile &file) const {
            header.Write (file, offset - HEADER_SIZE);
            if (header.IsFree () && header.IsBTreeNode ()) {
                file << nextBTreeNodeOffset;
            }
            header.Write (file, offset + header.size);
        }

    #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
        void TransactedFile::Allocator::Block::Invalidate (TransactedFile &file) const {
            file.Seek (offset - HEADER_SIZE, SEEK_SET);
            // Simply stepping on magic will invalidate
            // this block for all future reads.
            file << (ui32)0;
        }
    #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)

    } // namespace util
} // namespace thekogans
