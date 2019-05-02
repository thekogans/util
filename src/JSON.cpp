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

#include <cctype>
#include "thekogans/util/Exception.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/JSON.h"

namespace thekogans {
    namespace util {

        std::string JSON::Value::typeTostring (Type type) {
            return type == JSON_VALUE_TYPE_BOOL ? Bool::NAME :
                type == JSON_VALUE_TYPE_NULL ? Null::NAME :
                type == JSON_VALUE_TYPE_NUMBER ? Number::NAME :
                type == JSON_VALUE_TYPE_STRING ? String::NAME :
                type == JSON_VALUE_TYPE_ARRAY ? Array::NAME :
                type == JSON_VALUE_TYPE_OBJECT ? Object::NAME : NAME;
        }

        JSON::Value::Type JSON::Value::stringToType (const std::string &type) {
            return type == Bool::NAME ? JSON_VALUE_TYPE_BOOL :
                type == Null::NAME ? JSON_VALUE_TYPE_NULL :
                type == Number::NAME ? JSON_VALUE_TYPE_NUMBER :
                type == String::NAME ? JSON_VALUE_TYPE_STRING  :
                type == Array::NAME ? JSON_VALUE_TYPE_ARRAY :
                type == Object::NAME ? JSON_VALUE_TYPE_OBJECT : JSON_VALUE_TYPE_VALUE;
        }

        bool JSON::Value::ToBool () const {
            if (type == JSON_VALUE_TYPE_BOOL) {
                return static_cast<const Bool *> (this)->value;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value is not %s (%s).",
                    typeTostring (type).c_str (),
                    Bool::NAME);
            }
        }

        f64 JSON::Value::ToNumber () const {
            if (type == JSON_VALUE_TYPE_NUMBER) {
                return static_cast<const Number *> (this)->value;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value is not %s (%s).",
                    typeTostring (type).c_str (),
                    Number::NAME);
            }
        }

        std::string JSON::Value::ToString () const {
            if (type == JSON_VALUE_TYPE_STRING) {
                return static_cast<const String *> (this)->value;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Value is not %s (%s).",
                    typeTostring (type).c_str (),
                    String::NAME);
            }
        }

        void JSON::Array::AddValue (Value::Ptr value) {
            if (value.Get () != 0) {
                values.push_back (value);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        JSON::Value::Ptr JSON::Array::GetValue (std::size_t index) const {
            if (index < values.size ()) {
                return values[index];
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void JSON::Object::AddValue (
                Value::Ptr name,
                Value::Ptr value) {
            if (name.Get () != 0 && name->type == JSON_VALUE_TYPE_STRING && value.Get () != 0) {
                values.push_back (NameValue (name, value));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        JSON::Value::Ptr JSON::Object::GetValue (const std::string &name) const {
            for (std::size_t i = 0, count = values.size (); i < count; ++i) {
                if (values[i].first->ToString () == name) {
                    return values[i].second;
                }
            }
            return Value::Ptr ();
        }

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (JSON::Bool, SpinLock)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (JSON::Null, SpinLock)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (JSON::Number, SpinLock)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (JSON::String, SpinLock)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (JSON::Array, SpinLock)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (JSON::Object, SpinLock)

        const char *JSON::Value::NAME = "Value";
        const char *JSON::Bool::NAME = "Bool";
        const char *JSON::Null::NAME = "Null";
        const char *JSON::Number::NAME = "Number";
        const char *JSON::String::NAME = "String";
        const char *JSON::Array::NAME = "Array";
        const char *JSON::Object::NAME = "Obbject";

        namespace {
            struct Token {
                enum Type {
                    TYPE_END,
                    TYPE_STRING,
                    TYPE_NUMBER,
                    TYPE_TRUE,
                    TYPE_FALSE,
                    TYPE_NULL,
                    TYPE_ARRAY_START,
                    TYPE_ARRAY_END,
                    TYPE_OBJECT_START,
                    TYPE_OBJECT_END,
                    TYPE_COLON,
                    TYPE_COMMA
                } type;
                std::string value;
                f64 number;

                Token (Type type_ = TYPE_END) :
                    type (type_),
                    number (0.0) {}
                Token (
                    Type type_,
                    const std::string &value_) :
                    type (type_),
                    value (value_),
                    number (0.0) {}
                Token (
                    const std::string &value_,
                    f64 number_) :
                    type (TYPE_NUMBER),
                    value (value_),
                    number (number_) {}
            };

            inline bool isdelim (char c) {
                return c == ',' || c == ':' || c == ']' || c == '}' || isspace (c) || c == 0;
            }

            struct Tokenizer {
                const char *json;
                std::list<Token> stack;

                Tokenizer (const char *json_) :
                    json (json_) {}

                Token GetToken () {
                    if (!stack.empty ()) {
                        Token token = stack.back ();
                        stack.pop_back ();
                        return token;
                    }
                    else {
                        while (*json != 0 && isspace (*json)) {
                            ++json;
                        }
                        switch (*json) {
                            case '\0': {
                                return Token (Token::TYPE_END);
                            }
                            case '[': {
                                ++json;
                                return Token (Token::TYPE_ARRAY_START);
                            }
                            case ']': {
                                ++json;
                                return Token (Token::TYPE_ARRAY_END);
                            }
                            case '{': {
                                ++json;
                                return Token (Token::TYPE_OBJECT_START);
                            }
                            case '}': {
                                ++json;
                                return Token (Token::TYPE_OBJECT_END);
                            }
                            case ':': {
                                ++json;
                                return Token (Token::TYPE_COLON);
                            }
                            case ',': {
                                ++json;
                                return Token (Token::TYPE_COMMA);
                            }
                            case 't': {
                                if (json[1] == 'r' &&
                                        json[2] == 'u' &&
                                        json[3] == 'e' &&
                                        isdelim (json[4])) {
                                    json += 4;
                                    return Token (Token::TYPE_TRUE, XML_TRUE);
                                }
                                else {
                                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                        "Invalid token: %s", json);
                                }
                            }
                            case 'f': {
                                if (json[1] == 'a' &&
                                        json[2] == 'l' &&
                                        json[3] == 's' &&
                                        json[4] == 'e' &&
                                        isdelim (json[5])) {
                                    json += 5;
                                    return Token (Token::TYPE_FALSE, XML_FALSE);
                                }
                                else {
                                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                        "Invalid token: %s", json);
                                }
                            }
                            case 'n': {
                                if (json[1] == 'u' &&
                                        json[2] == 'l' &&
                                        json[3] == 'l' &&
                                        isdelim (json[4])) {
                                    json += 4;
                                    return Token (Token::TYPE_NULL, "null");
                                }
                                else {
                                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                        "Invalid token: %s", json);
                                }
                            }
                            case '-':
                            case '+':
                            case '.':
                            case '0':
                            case '1':
                            case '2':
                            case '3':
                            case '4':
                            case '5':
                            case '6':
                            case '7':
                            case '8':
                            case '9': {
                                const char *start = json;
                                f64 number = stringTof64 (json, (char **)&json);
                                if (isdelim (*json)) {
                                    return Token (std::string (start, json), number);
                                }
                                else {
                                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                        "Invalid number: %s", json);
                                }
                            }
                            case '\"': {
                                ++json;
                                std::string value;
                                while (*json != 0 && *json != '\"') {
                                    if (*json == '\\') {
                                        ui32 ch = *++json;
                                        switch (ch) {
                                            case '\\':
                                            case '"':
                                            case '/': {
                                                value += (char)ch;
                                                break;
                                            }
                                            case 'b': {
                                                value += '\b';
                                                break;
                                            }
                                            case 'f': {
                                                value += '\f';
                                                break;
                                            }
                                            case 'n': {
                                                value += '\n';
                                                break;
                                            }
                                            case 'r': {
                                                value += '\r';
                                                break;
                                            }
                                            case 't': {
                                                value += '\t';
                                                break;
                                            }
                                            case 'u': {
                                                ch = 0;
                                                for (int i = 0; i < 4; ++i, ++json) {
                                                    if (isxdigit (*json)) {
                                                        ch = ch * 16 + ((*json <= '9') ? (*json - '0') : ((*json & ~' ') - 'A' + 10));
                                                    }
                                                    else {
                                                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                                            "Invalid Unicode encoding: %s", json);
                                                    }
                                                }
                                                if (ch < 0x80) {
                                                    value += (char)ch;
                                                }
                                                else if (ch < 0x800) {
                                                    value +=  0xC0 | (ch >> 6);
                                                    value +=  0x80 | (ch & 0x3F);
                                                }
                                                else {
                                                    value +=  0xE0 | (ch >> 12);
                                                    value +=  0x80 | ((ch >> 6) & 0x3F);
                                                    value +=  0x80 | (ch & 0x3F);
                                                }
                                                break;
                                            }
                                            default: {
                                                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                                    "Invalid escape sequence: %s", json);
                                            }
                                        }
                                    }
                                    else if (*json < ' ' || *json == '\x7F') {
                                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                            "Invalid string: %s.", json);
                                    }
                                    else {
                                        value += (char)*json++;
                                    }
                                }
                                if (*json != 0) {
                                    assert (*json == '\"');
                                    if (!isdelim (*++json)) {
                                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                            "Invalid string: %s.", json);
                                    }
                                }
                                else {
                                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                        "%s", "Missing '\"'.");
                                }
                                return Token (Token::TYPE_STRING, value);
                            }
                            default: {
                                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                    "Invalid json: %s", json);
                            }
                        }
                    }
                    return Token ();
                }

                void PushBack (const Token &token) {
                    stack.push_back (token);
                }
            };

            void ParseArray (
                Tokenizer &tokenizer,
                JSON::Array::Ptr array);
            void ParseObject (
                Tokenizer &tokenizer,
                JSON::Object::Ptr object);

            JSON::Value::Ptr ParseValueHelper (Tokenizer &tokenizer) {
                Token token = tokenizer.GetToken ();
                switch (token.type) {
                    case Token::TYPE_TRUE:
                    case Token::TYPE_FALSE:
                        return JSON::Value::Ptr (new JSON::Bool (token.value == XML_TRUE));
                    case Token::TYPE_NULL:
                        return JSON::Value::Ptr (new JSON::Null);
                    case Token::TYPE_NUMBER:
                        return JSON::Value::Ptr (new JSON::Number (token.number));
                    case Token::TYPE_STRING:
                        return JSON::Value::Ptr (new JSON::String (token.value));
                    case Token::TYPE_ARRAY_START: {
                        JSON::Value::Ptr array (new JSON::Array);
                        ParseArray (tokenizer, dynamic_refcounted_pointer_cast<JSON::Array> (array));
                        return array;
                    }
                    case Token::TYPE_OBJECT_START: {
                        JSON::Value::Ptr object (new JSON::Object);
                        ParseObject (tokenizer, dynamic_refcounted_pointer_cast<JSON::Object> (object));
                        return object;
                    }
                    default: {
                        tokenizer.PushBack (token);
                        return JSON::Value::Ptr ();
                    }
                }
            }

            void ParseArray (
                    Tokenizer &tokenizer,
                    JSON::Array::Ptr array) {
                while (1) {
                    JSON::Value::Ptr value = ParseValueHelper (tokenizer);
                    if (value.Get () != 0) {
                        array->AddValue (value);
                    }
                    Token token = tokenizer.GetToken ();
                    if (token.type == Token::TYPE_COMMA) {
                        if (value.Get () == 0) {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "%s", "Expecting value.");
                        }
                        continue;
                    }
                    else if (token.type == Token::TYPE_ARRAY_END) {
                        break;
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Unexpected token %s. Expecting ',' or ']'.",
                            token.value.c_str ());
                    }
                }
            }

            void ParseObject (
                    Tokenizer &tokenizer,
                    JSON::Object::Ptr object) {
                while (1) {
                    JSON::Value::Ptr name = ParseValueHelper (tokenizer);
                    if (name.Get () != 0) {
                        if (name->GetType () == JSON::Value::JSON_VALUE_TYPE_STRING) {
                            Token token = tokenizer.GetToken ();
                            if (token.type == Token::TYPE_COLON) {
                                JSON::Value::Ptr value = ParseValueHelper (tokenizer);
                                if (value.Get () != 0) {
                                    object->AddValue (name, value);
                                }
                                else {
                                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                        "%s", "Expecting value.");
                                }
                            }
                            else {
                                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                    "Unexpected token '%s'. Expecting ':'.",
                                    token.value.c_str ());
                            }
                        }
                        else {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "Unexpected value type '%s'. Expecting '%s'.",
                                JSON::Value::typeTostring (name->GetType ()).c_str (),
                                JSON::String::NAME);
                        }
                    }
                    Token token = tokenizer.GetToken ();
                    if (token.type == Token::TYPE_COMMA) {
                        if (name.Get () == 0) {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "%s", "Expecting name : value.");
                        }
                        continue;
                    }
                    else if (token.type == Token::TYPE_OBJECT_END) {
                        break;
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Unexpected token %s. Expecting ',' or '}'.",
                            token.value.c_str ());
                    }
                }
            }
        }

        JSON::Value::Ptr JSON::ParseValue (const std::string &json) {
            Tokenizer tokenizer (json.c_str ());
            return ParseValueHelper (tokenizer);
        }

        namespace {
            void FormatString (
                    std::ostream &stream,
                    const char *str,
                    std::size_t indentationLevel) {
                stream << std::string (indentationLevel * 2, ' ') << "\"";
                while (*str != '\0') {
                    char ch = *str++;
                    switch (ch) {
                        case '\b':
                            stream << "\\b";
                            break;
                        case '\f':
                            stream << "\\f";
                            break;
                        case '\n':
                            stream << "\\n";
                            break;
                        case '\r':
                            stream << "\\r";
                            break;
                        case '\t':
                            stream << "\\t";
                            break;
                        case '\\':
                            stream << "\\\\";
                            break;
                        case '"':
                            stream << "\\\"";
                            break;
                        default:
                            stream << ch;
                    }
                }
                stream << "\"";
            }
        }

        void JSON::FormatValue (
                std::ostream &stream,
                Value::Ptr value,
                std::size_t indentationLevel) {
            if (value.Get () != 0) {
                switch (value->GetType ()) {
                    case Value::JSON_VALUE_TYPE_VALUE: {
                        stream << std::string (indentationLevel * 2, ' ') << Value::NAME;
                        break;
                    }
                    case Value::JSON_VALUE_TYPE_BOOL: {
                        stream << std::string (indentationLevel * 2, ' ') << (value->ToBool () ? "true" : "false");
                        break;
                    }
                    case Value::JSON_VALUE_TYPE_NULL: {
                        stream << std::string (indentationLevel * 2, ' ') << "null";
                        break;
                    }
                    case Value::JSON_VALUE_TYPE_NUMBER: {
                        stream << std::string (indentationLevel * 2, ' ') << value->ToNumber ();
                        break;
                    }
                    case Value::JSON_VALUE_TYPE_STRING: {
                        FormatString (stream, value->ToString ().c_str (), indentationLevel);
                        break;
                    }
                    case Value::JSON_VALUE_TYPE_ARRAY: {
                        stream << std::string (indentationLevel * 2, ' ') << "[\n";
//                         for (auto i : o) {
//                             fprintf(stdout, "%*s", indent + SHIFT_WIDTH, "");
//                             dumpValue(i->value, indent + SHIFT_WIDTH);
//                             fprintf(stdout, i->next ? ",\n" : "\n");
//                         }
                        stream << std::string (indentationLevel * 2, ' ') << "]\n";
                        break;
                    }
                    case Value::JSON_VALUE_TYPE_OBJECT: {
                        stream << std::string (indentationLevel * 2, ' ') << "{\n";
//                         for (auto i : o) {
//                             fprintf(stdout, "%*s", indent + SHIFT_WIDTH, "");
//                             dumpString(i->key);
//                             fprintf(stdout, ": ");
//                             dumpValue(i->value, indent + SHIFT_WIDTH);
//                             fprintf(stdout, i->next ? ",\n" : "\n");
//                         }
                        stream << std::string (indentationLevel * 2, ' ') << "}\n";
                        break;
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    } // namespace util
} // namespace thekogans
