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
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cctype>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include "thekogans/util/Array.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/XMLUtils.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/WindowsUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/StringUtils.h"

namespace thekogans {
    namespace util {

        _LIB_THEKOGANS_UTIL_DECL void _LIB_THEKOGANS_UTIL_API CopyString (
                char *destination,
                std::size_t destinationLength,
                const char *source) {
            if (destination != 0 && destinationLength != 0 && source != 0) {
                while (--destinationLength != 0 && *source != '\0') {
                    *destination++ = *source++;
                }
                *destination = '\0';
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API TrimLeftSpaces (
                const char *str) {
            if (str != 0) {
                while (*str != 0 && isspace (*str)) {
                    ++str;
                }
                if (*str != 0) {
                    return str;
                }
            }
            return std::string ();
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API TrimRightSpaces (
                const char *str) {
            if (str != 0) {
                for (const char *end = str + strlen (str); end > str; --end) {
                    if (!isspace (*(end - 1))) {
                        return std::string (str, end);
                    }
                }
            }
            return std::string ();
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API TrimSpaces (
                const char *str) {
            if (str != 0) {
                while (*str != 0 && isspace (*str)) {
                    ++str;
                }
                for (const char *end = str + strlen (str); end > str; --end) {
                    if (!isspace (*(end - 1))) {
                        return std::string (str, end);
                    }
                }
            }
            return std::string ();
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API StringToUpper (
                const char *str) {
            std::string upper;
            if (str != 0) {
                while (*str != 0) {
                    upper += toupper (*str++);
                }
            }
            return upper;
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API StringToLower (
                const char *str) {
            std::string lower;
            if (str != 0) {
                while (*str != 0) {
                    lower += tolower (*str++);
                }
            }
            return lower;
        }

        _LIB_THEKOGANS_UTIL_DECL int _LIB_THEKOGANS_UTIL_API StringCompareIgnoreCase (
                const char *str1,
                const char *str2) {
        #if defined (TOOLCHAIN_OS_Windows)
            return _stricmp (str1, str2);
        #else // defined (TOOLCHAIN_OS_Windows)
            return strcasecmp (str1, str2);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        _LIB_THEKOGANS_UTIL_DECL int _LIB_THEKOGANS_UTIL_API StringCompareIgnoreCase (
                const char *str1,
                const char *str2,
                std::size_t count) {
        #if defined (TOOLCHAIN_OS_Windows)
            return _strnicmp (str1, str2, count);
        #else // defined (TOOLCHAIN_OS_Windows)
            return strncasecmp (str1, str2, count);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        _LIB_THEKOGANS_UTIL_DECL const char * _LIB_THEKOGANS_UTIL_API IsUTF8String (
                const char *utf8,
                std::size_t length,
                std::size_t *utf8Length) {
            if (utf8 != 0 && length > 0) {
                std::size_t utf8Length_ = 0;
                for (const ui8 *start = (const ui8 *)utf8, *end = start + length; start < end;) {
                    if (start[0] < 0x80) {
                        // 0xxxxxxx
                        ++start;
                        ++utf8Length_;
                    }
                    else if ((start[0] & 0xe0) == 0xc0) {
                        // 110XXXXx 10xxxxxx
                        if ((start[1] & 0xc0) != 0x80 || (start[0] & 0xfe) == 0xc0) { // overlong?
                            return (const char *)start;
                        }
                        else {
                            start += 2;
                            ++utf8Length_;
                        }
                    }
                    else if ((start[0] & 0xf0) == 0xe0) {
                        // 1110XXXX 10Xxxxxx 10xxxxxx
                        if ((start[1] & 0xc0) != 0x80 ||
                                (start[2] & 0xc0) != 0x80 ||
                                (start[0] == 0xe0 && (start[1] & 0xe0) == 0x80) || // overlong?
                                (start[0] == 0xed && (start[1] & 0xe0) == 0xa0) || // surrogate?
                                (start[0] == 0xef && start[1] == 0xbf && (start[2] & 0xfe) == 0xbe)) { // U+FFFE or U+FFFF?
                            return (const char *)start;
                        }
                        else {
                            start += 3;
                            ++utf8Length_;
                        }
                    }
                    else if ((start[0] & 0xf8) == 0xf0) {
                        // 11110XXX 10XXxxxx 10xxxxxx 10xxxxxx
                        if ((start[1] & 0xc0) != 0x80 ||
                                (start[2] & 0xc0) != 0x80 ||
                                (start[3] & 0xc0) != 0x80 ||
                                (start[0] == 0xf0 && (start[1] & 0xf0) == 0x80) || // overlong? */
                                (start[0] == 0xf4 && start[1] > 0x8f) || start[0] > 0xf4) { // > U+10FFFF?
                            return (const char *)start;
                        }
                        else {
                            start += 4;
                            ++utf8Length_;
                        }
                    }
                    else {
                        return (const char *)start;
                    }
                }
                if (utf8Length != 0) {
                    *utf8Length = utf8Length_;
                }
                return 0;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        namespace {
            const char *hexTable = "0123456789abcdef";
        }

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API HexEncodeBuffer (
                const void *buffer,
                std::size_t length,
                char *hexBuffer) {
            if (buffer != 0 && length > 0 && hexBuffer != 0) {
                const char *ptr = (const char *)buffer;
                for (std::size_t i = 0; i < length; ++i) {
                    *hexBuffer++ = hexTable[(ptr[i] & 0xf0) >> 4];
                    *hexBuffer++ = hexTable[ptr[i] & 0x0f];
                }
                return length * 2;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }


        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API HexEncodeBuffer (
                const void *buffer,
                std::size_t length) {
            if (buffer != 0 && length > 0) {
                std::string hexString;
                hexString.resize (length * 2);
                HexEncodeBuffer (buffer, length, &hexString[0]);
                return hexString;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        namespace {
            inline char IsHexChar (char hexChar) {
                return
                    (hexChar >= '0' && hexChar <= '9') ||
                    (hexChar >= 'a' && hexChar <= 'f') ||
                    (hexChar >= 'A' && hexChar <= 'F');
            }

            inline char DecodeHexChar (char hexChar) {
                return
                    (hexChar >= '0' && hexChar <= '9') ? hexChar - '0' :
                    (hexChar >= 'a' && hexChar <= 'f') ? hexChar - 'a' + 10 :
                    (hexChar >= 'A' && hexChar <= 'F') ? hexChar - 'A' + 10 : -1;
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API HexDecodeBuffer (
                const char *hexBuffer,
                std::size_t length,
                void *buffer) {
            if (hexBuffer != 0 && length > 0 && (length & 1) == 0 && buffer != 0) {
                ui8 *ptr = (ui8 *)buffer;
                for (std::size_t i = 0; i < length; i += 2) {
                    if (IsHexChar (hexBuffer[i]) && IsHexChar (hexBuffer[i + 1])) {
                        *ptr++ = (DecodeHexChar (hexBuffer[i]) << 4) |
                            DecodeHexChar (hexBuffer[i + 1]);
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "%s", "hexBuffer is not a hex encoded buffer.");
                    }
                }
                return length / 2;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::vector<ui8> _LIB_THEKOGANS_UTIL_API HexDecodeBuffer (
                const char *hexBuffer,
                std::size_t length) {
            if (hexBuffer != 0 && length > 0 && (length & 1) == 0) {
                std::vector<ui8> buffer (length / 2);
                HexDecodeBuffer (hexBuffer, length, buffer.data ());
                return buffer;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API HexEncodestring (
                const std::string &str) {
            return HexEncodeBuffer (str.c_str (), str.size ());
        }

        _LIB_THEKOGANS_UTIL_DECL std::vector<ui8> _LIB_THEKOGANS_UTIL_API HexDecodestring (
                const std::string &hexString) {
            return HexDecodeBuffer (hexString.c_str (), hexString.size ());
        }

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API HexDecodestring (
                const std::string &hexString,
                void *buffer) {
            return HexDecodeBuffer (hexString.c_str (), hexString.size (), buffer);
        }

        namespace {
            void HexFormatRow (
                    std::ostringstream &stream,
                    const ui8 *ptr,
                    std::size_t columns) {
                for (const ui8 *begin = ptr,
                        *end = ptr + columns; begin != end; ++begin) {
                    const char hex[4] = {
                        hexTable[(*begin & 0xf0) >> 4],
                        hexTable[*begin & 0x0f],
                        ' ',
                        '\0'
                    };
                    stream << hex;
                }
                stream << " ";
            }

            void ASCIIFormatRow (
                    std::ostringstream &stream,
                    const ui8 *ptr,
                    std::size_t columns) {
                for (const ui8 *begin = ptr,
                        *end = ptr + columns; begin != end; ++begin) {
                    const char ch[2] = {
                        isprint (*begin) ? (char)*begin : (char)'.', (char)'\0'
                    };
                    stream << ch;
                }
                stream << std::endl;
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API HexFormatBuffer (
                const void *buffer,
                std::size_t length) {
            if (buffer != 0 && length > 0) {
                std::ostringstream stream;
                const ui8 *ptr = (const ui8 *)buffer;
                const std::size_t charsPerRow = 16;
                std::size_t rows = length / charsPerRow;
                for (std::size_t i = 0; i < rows; ++i) {
                    HexFormatRow (stream, ptr, charsPerRow);
                    ASCIIFormatRow (stream, ptr, charsPerRow);
                    ptr += charsPerRow;
                }
                std::size_t remainder = length % charsPerRow;
                if (remainder > 0) {
                    HexFormatRow (stream, ptr, remainder);
                    stream << std::string ((charsPerRow - remainder) * 3, ' ');
                    ASCIIFormatRow (stream, ptr, remainder);
                }
                return stream.str ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API HexFormatstring (
                const std::string &str) {
            return HexFormatBuffer (str.c_str (), str.size ());
        }

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API HashString (
                const std::string &str,
                std::size_t hashTableSize) {
            std::size_t hash = 5381;
            for (std::size_t i = 0, count = str.size (); i < count; ++i) {
                hash = ((hash << 5) + hash) ^ str[i]; // hash * 33 ^ str[i]
            }
            return hash % hashTableSize;
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API GetLongestCommonPrefix (
                const std::list<std::string> &strings) {
            std::string prefix;
            if (!strings.empty ()) {
                std::vector<std::string> vStrings;
                for (std::list<std::string>::const_iterator
                        it = strings.begin (),
                        end = strings.end (); it != end; ++it) {
                    vStrings.push_back (*it);
                }
                std::sort (vStrings.begin (), vStrings.end ());
                std::size_t index = 0;
                std::size_t lengthFirst = vStrings[0].size ();
                std::size_t lastIndex = vStrings.size () - 1;
                std::size_t lengthLast = vStrings[lastIndex].size ();
                for (; index < lengthFirst && index < lengthLast; ++index) {
                    if (vStrings[0][index] != vStrings[lastIndex][index]) {
                        break;
                    }
                }
                prefix = vStrings[0].substr (0, index);
            }
            return prefix;
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API FormatList (
                const std::list<std::string> &strings,
                const std::string &separator) {
            std::string formattedList;
            if (!strings.empty ()) {
                std::list<std::string>::const_iterator it = strings.begin ();
                formattedList = *it++;
                for (std::list<std::string>::const_iterator end = strings.end (); it != end; ++it) {
                    formattedList += separator + *it;
                }
            }
            return formattedList;
        }

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API stringTosize_t (
                const char *value,
                char **end,
                i32 base) {
            if (value != 0) {
            #if defined (TOOLCHAIN_ARCH_i386)
                return (std::size_t)strtoul (value, end, base);
            #elif defined (TOOLCHAIN_ARCH_x86_64)
                return (std::size_t)strtoull (value, end, base);
            #else // defined (TOOLCHAIN_ARCH_i386)
                #error "Unknown architecture."
            #endif // defined (TOOLCHAIN_ARCH_i386)
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API stringTobool (
                const char *value,
                char **end) {
            if (value != 0) {
                // Skip leading whitespace.
                while  (*value != 0 && isspace (*value)) {
                    ++value;
                }
                bool result = false;
                if (value[0] == 't' &&
                        value[1] == 'r' &&
                        value[2] == 'u' &&
                        value[3] == 'e') {
                    value += 4;
                    result = true;
                }
                else if (value[0] == 'f' &&
                        value[1] == 'a' &&
                        value[2] == 'l' &&
                        value[3] == 's' &&
                        value[4] == 'e') {
                    value += 5;
                }
                if (end != 0) {
                    *end = (char *)value;
                }
                return result;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL i8 _LIB_THEKOGANS_UTIL_API stringToi8 (
                const char *value,
                char **end,
                i32 base) {
            if (value != 0) {
                return (i8)strtol (value, end, base);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL ui8 _LIB_THEKOGANS_UTIL_API stringToui8 (
                const char *value,
                char **end,
                i32 base) {
            if (value != 0) {
                return (ui8)strtoul (value, end, base);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL i16 _LIB_THEKOGANS_UTIL_API stringToi16 (
                const char *value,
                char **end,
                i32 base) {
            if (value != 0) {
                return (i16)strtol (value, end, base);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL ui16 _LIB_THEKOGANS_UTIL_API stringToui16 (
                const char *value,
                char **end,
                i32 base) {
            if (value != 0) {
                return (ui16)strtoul (value, end, base);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL i32 _LIB_THEKOGANS_UTIL_API stringToi32 (
                const char *value,
                char **end,
                i32 base) {
            if (value != 0) {
                return (i32)strtol (value, end, base);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL ui32 _LIB_THEKOGANS_UTIL_API stringToui32 (
                const char *value,
                char **end,
                i32 base) {
            if (value != 0) {
                return (ui32)strtoul (value, end, base);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL i64 _LIB_THEKOGANS_UTIL_API stringToi64 (
                const char *value,
                char **end,
                i32 base) {
            if (value != 0) {
            #if defined (TOOLCHAIN_OS_Windows)
                return (i64)_strtoi64 (value, end, base);
            #else // defined (TOOLCHAIN_OS_Windows)
                return (i64)strtoll (value, end, base);
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL ui64 _LIB_THEKOGANS_UTIL_API stringToui64 (
                const char *value,
                char **end,
                i32 base) {
            if (value != 0) {
            #if defined (TOOLCHAIN_OS_Windows)
                return (ui64)_strtoui64 (value, end, base);
            #else // defined (TOOLCHAIN_OS_Windows)
                return (ui64)strtoull (value, end, base);
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL f32 _LIB_THEKOGANS_UTIL_API stringTof32 (
                const char *value,
                char **end) {
            if (value != 0) {
                return (f32)strtod (value, end);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL f64 _LIB_THEKOGANS_UTIL_API stringTof64 (
                const char *value,
                char **end) {
            if (value != 0) {
                return (f64)strtod (value, end);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL time_t _LIB_THEKOGANS_UTIL_API stringTotime_t (
                const char *value,
                char **end) {
            if (value != 0) {
            #if defined (TOOLCHAIN_ARCH_i386)
                return (time_t)stringToi32 (value, end);
            #elif defined (TOOLCHAIN_ARCH_x86_64)
                return (time_t)stringToi64 (value, end);
            #else // defined (TOOLCHAIN_ARCH_i386)
                #error "Unknown architecture."
            #endif // defined (TOOLCHAIN_ARCH_i386)
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL THEKOGANS_UTIL_ERROR_CODE _LIB_THEKOGANS_UTIL_API
        stringToTHEKOGANS_UTIL_ERROR_CODE (
                const char *value,
                char **end) {
            if (value != 0) {
            #if defined (TOOLCHAIN_OS_Windows)
                return (THEKOGANS_UTIL_ERROR_CODE)stringToui32 (value, end);
            #else // defined (TOOLCHAIN_OS_Windows)
                return (THEKOGANS_UTIL_ERROR_CODE)stringToi32 (value, end);
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API PointerTostring (
                const void *value) {
            return FormatString ("%p", value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API size_tTostring (
                std::size_t value,
                const char *format) {
            return FormatString (format, value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API boolTostring (
                bool value) {
            return value ? XML_TRUE : XML_FALSE;
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API i32Tostring (
                i32 value,
                const char *format) {
            return FormatString (format, value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API ui32Tostring (
                ui32 value,
                const char *format) {
            return FormatString (format, value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API i64Tostring (
                i64 value,
                const char *format) {
            return FormatString (format, value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API ui64Tostring (
                ui64 value,
                const char *format) {
            return FormatString (format, value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API f32Tostring (
                f32 value,
                const char *format) {
            return FormatString (format, value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API f64Tostring (
                f64 value,
                const char *format) {
            return FormatString (format, value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API time_tTostring (
                time_t value,
                const char *format) {
            return FormatString (format, value);
        }

    #if !defined (TOOLCHAIN_OS_Windows)
        namespace {
            int _vscprintf (
                    const char *format,
                    va_list argptr) {
                va_list myargptr;
                va_copy (myargptr, argptr);
                int result = vsnprintf (0, 0, format, myargptr);
                va_end (myargptr);
                return result;
            }
        }
    #endif // !defined (TOOLCHAIN_OS_Windows)

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API FormatStringHelper (
                const char *format,
                va_list argptr) {
            if (format != 0) {
                std::string str;
                int size = _vscprintf (format, argptr);
                if (size > 0) {
                    str.resize (size + 1);
                #if defined (TOOLCHAIN_OS_Windows)
                    vsprintf_s (&str[0], size + 1, format, argptr);
                #else // defined (TOOLCHAIN_OS_Windows)
                    vsnprintf (&str[0], size + 1, format, argptr);
                #endif // defined (TOOLCHAIN_OS_Windows)
                    str.resize (size);
                }
                return str;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::string FormatString (
                const char *format,
                ...) {
            if (format != 0) {
                va_list argptr;
                va_start (argptr, format);
                std::string str = FormatStringHelper (format, argptr);
                va_end (argptr);
                return str;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API ErrorCodeTostring (
                THEKOGANS_UTIL_ERROR_CODE value) {
        #if defined (TOOLCHAIN_OS_Windows)
            return ui32Tostring (value);
        #else // defined (TOOLCHAIN_OS_Windows)
            return i32Tostring (value);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API GetEnvironmentVariable (
                const std::string &name) {
            if (!name.empty ()) {
            #if defined (TOOLCHAIN_OS_Windows)
                // Windows limit on value length.
                wchar_t value[32767];
                std::size_t length = GetEnvironmentVariableW (UTF8ToUTF16 (name).c_str (), value, 32767);
                if (length > 0) {
                    return UTF16ToUTF8 (value, length);
                }
                else {
                    THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                    if (errorCode != ERROR_ENVVAR_NOT_FOUND) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                const char *value = getenv (name.c_str ());
                if (value != 0) {
                    return value;
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
                return std::string ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    } // namespace util
} // namespace thekogans
