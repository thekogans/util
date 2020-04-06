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
#endif // defined (TOOLCHAIN_OS_Windows)
#include <cstdarg>
#include <cstring>
#include <sstream>
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/Serializer.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/WindowsUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
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

        std::size_t Exception::Location::Size () const {
            return
                Serializer::Size (file) +
                Serializer::Size (function) +
                Serializer::Size (line) +
                Serializer::Size (buildTime);
        }

        void Exception::Location::Parse (const pugi::xml_node &node) {
            file = Decodestring (node.attribute (ATTR_FILE).value ());
            function = Decodestring (node.attribute (ATTR_FUNCTION).value ());
            line = stringToui32 (node.attribute (ATTR_LINE).value ());
            buildTime = Decodestring (node.attribute (ATTR_BUILD_TIME).value ());
        }

        std::string Exception::Location::ToString (
                std::size_t indentationLevel,
                const char *tagName) const {
            Attributes attributes;
            attributes.push_back (Attribute (ATTR_FILE, Encodestring (file)));
            attributes.push_back (Attribute (ATTR_FUNCTION, Encodestring (function)));
            attributes.push_back (Attribute (ATTR_LINE, ui32Tostring (line)));
            attributes.push_back (Attribute (ATTR_BUILD_TIME, Encodestring (buildTime)));
            return OpenTag (indentationLevel, tagName, attributes, true, true);
        }

        std::size_t Exception::Size () const {
            return
                Serializer::Size (errorCode) +
                Serializer::Size (message) +
                Serializer::Size (traceback);
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
            std::string GetErrorCodeMessage (
                    DWORD flags,
                    LPCVOID source,
                    DWORD errorCode) {
            wchar_t buffer[4096];
            return
                FormatMessageW (
                    flags,
                    source,
                    errorCode,
                    MAKELANGID (LANG_NEUTRAL, SUBLANG_NEUTRAL),
                    buffer,
                    _countof (buffer),
                    0) != 0 ? UTF16ToUTF8 (buffer) : std::string ();
            }

            struct NTDll : public Singleton<NTDll, SpinLock> {
                HMODULE handle;

                NTDll () :
                    handle (LoadLibrary ("NTDLL.DLL")) {}
                ~NTDll () {
                    FreeLibrary (handle);
                }

                std::string FormatMessage (DWORD errorCode) {
                    return handle != 0 ?
                        GetErrorCodeMessage (
                            FORMAT_MESSAGE_FROM_SYSTEM |
                            FORMAT_MESSAGE_IGNORE_INSERTS |
                            FORMAT_MESSAGE_MAX_WIDTH_MASK |
                            FORMAT_MESSAGE_FROM_HMODULE,
                            handle,
                            errorCode) :
                        std::string ();
                }
            };
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        std::string Exception::FromErrorCode (THEKOGANS_UTIL_ERROR_CODE errorCode) {
        #if defined (TOOLCHAIN_OS_Windows)
            std::string message = GetErrorCodeMessage (
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS |
                FORMAT_MESSAGE_MAX_WIDTH_MASK,
                0,
                errorCode);
            if (message.empty ()) {
                message = NTDll::Instance ().FormatMessage (errorCode);
            }
            return !message.empty () ?
                FormatString (
                    "[0x%x:%d - %s]",
                    errorCode, errorCode, message.c_str ()) :
                FormatString (
                    "[0x%x:%d - Unable to find message text]",
                    errorCode, errorCode);
        #else // defined (TOOLCHAIN_OS_Windows)
            return FromPOSIXErrorCode (errorCode);
        #endif // defined (TOOLCHAIN_OS_Windows)
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
            return GetErrorCodeMessage (FORMAT_MESSAGE_FROM_SYSTEM, 0, errorCode);
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
        #if defined (TOOLCHAIN_OS_Windows)
            wchar_t errorString[200];
            if (_wcserror_s (errorString, 200, errorCode) == 0) {
                return FormatString (
                    "[0x%x:%d - %s]",
                    errorCode, errorCode, UTF16ToUTF8 (errorString).c_str ());
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            char errorString[200];
            if (strerror_r (errorCode, errorString, 200) == 0) {
                return FormatString (
                    "[0x%x:%d - %s]",
                    errorCode, errorCode, errorString);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
            else {
                return FormatString (
                    "[0x%x:%d - Unable to find message text]",
                    errorCode, errorCode);
            }
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

        std::string Exception::ToString (
                std::size_t indentationLevel,
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

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const Exception::Location &location) {
            return serializer <<
                location.file <<
                location.function <<
                location.line <<
                location.buildTime;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Exception::Location &location) {
            return serializer >>
                location.file >>
                location.function >>
                location.line >>
                location.buildTime;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const Exception &exception) {
            return serializer <<
                exception.errorCode <<
                exception.message <<
                exception.traceback;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Exception &exception) {
            return serializer >>
                exception.errorCode >>
                exception.message >>
                exception.traceback;
        }

    } // namespace util
} // namespace thekogans
