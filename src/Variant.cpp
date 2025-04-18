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

#include <cstdlib>
#include <cmath>
#include <sstream>
#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Config.h"
#include "thekogans/util/Variant.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Hash.h"
#include "thekogans/util/XMLUtils.h"

namespace thekogans {
    namespace util {

        const char * const Variant::TAG_VARIANT = "Variant";
        const char * const Variant::TAG_TYPE = "Type";
        const char * const Variant::ATTR_TYPE = "Type";
        const char * const Variant::VALUE_INVALID = "invalid";
        const char * const Variant::VALUE_BOOL = "bool";
        const char * const Variant::VALUE_I8 = "i8";
        const char * const Variant::VALUE_UI8 = "ui8";
        const char * const Variant::VALUE_I16 = "i16";
        const char * const Variant::VALUE_UI16 = "ui16";
        const char * const Variant::VALUE_I32 = "i32";
        const char * const Variant::VALUE_UI32 = "ui32";
        const char * const Variant::VALUE_I64 = "i64";
        const char * const Variant::VALUE_UI64 = "ui64";
        const char * const Variant::VALUE_F32 = "f32";
        const char * const Variant::VALUE_F64 = "f64";
        const char * const Variant::VALUE_SizeT = "SizeT";
        const char * const Variant::VALUE_STRING = "string";
        const char * const Variant::VALUE_GUID = "GUID";
        const char * const Variant::TAG_VALUE = "Value";
        const char * const Variant::ATTR_VALUE = "Value";

        Variant::Variant (const Variant &variant) :
                type (variant.type) {
            switch (variant.type) {
                case Variant::TYPE_Invalid:
                    break;
                case Variant::TYPE_bool:
                    value._bool = variant.value._bool;
                    break;
                case Variant::TYPE_i8:
                    value._i8 = variant.value._i8;
                    break;
                case Variant::TYPE_ui8:
                    value._ui8 = variant.value._ui8;
                    break;
                case Variant::TYPE_i16:
                    value._i16 = variant.value._i16;
                    break;
                case Variant::TYPE_ui16:
                    value._ui16 = variant.value._ui16;
                    break;
                case Variant::TYPE_i32:
                    value._i32 = variant.value._i32;
                    break;
                case Variant::TYPE_ui32:
                    value._ui32 = variant.value._ui32;
                    break;
                case Variant::TYPE_i64:
                    value._i64 = variant.value._i64;
                    break;
                case Variant::TYPE_ui64:
                    value._ui64 = variant.value._ui64;
                    break;
                case Variant::TYPE_f32:
                    value._f32 = variant.value._f32;
                    break;
                case Variant::TYPE_f64:
                    value._f64 = variant.value._f64;
                    break;
                case Variant::TYPE_SizeT:
                    value._SizeT = new SizeT (*variant.value._SizeT);
                    break;
                case Variant::TYPE_string: {
                    value._string = new std::string (*variant.value._string);
                    break;
                }
                case Variant::TYPE_GUID: {
                    value._guid = new GUID (*variant.value._guid);
                    break;
                }
            }
        }

        Variant &Variant::operator = (const Variant &variant) {
            if (&variant != this) {
                Clear ();
                type = variant.type;
                switch (variant.type) {
                    case Variant::TYPE_Invalid:
                        break;
                    case Variant::TYPE_bool:
                        value._bool = variant.value._bool;
                        break;
                    case Variant::TYPE_i8:
                        value._i8 = variant.value._i8;
                        break;
                    case Variant::TYPE_ui8:
                        value._ui8 = variant.value._ui8;
                        break;
                    case Variant::TYPE_i16:
                        value._i16 = variant.value._i16;
                        break;
                    case Variant::TYPE_ui16:
                        value._ui16 = variant.value._ui16;
                        break;
                    case Variant::TYPE_i32:
                        value._i32 = variant.value._i32;
                        break;
                    case Variant::TYPE_ui32:
                        value._ui32 = variant.value._ui32;
                        break;
                    case Variant::TYPE_i64:
                        value._i64 = variant.value._i64;
                        break;
                    case Variant::TYPE_ui64:
                        value._ui64 = variant.value._ui64;
                        break;
                    case Variant::TYPE_f32:
                        value._f32 = variant.value._f32;
                        break;
                    case Variant::TYPE_f64:
                        value._f64 = variant.value._f64;
                        break;
                    case Variant::TYPE_SizeT:
                        value._SizeT = new SizeT (*variant.value._SizeT);
                        break;
                    case Variant::TYPE_string: {
                        value._string = new std::string (*variant.value._string);
                        break;
                    }
                    case Variant::TYPE_GUID: {
                        value._guid = new GUID (*variant.value._guid);
                        break;
                    }
                }
            }
            return *this;
        }

        std::string Variant::TypeTostring (Type type) {
            return
                type == TYPE_bool ? VALUE_BOOL :
                type == TYPE_i8 ? VALUE_I8 :
                type == TYPE_ui8 ? VALUE_UI8 :
                type == TYPE_i16 ? VALUE_I16 :
                type == TYPE_ui16 ? VALUE_UI16 :
                type == TYPE_i32 ? VALUE_I32 :
                type == TYPE_ui32 ? VALUE_UI32 :
                type == TYPE_i64 ? VALUE_I64 :
                type == TYPE_ui64 ? VALUE_UI64 :
                type == TYPE_f32 ? VALUE_F32 :
                type == TYPE_f64 ? VALUE_F64 :
                type == TYPE_SizeT ? VALUE_SizeT :
                type == TYPE_string ? VALUE_STRING :
                type == TYPE_GUID ? VALUE_GUID : VALUE_INVALID;
        }

        Variant::Type Variant::stringToType (const std::string &type) {
            return
                type == VALUE_BOOL ? TYPE_bool :
                type == VALUE_I8 ? TYPE_i8 :
                type == VALUE_UI8 ? TYPE_ui8 :
                type == VALUE_I16 ? TYPE_i16 :
                type == VALUE_UI16 ? TYPE_ui16 :
                type == VALUE_I32 ? TYPE_i32 :
                type == VALUE_UI32 ? TYPE_ui32 :
                type == VALUE_I64 ? TYPE_i64 :
                type == VALUE_UI64 ? TYPE_ui64 :
                type == VALUE_F32 ? TYPE_f32 :
                type == VALUE_F64 ? TYPE_f64 :
                type == VALUE_SizeT ? TYPE_SizeT :
                type == VALUE_STRING ? TYPE_string :
                type == VALUE_GUID ? TYPE_GUID : TYPE_Invalid;
        }

        std::size_t Variant::Size () const {
            std::size_t size = UI32_SIZE;
            switch (type) {
                case Variant::TYPE_Invalid:
                    break;
                case Variant::TYPE_bool:
                    size += UI8_SIZE;
                    break;
                case Variant::TYPE_i8:
                    size += I8_SIZE;
                    break;
                case Variant::TYPE_ui8:
                    size += UI8_SIZE;
                    break;
                case Variant::TYPE_i16:
                    size += I16_SIZE;
                    break;
                case Variant::TYPE_ui16:
                    size += UI16_SIZE;
                    break;
                case Variant::TYPE_i32:
                    size += I32_SIZE;
                    break;
                case Variant::TYPE_ui32:
                    size += UI32_SIZE;
                    break;
                case Variant::TYPE_i64:
                    size += I64_SIZE;
                    break;
                case Variant::TYPE_ui64:
                    size += UI64_SIZE;
                    break;
                case Variant::TYPE_f32:
                    size += F32_SIZE;
                    break;
                case Variant::TYPE_f64:
                    size += F64_SIZE;
                    break;
                case Variant::TYPE_SizeT:
                    assert (value._SizeT != nullptr);
                    size += Serializer::Size (*value._SizeT);
                    break;
                case Variant::TYPE_string: {
                    assert (value._string != nullptr);
                    size += Serializer::Size (*value._string);
                    break;
                }
                case Variant::TYPE_GUID: {
                    size += GUID::SIZE;
                    break;
                }
            }
            return size;
        }

        void Variant::Clear () {
            if (type == TYPE_SizeT) {
                delete value._SizeT;
                value._SizeT = nullptr;
            }
            if (type == TYPE_string) {
                delete value._string;
                value._string = nullptr;
            }
            else if (type == TYPE_GUID) {
                delete value._guid;
                value._guid = nullptr;
            }
            type = TYPE_Invalid;
        }

        int Variant::Compare (const Variant &variant) const {
            assert (type == variant.type);
            switch (type) {
                case Variant::TYPE_Invalid:
                    return 0;
                case Variant::TYPE_bool:
                    return !value._bool && variant.value._bool ?
                        -1 : value._bool && !variant.value._bool ? 1 : 0;
                case Variant::TYPE_i8:
                    return value._i8 < variant.value._i8 ?
                        -1 : value._i8 > variant.value._i8 ? 1 : 0;
                case Variant::TYPE_ui8:
                    return value._ui8 < variant.value._ui8 ?
                        -1 : value._ui8 > variant.value._ui8 ? 1 : 0;
                case Variant::TYPE_i16:
                    return value._i16 < variant.value._i16 ?
                        -1 : value._i16 > variant.value._i16 ? 1 : 0;
                case Variant::TYPE_ui16:
                    return value._ui16 < variant.value._ui16 ?
                        -1 : value._ui16 > variant.value._ui16 ? 1 : 0;
                case Variant::TYPE_i32:
                    return value._i32 < variant.value._i32 ?
                        -1 : value._i32 > variant.value._i32 ? 1 : 0;
                case Variant::TYPE_ui32:
                    return value._ui32 < variant.value._ui32 ?
                        -1 : value._ui32 > variant.value._ui32 ? 1 : 0;
                case Variant::TYPE_i64:
                    return value._i64 < variant.value._i64 ?
                        -1 : value._i64 > variant.value._i64 ? 1 : 0;
                case Variant::TYPE_ui64:
                    return value._ui64 < variant.value._ui64 ?
                        -1 : value._ui64 > variant.value._ui64 ? 1 : 0;
                case Variant::TYPE_f32:
                    return value._f32 < variant.value._f32 ?
                        -1 : value._f32 > variant.value._f32 ? 1 : 0;
                case Variant::TYPE_f64:
                    return value._f64 < variant.value._f64 ?
                        -1 : value._f64 > variant.value._f64 ? 1 : 0;
                case Variant::TYPE_SizeT:
                    return *value._SizeT < *variant.value._SizeT ?
                        -1 : *value._SizeT > *variant.value._SizeT ? 1 : 0;
                case Variant::TYPE_string:
                    return StringCompareIgnoreCase (value._string->c_str (),
                        variant.value._string->c_str ());
                case Variant::TYPE_GUID:
                    return *value._guid < *variant.value._guid ?
                        -1 : *value._guid > *variant.value._guid ? 1 : 0;
            }
            // Invalid are treated as same.
            return 0;
        }

        int Variant::PrefixCompare (const Variant &variant) const {
            assert (type == variant.type);
            switch (type) {
                case Variant::TYPE_Invalid:
                    return 0;
                case Variant::TYPE_bool:
                    return !value._bool && variant.value._bool ?
                        -1 : value._bool && !variant.value._bool ? 1 : 0;
                case Variant::TYPE_i8:
                    return value._i8 < variant.value._i8 ?
                        -1 : value._i8 > variant.value._i8 ? 1 : 0;
                case Variant::TYPE_ui8:
                    return value._ui8 < variant.value._ui8 ?
                        -1 : value._ui8 > variant.value._ui8 ? 1 : 0;
                case Variant::TYPE_i16:
                    return value._i16 < variant.value._i16 ?
                        -1 : value._i16 > variant.value._i16 ? 1 : 0;
                case Variant::TYPE_ui16:
                    return value._ui16 < variant.value._ui16 ?
                        -1 : value._ui16 > variant.value._ui16 ? 1 : 0;
                case Variant::TYPE_i32:
                    return value._i32 < variant.value._i32 ?
                        -1 : value._i32 > variant.value._i32 ? 1 : 0;
                case Variant::TYPE_ui32:
                    return value._ui32 < variant.value._ui32 ?
                        -1 : value._ui32 > variant.value._ui32 ? 1 : 0;
                case Variant::TYPE_i64:
                    return value._i64 < variant.value._i64 ?
                        -1 : value._i64 > variant.value._i64 ? 1 : 0;
                case Variant::TYPE_ui64:
                    return value._ui64 < variant.value._ui64 ?
                        -1 : value._ui64 > variant.value._ui64 ? 1 : 0;
                case Variant::TYPE_f32:
                    return value._f32 < variant.value._f32 ?
                        -1 : value._f32 > variant.value._f32 ? 1 : 0;
                case Variant::TYPE_f64:
                    return value._f64 < variant.value._f64 ?
                        -1 : value._f64 > variant.value._f64 ? 1 : 0;
                case Variant::TYPE_SizeT:
                    return *value._SizeT < *variant.value._SizeT ?
                        -1 : *value._SizeT > *variant.value._SizeT ? 1 : 0;
                case Variant::TYPE_string:
                    return StringCompareIgnoreCase (value._string->c_str (),
                        variant.value._string->c_str (), value._string->size ());
                case Variant::TYPE_GUID:
                    return *value._guid < *variant.value._guid ?
                        -1 : *value._guid > *variant.value._guid ? 1 : 0;
            }
            // Invalid are treated as same.
            return 0;
        }

        void Variant::Parse (const pugi::xml_node &node) {
            Clear ();
            type = stringToType (node.attribute (ATTR_TYPE).value ());
            switch (type) {
                case Variant::TYPE_Invalid:
                    break;
                case Variant::TYPE_bool:
                    value._bool = std::string (node.attribute (ATTR_VALUE).value ()) == XML_TRUE;
                    break;
                case Variant::TYPE_i8:
                    value._i8 = stringToi8 (node.attribute (ATTR_VALUE).value ());
                    break;
                case Variant::TYPE_ui8:
                    value._ui8 = stringToui8 (node.attribute (ATTR_VALUE).value ());
                    break;
                case Variant::TYPE_i16:
                    value._i16 = stringToi16 (node.attribute (ATTR_VALUE).value ());
                    break;
                case Variant::TYPE_ui16:
                    value._ui16 = stringToui16 (node.attribute (ATTR_VALUE).value ());
                    break;
                case Variant::TYPE_i32:
                    value._i32 = stringToi32 (node.attribute (ATTR_VALUE).value ());
                    break;
                case Variant::TYPE_ui32:
                    value._ui32 = stringToui32 (node.attribute (ATTR_VALUE).value ());
                    break;
                case Variant::TYPE_i64:
                    value._i64 = stringToi64 (node.attribute (ATTR_VALUE).value ());
                    break;
                case Variant::TYPE_ui64:
                    value._ui64 = stringToui64 (node.attribute (ATTR_VALUE).value ());
                    break;
                case Variant::TYPE_f32:
                    value._f32 = stringTof32 (node.attribute (ATTR_VALUE).value ());
                    break;
                case Variant::TYPE_f64:
                    value._f64 = stringTof64 (node.attribute (ATTR_VALUE).value ());
                    break;
                case Variant::TYPE_SizeT:
                    value._SizeT = new SizeT (stringTosize_t (node.attribute (ATTR_VALUE).value ()));
                    break;
                case Variant::TYPE_string:
                    value._string = new std::string (node.attribute (ATTR_VALUE).value ());
                    break;
                case Variant::TYPE_GUID:
                    value._guid = new GUID (
                        GUID::FromHexString (node.attribute (ATTR_VALUE).value ()));
                    break;
            }
        }

        std::string Variant::ToString (
                const char *tagName,
                std::size_t indentationLevel,
                std::size_t indentationWidth) const {
            std::string str;
            switch (type) {
                case Variant::TYPE_Invalid:
                    break;
                case Variant::TYPE_bool:
                    str = value._bool ? XML_TRUE : XML_FALSE;
                    break;
                case Variant::TYPE_i8:
                    str = i32Tostring (value._i8);
                    break;
                case Variant::TYPE_ui8:
                    str = ui32Tostring (value._ui8);
                    break;
                case Variant::TYPE_i16:
                    str = i32Tostring (value._i16);
                    break;
                case Variant::TYPE_ui16:
                    str = ui32Tostring (value._ui16);
                    break;
                case Variant::TYPE_i32:
                    str = i32Tostring (value._i32);
                    break;
                case Variant::TYPE_ui32:
                    str = ui32Tostring (value._ui32);
                    break;
                case Variant::TYPE_i64:
                    str = i64Tostring (value._i64);
                    break;
                case Variant::TYPE_ui64:
                    str = ui64Tostring (value._ui64);
                    break;
                case Variant::TYPE_f32:
                    str = f32Tostring (value._f32);
                    break;
                case Variant::TYPE_f64:
                    str = f64Tostring (value._f64);
                    break;
                case Variant::TYPE_SizeT:
                    str = size_tTostring (*value._SizeT);
                    break;
                case Variant::TYPE_string:
                    str = Encodestring (*value._string);
                    break;
                case Variant::TYPE_GUID:
                    str = value._guid->ToHexString ();
                    break;
            }
            Attributes attributes;
            attributes.push_back (Attribute (ATTR_TYPE, TypeTostring (type)));
            attributes.push_back (Attribute (ATTR_VALUE, str));
            return OpenTag (indentationLevel, tagName, attributes, false, true);
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator < (
                const Variant &variant1, const Variant &variant2) {
            return variant1.Compare (variant2) == -1;
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator > (
                const Variant &variant1, const Variant &variant2) {
            return variant1.Compare (variant2) == 1;
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator == (
                const Variant &variant1, const Variant &variant2) {
            return variant1.Compare (variant2) == 0;
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator != (
                const Variant &variant1, const Variant &variant2) {
            return variant1.Compare (variant2) != 0;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const Variant &variant) {
            ui32 type = variant.type;
            serializer << type;
            switch (variant.type) {
                case Variant::TYPE_Invalid:
                    break;
                case Variant::TYPE_bool:
                    serializer << variant.value._bool;
                    break;
                case Variant::TYPE_i8:
                    serializer << variant.value._i8;
                    break;
                case Variant::TYPE_ui8:
                    serializer << variant.value._ui8;
                    break;
                case Variant::TYPE_i16:
                    serializer << variant.value._i16;
                    break;
                case Variant::TYPE_ui16:
                    serializer << variant.value._ui16;
                    break;
                case Variant::TYPE_i32:
                    serializer << variant.value._i32;
                    break;
                case Variant::TYPE_ui32:
                    serializer << variant.value._ui32;
                    break;
                case Variant::TYPE_i64:
                    serializer << variant.value._i64;
                    break;
                case Variant::TYPE_ui64:
                    serializer << variant.value._ui64;
                    break;
                case Variant::TYPE_f32:
                    serializer << variant.value._f32;
                    break;
                case Variant::TYPE_f64:
                    serializer << variant.value._f64;
                    break;
                case Variant::TYPE_SizeT:
                    assert (variant.value._SizeT != nullptr);
                    serializer << *variant.value._SizeT;
                    break;
                case Variant::TYPE_string:
                    assert (variant.value._string != nullptr);
                    serializer << *variant.value._string;
                    break;
                case Variant::TYPE_GUID:
                    assert (variant.value._string != nullptr);
                    serializer << *variant.value._guid;
                    break;
            }
            return serializer;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Variant &variant) {
            variant.Clear ();
            ui32 type;
            serializer >> type;
            variant.type = (Variant::Type)type;
            switch (variant.type) {
                case Variant::TYPE_Invalid:
                    break;
                case Variant::TYPE_bool:
                    serializer >> variant.value._bool;
                    break;
                case Variant::TYPE_i8:
                    serializer >> variant.value._i8;
                    break;
                case Variant::TYPE_ui8:
                    serializer >> variant.value._ui8;
                    break;
                case Variant::TYPE_i16:
                    serializer >> variant.value._i16;
                    break;
                case Variant::TYPE_ui16:
                    serializer >> variant.value._ui16;
                    break;
                case Variant::TYPE_i32:
                    serializer >> variant.value._i32;
                    break;
                case Variant::TYPE_ui32:
                    serializer >> variant.value._ui32;
                    break;
                case Variant::TYPE_i64:
                    serializer >> variant.value._i64;
                    break;
                case Variant::TYPE_ui64:
                    serializer >> variant.value._ui64;
                    break;
                case Variant::TYPE_f32:
                    serializer >> variant.value._f32;
                    break;
                case Variant::TYPE_f64:
                    serializer >> variant.value._f64;
                    break;
                case Variant::TYPE_SizeT: {
                    variant.value._SizeT = new SizeT;
                    serializer >> *variant.value._SizeT;
                    break;
                }
                case Variant::TYPE_string: {
                    variant.value._string = new std::string;
                    serializer >> *variant.value._string;
                    break;
                }
                case Variant::TYPE_GUID: {
                    variant.value._guid = new GUID;
                    serializer >> *variant.value._guid;
                    break;
                }
            }
            return serializer;
        }

    } // namespace util
} // namespace thekogans
