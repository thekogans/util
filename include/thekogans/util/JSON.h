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
#include <ostream>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

        struct _LIB_THEKOGANS_UTIL_DECL JSON {
            struct _LIB_THEKOGANS_UTIL_DECL Value : public ThreadSafeRefCounted {
                typedef ThreadSafeRefCounted::Ptr<Value> Ptr;

                enum Type {
                    JSON_VALUE_TYPE_VALUE,
                    JSON_VALUE_TYPE_BOOL,
                    JSON_VALUE_TYPE_NULL,
                    JSON_VALUE_TYPE_NUMBER,
                    JSON_VALUE_TYPE_STRING,
                    JSON_VALUE_TYPE_ARRAY,
                    JSON_VALUE_TYPE_OBJECT
                } type;
                const char *name;

                static const char *NAME;

                static std::string typeTostring (Type type);
                static Type stringToType (const std::string &type);

                virtual ~Value () {}

                inline Type GetType () const {
                    return type;
                }
                inline const char *GetName () const {
                    return name;
                }

                bool ToBool () const;
                f64 ToNumber () const;
                std::string ToString () const;

            protected:
                Value (
                    Type type_,
                    const char *name_) :
                    type (type_),
                    name (name_) {}
            };

            struct _LIB_THEKOGANS_UTIL_DECL Bool : public Value {
                typedef ThreadSafeRefCounted::Ptr<Bool> Ptr;

                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Bool, SpinLock)

                static const char *NAME;

                bool value;

                Bool (bool value_ = false) :
                    Value (JSON_VALUE_TYPE_BOOL, NAME),
                    value (value_) {}
            };

            struct _LIB_THEKOGANS_UTIL_DECL Null : public Value {
                typedef ThreadSafeRefCounted::Ptr<Null> Ptr;

                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Null, SpinLock)

                static const char *NAME;

                Null () :
                    Value (JSON_VALUE_TYPE_NULL, NAME) {}
            };

            struct _LIB_THEKOGANS_UTIL_DECL Number : public Value {
                typedef ThreadSafeRefCounted::Ptr<Number> Ptr;

                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Number, SpinLock)

                static const char *NAME;

                f64 value;

                Number (f64 value_ = 0.0) :
                    Value (JSON_VALUE_TYPE_NUMBER, NAME),
                    value (value_) {}
            };

            struct _LIB_THEKOGANS_UTIL_DECL String : public Value {
                typedef ThreadSafeRefCounted::Ptr<String> Ptr;

                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (String, SpinLock)

                static const char *NAME;

                std::string value;

                String (const std::string &value_ = std::string ()) :
                    Value (JSON_VALUE_TYPE_STRING, NAME),
                    value (value_) {}
            };

            struct _LIB_THEKOGANS_UTIL_DECL Array : public Value {
                typedef ThreadSafeRefCounted::Ptr<Array> Ptr;

                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Array, SpinLock)

                static const char *NAME;

                std::vector<Value::Ptr> values;

                Array () :
                    Value (JSON_VALUE_TYPE_ARRAY, NAME) {}

                void AddValue (Value::Ptr value);
                inline std::size_t GetValueCount () const {
                    return values.size ();
                }
                Value::Ptr GetValue (std::size_t index) const;
            };

            struct _LIB_THEKOGANS_UTIL_DECL Object : public Value {
                typedef ThreadSafeRefCounted::Ptr<Object> Ptr;

                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Object, SpinLock)

                static const char *NAME;

                typedef std::pair<Value::Ptr, Value::Ptr> NameValue;
                std::vector<NameValue> values;

                Object () :
                    Value (JSON_VALUE_TYPE_OBJECT, NAME) {}

                void AddValue (
                    Value::Ptr name,
                    Value::Ptr value);
                inline std::size_t GetValueCount () const {
                    return values.size ();
                }
                Value::Ptr GetValue (const std::string &name) const;
            };

            static Value::Ptr ParseValue (const std::string &json);
            static void FormatValue (
                std::ostream &stream,
                Value::Ptr value,
                std::size_t indentationLevel);
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_JSON_h)
