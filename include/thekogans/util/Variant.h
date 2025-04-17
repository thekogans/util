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

#if !defined (__thekogans_util_Variant_h)
#define __thekogans_util_Variant_h

#include <cassert>
#include <functional>
#include <string>
#include "pugixml/pugixml.hpp"
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        /// \struct Variant Variant.h thekogans/util/Variant.h
        ///
        /// \brief
        /// Variant is a convenient union type representing most types
        /// supported by util. Use this class to parametarize
        /// functions/algorithms where templates are not a good fit.

        struct _LIB_THEKOGANS_UTIL_DECL Variant {
            /// \brief
            /// "Variant"
            static const char * const TAG_VARIANT;
            /// \brief
            /// "Type"
            static const char * const TAG_TYPE;
            /// \brief
            /// "Type"
            static const char * const ATTR_TYPE;
            /// \brief
            /// "invalid"
            static const char * const VALUE_INVALID;
            /// \brief
            /// "bool"
            static const char * const VALUE_BOOL;
            /// \brief
            /// "i8"
            static const char * const VALUE_I8;
            /// \brief
            /// "ui8"
            static const char * const VALUE_UI8;
            /// \brief
            /// "i16"
            static const char * const VALUE_I16;
            /// \brief
            /// "ui16"
            static const char * const VALUE_UI16;
            /// \brief
            /// "i32"
            static const char * const VALUE_I32;
            /// \brief
            /// "ui32"
            static const char * const VALUE_UI32;
            /// \brief
            /// "i64"
            static const char * const VALUE_I64;
            /// \brief
            /// "ui64"
            static const char * const VALUE_UI64;
            /// \brief
            /// "f32"
            static const char * const VALUE_F32;
            /// \brief
            /// "f64"
            static const char * const VALUE_F64;
            /// \brief
            /// "SizeT"
            static const char * const VALUE_SizeT;
            /// \brief
            /// "string"
            static const char * const VALUE_STRING;
            /// \brief
            /// "GUID"
            static const char * const VALUE_GUID;
            /// \brief
            /// "Value"
            static const char * const TAG_VALUE;
            /// \brief
            /// "Value"
            static const char * const ATTR_VALUE;

            /// \brief
            /// Variant types.
            enum Type : ui32 {
                /// \brief
                /// Invalid type.
                TYPE_Invalid,
                /// \brief
                /// bool
                TYPE_bool,
                /// \brief
                /// i8
                TYPE_i8,
                /// \brief
                /// ui8
                TYPE_ui8,
                /// \brief
                /// i16
                TYPE_i16,
                /// \brief
                /// ui16
                TYPE_ui16,
                /// \brief
                /// i32
                TYPE_i32,
                /// \brief
                /// ui32
                TYPE_ui32,
                /// \brief
                /// i64
                TYPE_i64,
                /// \brief
                /// ui64
                TYPE_ui64,
                /// \brief
                /// f32
                TYPE_f32,
                /// \brief
                /// f64
                TYPE_f64,
                /// \brief
                /// SizeT
                TYPE_SizeT,
                /// \brief
                /// std::string *
                TYPE_string,
                /// \brief
                /// GUID *
                TYPE_GUID
                // FIXME: add support for:
                //TYPE_BitSet
                //TYPE_Buffer
                //TYPE_Directory_Entry
                //TYPE_FixedBuffer
                //TYPE_Fraction
                //TYPE_Exception
                //TYPE_Flags
                //TYPE_TimeSpec
                //TYPE_Version
            } type;
            /// \brief
            /// A union of every type the Variant can take.
            union {
                /// \brief
                /// bool
                bool _bool;
                /// \brief
                /// i8
                i8 _i8;
                /// \brief
                /// ui8
                ui8 _ui8;
                /// \brief
                /// i16
                i16 _i16;
                /// \brief
                /// ui16
                ui16 _ui16;
                /// \brief
                /// i32
                i32 _i32;
                /// \brief
                /// ui32
                ui32 _ui32;
                /// \brief
                /// i64
                i64 _i64;
                /// \brief
                /// ui64
                ui64 _ui64;
                /// \brief
                /// f32
                f32 _f32;
                /// \brief
                /// f64
                f64 _f64;
                /// \brief
                /// SizeT
                SizeT *_SizeT;
                /// \brief
                /// std::String *
                std::string *_string;
                /// \brief
                /// GUID *
                GUID *_guid;
            } value;

            /// \brief
            /// Default ctor. Initialize to invalid.
            Variant () :
                type (TYPE_Invalid) {}
            /// \brief
            /// bool ctor.
            /// \param[in] value_ Value to assign.
            explicit Variant (bool value_) :
                    type (TYPE_bool) {
                value._bool = value_;
            }
            /// \brief
            /// i8 ctor.
            /// \param[in] value_ Value to assign.
            explicit Variant (i8 value_) :
                    type (TYPE_i8) {
                value._i8 = value_;
            }
            /// \brief
            /// ui8 ctor.
            /// \param[in] value_ Value to assign.
            explicit Variant (ui8 value_) :
                    type (TYPE_ui8) {
                value._ui8 = value_;
            }
            /// \brief
            /// i16 ctor.
            /// \param[in] value_ Value to assign.
            explicit Variant (i16 value_) :
                    type (TYPE_i16) {
                value._i16 = value_;
            }
            /// \brief
            /// ui16 ctor.
            /// \param[in] value_ Value to assign.
            explicit Variant (ui16 value_) :
                    type (TYPE_ui16) {
                value._ui16 = value_;
            }
            /// \brief
            /// i32 ctor.
            /// \param[in] value_ Value to assign.
            explicit Variant (i32 value_) :
                    type (TYPE_i32) {
                value._i32 = value_;
            }
            /// \brief
            /// ui32 ctor.
            /// \param[in] value_ Value to assign.
            explicit Variant (ui32 value_) :
                    type (TYPE_ui32) {
                value._ui32 = value_;
            }
            /// \brief
            /// i64 ctor.
            /// \param[in] value_ Value to assign.
            explicit Variant (i64 value_) :
                    type (TYPE_i64) {
                value._i64 = value_;
            }
            /// \brief
            /// ui64 ctor.
            /// \param[in] value_ Value to assign.
            explicit Variant (ui64 value_) :
                    type (TYPE_ui64) {
                value._ui64 = value_;
            }
            /// \brief
            /// f32 ctor.
            /// \param[in] value_ Value to assign.
            explicit Variant (f32 value_) :
                    type (TYPE_f32) {
                value._f32 = value_;
            }
            /// \brief
            /// f64 ctor.
            /// \param[in] value_ Value to assign.
            explicit Variant (f64 value_) :
                    type (TYPE_f64) {
                value._f64 = value_;
            }
            /// \brief
            /// SizeT ctor.
            /// \param[in] value_ Value to assign.
            explicit Variant (const SizeT &value_) :
                    type (TYPE_SizeT) {
                value._SizeT = new SizeT (value_);
            }
            /// \brief
            /// std::string ctor.
            /// \param[in] value_ Value to assign.
            explicit Variant (const std::string &value_) :
                    type (TYPE_string) {
                value._string = new std::string (value_);
            }
            /// \brief
            /// Default GUID.
            /// \param[in] value_ Value to assign.
            explicit Variant (const GUID &value_) :
                    type (TYPE_GUID) {
                value._guid = new GUID (value_);
            }
            /// \brief
            /// Copy ctor.
            /// \param[in] variant Value to assign.
            Variant (const Variant &variant);
            /// \brief
            /// dtor.
            ~Variant () {
                Clear ();
            }

            /// \brief
            /// Assignment operator.
            /// \param[in] variant Value to assign.
            /// \return *this
            Variant &operator = (const Variant &variant);

            /// \brief
            /// Convert integral type value to it's string equivalent.
            /// \param[in] type Intergal type value to convert.
            /// \return Type string equivalent.
            static std::string TypeTostring (Type type);
            /// \brief
            /// Convert string type value to it's integral equivalent.
            /// \param[in] type String type value to convert.
            /// \return Type intergal equivalent.
            static Type stringToType (const std::string &type);

            /// \brief
            /// Return the variant type.
            /// \return the variant type.
            inline Type GetType () const {
                return type;
            }

            /// \brief
            /// Return true if type != Invalid.
            /// \return true if type != Invalid.
            inline bool IsValid () const {
                return type != TYPE_Invalid;
            }

            /// \brief
            /// Return underlying variant type size.
            /// \return Size of underlying variant type.
            std::size_t Size () const;

            /// \brief
            /// After calling this method, the variant is Invalid.
            void Clear ();

            /// \brief
            /// Compare a given variant against this one.
            /// \param[in] variant Variant to compare to.
            /// NOTE: No implicit type conversion is performed.
            /// If type != variant.type, an exception is thrown.
            /// bool logic is true > false.
            /// \return *this < variant = -1, *this == variant = 0, *this > variant = 1.
            i32 Compare (const Variant &variant) const;
            /// \brief
            /// Useful for string variants only. Does a
            /// prefix compare.
            /// \param[in] variant Variant to compare prefixes to.
            /// \return <0 = less, =0 = equal, >0 = greater.
            i32 PrefixCompare (const Variant &variant) const;

        #if defined (TOOLCHAIN_COMPILER_cl)
            #pragma warning (push)
            #pragma warning (disable : 4244)
        #endif // defined (TOOLCHAIN_COMPILER_cl)

            /// \brief
            /// Numerical type conversion template.
            /// \return Number cast to appropriate type.
            template<typename T>
            T To () const {
                switch (type) {
                    case Variant::TYPE_i8:
                        return static_cast<T> (value._i8);
                    case Variant::TYPE_ui8:
                        return static_cast<T> (value._ui8);
                    case Variant::TYPE_i16:
                        return static_cast<T> (value._i16);
                    case Variant::TYPE_ui16:
                        return static_cast<T> (value._ui16);
                    case Variant::TYPE_i32:
                        return static_cast<T> (value._i32);
                    case Variant::TYPE_ui32:
                        return static_cast<T> (value._ui32);
                    case Variant::TYPE_i64:
                        return static_cast<T> (value._i64);
                    case Variant::TYPE_ui64:
                        return static_cast<T> (value._ui64);
                    case Variant::TYPE_f32:
                        return static_cast<T> (value._f32);
                    case Variant::TYPE_f64:
                        return static_cast<T> (value._f64);
                    case Variant::TYPE_SizeT:
                        return static_cast<T> (*value._SizeT);
                    default:
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Varint type (%s) is not a number.",
                            TypeTostring (type).c_str ());
                }
            }

        #if defined (TOOLCHAIN_COMPILER_cl)
            #pragma warning (pop)
        #endif // defined (TOOLCHAIN_COMPILER_cl)

            /// \brief
            /// Parse variant state from an xml node.
            /// NOTE: The XML node should look like this:
            /// <tagName Type = ""
            ///          Value = ""/>
            /// \param[in] node XML node to parse state.
            void Parse (const pugi::xml_node &node);
            /// \brief
            /// Serialize the variant to an xml node.
            /// \param[in] indentationLevel How far to indent the leading tag.
            /// \param[in] tagName The name of the leading tag.
            /// \return Serialized XML representation of the variant.
            std::string ToString (
                const char *tagName = TAG_VARIANT,
                std::size_t indentationLevel = 0,
                std::size_t indentationWidth = 2) const;
        };

        /// \brief
        /// Return true if variant1 < variant2.
        /// \param[in] variant1 First variant to compare.
        /// \param[in] variant2 Second variant to compare.
        /// \return true = variant1 < variant2, false = variant1 >= variant2
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator < (
            const Variant &variant1,
            const Variant &variant2);
        /// \brief
        /// Return true if variant1 > variant2.
        /// \param[in] variant1 First variant to compare.
        /// \param[in] variant2 Second variant to compare.
        /// \return true = variant1 > variant2, false = variant1 <= variant2
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator > (
            const Variant &variant1,
            const Variant &variant2);

        /// \brief
        /// Return true if variant1 == variant2.
        /// \param[in] variant1 First variant to compare.
        /// \param[in] variant2 Second variant to compare.
        /// \return true = variant1 == variant2, false = variant1 != variant2
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator == (
            const Variant &variant1,
            const Variant &variant2);
        /// \brief
        /// Return true if variant1 != variant2.
        /// \param[in] variant1 First variant to compare.
        /// \param[in] variant2 Second variant to compare.
        /// \return true = variant1 != variant2, false = variant1 == variant2
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator != (
            const Variant &variant1,
            const Variant &variant2);

        /// \brief
        /// Write the given variant to the given serializer.
        /// \param[in] serializer Where to write the given variant.
        /// \param[in] variant Variant to write.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
            Serializer &serializer,
            const Variant &variant);
        /// \brief
        /// Read a variant from the given serializer.
        /// \param[in] serializer Where to read the variant from.
        /// \param[in] variant Variant to read.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
            Serializer &serializer,
            Variant &variant);

    } // namespace util
} // namespace thekogans

namespace std {

    /// \struct hash<thekogans::util::Variant> Variant.h thekogans/util/Variant.h
    ///
    /// \brief
    /// Implementation of std::hash for thekogans::util::Variant.

    template <>
    struct hash<thekogans::util::Variant> {
        size_t operator () (const thekogans::util::Variant &value) const {
            switch (value.type) {
                case thekogans::util::Variant::TYPE_Invalid:
                    return 0;
                case thekogans::util::Variant::TYPE_bool:
                    return std::hash<bool> () (value.value._bool);
                case thekogans::util::Variant::TYPE_i8:
                    return std::hash<thekogans::util::i8> () (value.value._i8);
                case thekogans::util::Variant::TYPE_ui8:
                    return std::hash<thekogans::util::ui8> () (value.value._ui8);
                case thekogans::util::Variant::TYPE_i16:
                    return std::hash<thekogans::util::i16> () (value.value._i16);
                case thekogans::util::Variant::TYPE_ui16:
                    return std::hash<thekogans::util::ui16> () (value.value._ui16);
                case thekogans::util::Variant::TYPE_i32:
                    return std::hash<thekogans::util::i32> () (value.value._i32);
                case thekogans::util::Variant::TYPE_ui32:
                    return std::hash<thekogans::util::ui32> () (value.value._ui32);
                case thekogans::util::Variant::TYPE_i64:
                    return std::hash<thekogans::util::i64> () (value.value._i64);
                case thekogans::util::Variant::TYPE_ui64:
                    return std::hash<thekogans::util::ui64> () (value.value._ui64);
                case thekogans::util::Variant::TYPE_f32:
                    return std::hash<thekogans::util::f32> () (value.value._f32);
                case thekogans::util::Variant::TYPE_f64:
                    return std::hash<thekogans::util::f64> () (value.value._f64);
                case thekogans::util::Variant::TYPE_SizeT:
                    return std::hash<thekogans::util::SizeT> () (*value.value._SizeT);
                case thekogans::util::Variant::TYPE_string:
                    return hash<string> () (*value.value._string);
                case thekogans::util::Variant::TYPE_GUID:
                    return hash<thekogans::util::GUID> () (*value.value._guid);
            }
            assert (0);
            return 0;
        }
    };

} // namespace std

#endif // !defined (__thekogans_util_Variant_h)
