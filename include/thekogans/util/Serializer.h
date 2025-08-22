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
#include <map>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/ByteSwap.h"
#include "thekogans/util/SizeT.h"
#include "thekogans/util/DynamicCreatable.h"
#include "thekogans/util/SerializableHeader.h"
#include "thekogans/util/SecureAllocator.h"
#include "thekogans/util/XMLUtils.h"

namespace thekogans {
    namespace util {

        /// \struct Serializer Serializer.h thekogans/util/Serializer.h
        ///
        /// \brief
        /// Serializer provides the abstract base (Read and Write) for streamming
        /// binary data. Derivatives of Serializer provide implementations appropriate
        /// to them. Insertion/extraction operators for all thekogans util (See \see{Types.h})
        /// basic types as well as most other types (\see{Exception}, \see{Buffer}...)
        /// are provided. Serializer uses endianness to convert between in stream and
        /// in memory byte order.
        ///
        /// PRO TIP: If you want your code to be endianess agnostic, use signature (magic)
        /// bytes to deduce Serializer endianness. Ex:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        /// util::SimpleFile file (util::HostEndian, path, util::SimpleFile::ReadWrite);
        /// // Magic serves two purposes. Firstly it gives us a quick
        /// // check to make sure we're dealing with our file and second,
        /// // it allows us to move files from little to big endian (and
        /// // vise versa) machines.
        /// ui32 magic;
        /// file >> magic;
        /// if (magic == util::MAGIC32) {
        ///     // File is host endian.
        /// }
        /// else if (util::ByteSwap<util::GuestEndian, util::HostEndian> (magic) == util::MAGIC32) {
        ///     // File is guest endian.
        ///     file.endianness = util::GuestEndian;
        /// }
        /// else {
        ///     THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
        ///         "Corrupt file %s",
        ///         path.c_str ());
        /// }
        /// \endcode
        struct _LIB_THEKOGANS_UTIL_DECL Serializer : public DynamicCreatable {
            /// \brief
            /// Serializer is a \see{util::DynamicCreatable} abstract base.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_ABSTRACT_BASE (Serializer)

        #if defined (THEKOGANS_UTIL_TYPE_Static)
            /// \brief
            /// Register all known bases. This method is meant to be added
            /// to as new Serializer bases are added to the system.
            /// NOTE: If you create Serializer derived bases (see
            /// \see{RandomSeekSerializer}...) you should add your own static
            /// initializer to register their derived classes.
            static void StaticInit ();
        #endif // defined (THEKOGANS_UTIL_TYPE_Static)

            /// \brief
            /// Serializer endianness (LittleEndian or BigEndian).
            Endianness endianness;
            /// \brief
            /// Current governing \see{SerializableHeader} for
            /// \see{Serializable} insertion/extraction.
            SerializableHeader context;
            /// \brief
            /// Default \see{Serializable} factory.
            DynamicCreatable::FactoryType factory;

            /// \struct Serializer::ContextGuard Serializer.h thekogans/util/Serializer.h
            ///
            /// \brief
            /// ContextGuard provides scope for context.
            struct ContextGuard {
                /// \brief
                /// Serializer whose context to guard.
                Serializer &serializer;
                /// \brief
                /// Saved context.
                SerializableHeader context;

                /// \brief
                /// ctor,
                /// \param[in] serializer_ Serializer whose context to guard.
                /// \param[in] context_ New context.
                ContextGuard (
                        Serializer &serializer_,
                        const SerializableHeader &context_) :
                        serializer (serializer_),
                        context (serializer.context) {
                    serializer.context = context_;
                }
                /// \brief
                /// dtor,
                ~ContextGuard () {
                    serializer.context = context;
                }
            };

            /// \brief
            /// ctor.
            /// \param[in] endianness Serializer endianness.
            Serializer (Endianness endianness_ = HostEndian) :
                endianness (endianness_) {}

            /// \brief
            /// Read raw bytes.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            virtual std::size_t Read (
                void * /*buffer*/,
                std::size_t /*count*/) = 0;
            /// \brief
            /// Write raw bytes.
            /// \param[in] buffer Bytes to write.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual std::size_t Write (
                const void * /*buffer*/,
                std::size_t /*count*/) = 0;

            /// \brief
            /// Return serializer size.
            /// \return Serializer size.
            inline std::size_t Size () const {
                return ENDIANNESS_SIZE;
            }

            /// \brief
            /// std::swap for Serializer.
            /// \param[in,out] other Serializer to swap.
            inline void swap (Serializer &other) {
                std::swap (endianness, other.endianness);
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
            static std::size_t Size (const T &t) {
                return t.Size ();
            }

            /// \brief
            /// Return serialized size of an \see{Endianness}.
            /// \param[in] value \see{Endianness} whose size to return.
            /// \return Serialized size of \see{Endianness}.
            static constexpr std::size_t Size (Endianness /*value*/) {
                return ENDIANNESS_SIZE;
            }

            /// \brief
            /// Serialize an \see{Endianness}. It will be written as a single \see{ui8}.
            /// \param[in] value \see{Endianness} to serialize.
            /// \return *this.
            Serializer &operator << (Endianness value);
            /// \brief
            /// Extract an \see{Endianness}. It will be read as a single \see{ui8}.
            /// \param[out] value Where to place the extracted \see{Endianness}.
            /// \return *this.
            Serializer &operator >> (Endianness &value);

            /// \brief
            /// Return serialized size of bool.
            /// \param[in] value bool whose size to return.
            /// \return Serialized size of bool.
            static constexpr std::size_t Size (bool /*value*/) {
                return BOOL_SIZE;
            }

            /// \brief
            /// Serialize a bool. It will be written as a single \see{ui8}.
            /// 1 = true, 0 = false.
            /// \param[in] value bool to serialize.
            /// \return *this.
            Serializer &operator << (bool value);
            /// \brief
            /// Extract a bool. It will be read as a single \see{ui8}.
            /// 1 = true, 0 = false.
            /// \param[out] value Where to place the extracted bool.
            /// \return *this.
            Serializer &operator >> (bool &value);

            /// \brief
            /// Return serialized size of char.
            /// \param[in] value char whose size to return.
            /// \return Serialized size of char.
            static constexpr std::size_t Size (char /*value*/) {
                return CHAR_SIZE;
            }
            /// \brief
            /// Return serialized size of wchar_t.
            /// \param[in] value wchar_t whose size to return.
            /// \return Serialized size of wchar_t.
            static constexpr std::size_t Size (wchar_t /*value*/) {
                return WCHAR_T_SIZE;
            }

            /// \brief
            /// Serialize a wchar_t.
            /// \param[in] value wchar_t to serialize.
            /// \return *this.
            Serializer &operator << (wchar_t value);
            /// \brief
            /// Extract a wchar_t.
            /// \param[out] value Where to place the extracted wchar_t.
            /// \return *this.
            Serializer &operator >> (wchar_t &value);

            /// \brief
            /// Return serialized size of c-string.
            /// \param[in] value c-string whose size to return.
            /// \return Serialized size of c-string.
            static std::size_t Size (const char *value);

            /// \brief
            /// Serialize a c-string.
            /// \param[in] value c-string to serialize.
            /// \return *this.
            Serializer &operator << (const char *value);
            /// \brief
            /// Extract a c-string.
            /// \param[out] value Where to place the extracted c-string.
            /// \return *this.
            Serializer &operator >> (char *value);

            /// \brief
            /// Return serialized size of std::string.
            /// \param[in] value std::string whose size to return.
            /// \return Serialized size of std::string.
            static std::size_t Size (const std::string &value) {
                return SizeT (value.size ()).Size () + value.size ();
            }

            /// \brief
            /// Serialize a std::string.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (const std::string &value);
            /// \brief
            /// Extract a std::string.
            /// \param[out] value Where to place the extracted std::string.
            /// \return *this.
            Serializer &operator >> (std::string &value);

            /// \brief
            /// Return serialized size of wide c-string.
            /// \param[in] value Wide c-string whose size to return.
            /// \return Serialized size of wide c-string.
            static std::size_t Size (const wchar_t *value);

            /// \brief
            /// Serialize a wide c-string.
            /// \param[in] value Wide c-string to serialize.
            /// \return *this.
            Serializer &operator << (const wchar_t *value);
            /// \brief
            /// Extract a wide c-string.
            /// \param[out] value Where to place the extracted wide c-string.
            /// \return *this.
            Serializer &operator >> (wchar_t *value);

            /// \brief
            /// Return serialized size of std::wstring.
            /// \param[in] value std::wstring whose size to return.
            /// \return Serialized size of std::wstring.
            static std::size_t Size (const std::wstring &value) {
                return SizeT (value.size ()).Size () + value.size () * WCHAR_T_SIZE;
            }

            /// \brief
            /// Serialize a std::wstring.
            /// \param[in] value Value to serialize.
            /// \return *this.
            Serializer &operator << (const std::wstring &value);
            /// \brief
            /// Extract a std::wstring.
            /// \param[out] value Where to place the extracted std::wstring.
            /// \return *this.
            Serializer &operator >> (std::wstring &value);

            /// \brief
            /// Return serialized size of \see{SecureString}.
            /// \param[in] value \see{SecureString} whose size to return.
            /// \return Serialized size of \see{SecureString}.
            static std::size_t Size (const SecureString &value) {
                return SizeT (value.size ()).Size () + value.size ();
            }

            /// \brief
            /// Serialize a \see{SecureString}.
            /// \param[in] value \see{SecureString} to serialize.
            /// \return *this.
            Serializer &operator << (const SecureString &value);
            /// \brief
            /// Extract a \see{SecureString}.
            /// \param[out] value Where to place the extracted \see{SecureString}.
            /// \return *this.
            Serializer &operator >> (SecureString &value);

            /// \brief
            /// Return serialized size of \see{i8}.
            /// \param[in] value \see{i8} whose size to return.
            /// \return Serialized size of \see{i8}.
            static constexpr std::size_t Size (i8 /*value*/) {
                return I8_SIZE;
            }

            /// \brief
            /// Serialize an \see{i8}.
            /// \param[in] value \see{i8} to serialize.
            /// \return *this.
            Serializer &operator << (i8 value);
            /// \brief
            /// Extract an \see{i8}.
            /// \param[out] value Where to place the extracted \see{i8}.
            /// \return *this.
            Serializer &operator >> (i8 &value);

            /// \brief
            /// Return serialized size of \see{ui8}.
            /// \param[in] value \see{ui8} whose size to return.
            /// \return Serialized size of \see{ui8}.
            static constexpr std::size_t Size (ui8 /*value*/) {
                return UI8_SIZE;
            }

            /// \brief
            /// Serialize an \see{ui8}.
            /// \param[in] value \see{ui8} to serialize.
            /// \return *this.
            Serializer &operator << (ui8 value);
            /// \brief
            /// Extract an \see{ui8}.
            /// \param[out] value Where to place the extracted \see{ui8}.
            /// \return *this.
            Serializer &operator >> (ui8 &value);

            /// \brief
            /// Return serialized size of \see{i16}.
            /// \param[in] value \see{i16} whose size to return.
            /// \return Serialized size of \see{i16}.
            static constexpr std::size_t Size (i16 /*value*/) {
                return I16_SIZE;
            }

            /// \brief
            /// Serialize an \see{i16}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value \see{i16} to serialize.
            /// \return *this.
            Serializer &operator << (i16 value);
            /// \brief
            /// Extract an \see{i16}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted \see{i16}.
            /// \return *this.
            Serializer &operator >> (i16 &value);

            /// \brief
            /// Return serialized size of \see{ui16}.
            /// \param[in] value \see{ui16} whose size to return.
            /// \return Serialized size of \see{ui16}.
            static constexpr std::size_t Size (ui16 /*value*/) {
                return UI16_SIZE;
            }

            /// \brief
            /// Serialize an \see{ui16}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value \see{ui16} to serialize.
            /// \return *this.
            Serializer &operator << (ui16 value);
            /// \brief
            /// Extract an \see{ui16}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted \see{ui16}.
            /// \return *this.
            Serializer &operator >> (ui16 &value);

            /// \brief
            /// Return serialized size of \see{i32}.
            /// \param[in] value \see{i32} whose size to return.
            /// \return Serialized size of \see{i32}.
            static constexpr std::size_t Size (i32 /*value*/) {
                return I32_SIZE;
            }

            /// \brief
            /// Serialize an \see{i32}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value \see{i32} to serialize.
            /// \return *this.
            Serializer &operator << (i32 value);
            /// \brief
            /// Extract an \see{i32}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted \see{i32}.
            /// \return *this.
            Serializer &operator >> (i32 &value);

            /// \brief
            /// Return serialized size of \see{ui32}.
            /// \param[in] value \see{ui32} whose size to return.
            /// \return Serialized size of \see{ui32}.
            static constexpr std::size_t Size (ui32 /*value*/) {
                return UI32_SIZE;
            }

            /// \brief
            /// Serialize an \see{ui32}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value \see{ui32} to serialize.
            /// \return *this.
            Serializer &operator << (ui32 value);
            /// \brief
            /// Extract an \see{ui32}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted \see{ui32}.
            /// \return *this.
            Serializer &operator >> (ui32 &value);

            /// \brief
            /// Return serialized size of \see{i64}.
            /// \param[in] value \see{i64} whose size to return.
            /// \return Serialized size of \see{i64}.
            static constexpr std::size_t Size (i64 /*value*/) {
                return I64_SIZE;
            }

            /// \brief
            /// Serialize an \see{i64}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value \see{i64} to serialize.
            /// \return *this.
            Serializer &operator << (i64 value);
            /// \brief
            /// Extract an \see{i64}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted \see{i64}.
            /// \return *this.
            Serializer &operator >> (i64 &value);

            /// \brief
            /// Return serialized size of \see{ui64}.
            /// \param[in] value \see{ui64} whose size to return.
            /// \return Serialized size of \see{ui64}.
            static constexpr std::size_t Size (ui64 /*value*/) {
                return UI64_SIZE;
            }

            /// \brief
            /// Serialize an \see{ui64}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value \see{ui64} to serialize.
            /// \return *this.
            Serializer &operator << (ui64 value);
            /// \brief
            /// Extract an \see{ui64}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted \see{ui64}.
            /// \return *this.
            Serializer &operator >> (ui64 &value);

            /// \brief
            /// Return serialized size of \see{f32}.
            /// \param[in] value \see{f32} whose size to return.
            /// \return Serialized size of \see{f32}.
            static constexpr std::size_t Size (f32 /*value*/) {
                return F32_SIZE;
            }

            /// \brief
            /// Serialize an \see{f32}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value \see{f32} to serialize.
            /// \return *this.
            Serializer &operator << (f32 value);
            /// \brief
            /// Extract an \see{f32}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted \see{f32}.
            /// \return *this.
            Serializer &operator >> (f32 &value);

            /// \brief
            /// Return serialized size of \see{f64}.
            /// \param[in] value \see{f64} whose size to return.
            /// \return Serialized size of see{f64}.
            static constexpr std::size_t Size (f64 /*value*/) {
                return F64_SIZE;
            }

            /// \brief
            /// Serialize an \see{f64}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value \see{f64} to serialize.
            /// \return *this.
            Serializer &operator << (f64 value);
            /// \brief
            /// Extract an \see{f64}. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted \see{f64}.
            /// \return *this.
            Serializer &operator >> (f64 &value);

            /// \brief
            /// Return serialized size of an \see{Attribute}.
            /// \param[in] value \see{Attribute} whose size to return.
            /// \return Serialized size of \see{Attribute}.
            static std::size_t Size (const Attribute &value) {
                return Size (value.first) + Size (value.second);
            }

            /// \brief
            /// Serialize an \see{Attribute}.
            /// \param[in] value \see{Attribute} to serialize.
            /// \return *this.
            Serializer &operator << (const Attribute &value);
            /// \brief
            /// Extract an \see{Attribute}.
            /// \param[out] value Where to place the extracted \see{Attribute}.
            /// \return *this.
            Serializer &operator >> (Attribute &value);

            /// \brief
            /// Return serialized size of const std::vector<T> &.
            /// \return Serialized size of const std::vector<T> &.
            template<typename T>
            static std::size_t Size (const std::vector<T> &value) {
                std::size_t size = SizeT (value.size ()).Size ();
                for (std::size_t i = 0, count = value.size (); i < count; ++i) {
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
                *this << SizeT (value.size ());
                for (std::size_t i = 0, count = value.size (); i < count; ++i) {
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
                SizeT count;
                *this >> count;
                std::vector<T> temp (count);
                for (std::size_t i = 0; i < count; ++i) {
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
            static std::size_t Size (const std::vector<i8> &value) {
                return SizeT (value.size ()).Size () + value.size ();
            }

            /// \brief
            /// Serialize a const std::vector<i8>.
            /// \param[in] value Value to serialize.
            /// \return *this.
            template<>
            Serializer &operator << (const std::vector<i8> &value) {
                *this << SizeT (value.size ());
                if (value.size () > 0) {
                    std::size_t size = value.size () * I8_SIZE;
                    if (Write (value.data (), size) != size) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Write (value.data (), "
                            THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                            size,
                            size);
                    }
                }
                return *this;
            }
            /// \brief
            /// Extract a std::vector<i8>.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            template<>
            Serializer &operator >> (std::vector<i8> &value) {
                SizeT length;
                *this >> length;
                if (length > 0) {
                    std::vector<i8> temp (length);
                    std::size_t size = length * I8_SIZE;
                    if (Read (temp.data (), size) != size) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Read (value.data (), "
                            THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                            size,
                            size);
                    }
                    value.swap (temp);
                }
                else {
                    value.clear ();
                }
                return *this;
            }

            /// \brief
            /// Return serialized size of const std::vector<ui8> &.
            /// \return Serialized size of const std::vector<ui8> &.
            static std::size_t Size (const std::vector<ui8> &value) {
                return SizeT (value.size ()).Size () + value.size ();
            }

            /// \brief
            /// Serialize a const std::vector<ui8>.
            /// \param[in] value Value to serialize.
            /// \return *this.
            template<>
            Serializer &operator << (const std::vector<ui8> &value) {
                *this << SizeT (value.size ());
                if (value.size () > 0) {
                    std::size_t size = value.size () * UI8_SIZE;
                    if (Write (value.data (), size) != size) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Write (value.data (), "
                            THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                            size,
                            size);
                    }
                }
                return *this;
            }
            /// \brief
            /// Extract a std::vector<ui8>.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            template<>
            Serializer &operator >> (std::vector<ui8> &value) {
                SizeT length;
                *this >> length;
                if (length > 0) {
                    std::vector<ui8> temp (length);
                    std::size_t size = length * UI8_SIZE;
                    if (Read (temp.data (), size) != size) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Read (value.data (), "
                            THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                            size,
                            size);
                    }
                    value.swap (temp);
                }
                else {
                    value.clear ();
                }
                return *this;
            }

            /// \brief
            /// Return serialized size of const \see{SecureVector}<T> &.
            /// \return Serialized size of const \see{SecureVector}<T> &.
            template<typename T>
            static std::size_t Size (const SecureVector<T> &value) {
                std::size_t size = SizeT (value.size ()).Size ();
                for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                    size += Size (value[i]);
                }
                return size;
            }

            /// \brief
            /// Serialize a const \see{SecureVector}<T>. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            template<typename T>
            inline Serializer &operator << (const SecureVector<T> &value) {
                *this << SizeT (value.size ());
                for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                    *this << value[i];
                }
                return *this;
            }
            /// \brief
            /// Extract a \see{SecureVector}<T>. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            template<typename T>
            inline Serializer &operator >> (SecureVector<T> &value) {
                SizeT count;
                *this >> count;
                SecureVector<T> temp (count);
                for (std::size_t i = 0; i < count; ++i) {
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
            /// Return serialized size of const \see{SecureVector}<i8> &.
            /// \return Serialized size of const \see{SecureVector}<i8> &.
            static std::size_t Size (const SecureVector<i8> &value) {
                return SizeT (value.size ()).Size () + value.size ();
            }

            /// \brief
            /// Serialize a const \see{SecureVector}<i8>.
            /// \param[in] value Value to serialize.
            /// \return *this.
            template<>
            Serializer &operator << (const SecureVector<i8> &value) {
                *this << SizeT (value.size ());
                if (value.size () > 0) {
                    std::size_t size = value.size () * I8_SIZE;
                    if (Write (value.data (), size) != size) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Write (value.data (), "
                            THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                            size,
                            size);
                    }
                }
                return *this;
            }
            /// \brief
            /// Extract a \see{SecureVector}<i8>.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            template<>
            Serializer &operator >> (SecureVector<i8> &value) {
                SizeT length;
                *this >> length;
                if (length > 0) {
                    SecureVector<i8> temp (length);
                    std::size_t size = length * I8_SIZE;
                    if (Read (temp.data (), size) != size) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Read (value.data (), "
                            THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                            size,
                            size);
                    }
                    value.swap (temp);
                }
                else {
                    value.clear ();
                }
                return *this;
            }

            /// \brief
            /// Return serialized size of const \see{SecureVector}<ui8> &.
            /// \return Serialized size of const \see{SecureVector}<ui8> &.
            static std::size_t Size (const SecureVector<ui8> &value) {
                return SizeT (value.size ()).Size () + value.size ();
            }

            /// \brief
            /// Serialize a const \see{SecureVector}<ui8>.
            /// \param[in] value Value to serialize.
            /// \return *this.
            template<>
            Serializer &operator << (const SecureVector<ui8> &value) {
                *this << SizeT (value.size ());
                if (value.size () > 0) {
                    std::size_t size = value.size () * UI8_SIZE;
                    if (Write (value.data (), size) != size) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Write (value.data (), "
                            THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                            size,
                            size);
                    }
                }
                return *this;
            }
            /// \brief
            /// Extract a \see{SecureVector}<ui8>.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            template<>
            Serializer &operator >> (SecureVector<ui8> &value) {
                SizeT length;
                *this >> length;
                if (length > 0) {
                    SecureVector<ui8> temp (length);
                    std::size_t size = length * UI8_SIZE;
                    if (Read (temp.data (), size) != size) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Read (value.data (), "
                            THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                            size,
                            size);
                    }
                    value.swap (temp);
                }
                else {
                    value.clear ();
                }
                return *this;
            }

            /// \brief
            /// Return serialized size of const std::list<T> &.
            /// \return Serialized size of const std::list<T> &.
            template<typename T>
            static std::size_t Size (const std::list<T> &value) {
                std::size_t size = SizeT (value.size ()).Size ();
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
                *this << SizeT (value.size ());
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
                SizeT count;
                *this >> count;
                std::list<T> temp;
                for (std::size_t i = 0; i < count; ++i) {
                    T t;
                    *this >> t;
                    value.push_back (t);
                }
                value.swap (temp);
                return *this;
            }

            /// \brief
            /// Return serialized size of const std::map<T> &.
            /// \return Serialized size of const std::map<T> &.
            template<
                typename Key,
                typename T>
            static std::size_t Size (const std::map<Key, T> &value) {
                std::size_t size = SizeT (value.size ()).Size ();
                for (typename std::map<Key, T>::const_iterator
                        it = value.begin (),
                        end = value.end (); it != end; ++it) {
                    size += Size (it->first) + Size (it->second);
                }
                return size;
            }

            /// \brief
            /// Serialize a const std::map<T>. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[in] value Value to serialize.
            /// \return *this.
            template<
                typename Key,
                typename T>
            inline Serializer &operator << (const std::map<Key, T> &value) {
                *this << SizeT (value.size ());
                for (typename std::map<Key, T>::const_iterator
                        it = value.begin (),
                        end = value.end (); it != end; ++it) {
                    *this << it->first << it->second;
                }
                return *this;
            }
            /// \brief
            /// Extract a std::map<T>. endianness is used to properly
            /// convert between serializer and host byte order.
            /// \param[out] value Where to place the extracted value.
            /// \return *this.
            template<
                typename Key,
                typename T>
            inline Serializer &operator >> (std::map<Key, T> &value) {
                SizeT count;
                *this >> count;
                std::map<Key, T> temp;
                for (std::size_t i = 0; i < count; ++i) {
                    Key key;
                    T t;
                    *this >> key >> t;
                    value.push_back (std::map<Key, T>::value_type (key, t));
                }
                value.swap (temp);
                return *this;
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Serializer_h)
