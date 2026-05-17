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
                util::Allocator::SharedPtr allocator_) :
                Serializer (file_.endianness),
                file (file_),
                offset (offset_),
                length (length_),
                allocator (allocator_),
                data (nullptr),
                position (0),
                buffer (nullptr),
                owner (false) {
            buffer = file.GetBuffer (offset);
            std::size_t bufferOffset = offset - buffer->offset;
            // To us it maters not how long the actual block is.
            // All we care about is that our range fits in to it's
            // range. It's up to the down stream ReadOnlyRange and
            // WriteOnlyRange (below) to do the checking and
            // perform appropriate actions.
            if (length > Buffer::SIZE - bufferOffset) {
                data = (ui8 *)allocator->Alloc (length);
                owner = true;
            }
            else {
                data = buffer->data + bufferOffset;
            }
        }

        TransactedFile::Range::~Range () {
            if (owner) {
                allocator->Free (data, length);
            }
        }

        std::size_t TransactedFile::Range::Advance (std::size_t advance) {
            std::size_t available = GetDataAvailable ();
            if (advance > available) {
                advance = available;
            }
            position += advance;
            return advance;
        }

        std::size_t TransactedFile::Range::Read (
               void *buffer,
               std::size_t count) {
            THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("WriteOnlyRange can't read.");
        }

        std::size_t TransactedFile::Range::Write (
                const void *buffer,
                std::size_t count) {
            THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("ReadOnlyRange can't write.");
        }

        TransactedFile::UnsafeReadOnlyRange::UnsafeReadOnlyRange (
                TransactedFile &file,
                ui64 offset,
                std::size_t length,
                util::Allocator::SharedPtr allocator) :
                Range (file, offset, length, allocator) {
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
            ++file.stats.readOnlyRanges;
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
            ui64 available = file.GetSize () - offset;
            if (length > available) {
                length = available;
            }
            if (owner) {
            #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
                ++file.stats.readOnlyOwnerRanges;
            #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
                file.Seek (offset, SEEK_SET);
                file.Read (data, length);
            }
        }

        std::size_t TransactedFile::UnsafeReadOnlyRange::Read (
                void *buffer,
                std::size_t count) {
            std::memcpy (buffer, data + position, count);
            position += count;
            return count;
        }

        std::size_t TransactedFile::ReadOnlyRange::Read (
                void *buffer,
                std::size_t count) {
            if (buffer != nullptr) {
                std::size_t available = GetDataAvailable ();
                if (count > available) {
                    count = available;
                }
                return UnsafeReadOnlyRange::Read (buffer, count);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        TransactedFile::UnsafeWriteOnlyRange::UnsafeWriteOnlyRange (
                TransactedFile &file,
                ui64 offset,
                std::size_t length,
                util::Allocator::SharedPtr allocator) :
                Range (file, offset, length, allocator) {
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
            ++file.stats.writeOnlyRanges;
            if (owner) {
                ++file.stats.writeOnlyOwnerRanges;
            }
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
        }

        TransactedFile::UnsafeWriteOnlyRange::~UnsafeWriteOnlyRange () {
            if (position > 0) {
                if (owner) {
                    file.Seek (offset, SEEK_SET);
                    file.Write (data, position);
                }
                else {
                    buffer->dirty = true;
                    std::size_t bufferOffset = offset - buffer->offset;
                    std::size_t bufferLength = bufferOffset + position;
                    if (buffer->length < bufferLength) {
                        buffer->length = bufferLength;
                        // Only last block can be < SIZE. Therefore, if
                        // it's size changes, file size changed too.
                        // SetSize will call SetDirty (true).
                        file.SetSize (offset + position);
                    }
                    else {
                        file.SetDirty (true);
                    }
                }
            }
        }

        std::size_t TransactedFile::UnsafeWriteOnlyRange::Write (
                const void *buffer,
                std::size_t count) {
            std::memcpy (data + position, buffer, count);
            position += count;
            return count;
        }

        std::size_t TransactedFile::WriteOnlyRange::Write (
                const void *buffer,
                std::size_t count) {
            if (buffer != nullptr) {
                std::size_t available = GetDataAvailable ();
                if (count > available) {
                    count = available;
                }
                return UnsafeWriteOnlyRange::Write (buffer, count);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    } // namespace util
} // namespace thekogans
