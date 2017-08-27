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

#if !defined (__thekogans_util_XercesUtils_h)
#define __thekogans_util_XercesUtils_h

#if defined (THEKOGANS_UTIL_HAVE_XERCES)

#include <string>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLException.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/sax/ErrorHandler.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"

namespace thekogans {
    namespace util {

        /// \struct XercesPlatformInit XercesUtils.h thekogans/util/XercesUtils.h
        ///
        /// \brief
        /// Initialize the Xerces XML library.
        ///
        /// More often than not, you will create a stack instance of
        /// this class in main, and forget about it. But you can use
        /// it at any scope level you want.

        struct XercesPlatformInit {
            /// \brief
            /// ctor. Initialize the library.
            XercesPlatformInit () {
                XERCES_CPP_NAMESPACE::XMLPlatformUtils::Initialize ();
            }
            /// \brief
            /// dtor. Terminate the library.
            ~XercesPlatformInit () {
                XERCES_CPP_NAMESPACE::XMLPlatformUtils::Terminate ();
            }
        };

        /// \brief
        /// Convert a xerces XMLCh * to std::string.
        /// \param[in] xml XMLCh * pointing to a string to convert.
        /// \return A converted std::string.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API XMLChTostring (
            const XMLCh *xml);
        /// \brief
        /// Convert a char * to std::string.
        /// \param[in] ch char * pointing to a string to convert.
        /// \return A converted std::string.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API charTostring (
            const char *ch);
        /// \brief
        /// Given an attribute name, return it's value from the attribute list.
        /// \param[in] name Name of attribute whose value to return.
        /// \param[in] attributes List of attributes to search in.
        /// \return Value associated with the given attribute name.
        /// Empty string if not found.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API GetAttributeValue (
            const std::string &name,
            const XERCES_CPP_NAMESPACE::DOMNamedNodeMap *attributes);

        /// \brief
        /// Format a given xerces XMLFileLoc as a string.
        /// \param[in] value XMLFileLoc to format.
        /// \param[in] format printf style format string.
        /// \return Formated string.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API XMLFileLocTostring (
            XMLFileLoc value,
            const char *format = THEKOGANS_UTIL_UI32_FORMAT);

        /// \struct XercesErrorHandler XercesUtils.h thekogans/util/XercesUtils.h
        ///
        /// \brief
        /// A wrapper that transforms Xerces exceptions in to \see{Exception}.

        struct _LIB_THEKOGANS_UTIL_DECL XercesErrorHandler :
                public XERCES_CPP_NAMESPACE::ErrorHandler {
            /// \brief
            /// ctor.
            XercesErrorHandler () {}

            /// \brief
            /// Throw an THEKOGANS_UTIL_LOG_WARNING describing the warning.
            /// \param[in] exception Xerces warning.
            virtual void warning (
                const XERCES_CPP_NAMESPACE::SAXParseException &exception);
            /// \brief
            /// Throw an THEKOGANS_UTIL_THROW_STRING_EXCEPTION
            /// describing the error.
            /// \param[in] exception Xerces error.
            virtual void error (
                const XERCES_CPP_NAMESPACE::SAXParseException &exception);
            /// \brief
            /// Throw an THEKOGANS_UTIL_THROW_STRING_EXCEPTION
            /// describing the fatal error.
            /// \param[in] exception Xerces fatal error.
            virtual void fatalError (
                const XERCES_CPP_NAMESPACE::SAXParseException &exception);
            /// \brief
            /// Reset errors.
            virtual void resetErrors () {}

            /// \brief
            /// XercesErrorHandler is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (XercesErrorHandler)
        };

    } // namespace util
} // namespace thekogans

#endif // defined (THEKOGANS_UTIL_HAVE_XERCES)

#endif // !defined (__thekogans_util_XercesUtils_h)
