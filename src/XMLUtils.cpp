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

#include <cstdio>
#include <vector>
#include <sstream>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Array.h"
#include "thekogans/util/XMLUtils.h"

namespace thekogans {
    namespace util {

        _LIB_THEKOGANS_UTIL_DECL extern const char * const XML_HEADER =
            "<?xml version = \"1.0\" encoding = \"UTF-8\"?>";
        _LIB_THEKOGANS_UTIL_DECL extern const char * const XML_LT = "<";
        _LIB_THEKOGANS_UTIL_DECL extern const char * const XML_GT = ">";
        _LIB_THEKOGANS_UTIL_DECL extern const char * const XML_EQ = "=";
        _LIB_THEKOGANS_UTIL_DECL extern const char * const XML_TRUE = "true";
        _LIB_THEKOGANS_UTIL_DECL extern const char * const XML_FALSE = "false";

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API OpenTag (
                std::size_t indentationLevel,
                const char *tagName,
                const Attributes &attributes,
                bool close,
                bool endl) {
            std::ostringstream stream;
            stream << std::string (indentationLevel * 2, ' ') << XML_LT << tagName;
            if (!attributes.empty ()) {
                stream << " ";
                std::string pad = std::string (stream.str ().size (), ' ');
                Attributes::const_iterator it = attributes.begin ();
                stream << it->first << " = " << "\"" << it->second << "\"";
                while (++it != attributes.end ()) {
                    stream << std::endl;
                    stream << pad << it->first << " = " << "\"" << it->second << "\"";
                }
            }
            if (close) {
                stream << "/";
            }
            stream << XML_GT;
            if (endl) {
                stream << std::endl;
            }
            return stream.str ();
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API CloseTag (
                std::size_t indentationLevel,
                const char *tagName,
                bool endl) {
            std::ostringstream stream;
            stream << std::string (indentationLevel * 2, ' ') << XML_LT << "/" << tagName << XML_GT;
            if (endl) {
                stream << std::endl;
            }
            return stream.str ();
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API
        EncodeXMLCharEntities (const std::string &str) {
            std::string encodedStr;
            encodedStr.reserve (str.size ());
            for (std::size_t i = 0, count = str.size (); i < count; ++i) {
                switch (str[i]) {
                    case '\"':
                        encodedStr += "&quot;";
                        break;
                    case '&':
                        encodedStr += "&amp;";
                        break;
                    case '\'':
                        encodedStr += "&apos;";
                        break;
                    case '<':
                        encodedStr += "&lt;";
                        break;
                    case '>':
                        encodedStr += "&gt;";
                        break;
                    default:
                        encodedStr += str[i];
                        break;
                }
            }
            return encodedStr;
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API EncodeURI (
                const std::string &uri) {
            // Only letters and numbers are safe.
            static const bool safeChar[256] = {
                /*      0      1      2      3      4      5      6      7      8      9      A      B      C      D      E      F */
                /* 0 */ false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                /* 1 */ false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                /* 2 */ false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                /* 3 */  true,  true,  true,  true,  true,  true,  true,  true,  true,  true, false, false, false, false, false, false,
                /* 4 */ false,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                /* 5 */  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true, false, false, false, false, false,
                /* 6 */ false,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                /* 7 */  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true, false, false, false, false, false,
                /* 8 */ false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                /* 9 */ false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                /* A */ false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                /* B */ false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                /* C */ false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                /* D */ false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                /* E */ false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                /* F */ false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false
            };
            static const char *hexTable = "0123456789ABCDEF";
            Array<ui8> encodedUri (uri.size () * 3);
            ui8 *ptr = encodedUri;
            for (const ui8 *start = (const ui8 *)uri.c_str (),
                     *end = start + uri.size (); start < end; ++start) {
                if (safeChar[*start]) {
                    *ptr++ = *start;
                }
                else {
                    // escape this char
                    *ptr++ = '%';
                    *ptr++ = hexTable[*start >> 4];
                    *ptr++ = hexTable[*start & 0x0f];
                }
            }
            return std::string ((char *)(ui8 *)encodedUri, (char *)ptr);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API DecodeURI (
                const std::string &uri) {
            static const ui8 hexTable[256] = {
                /*         0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F */
                /* 0 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* 1 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* 2 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* 3 */    0,    1,    2,    3,    4,    5,    6,    7,    8,    9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* 4 */ 0xff,   10,   11,   12,   13,   14,   15, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* 5 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* 6 */ 0xff,   10,   11,   12,   13,   14,   15, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* 7 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* 8 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* 9 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* A */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* B */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* C */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* D */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* E */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                /* F */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
            };
            Array<ui8> decodedUri (uri.size ());
            ui8 *ptr = decodedUri;
            // IMPORTANT NOTE FROM RFC1630:
            // "Sequences which start with a percent sign but
            // are not followed by two hexadecimal characters
            // (0-9, A-F) are reserved for future extension"
            // This means that if you try to decode a string
            // encoded by anything other than Encodestring,
            // you might run in to problems, as those chars
            // have 0xff at their index in the hexTable above.
            for (const ui8 *start = (const ui8 *)uri.c_str (),
                     *end = start + uri.size (); start < end;) {
                if (*start == '%') {
                    *ptr++ = (hexTable[*(start + 1)] << 4) + hexTable[*(start + 2)];
                    start += 3;
                }
                else {
                    *ptr++ = *start++;
                }
            }
            return std::string ((char *)(ui8 *)decodedUri, (char *)ptr);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API Encodestring (
                const std::string &str) {
            return EncodeURI (str);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API Decodestring (
                const std::string &str) {
            return DecodeURI (str);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API
        FormatNode (
                const pugi::xml_node &node,
                std::size_t indentationLevel,
                std::size_t indentationWidth) {
            // FIXME: implement
            assert (0);
            return std::string ();
        }

    } // namespace util
} // namespace thekogans
