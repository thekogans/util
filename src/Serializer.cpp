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
#include <cwchar>
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/Exception.h"
#if defined (THEKOGANS_UTIL_TYPE_Static)
    #include "thekogans/util/Buffer.h"
    #include "thekogans/util/RandomSeekSerializer.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)
#include "thekogans/util/Serializable.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (thekogans::util::Serializer)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void Serializer::StaticInit () {
            Buffer::StaticInit ();
            RandomSeekSerializer::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        std::size_t Serializer::Size (Serializable &value) {
            return value.GetSize (*this);
        }

        Serializer &Serializer::operator << (Endianness value) {
            if (Write (&value, ENDIANNESS_SIZE) != ENDIANNESS_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    ENDIANNESS_SIZE,
                    ENDIANNESS_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (Endianness &value) {
            if (Read (&value, ENDIANNESS_SIZE) != ENDIANNESS_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    ENDIANNESS_SIZE,
                    ENDIANNESS_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator << (bool value) {
            ui8 b = value ? 1 : 0;
            if (Write (&b, BOOL_SIZE) != BOOL_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&b, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    BOOL_SIZE,
                    BOOL_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (bool &value) {
            ui8 b;
            if (Read (&b, BOOL_SIZE) != BOOL_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&b, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    BOOL_SIZE,
                    BOOL_SIZE);
            }
            value = b == 1;
            return *this;
        }


        Serializer &Serializer::operator << (wchar_t value) {
            if (endianness == GuestEndian) {
                value = ByteSwap<HostEndian, GuestEndian> (value);
            }
            if (Write (&value, WCHAR_T_SIZE) != WCHAR_T_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    WCHAR_T_SIZE,
                    WCHAR_T_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (wchar_t &value) {
            if (Read (&value, WCHAR_T_SIZE) != WCHAR_T_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    WCHAR_T_SIZE,
                    WCHAR_T_SIZE);
            }
            if (endianness == GuestEndian) {
                value = ByteSwap<GuestEndian, HostEndian> (value);
            }
            return *this;
        }

        std::size_t Serializer::Size (const char *value) {
            if (value != nullptr) {
                return strlen (value) + 1;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Serializer &Serializer::operator << (const char *value) {
            if (value != nullptr) {
                std::size_t size = Size (value);
                if (Write (value, size) != size) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Write (value, "
                        THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                        size,
                        size);
                }
                return *this;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Serializer &Serializer::operator >> (char *value) {
            if (value != nullptr) {
                for (;;) {
                    if (Read (value, I8_SIZE) != I8_SIZE) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Read (value, "
                            THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                            I8_SIZE,
                            I8_SIZE);
                    }
                    if (*value == 0) {
                        break;
                    }
                    ++value;
                }
                return *this;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Serializer &Serializer::operator << (const std::string &value) {
            *this << SizeT (value.size ());
            if (value.size () > 0) {
                if (Write (value.c_str (), value.size ()) != value.size ()) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Write (value.c_str (), "
                        THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                        value.size (),
                        value.size ());
                }
            }
            return *this;
        }

        Serializer &Serializer::operator >> (std::string &value) {
            SizeT length;
            *this >> length;
            if (length > 0) {
                std::string temp (length, 0);
                if (Read (&temp[0], length) != length) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Read (&value[0], "
                        THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                        length,
                        length);
                }
                value.swap (temp);
            }
            else {
                value.clear ();
            }
            return *this;
        }

        std::size_t Serializer::Size (const wchar_t *value) {
            if (value != nullptr) {
                return wcslen (value) + 1;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Serializer &Serializer::operator << (const wchar_t *value) {
            if (value != nullptr) {
                for (std::size_t i = 0, size = Size (value); i <= size; ++i) {
                    *this << value[i];
                }
                return *this;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Serializer &Serializer::operator >> (wchar_t *value) {
            if (value != nullptr) {
                for (;;) {
                    *this >> *value;
                    if (*value == 0) {
                        break;
                    }
                    ++value;
                }
                return *this;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Serializer &Serializer::operator << (const std::wstring &value) {
            *this << SizeT (value.size ());
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                wchar_t ch = value[i];
                if (endianness == GuestEndian) {
                    ch = ByteSwap<HostEndian, GuestEndian> (ch);
                }
                if (Write (&ch, WCHAR_T_SIZE) != WCHAR_T_SIZE) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Write (&ch, "
                        THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                        WCHAR_T_SIZE,
                        WCHAR_T_SIZE);
                }
            }
            return *this;
        }

        Serializer &Serializer::operator >> (std::wstring &value) {
            SizeT length;
            *this >> length;
            if (length > 0) {
                std::wstring temp (length, 0);
                for (std::size_t i = 0; i < length; ++i) {
                    wchar_t ch;
                    if (Read (&ch, WCHAR_T_SIZE) != WCHAR_T_SIZE) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Read (&ch, "
                            THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                            WCHAR_T_SIZE,
                            WCHAR_T_SIZE);
                    }
                    if (endianness == GuestEndian) {
                        ch = ByteSwap<GuestEndian, HostEndian> (ch);
                    }
                    temp[i] = ch;
                }
                value.swap (temp);
            }
            else {
                value.clear ();
            }
            return *this;
        }

    #if defined (TOOLCHAIN_OS_Windows) || defined (THEKOGANS_UTIL_HAVE_MMAP)
        Serializer &Serializer::operator << (const SecureString &value) {
            *this << SizeT (value.size ());
            if (value.size () > 0) {
                if (Write (value.c_str (), value.size ()) != value.size ()) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Write (value.c_str (), "
                        THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                        value.size (),
                        value.size ());
                }
            }
            return *this;
        }

        Serializer &Serializer::operator >> (SecureString &value) {
            SizeT length;
            *this >> length;
            if (length > 0) {
                SecureString temp (length, 0);
                if (Read (&temp[0], length) != length) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Read (&value[0], "
                        THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                        length,
                        length);
                }
                value.swap (temp);
            }
            else {
                value.clear ();
            }
            return *this;
        }
    #endif // defined (TOOLCHAIN_OS_Windows) || defined (THEKOGANS_UTIL_HAVE_MMAP)

        Serializer &Serializer::operator << (i8 value) {
            if (Write (&value, I8_SIZE) != I8_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    I8_SIZE,
                    I8_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (i8 &value) {
            if (Read (&value, I8_SIZE) != I8_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    I8_SIZE,
                    I8_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator << (ui8 value) {
            if (Write (&value, UI8_SIZE) != UI8_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    UI8_SIZE,
                    UI8_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (ui8 &value) {
            if (Read (&value, UI8_SIZE) != UI8_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    UI8_SIZE,
                    UI8_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator << (i16 value) {
            if (endianness == GuestEndian) {
                value = ByteSwap<HostEndian, GuestEndian> (value);
            }
            if (Write (&value, I16_SIZE) != I16_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    I16_SIZE,
                    I16_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (i16 &value) {
            if (Read (&value, I16_SIZE) != I16_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    I16_SIZE,
                    I16_SIZE);
            }
            if (endianness == GuestEndian) {
                value = ByteSwap<GuestEndian, HostEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (ui16 value) {
            if (endianness == GuestEndian) {
                value = ByteSwap<HostEndian, GuestEndian> (value);
            }
            if (Write (&value, UI16_SIZE) != UI16_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    UI16_SIZE,
                    UI16_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (ui16 &value) {
            if (Read (&value, UI16_SIZE) != UI16_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    UI16_SIZE,
                    UI16_SIZE);
            }
            if (endianness == GuestEndian) {
                value = ByteSwap<GuestEndian, HostEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (i32 value) {
            if (endianness == GuestEndian) {
                value = ByteSwap<HostEndian, GuestEndian> (value);
            }
            if (Write (&value, I32_SIZE) != I32_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    I32_SIZE,
                    I32_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (i32 &value) {
            if (Read (&value, I32_SIZE) != I32_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    I32_SIZE,
                    I32_SIZE);
            }
            if (endianness == GuestEndian) {
                value = ByteSwap<GuestEndian, HostEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (ui32 value) {
            if (endianness == GuestEndian) {
                value = ByteSwap<HostEndian, GuestEndian> (value);
            }
            if (Write (&value, UI32_SIZE) != UI32_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    UI32_SIZE,
                    UI32_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (ui32 &value) {
            if (Read (&value, UI32_SIZE) != UI32_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    UI32_SIZE,
                    UI32_SIZE);
            }
            if (endianness == GuestEndian) {
                value = ByteSwap<GuestEndian, HostEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (i64 value) {
            if (endianness == GuestEndian) {
                value = ByteSwap<HostEndian, GuestEndian> (value);
            }
            if (Write (&value, I64_SIZE) != I64_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    I64_SIZE,
                    I64_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (i64 &value) {
            if (Read (&value, I64_SIZE) != I64_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    I64_SIZE,
                    I64_SIZE);
            }
            if (endianness == GuestEndian) {
                value = ByteSwap<GuestEndian, HostEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (ui64 value) {
            if (endianness == GuestEndian) {
                value = ByteSwap<HostEndian, GuestEndian> (value);
            }
            if (Write (&value, UI64_SIZE) != UI64_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    UI64_SIZE,
                    UI64_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (ui64 &value) {
            if (Read (&value, UI64_SIZE) != UI64_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    UI64_SIZE,
                    UI64_SIZE);
            }
            if (endianness == GuestEndian) {
                value = ByteSwap<GuestEndian, HostEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (ui128 value) {
            if (endianness == GuestEndian) {
                value = ByteSwap<HostEndian, GuestEndian> (value);
            }
            if (Write (&value, UI128_SIZE) != UI128_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    UI128_SIZE,
                    UI128_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (ui128 &value) {
            if (Read (&value, UI128_SIZE) != UI128_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    UI128_SIZE,
                    UI128_SIZE);
            }
            if (endianness == GuestEndian) {
                value = ByteSwap<GuestEndian, HostEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (f32 value) {
            if (endianness == GuestEndian) {
                value = ByteSwap<HostEndian, GuestEndian> (value);
            }
            if (Write (&value, F32_SIZE) != F32_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    F32_SIZE,
                    F32_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (f32 &value) {
            if (Read (&value, F32_SIZE) != F32_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    F32_SIZE,
                    F32_SIZE);
            }
            if (endianness == GuestEndian) {
                value = ByteSwap<GuestEndian, HostEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (f64 value) {
            if (endianness == GuestEndian) {
                value = ByteSwap<HostEndian, GuestEndian> (value);
            }
            if (Write (&value, F64_SIZE) != F64_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    F64_SIZE,
                    F64_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (f64 &value) {
            if (Read (&value, F64_SIZE) != F64_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    F64_SIZE,
                    F64_SIZE);
            }
            if (endianness == GuestEndian) {
                value = ByteSwap<GuestEndian, HostEndian> (value);
            }
            return *this;
        }

        std::size_t Serializer::Size (const TimeSpec &timeSpec) const noexcept {
            return TimeSpec::SIZE;
        }

        Serializer &Serializer::operator << (const TimeSpec &timeSpec) {
            *this << timeSpec.seconds << timeSpec.nanoseconds;
            return *this;
        }

        Serializer &Serializer::operator >> (TimeSpec &timeSpec) {
            *this >> timeSpec.seconds >> timeSpec.nanoseconds;
            return *this;
        }

        std::size_t Serializer::Size (const SerializableHeader &header) noexcept {
            std::size_t size = 0;
            if (context.NeedType ()) {
                DynamicCreatable::TypeDictionary::SharedPtr dictionary = GetTypeDictionary ();
                if (dictionary != nullptr) {
                    DynamicCreatable::TypeDictionary::IdType id =
                        dictionary->GetOrCreateTypeId (header.type.c_str ());
                    size += id.Size ();
                }
                else {
                    size += Size (header.type);
                }
            }
            if (context.NeedVersion ()) {
                size += Size (header.version);
            }
            if (context.NeedSize ()) {
                size += Size (header.size);
            }
            return size;
        }

        Serializer &Serializer::operator << (const SerializableHeader &header) {
            if (context.NeedType ()) {
                DynamicCreatable::TypeDictionary::SharedPtr dictionary = GetTypeDictionary ();
                if (dictionary != nullptr) {
                    *this << dictionary->GetOrCreateTypeId (header.type.c_str ());
                }
                else {
                    *this << header.type;
                }
            }
            if (context.NeedVersion ()) {
                *this << header.version;
            }
            if (context.NeedSize ()) {
                *this << header.size;
            }
            return *this;
        }

        Serializer &Serializer::operator >> (SerializableHeader &header) {
            if (context.NeedType ()) {
                DynamicCreatable::TypeDictionary::SharedPtr dictionary = GetTypeDictionary ();
                if (dictionary != nullptr) {
                    DynamicCreatable::TypeDictionary::IdType id;
                    *this >> id;
                    header.type = dictionary->ResolveTypeId (id);
                }
                else {
                    *this >> header.type;
                }
            }
            else {
                header.type = context.type;
            }
            if (context.NeedVersion ()) {
                *this >> header.version;
            }
            else {
                header.version = context.version;
            }
            if (context.NeedSize ()) {
                *this >> header.size;
            }
            else {
                header.size = context.size;
            }
            return *this;
        }

        Serializer &Serializer::operator << (const Attribute &value) {
            *this << value.first << value.second;
            return *this;
        }

        Serializer &Serializer::operator >> (Attribute &value) {
            *this >> value.first >> value.second;
            return *this;
        }

    } //namespace util
} // namespace thekogans
