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

        TransactedFile::Range::Range (
                TransactedFile &file_,
                ui64 offset_,
                std::size_t length_,
                bool reading_,
                util::Allocator::SharedPtr allocator_) :
                RandomSeekSerializer (file_.endianness),
                file (file_),
                offset (offset_),
                length (length_),
                reading (reading_),
                allocator (allocator_),
                data (nullptr),
                position (0),
                owner (false) {
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
            if (reading) {
                ++file.stats.readingRanges;
            }
            else {
                ++file.stats.writingRanges;
            }
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
            ui64 pageOffset = offset & (file.GetPageSize () - 1);
            ////////////////////////////////////////////////////////////////////
            // The two designs considered for range were:
            // 1. Have range implement something simmilar to TransactedFile::ReadEx
            // and TransactedFile::WriteEx where it would check the page boundary
            // with every read and write or,
            // 2. Have range assume contiguity and eschew all bounds checking. And
            // if it happens to fall on a page boudary, allocate a backing buffer
            // to guarantee that assumption (with all of its inherent performance
            // penalties of alloc/[read|write]/free).
            // I chose to go with the approach #2 for the following reasons;
            // 1. Performance. The massive performace boost we get by removing
            // all obstacles from the critical path is impressive. By having
            // the critical path (read/write) move bits without constant checking
            // saves a lot of needless cycles.
            // 2. Use patterns and tunability. Range is specifically designed
            // to work with PageMap::Page and it's size. That size is parameterized
            // by bitsPerPage ctor value. You therefore have a lot of power to tune
            // the underlying PageMap to minimize boudary crossings. At the same
            // time range is designed to work with TransactedFile::Allocator::Block
            // (BlockRange). That means that most of range parameters will come from
            // block offset and size. And to that end...
            // 3. ...TransactedFileBTreeAllocator. TransactedFileBTreeAllocator
            // bends over backward to try and reduce the number of page boundary
            // crossing blocks it allocates.
            // All these things considered I made the calculated decision that page
            // boundary crossings will be so rare as to be negligible. And to devote
            // critical path code to deal with it would be the wrong way to go.
            ////////////////////////////////////////////////////////////////////
            // Check to see if the range straddles a page boundary...
            if (length > file.GetPageSize () - pageOffset) {
                // ... it does. Allocate a backing buffer.
                data = (ui8 *)allocator->Alloc (length);
                owner = true;
                if (reading) {
                #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
                    ++file.stats.ownerReadingRanges;
                #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
                    // and, if it's a read request, buffer the underlying file range.
                    file.ReadEx (offset, data, length);
                }
            #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
                else {
                    ++file.stats.ownerWritingRanges;
                }
            #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
            }
            else {
                // ...otherwise, read/wright directly into/from the page.
                page = file.GetPage (offset);
                data = page->data + pageOffset;
            }
        }

        TransactedFile::Range::~Range () {
            if (!reading && position > 0) {
                if (owner) {
                    file.WriteEx (offset, data, position);
                }
                else {
                    page->dirty = true;
                }
            }
            if (owner) {
                allocator->Free (data, length);
            }
        }

        std::size_t TransactedFile::Range::Read (
               void *buffer,
               std::size_t count) {
            std::memcpy (buffer, data + position, count);
            position += count;
            return count;
        }

        std::size_t TransactedFile::Range::Write (
                const void *buffer,
                std::size_t count) {
            std::memcpy (data + position, buffer, count);
            position += count;
            return count;
        }

        i64 TransactedFile::Range::Seek (
                i64 offset,
                i32 fromWhere) {
            switch (fromWhere) {
                case SEEK_SET:
                    position = offset;
                    break;
                case SEEK_CUR:
                    position += offset;
                    break;
                case SEEK_END:
                    position = (i64)length + offset;
                    break;
            }
            return position;
        }

        std::size_t TransactedFile::SafeRange::Read (
                void *buffer,
                std::size_t count) {
            if (buffer != nullptr) {
                std::size_t available = GetDataAvailable ();
                if (count > available) {
                    count = available;
                }
                return Range::Read (buffer, count);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        std::size_t TransactedFile::SafeRange::Write (
                const void *buffer,
                std::size_t count) {
            if (buffer != nullptr) {
                std::size_t available = GetDataAvailable ();
                if (count > available) {
                    count = available;
                }
                return Range::Write (buffer, count);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        i64 TransactedFile::SafeRange::Seek (
                i64 offset,
                i32 fromWhere) {
            switch (fromWhere) {
                case SEEK_SET:
                    if (offset < 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW);
                    }
                    position = offset;
                    break;
                case SEEK_CUR:
                    if (position + offset < 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW);
                    }
                    position += offset;
                    break;
                case SEEK_END:
                    if ((i64)length + offset < 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW);
                    }
                    position = (i64)length + offset;
                    break;
                default:
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
            return position;
        }

    } // namespace util
} // namespace thekogans
