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

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_COMPILER_gcc)
    #include <sec_api/stdio_s.h>
#endif // defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_COMPILER_gcc)
#include <cstdarg>
#include <cstring>
#include <sstream>
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        std::size_t Exception::Location::Size () const {
            return
                Serializer::Size (file) +
                Serializer::Size (function) +
                Serializer::Size (line) +
                Serializer::Size (buildTime);
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
            if (filter.get () != nullptr) {
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
            // Use a no throwing version here to prevent infinite recursion.
            std::string UTF16ToUTF8 (
                    const wchar_t *utf16,
                    std::size_t length) {
                if (utf16 != nullptr && length > 0) {
                    int utf8Length = WideCharToMultiByte (
                        CP_UTF8,
                        0,
                        utf16,
                        (int)length,
                        0,
                        0,
                        0,
                        0);
                    if (utf8Length > 0) {
                        std::string utf8 (utf8Length, '?');
                        WideCharToMultiByte (
                            CP_UTF8,
                            0,
                            utf16,
                            (int)length,
                            (LPSTR)utf8.data (),
                            (int)utf8.size (),
                            0,
                            0);
                        return utf8;
                    }
                }
                return std::string ();
            }

            std::string GetErrorCodeMessage (
                    DWORD flags,
                    LPCVOID source,
                    DWORD errorCode) {
                wchar_t buffer[4096] = {0};
                FormatMessageW (
                    flags,
                    source,
                    errorCode,
                    MAKELANGID (LANG_NEUTRAL, SUBLANG_NEUTRAL),
                    buffer,
                    _countof (buffer),
                    0);
                return UTF16ToUTF8 (buffer, wcslen (buffer));
            }

            struct NTDll : public Singleton<NTDll, SpinLock> {
                HMODULE handle;

                NTDll () :
                    handle (LoadLibraryExW (L"NTDLL.DLL", 0, LOAD_LIBRARY_SEARCH_SYSTEM32)) {}
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
                message = NTDll::Instance ()->FormatMessage (errorCode);
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
            wchar_t errorString[200] = {0};
            if (_wcserror_s (errorString, 200, errorCode) == 0) {
                return FormatString (
                    "[0x%x:%d - %s]",
                    errorCode,
                    errorCode,
                    UTF16ToUTF8 (errorString, wcslen (errorString)).c_str ());
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            char errorString[200] = {0};
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

        namespace {
            const char * const ATTR_ERROR_CODE = "ErrorCode";
            const char * const ATTR_MESSAGE = "Message";

            const char * const TAG_LOCATION = "Location";
            const char * const ATTR_FILE = "File";
            const char * const ATTR_FUNCTION = "Function";
            const char * const ATTR_LINE = "Line";
            const char * const ATTR_BUILD_TIME = "BuildTime";
        }

        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
                pugi::xml_node &node,
                const Exception::Location &location) {
            node.append_attribute (ATTR_FILE).set_value (Encodestring (location.file).c_str ());
            node.append_attribute (ATTR_FUNCTION).set_value (Encodestring (location.function).c_str ());
            node.append_attribute (ATTR_LINE).set_value (ui32Tostring (location.line).c_str ());
            node.append_attribute (ATTR_BUILD_TIME).set_value (Encodestring (location.buildTime).c_str ());
            return node;
        }

        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
                pugi::xml_node &node,
                Exception::Location &location) {
            location.file = Decodestring (node.attribute (ATTR_FILE).value ());
            location.function = Decodestring (node.attribute (ATTR_FUNCTION).value ());
            location.line = stringToui32 (node.attribute (ATTR_LINE).value ());
            location.buildTime = Decodestring (node.attribute (ATTR_BUILD_TIME).value ());
            return node;
        }

        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
                pugi::xml_node &node,
                const Exception &exception) {
            node.append_attribute (ATTR_ERROR_CODE).set_value (ErrorCodeTostring (exception.errorCode).c_str ());
            node.append_attribute (ATTR_MESSAGE).set_value (Encodestring (exception.message).c_str ());
            for (std::size_t i = 0, count = exception.traceback.size (); i < count; ++i) {
                pugi::xml_node location = node.append_child (TAG_LOCATION);
                location << exception.traceback[i];
            }
            return node;
        }

        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
                pugi::xml_node &node,
                Exception &exception) {
            exception.errorCode = stringToTHEKOGANS_UTIL_ERROR_CODE (node.attribute (ATTR_ERROR_CODE).value ());
            exception.message = Decodestring (node.attribute (ATTR_MESSAGE).value ());
            for (pugi::xml_node child = node.first_child ();
                    !child.empty (); child = child.next_sibling ()) {
                if (child.type () == pugi::node_element &&
                        std::string (child.name ()) == TAG_LOCATION) {
                    Exception::Location location;
                    child >> location;
                    exception.traceback.push_back (location);
                }
            }
            return node;
        }

    } // namespace util
} // namespace thekogans
