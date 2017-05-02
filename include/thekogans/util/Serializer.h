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

#if !defined (__thekogans_util_Serializer_h)
#define __thekogans_util_Serializer_h

#include <cstring>
#include <string>
#include <vector>
#include <list>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/ByteSwap.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        struct GUID;
        struct Fraction;
        struct TimeSpec;
        struct Variant;
        struct Buffer;
        template<ui32 length>
        struct FixedBuffer;

        /// \struct Serializer Serializer.h thekogans/util/Serializer.h
        ///
        /// \brief
        /// Serializer is an abstract base class. Derivatives of
        /// Serializer provide stream insertion and extraction
        /// capabilities. Serializer provides a convenient base
        /// for reading/writing binary protocols. Insertion/extraction
        /// operators for all thekogans basic types are provided.

        struct _LIB_THEKOGANS_UTIL_DECL Serializer {
            /// \brief
            /// Serializer endianness (LittleEndian or BigEndian).
            Endianness endianness;

            /// \brief
            /// Default ctor. Init all members.
            Serializer (Endianness endianness_ = HostEndian) :
                endianness (endianness_) {}
            /// \brief
            /// Default dtor.
            virtual ~Serializer () {}

            /// \brief
            /// Read raw bytes.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            virtual ui32 Read (
                void * /*buffer*/,
                ui32 /*count*/) = 0;
            /// \brief
            /// Write raw bytes.
            /// \param[in] buffer Bytes to write.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual ui32 Write (
                const void * /*buffer*/,
                ui32 /*count*/) = 0;

            // Binary Insertion/Extraction API.

            /// \brief
            /// Return serialized size of bool.
            /// \return Serialized size of bool.
            static ui32 Size (bool value) {
                return UI8_SIZE;
            }

            /// \brief
            /// Serialize a bool. It will be written as a single ui8.
            /// 1 = true, 0 = false.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (bool value);
            /// \brief
            /// Extract a bool. It will be read as a single ui8.
            /// 1 = true, 0 = false.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (bool &value);

            /// \brief
            /// Return serialized size of const char *.
            /// \return Serialized size of const char *.
            static ui32 Size (const char *value) {
                return (ui32)(strlen (value) + 1);
            }

            /// \brief
            /// Serialize a c-string.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (const char *value);
            /// \brief
            /// Extract a c-string.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (char *value);

            /// \brief
            /// Return serialized size of const std::string &.
            /// \return Serialized size of const std::string &.
            static ui32 Size (const std::string &value) {
                return (ui32)(UI32_SIZE + value.size ());
            }

            /// \brief
            /// Serialize a std::string.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (const std::string &value);
            /// \brief
            /// Extract a std::string.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (std::string &value);

            /// \brief
            /// Return serialized size of i8.
            /// \return Serialized size of i8.
            static ui32 Size (i8 value) {
                return I8_SIZE;
            }

            /// \brief
            /// Serialize an i8.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (i8 value);
            /// \brief
            /// Extract an i8.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (i8 &value);

            /// \brief
            /// Return serialized size of ui8.
            /// \return Serialized size of ui8.
            static ui32 Size (ui8 value) {
                return UI8_SIZE;
            }

            /// \brief
            /// Serialize an ui8.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (ui8 value);
            /// \brief
            /// Extract an ui8.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (ui8 &value);

            /// \brief
            /// Return serialized size of i16.
            /// \return Serialized size of i16.
            static ui32 Size (i16 value) {
                return I16_SIZE;
            }

            /// \brief
            /// Serialize an i16. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (i16 value);
            /// \brief
            /// Extract an i16. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (i16 &value);

            /// \brief
            /// Return serialized size of ui16.
            /// \return Serialized size of ui16.
            static ui32 Size (ui16 value) {
                return UI16_SIZE;
            }

            /// \brief
            /// Serialize an ui16. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (ui16 value);
            /// \brief
            /// Extract an ui16. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (ui16 &value);

            /// \brief
            /// Return serialized size of i32.
            /// \return Serialized size of i32.
            static ui32 Size (i32 value) {
                return I32_SIZE;
            }

            /// \brief
            /// Serialize an i32. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (i32 value);
            /// \brief
            /// Extract an i32. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (i32 &value);

            /// \brief
            /// Return serialized size of ui32.
            /// \return Serialized size of ui32.
            static ui32 Size (ui32 value) {
                return UI32_SIZE;
            }

            /// \brief
            /// Serialize an ui32. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (ui32 value);
            /// \brief
            /// Extract an ui32. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (ui32 &value);

            /// \brief
            /// Return serialized size of i64.
            /// \return Serialized size of i64.
            static ui32 Size (i64 value) {
                return I64_SIZE;
            }

            /// \brief
            /// Serialize an i64. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (i64 value);
            /// \brief
            /// Extract an i64. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (i64 &value);

            /// \brief
            /// Return serialized size of ui64.
            /// \return Serialized size of ui64.
            static ui32 Size (ui64 value) {
                return UI64_SIZE;
            }

            /// \brief
            /// Serialize an ui64. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (ui64 value);
            /// \brief
            /// Extract an ui64. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (ui64 &value);

            /// \brief
            /// Return serialized size of f32.
            /// \return Serialized size of f32.
            static ui32 Size (f32 value) {
                return F32_SIZE;
            }

            /// \brief
            /// Serialize an f32. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (f32 value);
            /// \brief
            /// Extract an f32. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (f32 &value);

            /// \brief
            /// Return serialized size of f64.
            /// \return Serialized size of f64.
            static ui32 Size (f64 value) {
                return F64_SIZE;
            }

            /// \brief
            /// Serialize an f64. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (f64 value);
            /// \brief
            /// Extract an f64. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (f64 &value);

            /// \brief
            /// Return serialized size of const Flags<T> &.
            /// \return Serialized size of const Flags<T> &.
            template<typename T>
            static ui32 Size (const Flags<T> &value) {
                return Size ((const T)value);
            }

            /// \brief
            /// Serialize a Flags<T>. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            template<typename T>
            inline Serializer &operator << (const Flags<T> &value) {
                *this << (const T)value;
                return *this;
            }
            /// \brief
            /// Extract a Flags<T>. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            template<typename T>
            inline Serializer &operator >> (Flags<T> &value) {
                T t;
                *this >> t;
                value = Flags<T> (t);
                return *this;
            }

            /// \brief
            /// Return serialized size of const Fraction &.
            /// \return Serialized size of const Fraction &.
            static ui32 Size (const Fraction &value) {
                return UI32_SIZE + UI32_SIZE + UI32_SIZE;
            }

            /// \brief
            /// Serialize a Fraction. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (const Fraction &value);
            /// \brief
            /// Extract a Fraction. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer & operator >> (Fraction &value);

            /// \brief
            /// Return serialized size of const TimeSpec &.
            /// \return Serialized size of const TimeSpec &.
            static ui32 Size (const TimeSpec &value) {
                return I64_SIZE + I64_SIZE;
            }

            /// \brief
            /// Serialize a TimeSpec. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (const TimeSpec &value);
            /// \brief
            /// Extract a TimeSpec. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer & operator >> (TimeSpec &value);

            /// \brief
            /// Return serialized size of const GUID &.
            /// \return Serialized size of const GUID &.
            static ui32 Size (const GUID &value);

            /// \brief
            /// Serialize a GUID. It will be written as a 16 byte binary blob.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (const GUID &value);
            /// \brief
            /// Extract a GUID. It will be read as a 16 binary blob.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (GUID &value);

            /// \brief
            /// Return serialized size of const Variant &.
            /// \return Serialized size of const Variant &.
            static ui32 Size (const Variant &value);

            /// \brief
            /// Serialize a Variant.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (const Variant &value);
            /// \brief
            /// Extract a Variant.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (Variant &value);

            /// \struct BufferHeader BufferHeader.h thekogans/packet/Serializer.h
            ///
            /// \brief
            /// BufferHeader represents that part of the serialized \see{Buffer} that is not data.
            /// It contains all \see{Buffer} metadata necessary to (de)serialize a buffer.
            struct _LIB_THEKOGANS_UTIL_DECL BufferHeader {
                /// \brief
                /// \see{Buffer} \see{Endianness}
                ui32 endianness;
                /// \brief
                /// \see{Buffer} data length.
                ui32 length;
                /// \brief
                /// \see{Buffer} read offset.
                ui32 readOffset;
                /// \brief
                /// \see{Buffer} write offset.
                ui32 writeOffset;

                enum {
                    /// \brief
                    /// BufferHeader serialized size.
                    SIZE = UI32_SIZE + UI32_SIZE + UI32_SIZE + UI32_SIZE
                };

                /// \brief
                /// ctor.
                BufferHeader () :
                    endianness (HostEndian),
                    length (0),
                    readOffset (0),
                    writeOffset (0) {}

                /// \brief
                /// ctor.
                /// \param[in] endianness_ \see{Buffer} \see{Endianness}
                /// \param[in] length_ \see{Buffer} data length.
                /// \param[in] readOffset_ \see{Buffer} read offset.
                /// \param[in] writeOffset_ \see{Buffer} write offset.
                BufferHeader (
                    ui32 endianness_,
                    ui32 length_,
                    ui32 readOffset_,
                    ui32 writeOffset_) :
                    endianness (endianness_),
                    length (length_),
                    readOffset (readOffset_),
                    writeOffset (writeOffset_) {}
            };

            /// \brief
            /// Serialize a BufferHeader.
            /// \param[in] value Value to serialize.
            /// \return *this.
            inline Serializer &operator << (
                    const BufferHeader &bufferHeader) {
                *this <<
                    bufferHeader.endianness <<
                    bufferHeader.length <<
                    bufferHeader.readOffset <<
                    bufferHeader.writeOffset;
                return *this;
            }

            /// \brief
            /// Extract a BufferHeader.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            inline Serializer &operator >> (
                    BufferHeader &bufferHeader) {
                *this >>
                    bufferHeader.endianness >>
                    bufferHeader.length >>
                    bufferHeader.readOffset >>
                    bufferHeader.writeOffset;
                return *this;
            }

            /// \brief
            /// Return serialized size of const Buffer &.
            /// \return Serialized size of const Buffer &.
            static ui32 Size (const Buffer &value);

            /// \brief
            /// Serialize a Buffer.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (const Buffer &value);
            /// \brief
            /// Extract a Buffer.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (Buffer &value);

            /// \struct FixedBufferHeader FixedBufferHeader.h thekogans/packet/Serializer.h
            ///
            /// \brief
            /// FixedBufferHeader represents that part of the serialized \see{FixedBuffer} that is not data.
            /// It contains all \see{FixedBuffer} metadata necessary to (de)serialize a fixed buffer.
            struct _LIB_THEKOGANS_UTIL_DECL FixedBufferHeader {
                /// \brief
                /// \see{FixedBuffer} \see{Endianness}
                ui32 endianness;
                /// \brief
                /// \see{FixedBuffer} read offset.
                ui32 readOffset;
                /// \brief
                /// \see{FixedBuffer} write offset.
                ui32 writeOffset;

                enum {
                    /// \brief
                    /// FixedBufferHeader serialized size.
                    SIZE = UI32_SIZE + UI32_SIZE + UI32_SIZE
                };

                /// \brief
                /// ctor.
                FixedBufferHeader () :
                    endianness (HostEndian),
                    readOffset (0),
                    writeOffset (0) {}

                /// \brief
                /// ctor.
                /// \param[in] endianness_ \see{FixedBuffer} \see{Endianness}
                /// \param[in] readOffset_ \see{FixedBuffer} read offset.
                /// \param[in] writeOffset_ \see{FixedBuffer} write offset.
                FixedBufferHeader (
                    ui32 endianness_,
                    ui32 readOffset_,
                    ui32 writeOffset_) :
                    endianness (endianness_),
                    readOffset (readOffset_),
                    writeOffset (writeOffset_) {}
            };

            /// \brief
            /// Serialize a FixedBufferHeader.
            /// \param[in] value Value to serialize.
            /// \return *this.
            inline Serializer &operator << (
                    const FixedBufferHeader &fixedBufferHeader) {
                *this <<
                    fixedBufferHeader.endianness <<
                    fixedBufferHeader.readOffset <<
                    fixedBufferHeader.writeOffset;
                return *this;
            }

            /// \brief
            /// Extract a FixedbufferHeader.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            inline Serializer &operator >> (
                    FixedBufferHeader &fixedBufferHeader) {
                *this >>
                    fixedBufferHeader.endianness >>
                    fixedBufferHeader.readOffset >>
                    fixedBufferHeader.writeOffset;
                return *this;
            }

            /// \brief
            /// Return serialized size of const FixedBuffer &.
            /// \return Serialized size of const FixedBuffer &.
            template<ui32 length>
            static ui32 Size (const FixedBuffer<length> & /*value*/) {
                return FixedBufferHeader::SIZE + length * UI8_SIZE;
            }

            /// \brief
            /// Serialize a FixedBuffer.
            /// \param[in] value Value to serialize.
            /// \return *this.
            template<ui32 length>
            Serializer &operator << (const FixedBuffer<length> &value) {
                *this << FixedBufferHeader (value.endianness, value.readOffset, value.writeOffset);
                ui32 size = length * UI8_SIZE;
                if (Write (value.data, size) != size) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Write (value.data, %u) != %u", size, size);
                }
                return *this;
            }
            /// \brief
            /// Extract a FixedBuffer.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            template<ui32 length>
            Serializer &operator >> (FixedBuffer<length> &value) {
                FixedBufferHeader fixedBufferHeader;
                *this >> fixedBufferHeader;
                value.endianness = (Endianness)fixedBufferHeader.endianness;
                value.readOffset = fixedBufferHeader.readOffset;
                value.writeOffset = fixedBufferHeader.writeOffset;
                ui32 size = length * UI8_SIZE;
                if (Read (value.data, size) != size) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Read (value.data, %u) != %u", size, size);
                }
                return *this;
            }

            /// \brief
            /// Return serialized size of const std::vector<T> &.
            /// \return Serialized size of const std::vector<T> &.
            template<typename T>
            static ui32 Size (const std::vector<T> &value) {
                ui32 size = UI32_SIZE;
                for (ui32 i = 0, count = (ui32)value.size (); i < count; ++i) {
                    size += Size (value[i]);
                }
                return size;
            }

            /// \brief
            /// Serialize a const std::vector<T>. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            template<typename T>
            inline Serializer &operator << (const std::vector<T> &value) {
                ui32 length = (ui32)value.size ();
                *this << length;
                for (ui32 i = 0; i < length; ++i) {
                    *this << value[i];
                }
                return *this;
            }
            /// \brief
            /// Extract a std::vector<T>. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            template<typename T>
            inline Serializer &operator >> (std::vector<T> &value) {
                ui32 length;
                *this >> length;
                if (length > 0) {
                    value.resize (length);
                    for (ui32 i = 0; i < length; ++i) {
                        *this >> value[i];
                    }
                }
                else {
                    value.clear ();
                }
                return *this;
            }

            /// \brief
            /// NOTE: The following two specializations (i8, ui8) are for
            /// performance. Since these vector elements are of uniform
            /// size and don't need to be byte swapped, we can read and
            /// write them as a vector.

            /// \brief
            /// Return serialized size of const std::vector<i8> &.
            /// \return Serialized size of const std::vector<i8> &.
            static ui32 Size (const std::vector<i8> &value) {
                return (ui32)(UI32_SIZE + value.size () * UI8_SIZE);
            }

            /// \brief
            /// Serialize a const std::vector<i8>.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (const std::vector<i8> &value);
            /// \brief
            /// Extract a std::vector<i8>.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (std::vector<i8> &value);

            /// \brief
            /// Return serialized size of const std::vector<ui8> &.
            /// \return Serialized size of const std::vector<ui8> &.
            static ui32 Size (const std::vector<ui8> &value) {
                return (ui32)(UI32_SIZE + value.size () * UI8_SIZE);
            }

            /// \brief
            /// Serialize a const std::vector<ui8>.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (const std::vector<ui8> &value);
            /// \brief
            /// Extract a std::vector<ui8>.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (std::vector<ui8> &value);

            /// \brief
            /// Return serialized size of const std::list<T> &.
            /// \return Serialized size of const std::list<T> &.
            template<typename T>
            static ui32 Size (const std::list<T> &value) {
                ui32 size = UI32_SIZE;
                for (typename std::list<T>::const_iterator
                        it = value.begin (),
                        end = value.end (); it != end; ++it) {
                    size += Size (*it);
                }
                return size;
            }

            /// \brief
            /// Serialize a const std::list<T>. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            template<typename T>
            inline Serializer &operator << (const std::list<T> &value) {
                ui32 length = value.size ();
                *this << length;
                for (typename std::list<T>::const_iterator
                        it = value.begin (),
                        end = value.end (); it != end; ++it) {
                    *this << *it;
                }
                return *this;
            }
            /// \brief
            /// Extract a std::list<T>. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            template<typename T>
            inline Serializer &operator >> (std::list<T> &value) {
                ui32 length;
                *this >> length;
                if (length > 0) {
                    for (ui32 i = 0; i < length; ++i) {
                        T t;
                        *this >> t;
                        value.push_back (t);
                    }
                }
                else {
                    value.clear ();
                }
                return *this;
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Serializer_h)
