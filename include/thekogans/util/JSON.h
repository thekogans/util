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
#include "thekogans/util/Exception.h"
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
            struct _LIB_THEKOGANS_UTIL_DECL Value : public RefCounted {
                /// \brief
                /// Convenient typedef for RefCounted::SharedPtr<Value>.
                typedef RefCounted::SharedPtr<Value> SharedPtr;

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

            /// \def THEKOGANS_UTIL_JSON_DECLARE_VALUE(type)
            /// Common declarations used by all Value derivatives.
            #define THEKOGANS_UTIL_JSON_DECLARE_VALUE(type)\
            public:\
                typedef RefCounted::SharedPtr<type> SharedPtr;\
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (type, SpinLock)\
                static const char * const TYPE;\
                virtual const char *GetType () const {\
                    return TYPE;\
                }

            /// \def THEKOGANS_UTIL_JSON_IMPLEMENT_VALUE(type)
            /// Common implementations used by all Value derivatives.
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
            /// NOTE: The parser will scan the number to determine the
            /// best type for variant. To that end, anything with a
            /// '.' or '[e|E]' will be treated as f64. If the number
            /// is negative it will be represented by i64. Otherwise,
            /// it will be stored in ui64. Use Value::To to cast the
            /// stored representation to whatever type is appropriate.
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
                std::vector<Value::SharedPtr> values;

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
                typename T::SharedPtr Get (std::size_t index) const {
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
                /// \param[in] value Value to add.
                template<typename T>
                void Add (T value) {
                    if (value.Get () != 0) {
                        values.push_back (Value::SharedPtr (value.Get ()));
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }

                /// \brief
                /// Insert the given value at the given index.
                /// \param[in] value Value to insert.
                /// \param[in] index Index where the new value will reside.
                template<typename T>
                void Insert (
                       T value,
                       std::size_t index) {
                    if (value.Get () != 0 && index <= values.size ()) {
                        values.insert (values.begin () + index, Value::SharedPtr (value.Get ()));
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
                /// Convenient typedef for std::pair<std::string, Value::SharedPtr>.
                typedef std::pair<std::string, Value::SharedPtr> NameValue;
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
                typename T::SharedPtr Get (const std::string &name) const {
                    for (std::size_t i = 0, count = values.size (); i < count; ++i) {
                        if (values[i].first == name) {
                            return dynamic_refcounted_pointer_cast<T> (values[i].second);
                        }
                    }
                    return typename T::SharedPtr (new T);
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
                        values.push_back (NameValue (name, Value::SharedPtr (value.Get ())));
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }

                /// \brief
                /// Insert a named value at the given index.
                /// \param[in] name A value name.
                /// \param[in] value A value associated with the name.
                /// \param[in] index Where the new named value will reside.
                template<typename T>
                void Insert (
                        const std::string &name,
                        T value,
                        std::size_t index) {
                    if (!name.empty () && value.Get () != 0 && index <= values.size ()) {
                        values.insert (
                            values.begin () + index,
                            NameValue (name, Value::SharedPtr (value.Get ())));
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }

                /// \brief
                /// Return true if a value with the given name exists.
                /// \param[in] name Name of value to check existence.
                /// \return true == a value with the given name exists.
                bool Contains (const std::string &name) const;

                /// \brief
                /// Remove value associated with the given name.
                /// \param[in] name Name whose value to remove.
                void Remove (const std::string &name);
            };

            /// \brief
            /// Parse a JSON formatted string.
            /// \param[in] value JSON formatted string.
            /// \retunr Value representation of the given JSON string.
            static Value::SharedPtr ParseValue (const std::string &value);
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

        /// \brief
        /// Specialization of Value::To for bool.
        /// \return Value cast to bool.
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

        /// \brief
        /// Specialization of Value::To for i8.
        /// \return Value cast to i8.
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

        /// \brief
        /// Specialization of Value::To for ui8.
        /// \return Value cast to ui8.
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

        /// \brief
        /// Specialization of Value::To for i16.
        /// \return Value cast to i16.
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

        /// \brief
        /// Specialization of Value::To for ui16.
        /// \return Value cast to ui16.
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

        /// \brief
        /// Specialization of Value::To for i32.
        /// \return Value cast to i32.
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

        /// \brief
        /// Specialization of Value::To for ui32.
        /// \return Value cast to ui32.
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

        /// \brief
        /// Specialization of Value::To for i64.
        /// \return Value cast to i64.
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

        /// \brief
        /// Specialization of Value::To for ui64.
        /// \return Value cast to ui64.
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

        /// \brief
        /// Specialization of Value::To for f32.
        /// \return Value cast to f32.
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

        /// \brief
        /// Specialization of Value::To for f64.
        /// \return Value cast to f64.
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

        /// \brief
        /// Specialization of Value::To for SizeT.
        /// \return Value cast to SizeT.
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

        /// \brief
        /// Specialization of Value::To for std::string.
        /// \return Value cast to std::string.
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

        /// \brief
        /// Specialization of Array::Add for bool.
        /// \param[in] value bool value to add to Array.
        template<>
        inline void JSON::Array::Add (bool value) {
            values.push_back (Value::SharedPtr (new Bool (value)));
        }

        /// \brief
        /// Specialization of Array::Add for i8.
        /// \param[in] value i8 value to add to Array.
        template<>
        inline void JSON::Array::Add (i8 value) {
            values.push_back (Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Add for ui8.
        /// \param[in] value ui8 value to add to Array.
        template<>
        inline void JSON::Array::Add (ui8 value) {
            values.push_back (Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Add for i16.
        /// \param[in] value i16 value to add to Array.
        template<>
        inline void JSON::Array::Add (i16 value) {
            values.push_back (Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Add for ui16.
        /// \param[in] value ui16 value to add to Array.
        template<>
        inline void JSON::Array::Add (ui16 value) {
            values.push_back (Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Add for i32.
        /// \param[in] value i32 value to add to Array.
        template<>
        inline void JSON::Array::Add (i32 value) {
            values.push_back (Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Add for ui32.
        /// \param[in] value ui32 value to add to Array.
        template<>
        inline void JSON::Array::Add (ui32 value) {
            values.push_back (Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Add for i64.
        /// \param[in] value i64 value to add to Array.
        template<>
        inline void JSON::Array::Add (i64 value) {
            values.push_back (Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Add for ui64.
        /// \param[in] value ui64 value to add to Array.
        template<>
        inline void JSON::Array::Add (ui64 value) {
            values.push_back (Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Add for f32.
        /// \param[in] value f32 value to add to Array.
        template<>
        inline void JSON::Array::Add (f32 value) {
            values.push_back (Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Add for f64.
        /// \param[in] value f64 value to add to Array.
        template<>
        inline void JSON::Array::Add (f64 value) {
            values.push_back (Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Add for const SizeT &.
        /// \param[in] value const SizeT & value to add to Array.
        template<>
        inline void JSON::Array::Add (const SizeT &value) {
            values.push_back (Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Add for const std::string &.
        /// \param[in] value const std::string & value to add to Array.
        template<>
        inline void JSON::Array::Add (const std::string &value) {
            values.push_back (Value::SharedPtr (new String (value)));
        }

        /// \brief
        /// Specialization of Array::Add for const char *.
        /// \param[in] value const char * value to add to Array.
        template<>
        inline void JSON::Array::Add (const char *value) {
            if (value != 0) {
                values.push_back (Value::SharedPtr (new String (value)));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Array::Insert for bool.
        /// \param[in] value bool value to insert into Array.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Array::Insert (
                bool value,
                std::size_t index) {
            values.insert (values.begin () + index, Value::SharedPtr (new Bool (value)));
        }

        /// \brief
        /// Specialization of Array::Insert for i8.
        /// \param[in] value i8 value to insert into Array.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Array::Insert (
                i8 value,
                std::size_t index) {
            values.insert (values.begin () + index, Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Insert for ui8.
        /// \param[in] value ui8 value to insert into Array.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Array::Insert (
                ui8 value,
                std::size_t index) {
            values.insert (values.begin () + index, Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Insert for i16.
        /// \param[in] value i16 value to insert into Array.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Array::Insert (
                i16 value,
                std::size_t index) {
            values.insert (values.begin () + index, Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Insert for ui16.
        /// \param[in] value ui16 value to insert into Array.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Array::Insert (
                ui16 value,
                std::size_t index) {
            values.insert (values.begin () + index, Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Insert for i32.
        /// \param[in] value i32 value to insert into Array.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Array::Insert (
                i32 value,
                std::size_t index) {
            values.insert (values.begin () + index, Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Insert for ui32.
        /// \param[in] value ui32 value to insert into Array.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Array::Insert (
                ui32 value,
                std::size_t index) {
            values.insert (values.begin () + index, Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Insert for i64.
        /// \param[in] value i64 value to insert into Array.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Array::Insert (
                i64 value,
                std::size_t index) {
            values.insert (values.begin () + index, Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Insert for ui64.
        /// \param[in] value ui64 value to insert into Array.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Array::Insert (
                ui64 value,
                std::size_t index) {
            values.insert (values.begin () + index, Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Insert for f32.
        /// \param[in] value f32 value to insert into Array.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Array::Insert (
                f32 value,
                std::size_t index) {
            values.insert (values.begin () + index, Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Insert for f64.
        /// \param[in] value f64 value to insert into Array.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Array::Insert (
                f64 value,
                std::size_t index) {
            values.insert (values.begin () + index, Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Insert for const SizeT &.
        /// \param[in] value const SizeT & value to insert into Array.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Array::Insert (
                const SizeT &value,
                std::size_t index) {
            values.insert (values.begin () + index, Value::SharedPtr (new Number (Variant (value))));
        }

        /// \brief
        /// Specialization of Array::Insert for const std::string &.
        /// \param[in] value const std::string & value to insert into Array.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Array::Insert (
                const std::string &value,
                std::size_t index) {
            values.insert (values.begin () + index, Value::SharedPtr (new String (value)));
        }

        /// \brief
        /// Specialization of Array::Insert for const char *.
        /// \param[in] value const char * value to insert into Array.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Array::Insert (
                const char *value,
                std::size_t index) {
            if (value != 0) {
                values.insert (values.begin () + index, Value::SharedPtr (new String (value)));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Add for bool.
        /// \param[in] name Name of value.
        /// \param[in] value bool value to add to Object.
        template<>
        inline void JSON::Object::Add (
                const std::string &name,
                bool value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::SharedPtr (new Bool (value))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Add for i8.
        /// \param[in] name Name of value.
        /// \param[in] value i8 value to add to Object.
        template<>
        inline void JSON::Object::Add (
                const std::string &name,
                i8 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Add for ui8.
        /// \param[in] name Name of value.
        /// \param[in] value ui8 value to add to Object.
        template<>
        inline void JSON::Object::Add (
                const std::string &name,
                ui8 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Add for i16.
        /// \param[in] name Name of value.
        /// \param[in] value i16 value to add to Object.
        template<>
        inline void JSON::Object::Add (
                const std::string &name,
                i16 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Add for ui16.
        /// \param[in] name Name of value.
        /// \param[in] value ui16 value to add to Object.
        template<>
        inline void JSON::Object::Add (
                const std::string &name,
                ui16 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Add for i32.
        /// \param[in] name Name of value.
        /// \param[in] value i32 value to add to Object.
        template<>
        inline void JSON::Object::Add (
                const std::string &name,
                i32 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Add for ui32.
        /// \param[in] name Name of value.
        /// \param[in] value ui32 value to add to Object.
        template<>
        inline void JSON::Object::Add (
                const std::string &name,
                ui32 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Add for i64.
        /// \param[in] name Name of value.
        /// \param[in] value i64 value to add to Object.
        template<>
        inline void JSON::Object::Add (
                const std::string &name,
                i64 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Add for ui64.
        /// \param[in] name Name of value.
        /// \param[in] value ui64 value to add to Object.
        template<>
        inline void JSON::Object::Add (
                const std::string &name,
                ui64 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Add for f32.
        /// \param[in] name Name of value.
        /// \param[in] value f32 value to add to Object.
        template<>
        inline void JSON::Object::Add (
                const std::string &name,
                f32 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Add for f64.
        /// \param[in] name Name of value.
        /// \param[in] value f64 value to add to Object.
        template<>
        inline void JSON::Object::Add (
                const std::string &name,
                f64 value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Add for SizeT.
        /// \param[in] name Name of value.
        /// \param[in] value SizeT value to add to Object.
        template<>
        inline void JSON::Object::Add (
                const std::string &name,
                const SizeT &value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Add for const std::string &.
        /// \param[in] name Name of value.
        /// \param[in] value const std::string & value to add to Object.
        template<>
        inline void JSON::Object::Add (
                const std::string &name,
                const std::string &value) {
            if (!name.empty ()) {
                values.push_back (NameValue (name, Value::SharedPtr (new String (value))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Add for const char *.
        /// \param[in] name Name of value.
        /// \param[in] value const char * value to add to Object.
        template<>
        inline void JSON::Object::Add (
                const std::string &name,
                const char *value) {
            if (!name.empty () && value != 0) {
                values.push_back (NameValue (name, Value::SharedPtr (new String (value))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Insert for bool.
        /// \param[in] name Name of value.
        /// \param[in] value bool value to insert into Object.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Object::Insert (
                const std::string &name,
                bool value,
                std::size_t index) {
            if (!name.empty ()) {
                values.insert (
                    values.begin () + index,
                    NameValue (name, Value::SharedPtr (new Bool (value))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Insert for i8.
        /// \param[in] name Name of value.
        /// \param[in] value i8 value to insert into Object.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Object::Insert (
                const std::string &name,
                i8 value,
                std::size_t index) {
            if (!name.empty ()) {
                values.insert (
                    values.begin () + index,
                    NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Insert for ui8.
        /// \param[in] name Name of value.
        /// \param[in] value ui8 value to insert into Object.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Object::Insert (
                const std::string &name,
                ui8 value,
                std::size_t index) {
            if (!name.empty ()) {
                values.insert (
                    values.begin () + index,
                    NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Insert for i16.
        /// \param[in] name Name of value.
        /// \param[in] value i16 value to insert into Object.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Object::Insert (
                const std::string &name,
                i16 value,
                std::size_t index) {
            if (!name.empty ()) {
                values.insert (
                    values.begin () + index,
                    NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Insert for ui16.
        /// \param[in] name Name of value.
        /// \param[in] value ui16 value to insert into Object.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Object::Insert (
                const std::string &name,
                ui16 value,
                std::size_t index) {
            if (!name.empty ()) {
                values.insert (
                    values.begin () + index,
                    NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Insert for i32.
        /// \param[in] name Name of value.
        /// \param[in] value i32 value to insert into Object.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Object::Insert (
                const std::string &name,
                i32 value,
                std::size_t index) {
            if (!name.empty ()) {
                values.insert (
                    values.begin () + index,
                    NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Insert for ui32.
        /// \param[in] name Name of value.
        /// \param[in] value ui32 value to insert into Object.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Object::Insert (
                const std::string &name,
                ui32 value,
                std::size_t index) {
            if (!name.empty ()) {
                values.insert (
                    values.begin () + index,
                    NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Insert for i64.
        /// \param[in] name Name of value.
        /// \param[in] value i64 value to insert into Object.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Object::Insert (
                const std::string &name,
                i64 value,
                std::size_t index) {
            if (!name.empty ()) {
                values.insert (
                    values.begin () + index,
                    NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Insert for ui64.
        /// \param[in] name Name of value.
        /// \param[in] value ui64 value to insert into Object.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Object::Insert (
                const std::string &name,
                ui64 value,
                std::size_t index) {
            if (!name.empty ()) {
                values.insert (
                    values.begin () + index,
                    NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Insert for f32.
        /// \param[in] name Name of value.
        /// \param[in] value f32 value to insert into Object.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Object::Insert (
                const std::string &name,
                f32 value,
                std::size_t index) {
            if (!name.empty ()) {
                values.insert (
                    values.begin () + index,
                    NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Insert for f64.
        /// \param[in] name Name of value.
        /// \param[in] value f64 value to insert into Object.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Object::Insert (
                const std::string &name,
                f64 value,
                std::size_t index) {
            if (!name.empty ()) {
                values.insert (
                    values.begin () + index,
                    NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Insert for SizeT.
        /// \param[in] name Name of value.
        /// \param[in] value SizeT value to insert into Object.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Object::Insert (
                const std::string &name,
                const SizeT &value,
                std::size_t index) {
            if (!name.empty ()) {
                values.insert (
                    values.begin () + index,
                    NameValue (name, Value::SharedPtr (new Number (Variant (value)))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Insert for const std::string &.
        /// \param[in] name Name of value.
        /// \param[in] value const std::string & value to insert into Object.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Object::Insert (
                const std::string &name,
                const std::string &value,
                std::size_t index) {
            if (!name.empty ()) {
                values.insert (
                    values.begin () + index,
                    NameValue (name, Value::SharedPtr (new String (value))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Specialization of Object::Insert for const char *.
        /// \param[in] name Name of value.
        /// \param[in] value const char * value to insert into Object.
        /// \param[in] index Index where the new value will reside.
        template<>
        inline void JSON::Object::Insert (
                const std::string &name,
                const char *value,
                std::size_t index) {
            if (!name.empty () && value != 0) {
                values.insert (
                    values.begin () + index,
                    NameValue (name, Value::SharedPtr (new String (value))));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        /// \brief
        /// Write the given Exception::Location to the given object.
        /// \param[in] object Where to write the given exception.
        /// \param[in] location Exception::Location to write.
        /// \return object.
        _LIB_THEKOGANS_UTIL_DECL JSON::Object & _LIB_THEKOGANS_UTIL_API operator << (
            JSON::Object &object,
            const Exception::Location &location);

        /// \brief
        /// Read an Exception::Location from the given object.
        /// \param[in] object Where to read the exception from.
        /// \param[out] location Exception::Location to read.
        /// \return object.
        _LIB_THEKOGANS_UTIL_DECL JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (
            JSON::Object &object,
            Exception::Location &location);

        /// \brief
        /// Write the given exception to the given node.
        /// The object syntax looks like this:
        /// {
        ///   ErrorCode: 'error code',
        ///   Message: 'error message',
        ///   Location: [
        ///     {
        ///       File: 'file name',
        ///       Function: 'function name',
        ///       Line: 'line number',
        ///       BuildTime: 'build time'
        ///     },
        ///     ...
        ///   ]
        /// }
        /// \param[in] object Where to write the given exception.
        /// \param[in] exception Exception to write.
        /// \return object.
        _LIB_THEKOGANS_UTIL_DECL JSON::Object & _LIB_THEKOGANS_UTIL_API operator << (
            JSON::Object &object,
            const Exception &exception);

        /// \brief
        /// Read an Exception from the given object.
        /// The object syntax looks like this:
        /// {
        ///   ErrorCode: 'error code',
        ///   Message: 'error message',
        ///   Location: [
        ///     {
        ///       File: 'file name',
        ///       Function: 'function name',
        ///       Line: 'line number',
        ///       BuildTime: 'build time'
        ///     },
        ///     ...
        ///   ]
        /// }
        /// \param[in] object Where to read the exception from.
        /// \param[out] exception Exception to read.
        /// \return object.
        _LIB_THEKOGANS_UTIL_DECL JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (
            JSON::Object &object,
            Exception &exception);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_JSON_h)
