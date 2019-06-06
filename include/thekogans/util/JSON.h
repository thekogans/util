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

#if !defined (__thekogans_util_JSON_h)
#define __thekogans_util_JSON_h

#include <string>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Variant.h"
#include "thekogans/util/SizeT.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

        /// \struct JSON JSON.h thekogans/util/JSON.h
        ///
        /// \brief
        /// JSON provides the classes and parsing/serialization facilities to read and
        /// write JSON formatted values. It's used by \see{Serializable} as one of it's
        /// serialization/deserialization methods. JSON::Array implements multi-line
        /// string handling by providing an appropriate ctor and ToString method.

        struct _LIB_THEKOGANS_UTIL_DECL JSON {
            /// \struct JSON::Value JSON.h thekogans/util/JSON.h
            ///
            /// \brief
            /// Base for all JSON value types.
            struct _LIB_THEKOGANS_UTIL_DECL Value : public ThreadSafeRefCounted {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<Value>.
                typedef ThreadSafeRefCounted::Ptr<Value> Ptr;

            public:
                /// \brief
                /// dtor.
                virtual ~Value () {}

                /// \brief
                /// Return value type.
                /// \return Value type.
                virtual const char *GetType () const = 0;

                /// \brief
                /// Convert Bool, Number or String to bool, one of the
                /// integral types (Types.h) or std::string.
                template<typename T>
                T To () const;
            };

            #define THEKOGANS_UTIL_JSON_DECLARE_VALUE(type)\
            public:\
                typedef ThreadSafeRefCounted::Ptr<type> Ptr;\
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (type, SpinLock)\
                static const char * const TYPE;\
                virtual const char *GetType () const {\
                    return TYPE;\
                }

            #define THEKOGANS_UTIL_JSON_IMPLEMENT_VALUE(type)\
                THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (JSON::type, SpinLock)\
                const char * const JSON::type::TYPE = #type;

            /// \struct JSON::Bool JSON.h thekogans/util/JSON.h
            ///
            /// \brief
            /// Represents a JSON bool (true/false) value.
            struct _LIB_THEKOGANS_UTIL_DECL Bool : public Value {
                /// \brief
                /// Bool is a value.
                THEKOGANS_UTIL_JSON_DECLARE_VALUE (Bool)

                /// \brief
                /// Bool value.
                bool value;

                /// \brief
                /// ctor.
                /// \param[in] value_ Bool value.
                Bool (bool value_ = false) :
                    value (value_) {}
            };

            /// \struct JSON::Null JSON.h thekogans/util/JSON.h
            ///
            /// \brief
            /// Represents a JSON null value.
            struct _LIB_THEKOGANS_UTIL_DECL Null : public Value {
                /// \brief
                /// Null is a value.
                THEKOGANS_UTIL_JSON_DECLARE_VALUE (Null)

                /// \brief
                /// ctor.
                Null () {}
            };

            /// \struct JSON::Number JSON.h thekogans/util/JSON.h
            ///
            /// \brief
            /// Represents a JSON number value.
            struct _LIB_THEKOGANS_UTIL_DECL Number : public Value {
                /// \brief
                /// Number is a value.
                THEKOGANS_UTIL_JSON_DECLARE_VALUE (Number)

                /// \brief
                /// Number value.
                Variant value;

                /// \brief
                /// ctor.
                /// \param[in] value_ Number value.
                Number (const Variant value_ = Variant::Empty) :
                    value (value_) {}
            };

            /// \struct JSON::String JSON.h thekogans/util/JSON.h
            ///
            /// \brief
            /// Represents a JSON string value.
            struct _LIB_THEKOGANS_UTIL_DECL String : public Value {
                /// \brief
                /// String is a value.
                THEKOGANS_UTIL_JSON_DECLARE_VALUE (String)

                /// \brief
                /// String value.
                std::string value;

                /// \brief
                /// ctor.
                /// \param[in] value_ String value.
                String (const std::string &value_ = std::string ()) :
                    value (value_) {}
            };

            /// \struct JSON::Array JSON.h thekogans/util/JSON.h
            ///
            /// \brief
            /// Represents a JSON array value.
            struct _LIB_THEKOGANS_UTIL_DECL Array : public Value {
                /// \brief
                /// Array is a value.
                THEKOGANS_UTIL_JSON_DECLARE_VALUE (Array)

                /// \brief
                /// Array of values.
                std::vector<Value::Ptr> values;

                /// \brief
                /// ctor.
                Array () {}
                /// \brief
                /// ctor. Create an array by breaking up the given string on delimiter boundary.
                /// \param[in] str String to break up.
                /// \param[in] delimiter Delimiter.
                /// \param[in] delimiterLength Delimiter length.
                Array (
                    const std::string &str,
                    const void *delimiter = "\n",
                    std::size_t delimiterLength = 1);

                /// \brief
                /// JSON's string handling is fucking abysmal (and that's putting it mildly).
                /// This function together with the ctor above ease the pain by storing and
                /// reconstituting a multi-line string in an array object.
                /// NOTE: Array must only contain String entries.
                /// \param[in] delimiter Delimiter.
                /// \param[in] delimiterLength Delimiter length.
                /// \return std::string containing array entries separated by the delimiter.
                std::string ToString (
                    const void *delimiter = "\n",
                    std::size_t delimiterLength = 1);

                /// \brief
                /// Return count of values in the array.
                /// \return Count of values in the array.
                inline std::size_t GetValueCount () const {
                    return values.size ();
                }

                /// \brief
                /// Return the value at the given index.
                /// \paramp[in] index Index whose value to retrieve.
                /// \return Value at index.
                template<typename T>
                typename T::Ptr Get (std::size_t index) const {
                    if (index < values.size ()) {
                        return dynamic_refcounted_pointer_cast<T> (values[index]);
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }

                /// \brief
                /// Add the given value to the end of the array.
                /// \param[in] value Vale to add.
                template<typename T>
                void Add (T value) {
                    if (value.Get () != 0) {
                        values.push_back (Value::Ptr (value.Get ()));
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }

                /// \brief
                /// Remove value at the given index.
                /// \param[in] index Index whose value to remove.
                void Remove (std::size_t index);
            };

            /// \struct JSON::Object JSON.h thekogans/util/JSON.h
            ///
            /// \brief
            /// Represents a JSON object value.
            struct _LIB_THEKOGANS_UTIL_DECL Object : public Value {
                /// \brief
                /// Object is a value.
                THEKOGANS_UTIL_JSON_DECLARE_VALUE (Object)

                /// \brief
                /// Convenient typedef for std::pair<std::string, Value::Ptr>.
                typedef std::pair<std::string, Value::Ptr> NameValue;
                /// \brief
                /// Array of name/value pairs.
                std::vector<NameValue> values;

                /// \brief
                /// ctor.
                Object () {}

                /// \brief
                /// Return count of name/values in the object.
                /// \return Count of name/values in the object.
                inline std::size_t GetValueCount () const {
                    return values.size ();
                }

                /// \brief
                /// Return the value with the given name.
                /// \paramp[in] name Name whose value to retrieve.
                /// \return Value with the given name.
                template<typename T>
                typename T::Ptr Get (const std::string &name) const {
                    for (std::size_t i = 0, count = values.size (); i < count; ++i) {
                        if (values[i].first == name) {
                            return dynamic_refcounted_pointer_cast<T> (values[i].second);
                        }
                    }
                    return typename T::Ptr (new T);
                }

                /// \brief
                /// Add a named value to the object.
                /// \param[in] name A value name.
                /// \param[in] value A value associated with the name.
                template<typename T>
                void Add (
                        const std::string &name,
                        T value) {
                    if (!name.empty () && value.Get () != 0) {
                        values.push_back (NameValue (name, Value::Ptr (value.Get ())));
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }

                /// \brief
                /// Remove value associated with the given name.
                /// \param[in] name Name whose value to remove.
                void Remove (const std::string &name);
            };

            /// \brief
            /// Parse a JSON formatted string.
            /// \param[in] value JSON formatted string.
            /// \retunr Value representation of the given JSON string.
            static Value::Ptr ParseValue (const std::string &value);
            /// \brief
            /// Format the given value.
            /// \param[in] value The value to format.
            /// \param[in] indentationLevel Pretty print parameter.
            /// \param[in] indentationWidth Pretty print parameter.
            /// \return Formatted value.
            static std::string FormatValue (
                const Value &value,
                std::size_t indentationLevel = 0,
                std::size_t indentationWidth = 2);
        };

        template<>
        inline bool JSON::Value::To<bool> () const {
            if (GetType () == Bool::TYPE) {
                return static_cast<const Bool *> (this)->value;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value type %s is not %s.",
                    GetType (),
                    Bool::TYPE);
            }
        }

        template<>
        inline i8 JSON::Value::To<i8> () const {
            if (GetType () == Number::TYPE) {
                return static_cast<const Number *> (this)->value.To<i8> ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value type %s is not %s.",
                    GetType (),
                    Number::TYPE);
            }
        }

        template<>
        inline ui8 JSON::Value::To<ui8> () const {
            if (GetType () == Number::TYPE) {
                return static_cast<const Number *> (this)->value.To<ui8> ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value type %s is not %s.",
                    GetType (),
                    Number::TYPE);
            }
        }

        template<>
        inline i16 JSON::Value::To<i16> () const {
            if (GetType () == Number::TYPE) {
                return static_cast<const Number *> (this)->value.To<i16> ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value type %s is not %s.",
                    GetType (),
                    Number::TYPE);
            }
        }

        template<>
        inline ui16 JSON::Value::To<ui16> () const {
            if (GetType () == Number::TYPE) {
                return static_cast<const Number *> (this)->value.To<ui16> ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value type %s is not %s.",
                    GetType (),
                    Number::TYPE);
            }
        }

        template<>
        inline i32 JSON::Value::To<i32> () const {
            if (GetType () == Number::TYPE) {
                return static_cast<const Number *> (this)->value.To<i32> ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value type %s is not %s.",
                    GetType (),
                    Number::TYPE);
            }
        }

        template<>
        inline ui32 JSON::Value::To<ui32> () const {
            if (GetType () == Number::TYPE) {
                return static_cast<const Number *> (this)->value.To<ui32> ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value type %s is not %s.",
                    GetType (),
                    Number::TYPE);
            }
        }

        template<>
        inline i64 JSON::Value::To<i64> () const {
            if (GetType () == Number::TYPE) {
                return static_cast<const Number *> (this)->value.To<i64> ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value type %s is not %s.",
                    GetType (),
                    Number::TYPE);
            }
        }

        template<>
        inline ui64 JSON::Value::To<ui64> () const {
            if (GetType () == Number::TYPE) {
                return static_cast<const Number *> (this)->value.To<ui64> ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value type %s is not %s.",
                    GetType (),
                    Number::TYPE);
            }
        }

        template<>
        inline f32 JSON::Value::To<f32> () const {
            if (GetType () == Number::TYPE) {
                return static_cast<const Number *> (this)->value.To<f32> ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value type %s is not %s.",
                    GetType (),
                    Number::TYPE);
            }
        }

        template<>
        inline f64 JSON::Value::To<f64> () const {
            if (GetType () == Number::TYPE) {
                return static_cast<const Number *> (this)->value.To<f64> ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value type %s is not %s.",
                    GetType (),
                    Number::TYPE);
            }
        }

        template<>
        inline SizeT JSON::Value::To<SizeT> () const {
            if (GetType () == Number::TYPE) {
                return static_cast<const Number *> (this)->value.To<SizeT> ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value type %s is not %s.",
                    GetType (),
                    Number::TYPE);
            }
        }

        template<>
        inline std::string JSON::Value::To<std::string> () const {
            if (GetType () == String::TYPE) {
                return static_cast<const String *> (this)->value;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value type %s is not %s.",
                    GetType (),
                    String::TYPE);
            }
        }

        template<>
        inline void JSON::Array::Add<bool> (bool value) {
            values.push_back (Value::Ptr (new Bool (value)));
        }

        template<>
        inline void JSON::Array::Add<i8> (i8 value) {
            values.push_back (Value::Ptr (new Number (Variant (value))));
        }

        template<>
        inline void JSON::Array::Add<ui8> (ui8 value) {
            values.push_back (Value::Ptr (new Number (Variant (value))));
        }

        template<>
        inline void JSON::Array::Add<i16> (i16 value) {
            values.push_back (Value::Ptr (new Number (Variant (value))));
        }

        template<>
        inline void JSON::Array::Add<ui16> (ui16 value) {
            values.push_back (Value::Ptr (new Number (Variant (value))));
        }

        template<>
        inline void JSON::Array::Add<i32> (i32 value) {
            values.push_back (Value::Ptr (new Number (Variant (value))));
        }

        template<>
        inline void JSON::Array::Add<ui32> (ui32 value) {
            values.push_back (Value::Ptr (new Number (Variant (value))));
        }

        template<>
        inline void JSON::Array::Add<i64> (i64 value) {
            values.push_back (Value::Ptr (new Number (Variant (value))));
        }

        template<>
        inline void JSON::Array::Add<ui64> (ui64 value) {
            values.push_back (Value::Ptr (new Number (Variant (value))));
        }

        template<>
        inline void JSON::Array::Add<f32> (f32 value) {
            values.push_back (Value::Ptr (new Number (Variant (value))));
        }

        template<>
        inline void JSON::Array::Add<f64> (f64 value) {
            values.push_back (Value::Ptr (new Number (Variant (value))));
        }

        template<>
        inline void JSON::Array::Add<SizeT> (SizeT value) {
            values.push_back (Value::Ptr (new Number (Variant (value.value))));
        }

        template<>
        inline void JSON::Array::Add<std::string> (std::string value) {
            values.push_back (Value::Ptr (new String (value)));
        }

        template<>
        inline void JSON::Array::Add<const char *> (const char *value) {
            if (value != 0) {
                values.push_back (Value::Ptr (new String (value)));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        template<>
        inline void JSON::Object::Add<bool> (
                const std::string &name,
                bool value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::Ptr (new Bool (value))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        template<>
        inline void JSON::Object::Add<i8> (
                const std::string &name,
                i8 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::Ptr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        template<>
        inline void JSON::Object::Add<ui8> (
                const std::string &name,
                ui8 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::Ptr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        template<>
        inline void JSON::Object::Add<i16> (
                const std::string &name,
                i16 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::Ptr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        template<>
        inline void JSON::Object::Add<ui16> (
                const std::string &name,
                ui16 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::Ptr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        template<>
        inline void JSON::Object::Add<i32> (
                const std::string &name,
                i32 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::Ptr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        template<>
        inline void JSON::Object::Add<ui32> (
                const std::string &name,
                ui32 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::Ptr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        template<>
        inline void JSON::Object::Add<i64> (
                const std::string &name,
                i64 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::Ptr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        template<>
        inline void JSON::Object::Add<ui64> (
                const std::string &name,
                ui64 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::Ptr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        template<>
        inline void JSON::Object::Add<f32> (
                const std::string &name,
                f32 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::Ptr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        template<>
        inline void JSON::Object::Add<f64> (
                const std::string &name,
                f64 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::Ptr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        template<>
        inline void JSON::Object::Add<SizeT> (
                const std::string &name,
                SizeT value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::Ptr (new Number (Variant (value.value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        template<>
        inline void JSON::Object::Add<std::string> (
                const std::string &name,
                std::string value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::Ptr (new String (value))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        template<>
        inline void JSON::Object::Add<const char *> (
                const std::string &name,
                const char *value) {
            if (!name.empty () && value != 0) {
                values.push_back (NameValue (name, Value::Ptr (new String (value))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_JSON_h)
