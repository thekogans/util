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

#if defined (TOOLCHAIN_OS_Windows)
    #if defined (__GNUC__)
        #include <sec_api/stdio_s.h>
    #endif // defined (__GNUC__)
    #include <comdef.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <cstdarg>
#include <cstring>
#include <sstream>
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/internal.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        const char * const Exception::TAG_EXCEPTION = "Exception";
        const char * const Exception::ATTR_ERROR_CODE = "ErrorCode";
        const char * const Exception::ATTR_MESSAGE = "Message";

        const char * const Exception::Location::TAG_LOCATION = "Location";
        const char * const Exception::Location::ATTR_FILE = "File";
        const char * const Exception::Location::ATTR_FUNCTION = "Function";
        const char * const Exception::Location::ATTR_LINE = "Line";
        const char * const Exception::Location::ATTR_BUILD_TIME = "BuildTime";

    #if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
        void Exception::Location::Parse (const pugi::xml_node &node) {
            file = Decodestring (node.attribute (ATTR_FILE).value ());
            function = Decodestring (node.attribute (ATTR_FUNCTION).value ());
            line = stringToui32 (node.attribute (ATTR_LINE).value ());
            buildTime = Decodestring (node.attribute (ATTR_BUILD_TIME).value ());
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)

        std::string Exception::Location::ToString (
                ui32 indentationLevel,
                const char *tagName) const {
            Attributes attributes;
            attributes.push_back (Attribute (ATTR_FILE, Encodestring (file)));
            attributes.push_back (Attribute (ATTR_FUNCTION, Encodestring (function)));
            attributes.push_back (Attribute (ATTR_LINE, ui32Tostring (line)));
            attributes.push_back (Attribute (ATTR_BUILD_TIME, Encodestring (buildTime)));
            return OpenTag (indentationLevel, tagName, attributes, true, true);
        }

        Exception::FilterList Exception::filterList;
        Mutex Exception::filterListMutex;

        void Exception::AddFilter (Filter::UniquePtr filter) {
            if (filter.get () != 0) {
                LockGuard<Mutex> guard (filterListMutex);
                filterList.push_back (std::move (filter));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool Exception::FilterException (const Exception &exception) {
            LockGuard<Mutex> guard (filterListMutex);
            for (FilterList::iterator
                    it = filterList.begin (),
                    end = filterList.end (); it != end; ++it) {
                if (!(*it)->FilterException (exception)) {
                    return false;
                }
            }
            return true;
        }

    #if defined (TOOLCHAIN_OS_Windows)
        namespace {
            struct NTDll : public Singleton<NTDll, SpinLock> {
                HMODULE handle;

                NTDll () :
                    handle (LoadLibrary ("NTDLL.DLL")) {}
                ~NTDll () {
                    FreeLibrary (handle);
                }

                DWORD FormatMessage (DWORD errorCode, char **buffer) {
                    return handle != 0 ? ::FormatMessage(
                        FORMAT_MESSAGE_ALLOCATE_BUFFER |
                        FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS |
                        FORMAT_MESSAGE_MAX_WIDTH_MASK |
                        FORMAT_MESSAGE_FROM_HMODULE,
                        handle,
                        errorCode,
                        MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                        (char *)buffer,
                        0,
                        0) : 0;
                }
            };
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        std::string Exception::FromErrorCode (THEKOGANS_UTIL_ERROR_CODE errorCode) {
            std::string message;
        #if defined (TOOLCHAIN_OS_Windows)
            char *buffer = 0;
            FormatMessage (
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS |
                FORMAT_MESSAGE_MAX_WIDTH_MASK,
                0,
                errorCode,
                MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                (char *)&buffer,
                0,
                0);
            if (buffer == 0) {
                NTDll::Instance ().FormatMessage (errorCode, &buffer);
            }
            if (buffer == 0) {
                message = FormatString (
                    "[0x%x:%d - Unable to find message text]",
                    errorCode, errorCode);
            }
            else {
                message = FormatString (
                    "[0x%x:%d - %s]",
                    errorCode, errorCode, buffer);
                LocalFree (buffer);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            message = FromPOSIXErrorCode (errorCode);
        #endif // defined (TOOLCHAIN_OS_Windows)
            return message;
        }

        std::string Exception::FromErrorCodeAndMessage (
                THEKOGANS_UTIL_ERROR_CODE errorCode,
                const char *format,
                ...) {
            std::string errorCodeMessage = FromErrorCode (errorCode);
            va_list argptr;
            va_start (argptr, format);
            std::string message = FormatStringHelper (format, argptr);
            va_end (argptr);
            return errorCodeMessage + message;
        }

    #if defined (TOOLCHAIN_OS_Windows)
        std::string Exception::FromHRESULTErrorCode (HRESULT errorCode) {
            _com_error comError (errorCode);
            return comError.ErrorMessage ();
        }

        std::string Exception::FromHRESULTErrorCodeAndMessage (
                HRESULT errorCode,
                const char *format,
                ...) {
            std::string errorCodeMessage = FromHRESULTErrorCode (errorCode);
            va_list argptr;
            va_start (argptr, format);
            std::string message = FormatStringHelper (format, argptr);
            va_end (argptr);
            return errorCodeMessage + message;
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        std::string Exception::FromPOSIXErrorCode (int errorCode) {
            std::string message;
            const char *buffer;
        #if defined (TOOLCHAIN_OS_Windows)
            char errorString[200];
            strerror_s (errorString, 200, errorCode);
            buffer = errorString;
        #else // defined (TOOLCHAIN_OS_Windows)
            buffer = strerror (errorCode);
        #endif // defined (TOOLCHAIN_OS_Windows)
            if (buffer != 0) {
                message = FormatString (
                    "[0x%x:%d - %s]",
                    errorCode, errorCode, buffer);
            }
            else {
                message = FormatString (
                    "[0x%x:%d - Unable to find message text]",
                    errorCode, errorCode);
            }
            return message;
        }

        std::string Exception::FromPOSIXErrorCodeAndMessage (
                int errorCode,
                const char *format,
                ...) {
            std::string errorCodeMessage = FromPOSIXErrorCode (errorCode);
            va_list argptr;
            va_start (argptr, format);
            std::string message = FormatStringHelper (format, argptr);
            va_end (argptr);
            return errorCodeMessage + message;
        }

        std::string Exception::GetTracebackReport () const {
            std::stringstream report;
            if (!traceback.empty ()) {
                report <<
                    "thrown from " << traceback[0].file <<
                    ", function " << traceback[0].function <<
                    ", line " << traceback[0].line <<
                    " (" << traceback[0].buildTime << ")";
                for (std::size_t i = 1, count = traceback.size (); i < count; ++i) {
                    report <<
                        "\n  seen at " << traceback[i].file <<
                        ", function " << traceback[i].function <<
                        ", line " << traceback[i].line <<
                        " (" << traceback[i].buildTime << ")";
                }
            }
            return report.str ();
        }

        std::string Exception::Report () const {
            return message + "\nTraceback:\n" + GetTracebackReport ();
        }

    #if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
        void Exception::Parse (const pugi::xml_node &node) {
            errorCode = stringToTHEKOGANS_UTIL_ERROR_CODE (node.attribute (ATTR_ERROR_CODE).value ());
            message = Decodestring (node.attribute (ATTR_MESSAGE).value ());
            for (pugi::xml_node child = node.first_child ();
                    !child.empty (); child = child.next_sibling ()) {
                if (child.type () == pugi::node_element &&
                        std::string (child.name ()) == Location::TAG_LOCATION) {
                    traceback.push_back (Location (child));
                }
            }
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)

        std::string Exception::ToString (
                ui32 indentationLevel,
                const char *tagName) const {
            Attributes attributes;
            attributes.push_back (Attribute (ATTR_ERROR_CODE, ErrorCodeTostring (errorCode)));
            attributes.push_back (Attribute (ATTR_MESSAGE, Encodestring (message)));
            std::stringstream stream;
            stream <<
                OpenTag (indentationLevel, tagName, attributes, false, true);
            for (std::size_t i = 0, count = traceback.size (); i < count; ++i) {
                stream << traceback[i].ToString (indentationLevel + 1);
            }
            stream << CloseTag (indentationLevel, tagName);
            return stream.str ();
        }

    } // namespace util
} // namespace thekogans
