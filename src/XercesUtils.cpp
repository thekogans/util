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

#if defined (THEKOGANS_UTIL_HAVE_XERCES)

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/XercesUtils.h"

namespace thekogans {
    namespace util {

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API XMLChTostring (
                const XMLCh *xml) {
            std::string str;
            if (xml != nullptr) {
                char *ch = XERCES_CPP_NAMESPACE::XMLString::transcode (xml);
                if (ch != nullptr) {
                    str = ch;
                    XERCES_CPP_NAMESPACE::XMLString::release (&ch);
                }
            }
            return str;
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API charTostring (
                const char *ch) {
            std::string str;
            if (ch != nullptr) {
                XMLCh *xml = XERCES_CPP_NAMESPACE::XMLString::transcode (ch);
                if (xml != nullptr) {
                    str = XMLChTostring (xml);
                    XERCES_CPP_NAMESPACE::XMLString::release (&xml);
                }
            }
            return str;
        }

        namespace {
            class XMLChPtr {
            private:
                XMLCh *xc;

            public:
                explicit XMLChPtr (XMLCh *xc_) :
                    xc (xc_) {}
                ~XMLChPtr () {
                    if (xc != nullptr) {
                        XERCES_CPP_NAMESPACE::XMLString::release (&xc);
                    }
                }
                inline XMLCh *c_str () const {
                    return xc;
                }
            };

            XMLChPtr stringToXMLCh (const std::string &s) {
                return XMLChPtr (!s.empty () ?
                    XERCES_CPP_NAMESPACE::XMLString::transcode (s.c_str ()) : nullptr);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API GetAttributeValue (
                const std::string &name,
                const XERCES_CPP_NAMESPACE::DOMNamedNodeMap *attributes) {
            XERCES_CPP_NAMESPACE::DOMNode *node =
                attributes->getNamedItem (stringToXMLCh (name).c_str ());
            return node ? XMLChTostring (node->getNodeValue ()) : std::string ();
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API XMLFileLocTostring (
                XMLFileLoc value,
                const char *format) {
            assert (format != nullptr);
            return format != nullptr ? FormatString (format, value) : std::string ();
        }

        void XercesErrorHandler::warning (
                const XERCES_CPP_NAMESPACE::SAXParseException &exception) {
            std::string message =
                "Warning in: " + XMLChTostring (exception.getSystemId ()) + "\n" +
                "Line: " + XMLFileLocTostring (exception.getLineNumber ()) + "\n" +
                "Char: " + XMLFileLocTostring (exception.getColumnNumber ()) + "\n" +
                "Message: " + XMLChTostring (exception.getMessage ()) + "\n";
            THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING (
                THEKOGANS_UTIL,
                message.c_str ());
        }

        void XercesErrorHandler::error (
                const XERCES_CPP_NAMESPACE::SAXParseException &exception) {
            std::string message =
                "Error in: " + XMLChTostring (exception.getSystemId ()) + "\n" +
                "Line: " + XMLFileLocTostring (exception.getLineNumber ()) + "\n" +
                "Char: " + XMLFileLocTostring (exception.getColumnNumber ()) + "\n" +
                "Message: " + XMLChTostring (exception.getMessage ()) + "\n";
            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (message.c_str ());
        }

        void XercesErrorHandler::fatalError (
                const XERCES_CPP_NAMESPACE::SAXParseException &exception) {
            std::string message =
                "Error in: " + XMLChTostring (exception.getSystemId ()) + "\n" +
                "Line: " + XMLFileLocTostring (exception.getLineNumber ()) + "\n" +
                "Char: " + XMLFileLocTostring (exception.getColumnNumber ()) + "\n" +
                "Message: " + XMLChTostring (exception.getMessage ()) + "\n";
            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (message.c_str ());
        }

    } // namespace util
} // namespace thekogans

#endif // defined (THEKOGANS_UTIL_HAVE_XERCES)
