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
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

        /// \struct JSON JSON.h thekogans/util/JSON.h
        ///
        /// \brief
        /// JSON provides the DOM and parsing/serialization facilities to read and
        /// write JSON formatted values. It's used by \see{Serializable} as one of it's
        /// serialization/deserialization methods.

        struct _LIB_THEKOGANS_UTIL_DECL JSON {
            struct Bool;
            struct Null;
            struct Number;
            struct String;
            struct Array;
            struct Object;

            /// \struct JSON::Value JSON.h thekogans/util/JSON.h
            ///
            /// \brief
            /// Base for all JSON DOM types.
            struct _LIB_THEKOGANS_UTIL_DECL Value : public ThreadSafeRefCounted {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<Value>.
                typedef ThreadSafeRefCounted::Ptr<Value> Ptr;

                /// \enum
                /// Value type.
                enum Type {
                    JSON_VALUE_TYPE_VALUE,
                    JSON_VALUE_TYPE_BOOL,
                    JSON_VALUE_TYPE_NULL,
                    JSON_VALUE_TYPE_NUMBER,
                    JSON_VALUE_TYPE_STRING,
                    JSON_VALUE_TYPE_ARRAY,
                    JSON_VALUE_TYPE_OBJECT
                };

            protected:
                /// \brief
                /// Value type.
                Type  type;
                /// \brief
                /// Value name.
                const char *name;

            public:
                /// \brief
                /// "Value"
                static const char *NAME;

                /// \brief
                /// Convert type to it's string equivalent.
                static std::string typeTostring (Type type);
                /// \brief
                /// Convert string to it's type equivalent.
                static Type stringToType (const std::string &type);

                /// \brief
                /// dtor.
                virtual ~Value () {}

                /// \brief
                /// Return value type.
                /// \return Value type.
                inline Type GetType () const {
                    return type;
                }
                /// \brief
                /// Return value name.
                /// \return Value name.
                inline const char *GetName () const {
                    return name;
                }

                /// \brief
                /// Return Bool value (throw if type is not Bool).
                /// \return Bool value.
                bool ToBool () const;
                /// \brief
                /// Return Number value (throw if type is not Number).
                /// \return Number value.
                f64 ToNumber () const;
                /// \brief
                /// Return String value (throw if type is not String).
                /// \return String value.
                std::string ToString () const;

            protected:
                /// \brief
                /// ctor.
                /// NOTE: protected because Value should not be instantiated.
                /// Use a concrete Value type below.
                /// \param[in] type_ Value type.
                /// \param[in] name_ Value name.
                Value (
                    Type type_,
                    const char *name_) :
                    type (type_),
                    name (name_) {}
            };

            /// \struct JSON::Bool JSON.h thekogans/util/JSON.h
            ///
            /// \brief
            /// Represents a JSON bool (true/false) value.
            struct _LIB_THEKOGANS_UTIL_DECL Bool : public Value {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<Bool>.
                typedef ThreadSafeRefCounted::Ptr<Bool> Ptr;

                /// \brief
                /// Bool has a private heap to help with memory management.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Bool, SpinLock)

                /// \brief
                /// "Bool"
                static const char *NAME;

                /// \brief
                /// Bool value.
                bool value;

                /// \brief
                /// ctor.
                /// \param[in] value_ Bool value.
                Bool (bool value_ = false) :
                    Value (JSON_VALUE_TYPE_BOOL, NAME),
                    value (value_) {}
            };

            /// \struct JSON::Null JSON.h thekogans/util/JSON.h
            ///
            /// \brief
            /// Represents a JSON null value.
            struct _LIB_THEKOGANS_UTIL_DECL Null : public Value {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<Null>.
                typedef ThreadSafeRefCounted::Ptr<Null> Ptr;

                /// \brief
                /// Null has a private heap to help with memory management.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Null, SpinLock)

                /// \brief
                /// "Null"
                static const char *NAME;

                /// \brief
                /// ctor.
                Null () :
                    Value (JSON_VALUE_TYPE_NULL, NAME) {}
            };

            /// \struct JSON::Number JSON.h thekogans/util/JSON.h
            ///
            /// \brief
            /// Represents a JSON number value.
            struct _LIB_THEKOGANS_UTIL_DECL Number : public Value {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<Number>.
                typedef ThreadSafeRefCounted::Ptr<Number> Ptr;

                /// \brief
                /// Number has a private heap to help with memory management.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Number, SpinLock)

                /// \brief
                /// "Number"
                static const char *NAME;

                /// \brief
                /// Number value.
                f64 value;

                /// \brief
                /// ctor.
                /// \param[in] value_ Number value.
                Number (f64 value_ = 0.0) :
                    Value (JSON_VALUE_TYPE_NUMBER, NAME),
                    value (value_) {}
            };

            /// \struct JSON::String JSON.h thekogans/util/JSON.h
            ///
            /// \brief
            /// Represents a JSON string value.
            struct _LIB_THEKOGANS_UTIL_DECL String : public Value {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<String>.
                typedef ThreadSafeRefCounted::Ptr<String> Ptr;

                /// \brief
                /// String has a private heap to help with memory management.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (String, SpinLock)

                /// \brief
                /// "String"
                static const char *NAME;

                /// \brief
                /// String value.
                std::string value;

                /// \brief
                /// ctor.
                /// \param[in] value_ String value.
                String (const std::string &value_ = std::string ()) :
                    Value (JSON_VALUE_TYPE_STRING, NAME),
                    value (value_) {}
            };

            /// \struct JSON::Array JSON.h thekogans/util/JSON.h
            ///
            /// \brief
            /// Represents a JSON array value.
            struct _LIB_THEKOGANS_UTIL_DECL Array : public Value {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<Array>.
                typedef ThreadSafeRefCounted::Ptr<Array> Ptr;

                /// \brief
                /// Array has a private heap to help with memory management.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Array, SpinLock)

                /// \brief
                /// "Array"
                static const char *NAME;

                /// \brief
                /// Array of values.
                std::vector<Value::Ptr> values;

                /// \brief
                /// ctor.
                Array () :
                    Value (JSON_VALUE_TYPE_ARRAY, NAME) {}

                /// \brief
                /// Append the given value to the array.
                /// \param[in] value Value to append.
                void AddValue (Value::Ptr value);
                /// \brief
                /// Append the given bool to the array.
                /// \param[in] value Bool to append.
                void AddBool (bool value);
                /// \brief
                /// Append a null to the array.
                void AddNull ();
                /// \brief
                /// Append the given number to the array.
                /// \param[in] value Number to append.
                void AddNumber (f64 value);
                /// \brief
                /// Append the given string to the array.
                /// \param[in] value String to append.
                void AddString (const std::string &value);
                /// \brief
                /// Append the given array to the array.
                /// \param[in] value Array to append.
                void AddArray (Array::Ptr value);
                /// \brief
                /// Append the given object to the array.
                /// \param[in] value Object to append.
                void AddObject (ThreadSafeRefCounted::Ptr<Object> value);

                /// \brief
                /// Return count of values in the array.
                /// \return Count of values in the array.
                inline std::size_t GetValueCount () const {
                    return values.size ();
                }
                /// \brief
                /// Return the value at the given index.
                /// \param[in] index Index whose value to retrieve.
                /// \return Value at the given index.
                Value::Ptr GetValue (std::size_t index) const;
                /// \brief
                /// Return the array at the given index.
                /// NOTE: If the type at index is not an Array,
                //// Array::Ptr () will be returned.
                /// \param[in] index Index whose array to retrieve.
                /// \return Array at the given index.
                Array::Ptr GetArray (std::size_t index) const;
                /// \brief
                /// Return the object at the given index.
                /// NOTE: If the type at index is not an Object,
                /// Object::Ptr () will be returned.
                /// \param[in] index Index whose object to retrieve.
                /// \return Object at the given index.
                ThreadSafeRefCounted::Ptr<Object> GetObject (std::size_t index) const;
            };

            /// \struct JSON::Object JSON.h thekogans/util/JSON.h
            ///
            /// \brief
            /// Represents a JSON object value.
            struct _LIB_THEKOGANS_UTIL_DECL Object : public Value {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<Object>.
                typedef ThreadSafeRefCounted::Ptr<Object> Ptr;

                /// \brief
                /// Object has a private heap to help with memory management.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Object, SpinLock)

                /// \brief
                /// "Object"
                static const char *NAME;

                /// \brief
                /// Convenient typedef for std::pair<Value::Ptr, Value::Ptr>.
                typedef std::pair<Value::Ptr, Value::Ptr> NameValue;
                /// \brief
                /// Array of name/value pairs.
                std::vector<NameValue> values;

                /// \brief
                /// ctor.
                Object () :
                    Value (JSON_VALUE_TYPE_OBJECT, NAME) {}

                /// \brief
                /// Add a named value to the object.
                /// \param[in] name A value name.
                /// \param[in] value A value associated with the name.
                void AddValue (
                    const std::string &name,
                    Value::Ptr value);
                /// \brief
                /// Append the given bool to the object.
                /// \param[in] name A bool name.
                /// \param[in] value A bool value associated with the name.
                void AddBool (
                    const std::string &name,
                    bool value);
                /// \brief
                /// Append the given null to the object.
                /// \param[in] name A null name.
                void AddNull (const std::string &name);
                /// \brief
                /// Append the given number to the object.
                /// \param[in] name A number name.
                /// \param[in] value A number value associated with the name.
                void AddNumber (
                    const std::string &name,
                    f64 value);
                /// \brief
                /// Append the given string to the object.
                /// \param[in] name A string name.
                /// \param[in] value A string value associated with the name.
                void AddString (
                    const std::string &name,
                    const std::string &value);
                /// \brief
                /// Append the given array to the object.
                /// \param[in] name A array name.
                /// \param[in] value A array value associated with the name.
                void AddArray (
                    const std::string &name,
                    Array::Ptr value);
                /// \brief
                /// Append the given object to the object.
                /// \param[in] name A object name.
                /// \param[in] value A object value associated with the name.
                void AddObject (
                    const std::string &name,
                    Object::Ptr value);

                /// \brief
                /// Return count of name/values in the object.
                /// \return Count of name/values in the object.
                inline std::size_t GetValueCount () const {
                    return values.size ();
                }
                /// \brief
                /// Return the value associated with the given name.
                /// \param[in] name Name whose value to retrieve.
                /// \return Value associated with the given name.
                Value::Ptr GetValue (const std::string &name) const;
                /// \brief
                /// Return the array associated with the given name.
                /// NOTE: If the type at name is not an Array,
                /// Array::Ptr () will be returned.
                /// \param[in] name Name whose array to retrieve.
                /// \return Array associated with the given name.
                Array::Ptr GetArray (const std::string &name) const;
                /// \brief
                /// Return the object associated with the given name.
                /// NOTE: If the type at name is not an Object,
                /// Object::Ptr () will be returned.
                /// \param[in] name Name whose object to retrieve.
                /// \return Object associated with the given name.
                Object::Ptr GetObject (const std::string &name) const;
            };

            /// \brief
            /// Parse a JSON value to DOM.
            /// \param[in] value JSON formatted value.
            /// \retunr DOM representation of the given value.
            static Value::Ptr ParseValue (const std::string &value);
            /// \brief
            /// Format the given value.
            /// \param[in] value The value to format.
            /// \param[in] indentationLevel Pretty print parameter.
            /// \return Formatted value.
            static std::string FormatValue (
                Value::Ptr value,
                std::size_t indentationLevel = 0);
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_JSON_h)
