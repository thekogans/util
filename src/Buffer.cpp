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
#include <vector>
#include <zlib.h>
#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Heap.h"
#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/Base64.h"
#include "thekogans/util/Buffer.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (
            thekogans::util::Buffer,
            Serializer::TYPE)

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (Buffer)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS_EX (
            SecureBuffer,
            SpinLock,
            THEKOGANS_UTIL_DEFAULT_HEAP_ITEMS_IN_PAGE,
            SecureAllocator::Instance ())

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
                Allocator::SharedPtr allocator_) :
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
            if (buffer != nullptr && count > 0) {
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
            if (buffer != nullptr && count > 0) {
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

        void Buffer::Clear (
                bool rewind,
                bool readOnly) {
            if (data != nullptr) {
                SecureZeroMemory (data, length);
            }
            if (rewind) {
                Rewind (readOnly);
            }
        }

        void Buffer::Rewind (bool readOnly) {
            readOffset = 0;
            if (!readOnly) {
                writeOffset = 0;
            }
        }

        void Buffer::Resize (
                std::size_t length_,
                Allocator::SharedPtr allocator_) {
            if (length != length_) {
                if (allocator_ == nullptr) {
                    allocator_ = allocator;
                }
                ui8 *data_ = nullptr;
                if (length_ > 0) {
                    data_ = (ui8 *)allocator_->Alloc (length_);
                    if (data != nullptr) {
                        memcpy (data_, data, std::min (length_, (std::size_t)length.value));
                    }
                }
                allocator->Free (data, length);
                data = data_;
                length = length_;
                if (readOffset > length) {
                    readOffset = length;
                }
                if (writeOffset > length) {
                    writeOffset = length;
                }
                allocator = allocator_;
            }
        }

        Buffer::SharedPtr Buffer::Clone (Allocator::SharedPtr allocator_) const {
            return SharedPtr (
                new Buffer (
                    endianness,
                    data,
                    data + length,
                    readOffset,
                    writeOffset,
                    allocator_ != nullptr ? allocator_ : allocator));
        }

        Buffer::SharedPtr Buffer::Subset (
                std::size_t offset,
                std::size_t count,
                Allocator::SharedPtr allocator_) const {
            if (offset < length && count > 0) {
                if (count == SIZE_T_MAX || offset + count > length) {
                    count = length - offset;
                }
                return SharedPtr (
                    new Buffer (
                        endianness,
                        data + offset,
                        data + offset + count,
                        0,
                        SIZE_T_MAX,
                        allocator_ != nullptr ? allocator_ : allocator));
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
                Serializer::Size (allocator->GetSerializedType ()) +
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

        namespace {
            struct OutBuffer {
                Allocator::SharedPtr allocator;
                ui8 *data;
                std::size_t length;

                explicit OutBuffer (Allocator::SharedPtr allocator_) :
                    allocator (allocator_),
                    data (nullptr),
                    length (0) {}

                void Write (
                        const ui8 *data_,
                        std::size_t length_) {
                    if (data_ != nullptr && length_ > 0) {
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
                                zStream.msg);
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
                            zStream.msg);
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
                    outBuffer.Write (
                        tmpOutBuffer.data (),
                        tmpOutBuffer.size () - zStream.avail_out);
                } while (zStream.avail_out == 0);
                result = inflateEnd (&zStream);
                assert (result == Z_OK);
            }
        }

        Buffer::SharedPtr Buffer::Deflate (Allocator::SharedPtr allocator_) const {
            if (GetDataAvailableForReading () != 0) {
                if (allocator_ == nullptr) {
                    allocator_ = allocator;
                }
                OutBuffer outBuffer (allocator_);
                DeflateHelper (
                    GetReadPtr (),
                    GetDataAvailableForReading (),
                    outBuffer);
                return SharedPtr (
                    new Buffer (
                        endianness,
                        outBuffer.data,
                        outBuffer.length,
                        0,
                        outBuffer.length,
                        allocator_));
            }
            return nullptr;
        }

        Buffer::SharedPtr Buffer::Inflate (Allocator::SharedPtr allocator_) const {
            if (GetDataAvailableForReading () != 0) {
                if (allocator_ == nullptr) {
                    allocator_ = allocator;
                }
                OutBuffer outBuffer (allocator_);
                InflateHelper (
                    GetReadPtr (),
                    GetDataAvailableForReading (),
                    outBuffer);
                return SharedPtr (
                    new Buffer (
                        endianness,
                        outBuffer.data,
                        outBuffer.length,
                        0,
                        outBuffer.length,
                        allocator_));
            }
            return nullptr;
        }

        Buffer::SharedPtr Buffer::FromHexBuffer (
                Endianness endianness,
                const char *hexBuffer,
                std::size_t hexBufferLength,
                Allocator::SharedPtr allocator) {
            if (hexBuffer != nullptr && hexBufferLength > 0 && IS_EVEN (hexBufferLength)) {
                if (allocator == nullptr) {
                    allocator = DefaultAllocator::Instance ();
                }
                void *data = allocator->Alloc (hexBufferLength / 2);
                std::size_t length = HexDecodeBuffer (hexBuffer, hexBufferLength, data);
                return SharedPtr (
                    new Buffer (endianness, data, length, 0, length, allocator));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    #if defined (TOOLCHAIN_OS_Windows)
        HGLOBAL Buffer::ToHGLOBAL (UINT flags) const {
            if (GetDataAvailableForReading () > 0) {
                os::windows::HGLOBALPtr global (flags, GetDataAvailableForReading ());
                if (global.Get () != nullptr) {
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
                Allocator::SharedPtr /*allocator*/) {
            Buffer::Resize (length, SecureAllocator::Instance ());
        }

        Buffer::SharedPtr SecureBuffer::Clone (Allocator::SharedPtr /*allocator*/) const {
            return SharedPtr (
                new SecureBuffer (
                    endianness,
                    data,
                    data + length,
                    readOffset,
                    writeOffset));
        }

        Buffer::SharedPtr SecureBuffer::Subset (
                std::size_t offset,
                std::size_t count,
                Allocator::SharedPtr /*allocator*/) const {
            return Buffer::Subset (offset, count, SecureAllocator::Instance ());
        }

        Buffer::SharedPtr SecureBuffer::Deflate (Allocator::SharedPtr /*allocator*/) const {
            if (GetDataAvailableForReading () != 0) {
                OutBuffer outBuffer (SecureAllocator::Instance ());
                DeflateHelper (
                    GetReadPtr (),
                    GetDataAvailableForReading (),
                    outBuffer);
                return SharedPtr (
                    new SecureBuffer (
                        endianness,
                        outBuffer.data,
                        outBuffer.length,
                        0,
                        outBuffer.length));
            }
            return SharedPtr ();
        }

        Buffer::SharedPtr SecureBuffer::Inflate (Allocator::SharedPtr /*allocator*/) const {
            if (GetDataAvailableForReading () != 0) {
                OutBuffer outBuffer (SecureAllocator::Instance ());
                InflateHelper (GetReadPtr (), GetDataAvailableForReading (), outBuffer);
                return SharedPtr (
                    new SecureBuffer (
                        endianness,
                        outBuffer.data,
                        outBuffer.length,
                        0,
                        outBuffer.length));
            }
            return SharedPtr ();
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const Buffer &buffer) {
            serializer <<
                buffer.endianness <<
                buffer.length <<
                buffer.readOffset <<
                buffer.writeOffset <<
                buffer.allocator->GetSerializedType ();
            if (buffer.length > 0) {
                std::size_t bytesWritten = serializer.Write (buffer.data, buffer.length);
                if (buffer.length != bytesWritten) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "serializer.Write (buffer.data, "
                        THEKOGANS_UTIL_SIZE_T_FORMAT ") == " THEKOGANS_UTIL_SIZE_T_FORMAT,
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
            std::string allocatorType;
            serializer >> endianness >> length >> readOffset >> writeOffset >> allocatorType;
            Allocator::SharedPtr allocator = Allocator::CreateType (allocatorType.c_str ());
            if (allocator == nullptr) {
                allocator = DefaultAllocator::Instance ();
            }
            buffer.Resize (length, allocator);
            if (length > 0) {
                std::size_t bytesRead = serializer.Read (buffer.data, length);
                if (length != bytesRead) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "serializer.Read (buffer.data, "
                        THEKOGANS_UTIL_SIZE_T_FORMAT ") == " THEKOGANS_UTIL_SIZE_T_FORMAT,
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
            const char * const ATTR_ENDIANESS = "Endianness";
            const char * const ATTR_LENGTH = "Length";
            const char * const ATTR_READ_OFFSET = "ReadOffset";
            const char * const ATTR_WRITE_OFFSET = "WriteOffset";
            const char * const ATTR_ALLOCATOR = "Allocator";
            const char * const ATTR_CONTENTS = "Contents";
        }

        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
                pugi::xml_node &node,
                const Buffer &buffer) {
            node.append_attribute (ATTR_ENDIANESS).set_value (
                EndiannessToString (buffer.endianness).c_str ());
            node.append_attribute (ATTR_LENGTH).set_value (
                util::ui64Tostring (buffer.length).c_str ());
            node.append_attribute (ATTR_READ_OFFSET).set_value (
                util::ui64Tostring (buffer.readOffset).c_str ());
            node.append_attribute (ATTR_WRITE_OFFSET).set_value (
                util::ui64Tostring (buffer.writeOffset).c_str ());
            node.append_attribute (ATTR_ALLOCATOR).set_value (
                EncodeXMLCharEntities (buffer.allocator->GetSerializedType ()).c_str ());
            if (buffer.length > 0) {
                node.text ().set (
                    Base64::Encode (buffer.data, buffer.length, 64)->Tostring ().c_str ());
            }
            return node;
        }

        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
                pugi::xml_node &node,
                Buffer &buffer) {
            Endianness endianness =
                StringToEndianness (node.attribute (ATTR_ENDIANESS).value ());
            SizeT length =
                util::stringToui64 (node.attribute (ATTR_LENGTH).value ());
            SizeT readOffset =
                util::stringToui64 (node.attribute (ATTR_READ_OFFSET).value ());
            SizeT writeOffset =
                util::stringToui64 (node.attribute (ATTR_WRITE_OFFSET).value ());
            Allocator::SharedPtr allocator =
                Allocator::CreateType (node.attribute (ATTR_ALLOCATOR).value ());
            if (allocator == nullptr) {
                allocator = DefaultAllocator::Instance ();
            }
            buffer.Resize (length, allocator);
            if (length > 0) {
                const char *encodedBuffer = node.text ().get ();
                std::size_t encodedBufferLength = strlen (encodedBuffer);
                if (encodedBufferLength > 0) {
                    std::size_t bytesDecoded =
                        Base64::Decode (encodedBuffer, encodedBufferLength, buffer.data);
                    if (length != bytesDecoded) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Base64::Decode (node.text ().get (), " THEKOGANS_UTIL_SIZE_T_FORMAT
                            ", buffer.data) == " THEKOGANS_UTIL_SIZE_T_FORMAT
                            ", expecting " THEKOGANS_UTIL_SIZE_T_FORMAT,
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

        _LIB_THEKOGANS_UTIL_DECL JSON::Object & _LIB_THEKOGANS_UTIL_API operator << (
                JSON::Object &object,
                const Buffer &buffer) {
            object.Add<const std::string &> (
                ATTR_ENDIANESS, EndiannessToString (buffer.endianness));
            object.Add (ATTR_LENGTH, (ui64)buffer.length);
            object.Add (ATTR_READ_OFFSET, (ui64)buffer.readOffset);
            object.Add (ATTR_WRITE_OFFSET, (ui64)buffer.writeOffset);
            object.Add<const std::string &> (
                ATTR_ALLOCATOR,
                buffer.allocator->GetSerializedType ());
            if (buffer.length > 0) {
                object.Add (
                    ATTR_CONTENTS,
                    JSON::Array::SharedPtr (
                        new JSON::Array (
                            Base64::Encode (buffer.data, buffer.length, 64)->Tostring ())));
            }
            return object;
        }

        _LIB_THEKOGANS_UTIL_DECL JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (
                JSON::Object &object,
                Buffer &buffer) {
            Endianness endianness =
                StringToEndianness (object.Get<JSON::String> (ATTR_ENDIANESS)->value);
            SizeT length =
                object.Get<JSON::Number> (ATTR_LENGTH)->To<ui64> ();
            SizeT readOffset =
                object.Get<JSON::Number> (ATTR_READ_OFFSET)->To<ui64> ();
            SizeT writeOffset =
                object.Get<JSON::Number> (ATTR_WRITE_OFFSET)->To<ui64> ();
            Allocator::SharedPtr allocator =
                Allocator::CreateType (
                    object.Get<JSON::String> (ATTR_ALLOCATOR)->value.c_str ());
            if (allocator == nullptr) {
                allocator = DefaultAllocator::Instance ();
            }
            buffer.Resize (length, allocator);
            if (length > 0) {
                std::string contents =
                    object.Get<JSON::Array> (ATTR_CONTENTS)->ToString ();
                if (!contents.empty ()) {
                    std::size_t bytesDecoded =
                        Base64::Decode (contents.data (), contents.size (), buffer.data);
                    if (length != bytesDecoded) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Base64::Decode (node.text ().get (), " THEKOGANS_UTIL_SIZE_T_FORMAT
                            ", buffer.data) == " THEKOGANS_UTIL_SIZE_T_FORMAT
                            ", expecting " THEKOGANS_UTIL_SIZE_T_FORMAT,
                            contents.size (),
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
            return object;
        }

    } // namespace util
} // namespace thekogans
