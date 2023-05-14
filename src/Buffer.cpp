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
    #include <vector>
    #include <zlib.h>
#endif // defined (THEKOGANS_UTIL_HAVE_ZLIB)
#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/WindowsUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/Base64.h"
#include "thekogans/util/Buffer.h"

namespace thekogans {
    namespace util {

        Buffer::Buffer (const Buffer &other) :
                Serializer (other.endianness),
                data ((ui8 *)other.allocator->Alloc (other.length)),
                length (other.length),
                readOffset (other.readOffset),
                writeOffset (other.writeOffset),
                allocator (other.allocator) {
            if (length > 0) {
                memcpy (data, other.data, length);
            }
        }

        Buffer::Buffer (
                Endianness endianness,
                const void *begin,
                const void *end,
                std::size_t readOffset_,
                std::size_t writeOffset_,
                Allocator *allocator_) :
                Serializer (endianness),
                data ((ui8 *)allocator_->Alloc ((const ui8 *)end - (const ui8 *)begin)),
                length ((const ui8 *)end - (const ui8 *)begin),
                readOffset (readOffset_),
                writeOffset (writeOffset_ == SIZE_T_MAX ? length.value : writeOffset_),
                allocator (allocator_) {
            if (length > 0) {
                memcpy (data, begin, length);
            }
        }

        Buffer &Buffer::operator = (const Buffer &other) {
            if (this != &other) {
                Resize (0);
                endianness = other.endianness;
                data = (ui8 *)other.allocator->Alloc (other.length);
                length = other.length;
                readOffset = other.readOffset;
                writeOffset = other.writeOffset;
                allocator = other.allocator;
                if (length > 0) {
                    memcpy (data, other.data, length);
                }
            }
            return *this;
        }

        Buffer &Buffer::operator = (Buffer &&other) {
            if (this != &other) {
                Buffer temp (std::move (other));
                swap (temp);
            }
            return *this;
        }

        void Buffer::swap (Buffer &other) {
            Serializer::swap (other);
            std::swap (data, other.data);
            std::swap (length, other.length);
            std::swap (readOffset, other.readOffset);
            std::swap (writeOffset, other.writeOffset);
            std::swap (allocator, other.allocator);
        }

        std::size_t Buffer::Read (
                void *buffer,
                std::size_t count) {
            if (buffer != 0 && count > 0) {
                std::size_t availableForReading = GetDataAvailableForReading ();
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

        std::size_t Buffer::Write (
                const void *buffer,
                std::size_t count) {
            if (buffer != 0 && count > 0) {
                std::size_t availableForWriting = GetDataAvailableForWriting ();
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
            if (buffer.GetDataAvailableForReading () > 0) {
                if (GetDataAvailableForWriting () < buffer.GetDataAvailableForReading ()) {
                    Resize (GetDataAvailableForReading () + buffer.GetDataAvailableForReading ());
                }
                Write (buffer.GetReadPtr (), buffer.GetDataAvailableForReading ());
            }
            return *this;
        }

        void Buffer::Clear (
                bool rewind,
                bool readOnly) {
            if (data != 0) {
                memset (data, 0, length);
            }
            if (rewind) {
                Clear (readOnly);
            }
        }

        void Buffer::Resize (
                std::size_t length_,
                Allocator *allocator_) {
            if (length != length_) {
                if (length_ > 0) {
                    ui8 *data_ = (ui8 *)allocator_->Alloc (length_);
                    if (data != 0) {
                        memcpy (data_, data, std::min (length_, (std::size_t)length.value));
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

        Buffer Buffer::Clone (Allocator *allocator) const {
            if (allocator != 0) {
                return Buffer (
                    endianness,
                    data,
                    data + length,
                    readOffset,
                    writeOffset,
                    allocator);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Buffer Buffer::Subset (
                std::size_t offset,
                std::size_t count,
                Allocator *allocator) const {
            if (offset < length && count > 0 && allocator != 0) {
                if (count == SIZE_T_MAX || offset + count > length) {
                    count = length - offset;
                }
                return Buffer (
                    endianness,
                    data + offset,
                    data + offset + count,
                    0,
                    SIZE_T_MAX,
                    allocator);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        std::size_t Buffer::Size () const {
            return
                Serializer::Size () +
                Serializer::Size (length) +
                Serializer::Size (readOffset) +
                Serializer::Size (writeOffset) +
                Serializer::Size (allocator->GetSerializedName ()) +
                length;
        }

        std::size_t Buffer::AdvanceReadOffset (std::size_t advance) {
            if (advance > 0) {
                std::size_t availableForReading = GetDataAvailableForReading ();
                if (advance > availableForReading) {
                    advance = availableForReading;
                }
                readOffset += advance;
            }
            return advance;
        }

        std::size_t Buffer::AdvanceWriteOffset (std::size_t advance) {
            if (advance > 0) {
                std::size_t availableForWriting = GetDataAvailableForWriting ();
                if (advance > availableForWriting) {
                    advance = availableForWriting;
                }
                writeOffset += advance;
            }
            return advance;
        }

    #if defined (THEKOGANS_UTIL_HAVE_ZLIB)
        namespace {
            struct OutBuffer {
                Allocator *allocator;
                ui8 *data;
                std::size_t length;

                explicit OutBuffer (Allocator *allocator_) :
                    allocator (allocator_),
                    data (0),
                    length (0) {}

                void Write (
                        const ui8 *data_,
                        std::size_t length_) {
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
                    std::size_t length,
                    OutBuffer &outBuffer) {
                std::vector<ui8> tmpOutBuffer (length);
                z_stream zStream;
                zStream.zalloc = 0;
                zStream.zfree = 0;
                zStream.opaque = 0;
                zStream.next_in = (Bytef *)data;
                zStream.avail_in = (uInt)length;
                zStream.next_out = tmpOutBuffer.data ();
                zStream.avail_out = (uInt)length;
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
                        outBuffer.Write (tmpOutBuffer.data (), length);
                        zStream.next_out = tmpOutBuffer.data ();
                        zStream.avail_out = (uInt)length;
                    }
                }
                int result = Z_OK;
                while (result == Z_OK) {
                    if (zStream.avail_out == 0) {
                        outBuffer.Write (tmpOutBuffer.data (), length);
                        zStream.next_out = tmpOutBuffer.data ();
                        zStream.avail_out = (uInt)length;
                    }
                    result = deflate (&zStream, Z_FINISH);
                }
                assert (result == Z_STREAM_END);
                outBuffer.Write (tmpOutBuffer.data (), length - zStream.avail_out);
                deflateEnd (&zStream);
            }

            void InflateHelper (
                    const ui8 *data,
                    std::size_t length,
                    OutBuffer &outBuffer) {
                std::vector<ui8> tmpOutBuffer (length * 2);
                z_stream zStream;
                zStream.zalloc = 0;
                zStream.zfree = 0;
                zStream.opaque = 0;
                zStream.next_in = (Bytef *)data;
                zStream.avail_in = (uInt)tmpOutBuffer.size ();
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
                    zStream.next_out = tmpOutBuffer.data ();
                    zStream.avail_out = (uInt)tmpOutBuffer.size ();
                    result = inflate (&zStream, Z_NO_FLUSH);
                    assert (result != Z_STREAM_ERROR);
                    outBuffer.Write (tmpOutBuffer.data (), tmpOutBuffer.size () - zStream.avail_out);
                } while (zStream.avail_out == 0);
                result = inflateEnd (&zStream);
                assert (result == Z_OK);
            }
        }

        Buffer Buffer::Deflate (Allocator *allocator) const {
            if (allocator != 0) {
                if (GetDataAvailableForReading () != 0) {
                    OutBuffer outBuffer (allocator);
                    DeflateHelper (GetReadPtr (), GetDataAvailableForReading (), outBuffer);
                    return Buffer (
                        endianness,
                        outBuffer.data,
                        outBuffer.length,
                        0,
                        outBuffer.length,
                        allocator);
                }
                return Buffer ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Buffer Buffer::Inflate (Allocator *allocator) const {
            if (allocator != 0) {
                if (GetDataAvailableForReading () != 0) {
                    OutBuffer outBuffer (allocator);
                    InflateHelper (GetReadPtr (), GetDataAvailableForReading (), outBuffer);
                    return Buffer (
                        endianness,
                        outBuffer.data,
                        outBuffer.length,
                        0,
                        outBuffer.length,
                        allocator);
                }
                return Buffer ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_ZLIB)

        Buffer Buffer::FromHexBuffer (
                Endianness endianness,
                const char *hexBuffer,
                std::size_t hexBufferLength,
                Allocator *allocator) {
            if (hexBuffer != 0 && hexBufferLength > 0 && IS_EVEN (hexBufferLength) && allocator != 0) {
                void *data = allocator->Alloc (hexBufferLength / 2);
                std::size_t length =  HexDecodeBuffer (hexBuffer, hexBufferLength, data);
                return Buffer (endianness, data, length, 0, length, allocator);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    #if defined (TOOLCHAIN_OS_Windows)
        HGLOBAL Buffer::ToHGLOBAL (UINT flags) const {
            if (GetDataAvailableForReading () > 0) {
                HGLOBALPtr global (flags, GetDataAvailableForReading ());
                if (global.Get () != 0) {
                    memcpy (
                        global,
                        GetReadPtr (),
                        GetDataAvailableForReading ());
                    return global.Release ();
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
            return 0;
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        SecureBuffer::~SecureBuffer () {
            Clear ();
        }

        SecureBuffer &SecureBuffer::operator = (const SecureBuffer &other) {
            if (this != &other) {
                Resize (0);
                endianness = other.endianness;
                data = (ui8 *)other.allocator->Alloc (other.length);
                length = other.length;
                readOffset = other.readOffset;
                writeOffset = other.writeOffset;
                allocator = other.allocator;
                if (length > 0) {
                    memcpy (data, other.data, length);
                }
            }
            return *this;
        }

        SecureBuffer &SecureBuffer::operator = (SecureBuffer &&other) {
            if (this != &other) {
                SecureBuffer temp (std::move (other));
                swap (temp);
            }
            return *this;
        }

        void SecureBuffer::Resize (
                std::size_t length,
                Allocator * /*allocator*/) {
            Buffer::Resize (length, &SecureAllocator::Instance ());
        }

        Buffer SecureBuffer::Clone (Allocator * /*allocator*/) const {
            return SecureBuffer (
                endianness,
                data,
                data + length,
                readOffset,
                writeOffset);
        }

        Buffer SecureBuffer::Subset (
                std::size_t offset,
                std::size_t count,
                Allocator * /*allocator*/) const {
            return Buffer::Subset (offset, count, &SecureAllocator::Instance ());
        }

    #if defined (THEKOGANS_UTIL_HAVE_ZLIB)
        Buffer SecureBuffer::Deflate (Allocator * /*allocator*/) const {
            if (GetDataAvailableForReading () != 0) {
                OutBuffer outBuffer (&SecureAllocator::Instance ());
                DeflateHelper (GetReadPtr (), GetDataAvailableForReading (), outBuffer);
                return SecureBuffer (
                    endianness,
                    outBuffer.data,
                    outBuffer.length,
                    0,
                    outBuffer.length);
            }
            return SecureBuffer ();
        }

        Buffer SecureBuffer::Inflate (Allocator * /*allocator*/) const {
            if (GetDataAvailableForReading () != 0) {
                OutBuffer outBuffer (&SecureAllocator::Instance ());
                InflateHelper (GetReadPtr (), GetDataAvailableForReading (), outBuffer);
                return SecureBuffer (
                    endianness,
                    outBuffer.data,
                    outBuffer.length,
                    0,
                    outBuffer.length);
            }
            return SecureBuffer ();
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_ZLIB)

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const Buffer &buffer) {
            serializer <<
                buffer.endianness <<
                buffer.length <<
                buffer.readOffset <<
                buffer.writeOffset <<
                buffer.allocator->GetSerializedName ();
            if (buffer.length > 0) {
                std::size_t bytesWritten = serializer.Write (buffer.data, buffer.length);
                if (buffer.length != bytesWritten) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "serializer.Write (buffer.data, " THEKOGANS_UTIL_SIZE_T_FORMAT ") == " THEKOGANS_UTIL_SIZE_T_FORMAT,
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
            SizeT length;
            SizeT readOffset;
            SizeT writeOffset;
            std::string allocatorName;
            serializer >> endianness >> length >> readOffset >> writeOffset >> allocatorName;
            Allocator *allocator = Allocator::Get (allocatorName);
            if (allocator == 0) {
                allocator = &DefaultAllocator::Instance ();
            }
            buffer.Resize (length, allocator);
            if (length > 0) {
                std::size_t bytesRead = serializer.Read (buffer.data, length);
                if (length != bytesRead) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "serializer.Read (buffer.data, " THEKOGANS_UTIL_SIZE_T_FORMAT ") == " THEKOGANS_UTIL_SIZE_T_FORMAT,
                        length,
                        bytesRead);
                }
            }
            buffer.endianness = endianness;
            buffer.readOffset = readOffset;
            buffer.writeOffset = writeOffset;
            return serializer;
        }

        namespace {
            const char *ATTR_ENDIANESS = "Endianness";
            const char *ATTR_LENGTH = "Length";
            const char *ATTR_READ_OFFSET = "ReadOffset";
            const char *ATTR_WRITE_OFFSET = "WriteOffset";
            const char *ATTR_ALLOCATOR = "Allocator";
        }

        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
                pugi::xml_node &node,
                const Buffer &buffer) {
            node.append_attribute (ATTR_ENDIANESS).set_value (EndiannessToString (buffer.endianness).c_str ());
            node.append_attribute (ATTR_LENGTH).set_value (util::ui64Tostring (buffer.length).c_str ());
            node.append_attribute (ATTR_READ_OFFSET).set_value (util::ui64Tostring (buffer.readOffset).c_str ());
            node.append_attribute (ATTR_WRITE_OFFSET).set_value (util::ui64Tostring (buffer.writeOffset).c_str ());
            node.append_attribute (ATTR_ALLOCATOR).set_value (
                EncodeXMLCharEntities (buffer.allocator->GetSerializedName ()).c_str ());
            if (buffer.length > 0) {
                node.text ().set (Base64::Encode (buffer.data, buffer.length, 64).Tostring ().c_str ());
            }
            return node;
        }

        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
                pugi::xml_node &node,
                Buffer &buffer) {
            Endianness endianness = StringToEndianness (node.attribute (ATTR_ENDIANESS).value ());
            SizeT length = util::stringToui64 (node.attribute (ATTR_LENGTH).value ());
            SizeT readOffset = util::stringToui64 (node.attribute (ATTR_READ_OFFSET).value ());
            SizeT writeOffset = util::stringToui64 (node.attribute (ATTR_WRITE_OFFSET).value ());
            Allocator *allocator = Allocator::Get (node.attribute (ATTR_ALLOCATOR).value ());
            if (allocator == 0) {
                allocator = &DefaultAllocator::Instance ();
            }
            buffer.Resize (length, allocator);
            if (length > 0) {
                const char *encodedBuffer = node.text ().get ();
                std::size_t encodedBufferLength = strlen (encodedBuffer);
                if (encodedBufferLength > 0) {
                    std::size_t bytesDecoded = Base64::Decode (encodedBuffer, encodedBufferLength, buffer.data);
                    if (length != bytesDecoded) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Base64::Decode (node.text ().get (), " THEKOGANS_UTIL_SIZE_T_FORMAT
                            ", buffer.data) == " THEKOGANS_UTIL_SIZE_T_FORMAT ", expecting " THEKOGANS_UTIL_SIZE_T_FORMAT,
                            encodedBufferLength,
                            bytesDecoded,
                            length);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Empty encoded buffer, expecting length " THEKOGANS_UTIL_SIZE_T_FORMAT,
                        length);
                }
            }
            buffer.endianness = endianness;
            buffer.readOffset = readOffset;
            buffer.writeOffset = writeOffset;
            return node;
        }

    } // namespace util
} // namespace thekogans
