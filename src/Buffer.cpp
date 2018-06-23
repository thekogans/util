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

#include <cassert>
#include <cstring>
#include <algorithm>
#if defined (THEKOGANS_UTIL_HAVE_ZLIB)
    #include <zlib.h>
#endif // defined (THEKOGANS_UTIL_HAVE_ZLIB)
#include "thekogans/util/Exception.h"
#include "thekogans/util/Buffer.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (Buffer, SpinLock)

        Buffer::Buffer (
            Endianness endianness,
            ui32 length_,
            ui32 readOffset_,
            ui32 writeOffset_,
            Allocator *allocator_) :
            Serializer (endianness),
            data ((ui8 *)allocator_->Alloc (length_)),
            length (length_),
            readOffset (readOffset_),
            writeOffset (writeOffset_),
            allocator (allocator_) {}

        Buffer::Buffer (
                Endianness endianness,
                const ui8 *begin,
                const ui8 *end,
                ui32 readOffset_,
                ui32 writeOffset_,
                Allocator *allocator_) :
                Serializer (endianness),
                data ((ui8 *)allocator_->Alloc (end - begin)),
                length ((ui32)(end - begin)),
                readOffset (readOffset_),
                writeOffset (writeOffset_ == UI32_MAX ? length : writeOffset_),
                allocator (allocator_) {
            if (length > 0) {
                memcpy (data, begin, length);
            }
        }

        ui32 Buffer::Read (
                void *buffer,
                ui32 count) {
            if (buffer != 0 && count > 0) {
                ui32 availableForReading = GetDataAvailableForReading ();
                if (count > availableForReading) {
                    count = availableForReading;
                }
                if (count != 0) {
                    memcpy (buffer, GetReadPtr (), count);
                    AdvanceReadOffset (count);
                }
                return count;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        ui32 Buffer::Write (
                const void *buffer,
                ui32 count) {
            if (buffer != 0 && count > 0) {
                ui32 availableForWriting = GetDataAvailableForWriting ();
                if (count > availableForWriting) {
                    count = availableForWriting;
                }
                if (count != 0) {
                    memcpy (GetWritePtr (), buffer, count);
                    AdvanceWriteOffset (count);
                }
                return count;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Buffer &Buffer::operator += (const Buffer &buffer) {
            if (GetDataAvailableForWriting () < buffer.GetDataAvailableForReading ()) {
                Resize (GetDataAvailableForReading () + buffer.GetDataAvailableForReading ());
            }
            Write (buffer.GetReadPtr (), buffer.GetDataAvailableForReading ());
            return *this;
        }

        void Buffer::Resize (
                ui32 length_,
                Allocator *allocator_) {
            if (length != length_) {
                if (length_ > 0) {
                    ui8 *data_ = (ui8 *)allocator_->Alloc (length_);
                    if (data != 0) {
                        memcpy (data_, data, std::min (length_, length));
                        allocator->Free (data, length);
                    }
                    data = data_;
                    length = length_;
                    if (readOffset > length) {
                        readOffset = length;
                    }
                    if (writeOffset > length) {
                        writeOffset = length;
                    }
                }
                else {
                    allocator->Free (data, length);
                    data = 0;
                    length = 0;
                    readOffset = 0;
                    writeOffset = 0;
                }
                allocator = allocator_;
            }
        }

        Buffer::UniquePtr Buffer::Clone (Allocator *allocator) const {
            if (allocator != 0) {
                return UniquePtr (
                    new Buffer (
                        endianness,
                        data,
                        data + length,
                        readOffset,
                        writeOffset,
                        allocator));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Buffer::UniquePtr Buffer::Subset (
                ui32 offset,
                ui32 count,
                Allocator *allocator) const {
            if (offset < length && count > 0 && allocator != 0) {
                if (offset + count > length) {
                    count = length - offset;
                }
                return UniquePtr (
                    new Buffer (
                        endianness,
                        data + offset,
                        data + offset + count,
                        0,
                        UI32_MAX,
                        allocator));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        ui32 Buffer::AdvanceReadOffset (ui32 advance) {
            if (advance > 0) {
                ui32 availableForReading = GetDataAvailableForReading ();
                if (advance > availableForReading) {
                    advance = availableForReading;
                }
                readOffset += advance;
            }
            return advance;
        }

        ui32 Buffer::AdvanceWriteOffset (ui32 advance) {
            if (advance > 0) {
                ui32 availableForWriting = GetDataAvailableForWriting ();
                if (advance > availableForWriting) {
                    advance = availableForWriting;
                }
                writeOffset += advance;
            }
            return advance;
        }

    #if defined (THEKOGANS_UTIL_HAVE_ZLIB)
        namespace {
            const ui32 BUFFER_SIZE = 16384;

            struct OutBuffer {
                Allocator *allocator;
                ui8 *data;
                ui32 length;

                explicit OutBuffer (Allocator *allocator_) :
                    allocator (allocator_),
                    data (0),
                    length (0) {}

                void Write (
                        const ui8 *data_,
                        ui32 length_) {
                    if (data_ != 0 && length_ > 0) {
                        ui8 *newData = (ui8 *)allocator->Alloc (length + length_);
                        memcpy (newData, data, length);
                        memcpy (newData + length, data_, length_);
                        allocator->Free (data, length);
                        data = newData;
                        length += length_;
                    }
                }
            };

            void DeflateHelper (
                    const ui8 *data,
                    ui32 length,
                    OutBuffer &outBuffer) {
                ui8 tmpOutBuffer[BUFFER_SIZE];
                z_stream zStream;
                zStream.zalloc = 0;
                zStream.zfree = 0;
                zStream.opaque = 0;
                zStream.next_in = (Bytef *)data;
                zStream.avail_in = (uInt)length;
                zStream.next_out = tmpOutBuffer;
                zStream.avail_out = BUFFER_SIZE;
                deflateInit (&zStream, Z_BEST_COMPRESSION);
                while (zStream.avail_in != 0) {
                    int result = deflate (&zStream, Z_NO_FLUSH);
                    if (result != Z_OK) {
                        if (zStream.msg != 0) {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "%s", zStream.msg);
                        }
                        else {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "%d", result);
                        }
                    }
                    if (zStream.avail_out == 0) {
                        outBuffer.Write (tmpOutBuffer, BUFFER_SIZE);
                        zStream.next_out = tmpOutBuffer;
                        zStream.avail_out = BUFFER_SIZE;
                    }
                }
                int result = Z_OK;
                while (result == Z_OK) {
                    if (zStream.avail_out == 0) {
                        outBuffer.Write (tmpOutBuffer, BUFFER_SIZE);
                        zStream.next_out = tmpOutBuffer;
                        zStream.avail_out = BUFFER_SIZE;
                    }
                    result = deflate (&zStream, Z_FINISH);
                }
                assert (result == Z_STREAM_END);
                outBuffer.Write (tmpOutBuffer, BUFFER_SIZE - zStream.avail_out);
                deflateEnd (&zStream);
            }

            void InflateHelper (
                    const ui8 *data,
                    ui32 length,
                    OutBuffer &outBuffer) {
                ui8 tmpOutBuffer[BUFFER_SIZE];
                z_stream zStream;
                zStream.zalloc = 0;
                zStream.zfree = 0;
                zStream.opaque = 0;
                zStream.next_in = (Bytef *)data;
                zStream.avail_in = (uInt)length;
                int result = inflateInit (&zStream);
                if (result != Z_OK) {
                    if (zStream.msg != 0) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "%s", zStream.msg);
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "%d", result);
                    }
                }
                do {
                    zStream.next_out = tmpOutBuffer;
                    zStream.avail_out = BUFFER_SIZE;
                    result = inflate (&zStream, Z_NO_FLUSH);
                    assert (result != Z_STREAM_ERROR);
                    outBuffer.Write (tmpOutBuffer, BUFFER_SIZE - zStream.avail_out);
                } while (zStream.avail_out == 0);
                result = inflateEnd (&zStream);
                assert (result == Z_OK);
            }
        }

        Buffer::UniquePtr Buffer::Deflate (Allocator *allocator) {
            if (allocator != 0) {
                UniquePtr buffer;
                if (GetDataAvailableForReading () != 0) {
                    OutBuffer outBuffer (allocator);
                    DeflateHelper (GetReadPtr (), GetDataAvailableForReading (), outBuffer);
                    buffer.reset (
                        new Buffer (
                            endianness,
                            outBuffer.data,
                            outBuffer.length,
                            0,
                            outBuffer.length,
                            allocator));
                }
                return buffer;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Buffer::UniquePtr Buffer::Inflate (Allocator *allocator) {
            if (allocator != 0) {
                UniquePtr buffer;
                if (GetDataAvailableForReading () != 0) {
                    OutBuffer outBuffer (allocator);
                    InflateHelper (GetReadPtr (), GetDataAvailableForReading (), outBuffer);
                    buffer.reset (
                        new Buffer (
                            endianness,
                            outBuffer.data,
                            outBuffer.length,
                            0,
                            outBuffer.length,
                            allocator));
                }
                return buffer;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_ZLIB)

    #if defined (TOOLCHAIN_OS_Windows)
        HGLOBAL Buffer::ToHGLOBAL (UINT flags) const {
            HGLOBAL global = 0;
            if (GetDataAvailableForReading () > 0) {
                global = GlobalAlloc (flags, GetDataAvailableForReading ());
                if (global != 0) {
                    memcpy (
                        GlobalLock (global),
                        GetReadPtr (),
                        GetDataAvailableForReading ());
                    GlobalUnlock (global);
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
            return global;
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        void SecureBuffer::Resize (
                ui32 length,
                Allocator * /*allocator*/) {
            Buffer::Resize (length, &SecureAllocator::Global);
        }

        Buffer::UniquePtr SecureBuffer::Clone (Allocator * /*allocator*/) const {
            return UniquePtr (
                new SecureBuffer (
                    endianness,
                    data,
                    data + length,
                    readOffset,
                    writeOffset));
        }

        Buffer::UniquePtr SecureBuffer::Subset (
                ui32 offset,
                ui32 count,
                Allocator * /*allocator*/) const {
            return Buffer::Subset (offset, count, &SecureAllocator::Global);
        }

    #if defined (THEKOGANS_UTIL_HAVE_ZLIB)
        Buffer::UniquePtr SecureBuffer::Deflate (Allocator * /*allocator*/) {
            UniquePtr buffer;
            if (GetDataAvailableForReading () != 0) {
                OutBuffer outBuffer (&SecureAllocator::Global);
                DeflateHelper (GetReadPtr (), GetDataAvailableForReading (), outBuffer);
                buffer.reset (
                    new SecureBuffer (
                        endianness,
                        outBuffer.data,
                        outBuffer.length,
                        0,
                        outBuffer.length));
            }
            return buffer;
        }

        Buffer::UniquePtr SecureBuffer::Inflate (Allocator * /*allocator*/) {
            UniquePtr buffer;
            if (GetDataAvailableForReading () != 0) {
                OutBuffer outBuffer (&SecureAllocator::Global);
                InflateHelper (GetReadPtr (), GetDataAvailableForReading (), outBuffer);
                buffer.reset (
                    new SecureBuffer (
                        endianness,
                        outBuffer.data,
                        outBuffer.length,
                        0,
                        outBuffer.length));
            }
            return buffer;
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_ZLIB)

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const Buffer &buffer) {
            serializer <<
                buffer.endianness <<
                buffer.length <<
                buffer.readOffset <<
                buffer.writeOffset;
            if (buffer.length != 0) {
                ui32 bytesWritten = serializer.Write (buffer.data, buffer.length);
                if (buffer.length != bytesWritten) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "serializer.Write (buffer.data, %u) == %u",
                        buffer.length,
                        bytesWritten);
                }
            }
            return serializer;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Buffer &buffer) {
            Endianness endianness;
            ui32 length;
            ui32 readOffset;
            ui32 writeOffset;
            serializer >> endianness >> length >> readOffset >> writeOffset;
            buffer.Resize (length);
            if (length > 0) {
                ui32 bytesRead = serializer.Read (buffer.data, length);
                if (length != bytesRead) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "serializer.Read (buffer.data, %u) == %u",
                        length,
                        bytesRead);
                }
            }
            buffer.endianness = endianness;
            buffer.readOffset = readOffset;
            buffer.writeOffset = writeOffset;
            return serializer;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Buffer::UniquePtr &buffer) {
            if (buffer.get () == 0) {
                // It maters not what endianness we assign to the new buffer.
                // The real endianness will be read from the serializer.
                buffer.reset (new util::Buffer (util::HostEndian));
            }
            serializer >> *buffer;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
