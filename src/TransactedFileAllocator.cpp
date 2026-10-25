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

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFile::Allocator::Header &header) {
            serializer >> header.flags >> header.registryOffset;
            return serializer;
        }

        void TransactedFile::Allocator::Read (Serializer &serializer) {
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
            serializer << header.flags << header.registryOffset;
            return serializer;
        }

        void TransactedFile::Allocator::Write (Serializer &serializer) {
            serializer << MAGIC32 << header;
        }

    } // namespace util
} // namespace thekogans
