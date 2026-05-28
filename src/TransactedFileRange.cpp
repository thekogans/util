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
                buffer (nullptr),
                owner (false) {
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
            if (reading) {
                ++file.stats.readOnlyRanges;
            }
            else {
                ++file.stats.writeOnlyRanges;
            }
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
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
                if (reading) {
                #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
                    ++file.stats.readOnlyOwnerRanges;
                #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
                    file.ReadEx (offset, data, length);
                }
            #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
                else {
                    ++file.stats.writeOnlyOwnerRanges;
                }
            #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
            }
            else {
                data = buffer->data + bufferOffset;
            }
        }

        TransactedFile::Range::~Range () {
            if (!reading && position > 0) {
                if (owner) {
                    file.WriteEx (offset, data, position);
                }
                else {
                    LockGuard<SpinLock> guard (file.spinLock);
                    buffer->dirty = true;
                    std::size_t bufferLength = offset - buffer->offset + position;
                    if (buffer->length < bufferLength) {
                        buffer->length = bufferLength;
                        // Only last block can be < SIZE. Therefore, if
                        // it's size changes, file size changed too.
                        // SetSize will call SetDirty (true).
                        file.size = offset + position;
                    }
                    file.SetDirty (true);
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
