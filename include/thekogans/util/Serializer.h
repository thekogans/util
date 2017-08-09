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

namespace thekogans {
    namespace util {

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

            /// \brief
            /// Return serializer size.
            /// \return Serializer size.
            inline ui32 Size () const {
                return ENDIANNESS_SIZE;
            }

            // Binary Insertion/Extraction API.

            /// \brief
            /// This template is used to match non-integral types (structs).
            /// NOTE: If you get a compiler error that leads you here, it
            /// usually means that you're trying to serialize a struct and
            /// you haven't defined a std::size_t Size () const for it.
            /// \param[in] t Type whose Size function to call.
            /// \return Size of serialized type.
            template<typename T>
            static ui32 Size (const T &t) {
                return t.Size ();
            }

            /// \brief
            /// Return serialized size of bool.
            /// \return Serialized size of bool.
            static ui32 Size (Endianness /*value*/) {
                return ENDIANNESS_SIZE;
            }

            /// \brief
            /// Serialize a endianness. It will be written as a single ui8.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (Endianness value);
            /// \brief
            /// Extract a endianness. It will be read as a single ui8.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            Serializer &operator >> (Endianness &value);

            /// \brief
            /// Return serialized size of bool.
            /// \return Serialized size of bool.
            static ui32 Size (bool /*value*/) {
                return BOOL_SIZE;
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
                return UI32_SIZE + (ui32)value.size ();
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
            static ui32 Size (i8 /*value*/) {
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
            static ui32 Size (ui8 /*value*/) {
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
            static ui32 Size (i16 /*value*/) {
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
            static ui32 Size (ui16 /*value*/) {
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
            static ui32 Size (i32 /*value*/) {
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
            static ui32 Size (ui32 /*value*/) {
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
            static ui32 Size (i64 /*value*/) {
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
            static ui32 Size (ui64 /*value*/) {
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
            static ui32 Size (f32 /*value*/) {
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
            static ui32 Size (f64 /*value*/) {
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
                ui32 count = (ui32)value.size ();
                *this << count;
                for (ui32 i = 0; i < count; ++i) {
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
                ui32 count;
                *this >> count;
                std::vector<T> temp (count);
                for (ui32 i = 0; i < count; ++i) {
                    *this >> temp[i];
                }
                value.swap (temp);
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
                return UI32_SIZE + (ui32)value.size ();
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
                return UI32_SIZE + (ui32)value.size ();
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
                ui32 count = (ui32)value.size ();
                *this << count;
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
                ui32 count;
                *this >> count;
                std::list<T> temp;
                for (ui32 i = 0; i < count; ++i) {
                    T t;
                    *this >> t;
                    value.push_back (t);
                }
                value.swap (temp);
                return *this;
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Serializer_h)
