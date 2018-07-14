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
#include "thekogans/util/Exception.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        Serializer &Serializer::operator << (Endianness value) {
            if (Write (&value, ENDIANNESS_SIZE) != ENDIANNESS_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", ENDIANNESS_SIZE, ENDIANNESS_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (Endianness &value) {
            if (Read (&value, ENDIANNESS_SIZE) != ENDIANNESS_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", ENDIANNESS_SIZE, ENDIANNESS_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator << (bool value) {
            ui8 b = value ? 1 : 0;
            if (Write (&b, BOOL_SIZE) != BOOL_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&b, %u) != %u", BOOL_SIZE, BOOL_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (bool &value) {
            ui8 b;
            if (Read (&b, BOOL_SIZE) != BOOL_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&b, %u) != %u", BOOL_SIZE, BOOL_SIZE);
            }
            value = b == 1;
            return *this;
        }

        std::size_t Serializer::Size (const char *value) {
            if (value != 0) {
                return strlen (value) + 1;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Serializer &Serializer::operator << (const char *value) {
            if (value != 0) {
                std::size_t size = Size (value);
                if (Write (value, size) != size) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Write (value, %u) != %u", size, size);
                }
                return *this;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Serializer &Serializer::operator >> (char *value) {
            if (value != 0) {
                for (;;) {
                    if (Read (value, I8_SIZE) != I8_SIZE) {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Read (value, %u) != %u", I8_SIZE, I8_SIZE);
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
                        "Write (value.c_str (), %u) != %u",
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
                value.resize (length);
                if (Read (&value[0], length) != length) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Read (&value[0], %u) != %u", length, length);
                }
            }
            else {
                value.clear ();
            }
            return *this;
        }

        Serializer &Serializer::operator << (i8 value) {
            if (Write (&value, I8_SIZE) != I8_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", I8_SIZE, I8_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (i8 &value) {
            if (Read (&value, I8_SIZE) != I8_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", I8_SIZE, I8_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator << (ui8 value) {
            if (Write (&value, UI8_SIZE) != UI8_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", UI8_SIZE, UI8_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (ui8 &value) {
            if (Read (&value, UI8_SIZE) != UI8_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", UI8_SIZE, UI8_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator << (i16 value) {
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            if (Write (&value, I16_SIZE) != I16_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", I16_SIZE, I16_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (i16 &value) {
            if (Read (&value, I16_SIZE) != I16_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", I16_SIZE, I16_SIZE);
            }
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (ui16 value) {
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            if (Write (&value, UI16_SIZE) != UI16_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", UI16_SIZE, UI16_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (ui16 &value) {
            if (Read (&value, UI16_SIZE) != UI16_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", UI16_SIZE, UI16_SIZE);
            }
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (i32 value) {
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            if (Write (&value, I32_SIZE) != I32_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", I32_SIZE, I32_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (i32 &value) {
            if (Read (&value, I32_SIZE) != I32_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", I32_SIZE, I32_SIZE);
            }
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (ui32 value) {
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            if (Write (&value, UI32_SIZE) != UI32_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", UI32_SIZE, UI32_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (ui32 &value) {
            if (Read (&value, UI32_SIZE) != UI32_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", UI32_SIZE, UI32_SIZE);
            }
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (i64 value) {
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            if (Write (&value, I64_SIZE) != I64_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", I64_SIZE, I64_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (i64 &value) {
            if (Read (&value, I64_SIZE) != I64_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", I64_SIZE, I64_SIZE);
            }
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (ui64 value) {
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            if (Write (&value, UI64_SIZE) != UI64_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", UI64_SIZE, UI64_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (ui64 &value) {
            if (Read (&value, UI64_SIZE) != UI64_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", UI64_SIZE, UI64_SIZE);
            }
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (f32 value) {
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            if (Write (&value, F32_SIZE) != F32_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", F32_SIZE, F32_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (f32 &value) {
            if (Read (&value, F32_SIZE) != F32_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", F32_SIZE, F32_SIZE);
            }
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (f64 value) {
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            if (Write (&value, F64_SIZE) != F64_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", F64_SIZE, F64_SIZE);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (f64 &value) {
            if (Read (&value, F64_SIZE) != F64_SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", F64_SIZE, F64_SIZE);
            }
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (const std::vector<i8> &value) {
            *this << SizeT (value.size ());
            if (value.size () > 0) {
                std::size_t size = value.size () * I8_SIZE;
                if (Write (&value[0], size) != size) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Write (&value[0], %u) != %u", size, size);
                }
            }
            return *this;
        }

        Serializer &Serializer::operator >> (std::vector<i8> &value) {
            SizeT length;
            *this >> length;
            if (length > 0) {
                value.resize (length);
                std::size_t size = length * I8_SIZE;
                if (Read (&value[0], size) != size) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Read (&value[0], %u) != %u", size, size);
                }
            }
            else {
                value.clear ();
            }
            return *this;
        }

        Serializer &Serializer::operator << (const std::vector<ui8> &value) {
            *this << SizeT (value.size ());
            if (value.size () > 0) {
                std::size_t size = value.size () * UI8_SIZE;
                if (Write (&value[0], size) != size) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Write (&value[0], %u) != %u", size, size);
                }
            }
            return *this;
        }

        Serializer &Serializer::operator >> (std::vector<ui8> &value) {
            SizeT length;
            *this >> length;
            if (length > 0) {
                value.resize (length);
                std::size_t size = length * UI8_SIZE;
                if (Read (&value[0], size) != size) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Read (&value[0], %u) != %u", size, size);
                }
            }
            else {
                value.clear ();
            }
            return *this;
        }

    } //namespace util
} // namespace thekogans
