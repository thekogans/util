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
#include <cstdlib>
#include <cmath>
#include <sstream>
#include "thekogans/util/internal.h"
#include "thekogans/util/Config.h"
#include "thekogans/util/Variant.h"
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
        const char * const Variant::VALUE_STRING = "string";
        const char * const Variant::VALUE_GUID = "guid";
        const char * const Variant::TAG_VALUE = "Value";
        const char * const Variant::ATTR_VALUE = "Value";

        const Variant Variant::Empty;

        Variant::Variant (const Variant &variant) :
                type (variant.type) {
            switch (variant.type) {
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

        std::string Variant::TypeToString (ui32 type) {
            return type == TYPE_bool ? VALUE_BOOL :
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
                type == TYPE_string ? VALUE_STRING :
                type == TYPE_GUID ? VALUE_GUID : VALUE_INVALID;
        }

        ui32 Variant::StringToType (const std::string &type) {
            return type == VALUE_BOOL ? TYPE_bool :
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
                type == VALUE_STRING ? TYPE_string :
                type == VALUE_GUID ? TYPE_GUID : TYPE_Invalid;
        }

        ui32 Variant::Size () const {
            ui32 size = UI32_SIZE;
            switch (type) {
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
                case Variant::TYPE_string: {
                    size += (ui32)(value._string->size () + 1);
                    break;
                }
                case Variant::TYPE_GUID: {
                    size += GUID_SIZE;
                    break;
                }
            }
            return size;
        }

        ui32 Variant::Hash (ui32 radix) const {
            switch (type) {
                case Variant::TYPE_bool:
                    return (value._bool ? 1 : 0) % radix;
                case Variant::TYPE_i8:
                    return value._i8 % radix;
                case Variant::TYPE_ui8:
                    return value._ui8 % radix;
                case Variant::TYPE_i16:
                    return value._i16 % radix;
                case Variant::TYPE_ui16:
                    return value._ui16 % radix;
                case Variant::TYPE_i32:
                    return value._i32 % radix;
                case Variant::TYPE_ui32:
                    return value._ui32 % radix;
                case Variant::TYPE_i64:
                    return value._i64 % radix;
                case Variant::TYPE_ui64:
                    return value._ui64 % radix;
                case Variant::TYPE_f32:
                    return (ui32)fmod (value._f32, (f32)radix);
                case Variant::TYPE_f64:
                    return (ui32)fmod (value._f64, (f64)radix);
                case Variant::TYPE_string:
                    return HashString (*value._string, radix);
                case Variant::TYPE_GUID:
                    return HashString (value._guid->ToString (), radix);
            }
            assert (0);
            return 0;
        }

        void Variant::Clear () {
            if (type == TYPE_string) {
                delete value._string;
                value._string = 0;
            }
            else if (type == TYPE_GUID) {
                delete value._guid;
                value._guid = 0;
            }
            type = TYPE_Invalid;
        }

        int Variant::Compare (const Variant &variant) const {
            assert (type == variant.type);
            switch (type) {
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
                case Variant::TYPE_string:
                    return strcasecmp (value._string->c_str (),
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
                case Variant::TYPE_string:
                    return strncasecmp (value._string->c_str (),
                        variant.value._string->c_str (), value._string->size ());
                case Variant::TYPE_GUID:
                    return *value._guid < *variant.value._guid ?
                        -1 : *value._guid > *variant.value._guid ? 1 : 0;
            }
            // Invalid are treated as same.
            return 0;
        }

    #if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
        void Variant::Parse (const pugi::xml_node &node) {
            Clear ();
            for (pugi::xml_node child = node.first_child ();
                    !child.empty (); child = child.next_sibling ()) {
                if (child.type () == pugi::node_element) {
                    std::string childName = child.name ();
                    if (childName == TAG_TYPE) {
                        type = StringToType (child.text ().get ());
                    }
                    else if (childName == TAG_VALUE) {
                        switch (type) {
                            case Variant::TYPE_bool:
                                value._bool =
                                    std::string (child.text ().get ()) == XML_TRUE;
                                break;
                            case Variant::TYPE_i8:
                                value._i8 = (i8)strtol (child.text ().get (), 0, 10);
                                break;
                            case Variant::TYPE_ui8:
                                value._ui8 = (ui8)strtoul (child.text ().get (), 0, 10);
                                break;
                            case Variant::TYPE_i16:
                                value._i16 = (i16)strtol (child.text ().get (), 0, 10);
                                break;
                            case Variant::TYPE_ui16:
                                value._ui16 = (ui16)strtoul (child.text ().get (), 0, 10);
                                break;
                            case Variant::TYPE_i32:
                                value._i32 = strtol (child.text ().get (), 0, 10);
                                break;
                            case Variant::TYPE_ui32:
                                value._ui32 = strtoul (child.text ().get (), 0, 10);
                                break;
                            case Variant::TYPE_i64:
                                value._i64 = strtoll (child.text ().get (), 0, 10);
                                break;
                            case Variant::TYPE_ui64:
                                value._ui64 = strtoull (child.text ().get (), 0, 10);
                                break;
                            case Variant::TYPE_f32:
                                value._f32 = (f32)strtof (child.text ().get (), 0);
                                break;
                            case Variant::TYPE_f64:
                                value._f64 = strtod (child.text ().get (), 0);
                                break;
                            case Variant::TYPE_string:
                                value._string = new std::string (child.text ().get ());
                                break;
                            case Variant::TYPE_GUID:
                                value._guid = new GUID (child.text ().get ());
                                break;
                        }
                    }
                }
            }
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)

        std::string Variant::ToString (
                ui32 indentationLevel,
                const char *tagName) const {
            std::string str;
            switch (type) {
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
                case Variant::TYPE_string:
                    str = Encodestring (*value._string);
                    break;
                case Variant::TYPE_GUID:
                    str = value._guid->ToString ();
                    break;
            }
            std::stringstream stream;
            stream <<
                OpenTag (indentationLevel, tagName) <<
                    OpenTag (indentationLevel + 1, TAG_TYPE) <<
                        TypeToString (type) <<
                    CloseTag (indentationLevel + 1, TAG_TYPE) <<
                    OpenTag (indentationLevel + 1, TAG_VALUE) <<
                        str <<
                    CloseTag (indentationLevel + 1, TAG_VALUE) <<
                CloseTag (indentationLevel, tagName);
            return stream.str ();
        }

    #if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
        void Variant::ParseWithAttributes (const pugi::xml_node &node) {
            Clear ();
            type = StringToType (node.attribute (ATTR_TYPE).value ());
            std::string value_ = node.attribute (ATTR_VALUE).value ();
            switch (type) {
                case Variant::TYPE_bool:
                    value._bool = value_ == XML_TRUE;
                    break;
                case Variant::TYPE_i8:
                    value._i8 = (i8)strtol (value_.c_str (), 0, 10);
                    break;
                case Variant::TYPE_ui8:
                    value._ui8 = (ui8)strtoul (value_.c_str (), 0, 10);
                    break;
                case Variant::TYPE_i16:
                    value._i16 = (i16)strtol (value_.c_str (), 0, 10);
                    break;
                case Variant::TYPE_ui16:
                    value._ui16 = (ui16)strtoul (value_.c_str (), 0, 10);
                    break;
                case Variant::TYPE_i32:
                    value._i32 = strtol (value_.c_str (), 0, 10);
                    break;
                case Variant::TYPE_ui32:
                    value._ui32 = strtoul (value_.c_str (), 0, 10);
                    break;
                case Variant::TYPE_i64:
                    value._i64 = strtoll (value_.c_str (), 0, 10);
                    break;
                case Variant::TYPE_ui64:
                    value._ui64 = strtoull (value_.c_str (), 0, 10);
                    break;
                case Variant::TYPE_f32:
                    value._f32 = (f32)strtof (value_.c_str (), 0);
                    break;
                case Variant::TYPE_f64:
                    value._f64 = strtod (value_.c_str (), 0);
                    break;
                case Variant::TYPE_string:
                    value._string = new std::string (value_);
                    break;
                case Variant::TYPE_GUID:
                    value._guid = new GUID (value_);
                    break;
            }
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)

        std::string Variant::ToStringWithAttributes (
                ui32 indentationLevel,
                const char *tagName) const {
            std::string str;
            switch (type) {
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
                case Variant::TYPE_string:
                    str = Encodestring (*value._string);
                    break;
                case Variant::TYPE_GUID:
                    str = value._guid->ToString ();
                    break;
            }
            Attributes attributes;
            attributes.push_back (Attribute (ATTR_TYPE, TypeToString (type)));
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

        Serializer &operator << (
                Serializer &serializer,
                const Variant &variant) {
            serializer << variant.type;
            switch (variant.type) {
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
                case Variant::TYPE_string:
                    assert (variant.value._string != 0);
                    serializer << *variant.value._string;
                    break;
                case Variant::TYPE_GUID:
                    assert (variant.value._string != 0);
                    serializer << *variant.value._guid;
                    break;
            }
            return serializer;
        }

        Serializer &operator >> (
                Serializer &serializer,
                Variant &variant) {
            variant.Clear ();
            serializer >> variant.type;
            switch (variant.type) {
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
                case Variant::TYPE_string: {
                    variant.value._string = new std::string ();
                    serializer >> *variant.value._string;
                    break;
                }
                case Variant::TYPE_GUID: {
                    variant.value._guid = new GUID ();
                    serializer >> *variant.value._guid;
                    break;
                }
            }
            return serializer;
        }

    } // namespace util
} // namespace thekogans
