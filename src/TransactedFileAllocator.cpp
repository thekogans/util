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
#if defined (THEKOGANS_UTIL_TYPE_Static)
    #include "thekogans/util/TransactedFileBTreeAllocator.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (
            thekogans::util::TransactedFile::Allocator)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void TransactedFile::Allocator::StaticInit () {
            TransactedFileBTreeAllocator::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        void TransactedFile::Allocator::Block::Header::Read (
                TransactedFile &file,
                PtrType offset) {
            UnsafeReadOnlyRange buffer (file, offset, SIZE);
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            ui32 magic;
            buffer >> magic;
            if (magic == MAGIC32) {
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                buffer >> flags >> size;
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFile::Allocator::Block::Header @"
                    THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
        }

        void TransactedFile::Allocator::Block::Header::Write (
                TransactedFile &file,
                PtrType offset) const {
            UnsafeWriteOnlyRange buffer (file, offset, SIZE);
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            buffer << MAGIC32;
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            buffer << flags << size;
        }

        inline bool operator != (
                const TransactedFile::Allocator::Block::Header &header,
                const TransactedFile::Allocator::Block::Header &footer) {
            return header.flags != footer.flags || header.size != footer.size;
        }

        ui64 TransactedFile::Allocator::Block::GetSize (
                TransactedFile &file,
                PtrType offset) {
            Block block (offset);
            block.Read (file);
            return block.GetSize ();
        }

        bool TransactedFile::Allocator::Block::Prev (
                TransactedFile &file,
                Block &prev) const {
            if (!IsFirst ()) {
                Header footer;
                footer.Read (file, offset - SIZE);
                prev.offset = offset - SIZE - footer.size;
                prev.header.Read (file, prev.offset - HEADER_SIZE);
                if (prev.header != footer) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt TransactedFile::Allocator::Block @" THEKOGANS_UTIL_UI64_FORMAT "\n"
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
                TransactedFile &file,
                Block &next) const {
            if (!IsLast (file)) {
                next.offset = offset + header.size + SIZE;
                next.Read (file);
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
            header.Write (file, offset + header.size);
        }

    #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
        void TransactedFile::Allocator::Block::Invalidate (TransactedFile &file) const {
            UnsafeWriteOnlyRange buffer (file, offset - HEADER_SIZE, UI32_SIZE);
            // Simply stepping on magic will invalidate
            // this block for all future reads.
            buffer << (ui32)0;
        }
    #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFile::Allocator::Header &header) {
            serializer >>
                header.flags >>
                header.registryOffset;
            return serializer;
        }

        void TransactedFile::Allocator::Read (
                const SerializableHeader &header_,
                Serializer &serializer) {
            ui32 magic;
            serializer >> magic;
            if (magic == MAGIC32) {
                serializer >> header;
            #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                if (!header.IsBlockUsesMagic ()) {
            #else // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                if (header.IsBlockUsesMagic ()) {
            #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "This TransactedFile::Allocator file cannot be opened by this version of %s.",
                        THEKOGANS_UTIL);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFile::Allocator file.");
            }
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const TransactedFile::Allocator::Header &header) {
            serializer <<
                header.flags <<
                header.registryOffset;
            return serializer;
        }

        void TransactedFile::Allocator::Write (Serializer &serializer) const {
            serializer << MAGIC32 << header;
        }

    } // namespace util
} // namespace thekogans
