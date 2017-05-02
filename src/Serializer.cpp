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
#include "thekogans/util/GUID.h"
#include "thekogans/util/Fraction.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/Variant.h"
#include "thekogans/util/Buffer.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        Serializer &Serializer::operator << (bool value) {
            ui8 b = value ? 1 : 0;
            ui32 size = Size (value);
            if (Write (&b, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&b, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (bool &value) {
            ui8 b;
            ui32 size = Size (value);
            if (Read (&b, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&b, %u) != %u", size, size);
            }
            value = b == 1;
            return *this;
        }

        Serializer &Serializer::operator << (const char *value) {
            ui32 size = Size (value);
            if (Write (value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (value, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (char *value) {
            for (;;) {
                ui32 size = sizeof (char);
                if (Read (value, size) != size) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Read (value, %u) != %u", size, size);
                }
                if (*value == 0) {
                    break;
                }
                ++value;
            }
            return *this;
        }

        Serializer &Serializer::operator << (const std::string &value) {
            ui32 length = (ui32)value.size ();
            *this << length;
            if (length > 0) {
                if (Write (value.c_str (), length) != length) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Write (value.c_str (), %u) != %u", length, length);
                }
            }
            return *this;
        }

        Serializer &Serializer::operator >> (std::string &value) {
            ui32 length;
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
            ui32 size = Size (value);
            if (Write (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (i8 &value) {
            ui32 size = Size (value);
            if (Read (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator << (ui8 value) {
            ui32 size = Size (value);
            if (Write (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (ui8 &value) {
            ui32 size = Size (value);
            if (Read (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator << (i16 value) {
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            ui32 size = Size (value);
            if (Write (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (i16 &value) {
            ui32 size = Size (value);
            if (Read (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", size, size);
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
            ui32 size = Size (value);
            if (Write (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (ui16 &value) {
            ui32 size = Size (value);
            if (Read (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", size, size);
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
            ui32 size = Size (value);
            if (Write (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (i32 &value) {
            ui32 size = Size (value);
            if (Read (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", size, size);
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
            ui32 size = Size (value);
            if (Write (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (ui32 &value) {
            ui32 size = Size (value);
            if (Read (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", size, size);
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
            ui32 size = Size (value);
            if (Write (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (i64 &value) {
            ui32 size = Size (value);
            if (Read (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", size, size);
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
            ui32 size = Size (value);
            if (Write (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (ui64 &value) {
            ui32 size = Size (value);
            if (Read (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", size, size);
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
            ui32 size = Size (value);
            if (Write (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (f32 &value) {
            ui32 size = Size (value);
            if (Read (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", size, size);
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
            ui32 size = Size (value);
            if (Write (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (&value, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (f64 &value) {
            ui32 size = Size (value);
            if (Read (&value, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (&value, %u) != %u", size, size);
            }
            if (endianness != HostEndian) {
                value = ByteSwap<LittleEndian, BigEndian> (value);
            }
            return *this;
        }

        Serializer &Serializer::operator << (const Fraction &value) {
            *this << value.numerator << value.denominator << (ui32)value.sign;
            return *this;
        }

        Serializer & Serializer::operator >> (Fraction &value) {
            ui32 sign;
            *this >> value.numerator >> value.denominator >> sign;
            value.sign = static_cast<Fraction::Sign> (sign);
            return *this;
        }

        Serializer &Serializer::operator << (const TimeSpec &value) {
            i64 sec = value.tv_sec;
            i64 nsec = value.tv_nsec;
            *this << sec << nsec;
            return *this;
        }

        Serializer & Serializer::operator >> (TimeSpec &value) {
            i64 sec = value.tv_sec;
            i64 nsec = value.tv_nsec;
            *this >> sec  >> nsec;
            value.tv_sec = sec;
            value.tv_nsec = (long)nsec;
            return *this;
        }

        ui32 Serializer::Size (const GUID &value) {
            return GUID::LENGTH;
        }

        Serializer &Serializer::operator << (const GUID &value) {
            ui32 size = GUID::LENGTH;
            if (Write (value.data, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (value.data, %u) != %u", size, size);
            }
            return *this;
        }

        Serializer &Serializer::operator >> (GUID &value) {
            ui32 size = GUID::LENGTH;
            if (Read (value.data, size) != size) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (value.data, %u) != %u", size, size);
            }
            return *this;
        }

        ui32 Serializer::Size (const Variant &value) {
            switch (value.type) {
                case Variant::TYPE_bool:
                    return Size (value.value._bool);
                case Variant::TYPE_i8:
                    return Size (value.value._i8);
                case Variant::TYPE_ui8:
                    return Size (value.value._ui8);
                case Variant::TYPE_i16:
                    return Size (value.value._i16);
                case Variant::TYPE_ui16:
                    return Size (value.value._ui16);
                case Variant::TYPE_i32:
                    return Size (value.value._i32);
                case Variant::TYPE_ui32:
                    return Size (value.value._ui32);
                case Variant::TYPE_i64:
                    return Size (value.value._i64);
                case Variant::TYPE_ui64:
                    return Size (value.value._ui64);
                case Variant::TYPE_f32:
                    return Size (value.value._f32);
                case Variant::TYPE_f64:
                    return Size (value.value._f64);
                case Variant::TYPE_string:
                    return Size (*value.value._string);
                case Variant::TYPE_GUID:
                    return Size (*value.value._guid);
            }
            return 0;
        }

        Serializer &Serializer::operator << (const Variant &value) {
            *this << value.type;
            switch (value.type) {
                case Variant::TYPE_bool:
                    *this << value.value._bool;
                    break;
                case Variant::TYPE_i8:
                    *this << value.value._i8;
                    break;
                case Variant::TYPE_ui8:
                    *this << value.value._ui8;
                    break;
                case Variant::TYPE_i16:
                    *this << value.value._i16;
                    break;
                case Variant::TYPE_ui16:
                    *this << value.value._ui16;
                    break;
                case Variant::TYPE_i32:
                    *this << value.value._i32;
                    break;
                case Variant::TYPE_ui32:
                    *this << value.value._ui32;
                    break;
                case Variant::TYPE_i64:
                    *this << value.value._i64;
                    break;
                case Variant::TYPE_ui64:
                    *this << value.value._ui64;
                    break;
                case Variant::TYPE_f32:
                    *this << value.value._f32;
                    break;
                case Variant::TYPE_f64:
                    *this << value.value._f64;
                    break;
                case Variant::TYPE_string:
                    assert (value.value._string != 0);
                    *this << *value.value._string;
                    break;
                case Variant::TYPE_GUID:
                    assert (value.value._string != 0);
                    *this << *value.value._guid;
                    break;
            }
            return *this;
        }

        Serializer &Serializer::operator >> (Variant &value) {
            value.Clear ();
            *this >> value.type;
            switch (value.type) {
                case Variant::TYPE_bool:
                    *this >> value.value._bool;
                    break;
                case Variant::TYPE_i8:
                    *this >> value.value._i8;
                    break;
                case Variant::TYPE_ui8:
                    *this >> value.value._ui8;
                    break;
                case Variant::TYPE_i16:
                    *this >> value.value._i16;
                    break;
                case Variant::TYPE_ui16:
                    *this >> value.value._ui16;
                    break;
                case Variant::TYPE_i32:
                    *this >> value.value._i32;
                    break;
                case Variant::TYPE_ui32:
                    *this >> value.value._ui32;
                    break;
                case Variant::TYPE_i64:
                    *this >> value.value._i64;
                    break;
                case Variant::TYPE_ui64:
                    *this >> value.value._ui64;
                    break;
                case Variant::TYPE_f32:
                    *this >> value.value._f32;
                    break;
                case Variant::TYPE_f64:
                    *this >> value.value._f64;
                    break;
                case Variant::TYPE_string: {
                    value.value._string = new std::string ();
                    *this >> *value.value._string;
                    break;
                }
                case Variant::TYPE_GUID: {
                    value.value._guid = new GUID ();
                    *this >> *value.value._guid;
                    break;
                }
            }
            return *this;
        }

        ui32 Serializer::Size (const Buffer &value) {
            return BufferHeader::SIZE + value.length * UI8_SIZE;
        }

        Serializer &Serializer::operator << (const Buffer &value) {
            *this << BufferHeader (
                value.endianness,
                value.length,
                value.readOffset,
                value.writeOffset);
            if (value.length != 0) {
                ui32 size = value.length * UI8_SIZE;
                if (Write (value.data, size) != size) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Write (value.data, %u) != %u", size, size);
                }
            }
            return *this;
        }

        Serializer &Serializer::operator >> (Buffer &value) {
            BufferHeader bufferHeader;
            *this >> bufferHeader;
            value.endianness = (Endianness)bufferHeader.endianness;
            value.Resize (bufferHeader.length);
            value.readOffset = bufferHeader.readOffset;
            value.writeOffset = bufferHeader.writeOffset;
            if (value.length != 0) {
                ui32 size = value.length * UI8_SIZE;
                if (Read (value.data, size) != size) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Read (value.data, %u) != %u", size, size);
                }
            }
            return *this;
        }

        Serializer &Serializer::operator << (const std::vector<i8> &value) {
            ui32 length = (ui32)value.size ();
            *this << length;
            if (length > 0) {
                ui32 size = length * I8_SIZE;
                if (Write (&value[0], size) != size) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Write (&value[0], %u) != %u", size, size);
                }
            }
            return *this;
        }

        Serializer &Serializer::operator >> (std::vector<i8> &value) {
            ui32 length;
            *this >> length;
            if (length > 0) {
                value.resize (length);
                ui32 size = length * I8_SIZE;
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
            ui32 length = (ui32)value.size ();
            *this << length;
            if (length > 0) {
                ui32 size = length * UI8_SIZE;
                if (Write (&value[0], size) != size) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Write (&value[0], %u) != %u", size, size);
                }
            }
            return *this;
        }

        Serializer &Serializer::operator >> (std::vector<ui8> &value) {
            ui32 length;
            *this >> length;
            if (length > 0) {
                value.resize (length);
                ui32 size = length * UI8_SIZE;
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
