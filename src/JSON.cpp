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
#include <limits>
#include <sstream>
#include "thekogans/util/Heap.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/Base64.h"
#include "thekogans/util/JSON.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (thekogans::util::JSON::Value)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void JSON::Value::StaticInit () {
            Bool::StaticInit ();
            Null::StaticInit ();
            Number::StaticInit ();
            String::StaticInit ();
            Array::StaticInit ();
            Object::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        THEKOGANS_UTIL_IMPLEMENT_JSON_VALUE (thekogans::util::JSON::Bool)
        THEKOGANS_UTIL_IMPLEMENT_JSON_VALUE (thekogans::util::JSON::Null)
        THEKOGANS_UTIL_IMPLEMENT_JSON_VALUE (thekogans::util::JSON::Number)
        THEKOGANS_UTIL_IMPLEMENT_JSON_VALUE (thekogans::util::JSON::String)
        THEKOGANS_UTIL_IMPLEMENT_JSON_VALUE (thekogans::util::JSON::Array)
        THEKOGANS_UTIL_IMPLEMENT_JSON_VALUE (thekogans::util::JSON::Object)

        JSON::Array::Array (
                const std::string &str,
                const void *delimiter,
                std::size_t delimiterLength) {
            if (delimiter != nullptr && delimiterLength > 0) {
                std::string::size_type start = 0;
                std::string::size_type end =
                    str.find ((const char *)delimiter, start, delimiterLength);
                while (end != std::string::npos) {
                    Add<const std::string &> (str.substr (start, end - start));
                    start = end + delimiterLength;
                    end = str.find ((const char *)delimiter, start, delimiterLength);
                }
                if (start < str.size ()) {
                    Add<const std::string &> (str.substr (start));
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        std::string JSON::Array::ToString (
                const void *delimiter,
                std::size_t delimiterLength) {
            if (delimiter != nullptr && delimiterLength > 0) {
                const std::string _delimiter (
                    (const char *)delimiter,
                    (const char *)delimiter + delimiterLength);
                std::string str;
                for (std::size_t i = 0, count = values.size (); i < count; ++i) {
                    if (values[i]->Type () == String::TYPE) {
                        str += values[i]->To<std::string> () + _delimiter;
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Array contains non string values.");
                    }
                }
                return str;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void JSON::Array::Remove (std::size_t index) {
            if (index < values.size ()) {
                values.erase (values.begin () + index);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool JSON::Object::Contains (const std::string &name) const {
            for (std::size_t i = 0, count = values.size (); i < count; ++i) {
                if (values[i].first == name) {
                    return true;
                }
            }
            return false;
        }

        void JSON::Object::Remove (const std::string &name) {
            for (std::size_t i = 0, count = values.size (); i < count; ++i) {
                if (values[i].first == name) {
                    values.erase (values.begin () + i);
                    break;
                }
            }
        }

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
                Variant number;

                Token (Type type_ = TYPE_END) :
                    type (type_),
                    number (Variant {}) {}
                Token (
                    Type type_,
                    const std::string &value_) :
                    type (type_),
                    value (value_),
                    number (Variant {}) {}
                Token (
                    const std::string &value_,
                    const Variant &number_) :
                    type (TYPE_NUMBER),
                    value (value_),
                    number (number_) {}
            };

            enum {
                ARRAY_START = '[',
                ARRAY_END = ']',
                OBJECT_START = '{',
                OBJECT_END = '}',
                COLON = ':',
                COMMA = ',',
                QUOTE = '\"',
                SPACE = ' ',
            };

            inline bool isdelim (char c) {
                return
                    c == COMMA ||
                    c == COLON ||
                    c == ARRAY_END ||
                    c == OBJECT_END ||
                    isspace (c) ||
                    c == '\0';
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
                            case ARRAY_START: {
                                ++json;
                                return Token (Token::TYPE_ARRAY_START);
                            }
                            case ARRAY_END: {
                                ++json;
                                return Token (Token::TYPE_ARRAY_END);
                            }
                            case OBJECT_START: {
                                ++json;
                                return Token (Token::TYPE_OBJECT_START);
                            }
                            case OBJECT_END: {
                                ++json;
                                return Token (Token::TYPE_OBJECT_END);
                            }
                            case COLON: {
                                ++json;
                                return Token (Token::TYPE_COLON);
                            }
                            case COMMA: {
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
                            case QUOTE: {
                                ++json;
                                std::string value;
                                while (*json != 0 && *json != QUOTE) {
                                    ui32 ch = *json++;
                                    if (ch == '\\') {
                                        ch = *json++;
                                        switch (ch) {
                                            case '\\':
                                            case QUOTE:
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
                                                        ch = ch * 16 + ((*json <= '9') ?
                                                            (*json - '0') :
                                                            ((*json & ~SPACE) - 'A' + 10));
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
                                    else if (ch < SPACE || ch == '\x7F') {
                                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                            "Invalid string: %s.", json);
                                    }
                                    else {
                                        value += (char)ch;
                                    }
                                }
                                if (*json != 0) {
                                    assert (*json == QUOTE);
                                    if (!isdelim (*++json)) {
                                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                            "Invalid string: %s.", json);
                                    }
                                }
                                else {
                                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                        "Missing '\"'.");
                                }
                                return Token (Token::TYPE_STRING, value);
                            }
                            default: {
                                // It's not any of the well defined tokens. Try parsing a number.
                                // Numbers can have the following format:
                                // NaN
                                // [+ | -]Inf[inity]
                                // [+ | -][0..9]*[.][0..9]*[[E | e][+ | -][0..9]+]
                                const char *start = json;
                                // Check for NaN.
                                if (json[0] == 'N' && json[1] == 'a' && json[2] == 'N') {
                                    json += 3;
                                    if (isdelim (*json)) {
                                        return Token (
                                            std::string (start, json),
                                            Variant (std::numeric_limits<f64>::quiet_NaN ()));
                                    }
                                    else {
                                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                            "Invalid json: %s", json);
                                    }
                                }
                                // First, parse out the sign.
                                bool minus = false;
                                if (*json == '-') {
                                    minus = true;
                                    ++json;
                                }
                                else if (*json == '+') {
                                    // Leading '+' is harmless.
                                    ++json;
                                }
                                // Check for [+ | -]Inf[inity].
                                if (json[0] == 'I' && json[1] == 'n' && json[2] == 'f') {
                                    json += 3;
                                    if (json[0] == 'i' && json[1] == 'n' &&
                                            json[2] == 'i' && json[3] == 't' && json[4] == 'y') {
                                        json += 5;
                                    }
                                    if (isdelim (*json)) {
                                        f64 inf = std::numeric_limits<f64>::infinity ();
                                        if (minus) {
                                            inf = -inf;
                                        }
                                        return Token (std::string (start, json), Variant (inf));
                                    }
                                    else {
                                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                            "Invalid json: %s", json);
                                    }
                                }
                                // Skip over the integer part.
                                while (isdigit (*json)) {
                                    ++json;
                                }
                                // Create proper variant type based on the format of the number.
                                Variant value ((*json == '.' || *json == 'e' || *json == 'E') ?
                                    stringTof64 (start, (char **)&json) :
                                    minus ?
                                        stringToi64 (start, (char **)&json) :
                                        stringToui64 (start, (char **)&json));
                                if (json > start && isdelim (*json)) {
                                    return Token (std::string (start, json), value);
                                }
                                else {
                                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                        "Invalid json: %s", json);
                                }
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
                JSON::Array::SharedPtr array);
            void ParseObject (
                Tokenizer &tokenizer,
                JSON::Object::SharedPtr object);

            JSON::Value::SharedPtr ParseValueHelper (Tokenizer &tokenizer) {
                Token token = tokenizer.GetToken ();
                switch (token.type) {
                    case Token::TYPE_TRUE:
                    case Token::TYPE_FALSE:
                        return new JSON::Bool (token.value == XML_TRUE);
                    case Token::TYPE_NULL:
                        return new JSON::Null;
                    case Token::TYPE_NUMBER:
                        return new JSON::Number (token.number);
                    case Token::TYPE_STRING:
                        return new JSON::String (token.value);
                    case Token::TYPE_ARRAY_START: {
                        JSON::Value::SharedPtr array (new JSON::Array);
                        ParseArray (
                            tokenizer,
                            dynamic_refcounted_sharedptr_cast<JSON::Array> (array));
                        return array;
                    }
                    case Token::TYPE_OBJECT_START: {
                        JSON::Value::SharedPtr object (new JSON::Object);
                        ParseObject (
                            tokenizer,
                            dynamic_refcounted_sharedptr_cast<JSON::Object> (object));
                        return object;
                    }
                    default: {
                        tokenizer.PushBack (token);
                        return JSON::Value::SharedPtr ();
                    }
                }
            }

            void ParseArray (
                    Tokenizer &tokenizer,
                    JSON::Array::SharedPtr array) {
                bool expectValue = false;
                bool expectComma = false;
                while (1) {
                    JSON::Value::SharedPtr value = ParseValueHelper (tokenizer);
                    if (value != nullptr) {
                        if (!expectComma) {
                            array->Add (value);
                            expectValue = false;
                            expectComma = true;
                        }
                        else {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "Expecting value.");
                        }
                    }
                    Token token = tokenizer.GetToken ();
                    if (token.type == Token::TYPE_ARRAY_END) {
                        if (!expectValue) {
                            break;
                        }
                        else {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "Unexpected token '%s'. Expecting value.",
                                token.value.c_str ());
                        }
                    }
                    else if (token.type == Token::TYPE_COMMA) {
                        if (expectComma) {
                            expectValue = true;
                            expectComma = false;
                        }
                        else {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "Unexpected token '%s'. Expecting ','.",
                                token.value.c_str ());
                        }
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
                    JSON::Object::SharedPtr object) {
                bool expectNameValue = false;
                bool expectComma = false;
                while (1) {
                    Token token = tokenizer.GetToken ();
                    if (token.type == Token::TYPE_STRING) {
                        if (!expectComma) {
                            Token colon = tokenizer.GetToken ();
                            if (colon.type == Token::TYPE_COLON) {
                                JSON::Value::SharedPtr value = ParseValueHelper (tokenizer);
                                if (value != nullptr) {
                                    object->Add (token.value, value);
                                    expectNameValue = false;
                                    expectComma = true;
                                }
                                else {
                                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                        "Expecting value.");
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
                                "Unexpected token '%s'. Expecting ','.",
                                token.value.c_str ());
                        }
                    }
                    else if (token.type == Token::TYPE_OBJECT_END) {
                        if (!expectNameValue) {
                            break;
                        }
                        else {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "Unexpected token '%s'. Expecting \"name\".",
                                token.value.c_str ());
                        }
                    }
                    else if (token.type == Token::TYPE_COMMA) {
                        if (expectComma) {
                            expectNameValue = true;
                            expectComma = false;
                        }
                        else {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "Unexpected token '%s'. Expecting ','.",
                                token.value.c_str ());
                        }
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Unexpected token %s. Expecting ',' or '}'.",
                            token.value.c_str ());
                    }
                }
            }
        }

        JSON::Value::SharedPtr JSON::ParseValue (const std::string &value) {
            Tokenizer tokenizer (value.c_str ());
            return ParseValueHelper (tokenizer);
        }

        namespace {
            void FormatString (
                    std::ostream &stream,
                    const char *str) {
                stream << "\"";
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
                        case '\"':
                            stream << "\\\"";
                            break;
                        default:
                            stream << ch;
                    }
                }
                stream << "\"";
            }

            void FormatValueHelper (
                    std::ostream &stream,
                    const JSON::Value &value,
                    std::size_t indentationLevel,
                    std::size_t indentationWidth) {
                if (value.Type () == JSON::Bool::TYPE) {
                    stream << (value.To<bool> () ? XML_TRUE : XML_FALSE);
                }
                else if (value.Type () == JSON::Null::TYPE) {
                    stream << "null";
                }
                else if (value.Type () == JSON::Number::TYPE) {
                    stream << value.To<f64> ();
                }
                else if (value.Type () == JSON::String::TYPE) {
                    FormatString (stream, value.To<std::string> ().c_str ());
                }
                else if (value.Type () == JSON::Array::TYPE) {
                    stream << "[";
                    const JSON::Array &array = dynamic_cast<const JSON::Array &> (value);
                    if (!array.values.empty ()) {
                        stream << "\n" << std::string (indentationLevel * indentationWidth, SPACE);
                        for (std::size_t i = 0, count = array.values.size (); i < count; ++i) {
                            stream << std::string (indentationWidth, SPACE);
                            FormatValueHelper (
                                stream,
                                *array.values[i],
                                indentationLevel + 1,
                                indentationWidth);
                            if (i < count - 1) {
                                stream << ",";
                            }
                            stream << "\n" << std::string (indentationLevel * indentationWidth, SPACE);
                        }
                    }
                    stream << "]";
                }
                else if (value.Type () == JSON::Object::TYPE) {
                    stream << "{";
                    const JSON::Object &object = dynamic_cast<const JSON::Object &> (value);
                    if (!object.values.empty ()) {
                        stream << "\n" << std::string (indentationLevel * indentationWidth, SPACE);
                        for (std::size_t i = 0, count = object.values.size (); i < count; ++i) {
                            stream << std::string (indentationWidth, SPACE);
                            FormatString (stream, object.values[i].first.c_str ());
                            stream << ": ";
                            FormatValueHelper (
                                stream,
                                *object.values[i].second,
                                indentationLevel + 1,
                                indentationWidth);
                            if (i < count - 1) {
                                stream << ",";
                            }
                            stream << "\n" <<
                                std::string (indentationLevel * indentationWidth, SPACE);
                        }
                    }
                    stream << "}";
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Unknown value type %s.",
                        value.Type ());
                }
            }
        }

        std::string JSON::FormatValue (
                const Value &value,
                std::size_t indentationLevel,
                std::size_t indentationWidth) {
            std::stringstream stream;
            FormatValueHelper (stream, value, indentationLevel, indentationWidth);
            return stream.str ();
        }

    } // namespace util
} // namespace thekogans
