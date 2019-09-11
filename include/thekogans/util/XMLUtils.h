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

#if !defined (__thekogans_util_XMLUtils_h)
#define __thekogans_util_XMLUtils_h

#include <string>
#include <vector>
#include "pugixml/pugixml.hpp"
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/StringUtils.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// <?xml version = \"1.0\" encoding = \"UTF-8\"?>
        _LIB_THEKOGANS_UTIL_DECL extern const char * const XML_HEADER;
        /// \brief
        /// <
        _LIB_THEKOGANS_UTIL_DECL extern const char * const XML_LT;
        /// \brief
        /// >
        _LIB_THEKOGANS_UTIL_DECL extern const char * const XML_GT;
        /// \brief
        /// =
        _LIB_THEKOGANS_UTIL_DECL extern const char * const XML_EQ;
        /// \brief
        /// true
        _LIB_THEKOGANS_UTIL_DECL extern const char * const XML_TRUE;
        /// \brief
        /// false
        _LIB_THEKOGANS_UTIL_DECL extern const char * const XML_FALSE;

        /// \brief
        /// Given a version and an encoding, format an XML
        /// document header.
        /// \param[in] version XML version string.
        /// \param[in] encoding XML encoding string.
        /// \return Formattted XML document header.
        inline std::string GetXMLHeader (
                const std::string &version = "1.0",
                const std::string &encoding = "UTF-8") {
            return FormatString (
                "<?xml version = \"%s\" encoding = \"%s\"?>",
                version.c_str (), encoding.c_str ());
        }

        /// \brief
        /// A convenient typedef for std::pair<std::string, std::string>.
        typedef std::pair<std::string, std::string> Attribute;
        /// \brief
        /// A convenient typedef for std::list<Attribute>.
        typedef std::vector<Attribute> Attributes;

        /// \brief
        /// Format an XML open tag (i.e. <tag>).
        /// \param[in] indentationLevel Level at which to indent the tag.
        /// \param[in] tagName Tag name to format.
        /// \param[in] attributes Optional list of attributes and their values.
        /// \param[in] close Optional close the tag (i.e. '/>' instead of '>').
        /// \param[in] endl Optional append a '\n'.
        /// \return Formated open tag.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API
            OpenTag (
                std::size_t indentationLevel,
                const char *tagName,
                const Attributes &attributes = Attributes (),
                bool close = false,
                bool endl = false);
        /// \brief
        /// Format an XML close tag (i.e. </tag>).
        /// \param[in] indentationLevel Level at which to indent the tag.
        /// \param[in] tagName Tag name to format.
        /// \param[in] endl Optional append a '\n'.
        /// \return Formated close tag.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API
            CloseTag (
                std::size_t indentationLevel,
                const char *tagName,
                bool endl = true);

        /// \brief
        /// If a string contains any of these: '"', '&', ''', '<', '>'
        /// Encode them using &'char entity name';
        /// \param[in] str String to encode.
        /// \return Encoded string.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API
            EncodeXMLCharEntities (const std::string &str);
        /// \brief
        /// Encode a URI replacing XML char entities
        /// with their hexadecimal equivalents.
        /// \param[in] uri URI to encode.
        /// \return Encoded URI.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API
            EncodeURI (const std::string &uri);
        /// \brief
        /// Decode a previously encoded URI. Replace hexadecimal
        /// numbers with their XML character entity equivalents.
        /// \param[in] uri URI to decode.
        /// \return Decoded URI.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API
            DecodeURI (const std::string &uri);
        /// \brief
        /// Wrapper for EncodeURI.
        /// \param[in] str String to encode.
        /// \return Encoded string.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API
            Encodestring (const std::string &str);
        /// \brief
        /// Wrapper for DecodeURI.
        /// \param[in] str String to decode.
        /// \return Decoded string.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API
            Decodestring (const std::string &str);

        /// \brief
        /// Format the given document.
        /// \param[in] document The document to format.
        /// \param[in] indentationLevel Pretty print parameter
        /// (Currently unused due to the limitations of pugixml).
        /// \param[in] indentationWidth Pretty print parameter.
        /// \return Formatted document.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API
            FormatDocument (
                const pugi::xml_document &document,
                std::size_t /*indentationLevel*/ = 0,
                std::size_t indentationWidth = 2);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_XMLUtils_h)
