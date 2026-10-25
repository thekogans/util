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
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        void TransactedFile::Block::Header::Read (
                TransactedFile::SharedPtr file,
                AddressSpaceType::AddressType offset) {
            Range range (file, offset, SIZE);
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            ui32 magic;
            range >> magic;
            if (magic == MAGIC32) {
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                range >> flags >> size;
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFile::Block::Header @"
                    THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
        }

        void TransactedFile::Block::Header::Write (
                TransactedFile::SharedPtr file,
                AddressSpaceType::AddressType offset) const {
            Range range (file, offset, SIZE, false);
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            range << MAGIC32;
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            range << flags << size;
        }

        inline bool operator != (
                const TransactedFile::Block::Header &header,
                const TransactedFile::Block::Header &footer) {
            return header.flags != footer.flags || header.size != footer.size;
        }

        ui64 TransactedFile::Block::GetSize (
                TransactedFile::SharedPtr file,
                AddressSpaceType::AddressType offset) {
            Block block (file, offset);
            block.Read ();
            return block.GetSize ();
        }

        bool TransactedFile::Block::Prev (Block &prev) const {
            if (!IsFirst ()) {
                Header footer;
                footer.Read (file, offset - SIZE);
                prev.offset = offset - SIZE - footer.size;
                prev.header.Read (file, prev.offset - HEADER_SIZE);
                if (prev.header != footer) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt TransactedFile::Block @" THEKOGANS_UTIL_UI64_FORMAT "\n"
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

        bool TransactedFile::Block::Next (Block &next) const {
            if (!IsLast ()) {
                next.offset = offset + header.size + SIZE;
                next.Read ();
                return true;
            }
            return false;
        }

        void TransactedFile::Block::Read () {
            header.Read (file, offset - HEADER_SIZE);
            Header footer;
            footer.Read (file, offset + header.size);
            if (header != footer) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFile::Block @" THEKOGANS_UTIL_UI64_FORMAT "\n"
                    " header.flags: %u header.size: " THEKOGANS_UTIL_UI64_FORMAT "\n"
                    " footer.flags: %u footer.size: " THEKOGANS_UTIL_UI64_FORMAT,
                    offset,
                    header.flags, header.size,
                    footer.flags, footer.size);
            }
        }

        void TransactedFile::Block::Write () const {
            header.Write (file, offset - HEADER_SIZE);
            header.Write (file, offset + header.size);
        }

        void TransactedFile::Block::Clear () const {
            {
                Range range (file, offset - HEADER_SIZE, HEADER_SIZE, false);
                range.Seek (SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable ()), SEEK_CUR);
            }
            {
                Range range (file, offset + header.size, HEADER_SIZE, false);
                range.Seek (SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable ()), SEEK_CUR);
            }
        }

    } // namespace util
} // namespace thekogans
