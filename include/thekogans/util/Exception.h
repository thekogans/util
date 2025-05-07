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

#if !defined (__thekogans_util_Exception_h)
#define __thekogans_util_Exception_h

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
    #include <cstdio>
#else // defined (TOOLCHAIN_OS_Windows)
    #include <errno.h>
    #if defined (TOOLCHAIN_OS_OSX)
        #include <mach/mach_error.h>
        #include <SystemConfiguration/SystemConfiguration.h>
    #endif // defined (TOOLCHAIN_OS_OSX)
#endif // defined (TOOLCHAIN_OS_Windows)
#include <memory>
#include <string>
#include <vector>
#include <list>
#include <exception>
#include <functional>
#include <ostream>
#include "pugixml/pugixml.hpp"
#include "thekogans/util/Config.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/StringUtils.h"
#if defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/os/osx/OSXUtils.h"
#endif // defined (TOOLCHAIN_OS_OSX)

namespace thekogans {
    namespace util {

        struct Serializer;

        /// \struct Exception Exception.h thekogans/util/Exception.h
        ///
        /// \brief
        /// Exception adds traceback on top of std::exception. Use
        /// various macros below to capture location, and re-throw
        /// (or log) exceptions. Exception can be formated using os
        /// specific error codes in a platform independent manner.
        /// Exception is integrated with \see{LoggerMgr} to make it
        /// easy to record, and analyze application runtime.

    #if defined (TOOLCHAIN_COMPILER_cl)
        #pragma warning (push)
        #pragma warning (disable : 4275)
    #endif // defined (TOOLCHAIN_COMPILER_cl)

        struct _LIB_THEKOGANS_UTIL_DECL Exception : public std::exception {
            /// \brief
            /// Alias for std::function<bool (const Exception & /*exception*/)>.
            using Filter = std::function<bool (const Exception & /*exception*/)>;

            /// \struct Exception::Location Exception.h thekogans/util/Exception.h
            ///
            /// \brief
            /// Structure to hold location information, as the
            /// exception propagates through the system.
            struct Location {
                /// \brief
                /// Module where the exception is seen.
                std::string file;
                /// \brief
                /// Function where the exception is seen.
                std::string function;
                /// \brief
                /// Exception line number.
                ui32 line;
                /// \brief
                /// Module build date and time.
                std::string buildTime;

                /// \brief
                /// ctor.
                Location () :
                    line (0) {}
                /// \brief
                /// ctor.
                /// \param[in] file_ Module path.
                /// \param[in] function_ Function in module.
                /// \param[in] line_ Line number.
                /// \param[in] buildTime_ Module build date and time.
                Location (
                    const std::string &file_,
                    const std::string &function_,
                    ui32 line_,
                    const std::string &buildTime_) :
                    file (file_),
                    function (function_),
                    line (line_),
                    buildTime (buildTime_) {}

                /// \brief
                /// Return location serialized size.
                /// \return Location serialized size.
                std::size_t Size () const;
            };

        private:
            /// \brief
            /// Exception error code.
            THEKOGANS_UTIL_ERROR_CODE errorCode;
            /// \brief
            /// Message describing the error.
            std::string message;
            /// \brief
            /// Stack of locations representing the
            /// exception path through the system.
            mutable std::vector<Location> traceback;
            /// \brief
            /// Alias for std::list<Filter::UniquePtr>.
            using FilterList = std::list<Filter>;
            /// \brief
            /// List of registered filters.
            static FilterList filterList;
            /// \brief
            /// Synchronization mutex.
            static Mutex filterListMutex;

        public:
            /// \brief
            /// ctor.
            /// \param[in] errorCode_ Exception error code.
            /// \param[in] message_ Message describing the error.
            /// \param[in] traceback_ of locations representing the exception path through the system.
            Exception (
                THEKOGANS_UTIL_ERROR_CODE errorCode_ = 0,
                const std::string &message_ = std::string (),
                const std::vector<Location> &traceback_ = std::vector<Location> ()) :
                errorCode (errorCode_),
                message (message_),
                traceback (traceback_) {}
            /// \brief
            /// ctor.
            /// \param[in] file Translation unit of this exception.
            /// \param[in] function Function in the translation unit of this exception.
            /// \param[in] line Translation unit line number of this exception.
            /// \param[in] buildTime Translation unit build time of this exception.
            /// \param[in] errorCode_ Exception error code.
            /// \param[in] message_  Exception message.
            Exception (
                const char *file,
                const char *function,
                ui32 line,
                const char *buildTime,
                THEKOGANS_UTIL_ERROR_CODE errorCode_,
                const std::string message_) :
                errorCode (errorCode_),
                message (message_),
                traceback (1, Location (file, function, line, buildTime)) {}
            /// \brief
            /// copy ctor.
            /// \param[in] exception Exception to copy.
            Exception (const Exception &exception) :
                errorCode (exception.errorCode),
                message (exception.message),
                traceback (exception.traceback) {}

            /// \brief
            /// Assignment operator.
            /// \param[in] exception Exception to copy.
            /// \return *this
            Exception &operator = (const Exception &exception);

            /// \brief
            /// Return exception serialized size.
            /// \return Exception serialized size.
            std::size_t Size () const;

            /// \brief
            /// Return exception error code.
            /// \return Exception error code.
            inline THEKOGANS_UTIL_ERROR_CODE GetErrorCode () const {
                return errorCode;
            }

            // std::exception
            /// \brief
            /// Return the message text.
            /// \return '\0' terminated message text.
            virtual const char *what () const noexcept override {
                return message.c_str ();
            }

            /// \brief
            /// Return the list of locations this exception hit while unwinding the stack.
            /// \return List of locations this exception hit while unwinding the stack.
            inline const std::vector<Location> &GetTraceback () const {
                return traceback;
            }

            /// \brief
            /// Add a filter to the Exception.
            /// \param[in] filter Filter to add.
            static void AddFilter (const Filter &filter);

            /// \brief
            /// Return true if exception passed all registered filters.
            /// \param[in] exception Exception to filter.
            /// \return true if exception passed all registered filters.
            static bool FilterException (const Exception &exception);

            /// \brief
            /// Look up text for error code.
            /// \param[in] errorCode Error code which to get a text message.
            /// \return Text message for error code.
            static std::string FromErrorCode (
                THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE);
            /// \brief
            /// Look up text for error code and combine it with a supplied message.
            /// \param[in] errorCode Error code which to get a text message.
            /// \param[in] format Message text printf style format string.
            /// \param[in] ... Message text printf style arguments.
            /// \return Text message for error code + message.
            static std::string FromErrorCodeAndMessage (
                THEKOGANS_UTIL_ERROR_CODE errorCode,
                const char *format,
                ...);
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Look up text for HRESULT error code.
            /// \param[in] errorCode Error code which to get a text message.
            /// \return Text message for HRESULT error code.
            static std::string FromHRESULTErrorCode (HRESULT errorCode);
            /// \brief
            /// Look up text for HRESULT error code and combine it with a supplied message.
            /// \param[in] errorCode Error code which to get a text message.
            /// \param[in] format Message text printf style format string.
            /// \param[in] ... Message text printf style arguments.
            /// \return Text message for HRESULT error code + message.
            static std::string FromHRESULTErrorCodeAndMessage (
                HRESULT errorCode,
                const char *format,
                ...);
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Look up text for POSIX error code.
            /// \param[in] errorCode Error code which to get a text message.
            /// \return Text message for POSIX error code.
            static std::string FromPOSIXErrorCode (i32 errorCode = errno);
            /// \brief
            /// Look up text for POSIX error code and combine it with a supplied message.
            /// \param[in] errorCode Error code which to get a text message.
            /// \param[in] format Message text printf style format string.
            /// \param[in] ... Message text printf style arguments.
            /// \return Text message for POSIX error code + message.
            static std::string FromPOSIXErrorCodeAndMessage (
                i32 errorCode,
                const char *format,
                ...);

            /// \brief
            /// Add a traceback location, This function is used by the
            /// THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION macro below to
            /// capture the path of the exception as it's being unwound.
            /// \param[in] file Translation unit of this location.
            /// \param[in] function Function in the translation unit of this location.
            /// \param[in] line Translation unit line number of this location.
            /// \param[in] buildTime Translation unit build time of this location.
            inline void NoteLocation (
                    const char *file,
                    const char *function,
                    ui32 line,
                    const char *buildTime) const {
                traceback.push_back (Location (file, function, line, buildTime));
            }
            /// \brief
            /// Format a traceback report.
            /// \return A formated traceback report suitable for logging.
            std::string GetTracebackReport () const;
            /// \brief
            /// Format exception message, and traceback.
            /// \return A formated message and traceback suitable for logging.
            std::string Report () const;

            /// \brief
            /// operator << needs access to Location.
            friend _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const Location &location);
            /// \brief
            /// operator >> needs access to Location.
            friend _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Location &location);

            /// \brief
            /// operator << needs access to provate members.
            friend _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const Exception &exception);
            /// \brief
            /// operator >> needs access to provate members.
            friend _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Exception &exception);

            /// \brief
            /// operator << needs access to Location.
            friend _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
                pugi::xml_node &node,
                const Location &location);
            /// \brief
            /// operator >> needs access to Location.
            friend _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
                pugi::xml_node &node,
                Location &location);

            /// \brief
            /// operator << needs access to provate members.
            friend _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
                pugi::xml_node &node,
                const Exception &exception);
            /// \brief
            /// operator >> needs access to provate members.
            friend _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
                pugi::xml_node &node,
                Exception &exception);
        };

    #if defined (TOOLCHAIN_COMPILER_cl)
        #pragma warning (pop)
    #endif // defined (TOOLCHAIN_COMPILER_cl)

    #if defined (TOOLCHAIN_OS_Windows)
        /// \def THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL
        /// OS specific invalid parameter error code.
        #define THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL ERROR_INVALID_PARAMETER
        /// \def THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL
        /// OS specific out of memory error code.
        #define THEKOGANS_UTIL_OS_ERROR_CODE_ENOMEM ERROR_NOT_ENOUGH_MEMORY
        /// \def THEKOGANS_UTIL_OS_ERROR_CODE_TIMEOUT
        /// OS specific timeout error code.
        #define THEKOGANS_UTIL_OS_ERROR_CODE_TIMEOUT WAIT_TIMEOUT
        /// \def THEKOGANS_UTIL_OS_ERROR_CODE_EBADF
        /// OS specific file not open error code.
        #define THEKOGANS_UTIL_OS_ERROR_CODE_EBADF ERROR_INVALID_HANDLE
        /// \def THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW
        /// OS specific overflow error code.
        #define THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW ERROR_BUFFER_OVERFLOW
        /// \def THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW
        /// OS specific no file name error code.
        #define THEKOGANS_UTIL_OS_ERROR_CODE_ENOENT ERROR_BAD_PATHNAME
    #else // defined (TOOLCHAIN_OS_Windows)
        /// \def THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL
        /// OS specific invalid parameter error code.
        #define THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL EINVAL
        /// \def THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL
        /// OS specific out of memory error code.
        #define THEKOGANS_UTIL_OS_ERROR_CODE_ENOMEM ENOMEM
        /// \def THEKOGANS_UTIL_OS_ERROR_CODE_TIMEOUT
        /// OS specific timeout error code.
        #define THEKOGANS_UTIL_OS_ERROR_CODE_TIMEOUT ETIMEDOUT
        /// \def THEKOGANS_UTIL_OS_ERROR_CODE_EBADF
        /// OS specific file not open error code.
        #define THEKOGANS_UTIL_OS_ERROR_CODE_EBADF EBADF
        /// \def THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW
        /// OS specific overflow error code.
        #define THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW EOVERFLOW
        /// \def THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW
        /// OS specific no file name error code.
        #define THEKOGANS_UTIL_OS_ERROR_CODE_ENOENT ENOENT
    #endif // defined (TOOLCHAIN_OS_Windows)

        /// \brief
        /// Dump the exception to a stream.
        /// \param[in] stream Stream to dump to.
        /// \param[in] exception Exception to dump.
        /// \return stream.
        inline std::ostream & _LIB_THEKOGANS_UTIL_API operator << (
                std::ostream &stream,
                const Exception &exception) {
            stream << exception.Report ();
            return stream;
        }

        /// \def THEKOGANS_UTIL_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode, format, ...)
        /// Build an Exception from errorCode and message.
        #define THEKOGANS_UTIL_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode, format, ...)\
            thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, thekogans::util::FormatString (format, ##__VA_ARGS__))
        /// \def THEKOGANS_UTIL_EXCEPTION(errorCode, format, ...)
        /// Build an Exception from errorCode and message.
        #define THEKOGANS_UTIL_EXCEPTION(errorCode, format, ...)\
            THEKOGANS_UTIL_EXCEPTION_EX (__FILE__, __FUNCTION__, __LINE__,\
                __DATE__ " " __TIME__, errorCode, format, ##__VA_ARGS__)

        /// \def THEKOGANS_UTIL_THROW_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode, format, ...)
        /// Throw an Exception from errorCode and message.
        #define THEKOGANS_UTIL_THROW_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode, format, ...)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw THEKOGANS_UTIL_EXCEPTION_EX (file, function,\
                line, buildTime, errorCode, format, ##__VA_ARGS__)
        /// \def THEKOGANS_UTIL_THROW_EXCEPTION(errorCode, format, ...)
        /// Throw an Exception from errorCode and message.
        #define THEKOGANS_UTIL_THROW_EXCEPTION(errorCode, format, ...)\
            THEKOGANS_UTIL_THROW_EXCEPTION_EX (__FILE__, __FUNCTION__,\
                __LINE__, __DATE__ " " __TIME__, errorCode, format, ##__VA_ARGS__)

        /// \def THEKOGANS_UTIL_STRING_EXCEPTION_EX(
        ///          file, function, line, buildTime, format, ...)
        /// Build an Exception from message.
        #define THEKOGANS_UTIL_STRING_EXCEPTION_EX(\
                file, function, line, buildTime, format, ...)\
            THEKOGANS_UTIL_EXCEPTION_EX (file, function,\
                line, buildTime, (THEKOGANS_UTIL_ERROR_CODE)-1, format, ##__VA_ARGS__)
        /// \def THEKOGANS_UTIL_STRING_EXCEPTION(format, ...)
        /// Build an Exception from message.
        #define THEKOGANS_UTIL_STRING_EXCEPTION(format, ...)\
            THEKOGANS_UTIL_STRING_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__,\
                __DATE__ " " __TIME__, format, ##__VA_ARGS__)

        /// \def THEKOGANS_UTIL_THROW_STRING_EXCEPTION_EX(
        ///          file, function, line, buildTime, format, ...)
        /// Throw an Exception from message.
        #define THEKOGANS_UTIL_THROW_STRING_EXCEPTION_EX(\
                file, function, line, buildTime, format, ...)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw THEKOGANS_UTIL_STRING_EXCEPTION_EX (file,\
                function, line, buildTime, format, ##__VA_ARGS__)
        /// \def THEKOGANS_UTIL_THROW_STRING_EXCEPTION(format, ...)
        /// Throw an Exception from message.
        #define THEKOGANS_UTIL_THROW_STRING_EXCEPTION(format, ...)\
            THEKOGANS_UTIL_THROW_STRING_EXCEPTION_EX (__FILE__, __FUNCTION__,\
                __LINE__, __DATE__ " " __TIME__, format, ##__VA_ARGS__)

        /// \def THEKOGANS_UTIL_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Build an Exception from system errorCode.
        #define THEKOGANS_UTIL_ERROR_CODE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode)\
            thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, thekogans::util::Exception::FromErrorCode (errorCode))
        /// \def THEKOGANS_UTIL_ERROR_CODE_EXCEPTION(errorCode)
        /// Build an Exception from system errorCode.
        #define THEKOGANS_UTIL_ERROR_CODE_EXCEPTION(errorCode)\
            THEKOGANS_UTIL_ERROR_CODE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, errorCode)

    #if defined (TOOLCHAIN_OS_Windows)
        /// \def THEKOGANS_UTIL_HRESULT_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Build an Exception from HRESULT errorCode.
        #define THEKOGANS_UTIL_HRESULT_ERROR_CODE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode)\
            thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, thekogans::util::Exception::FromHRESULTErrorCode (errorCode))
        /// \def THEKOGANS_UTIL_HRESULT_ERROR_CODE_EXCEPTION(errorCode)
        /// Build an Exception from HRESULT errorCode.
        #define THEKOGANS_UTIL_HRESULT_ERROR_CODE_EXCEPTION(errorCode)\
            THEKOGANS_UTIL_HRESULT_ERROR_CODE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, errorCode)
    #elif defined (TOOLCHAIN_OS_OSX)
        /// \def THEKOGANS_UTIL_MACH_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Build an Exception from mach_return_t errorCode.
        #define THEKOGANS_UTIL_MACH_ERROR_CODE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode)\
            thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, mach_error_string (errorCode))
        /// \def THEKOGANS_UTIL_MACH_ERROR_CODE_EXCEPTION(errorCode)
        /// Build an Exception from mach_return_t errorCode.
        #define THEKOGANS_UTIL_MACH_ERROR_CODE_EXCEPTION(errorCode)\
            THEKOGANS_UTIL_MACH_ERROR_CODE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, errorCode)

        /// \def THEKOGANS_UTIL_SEC_OSSTATUS_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Build an Exception from Security framework OSStatus errorCode.
        #define THEKOGANS_UTIL_SEC_OSSTATUS_ERROR_CODE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode)\
            thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, thekogans::util::os::osx::DescriptionFromSecOSStatus (errorCode).c_str ())
        /// \def THEKOGANS_UTIL_SEC_OSSTATUS_ERROR_CODE_EXCEPTION(errorCode)
        /// Build an Exception from Security framework OSStatus errorCode.
        #define THEKOGANS_UTIL_SEC_OSSTATUS_ERROR_CODE_EXCEPTION(errorCode)\
            THEKOGANS_UTIL_SEC_OSSTATUS_ERROR_CODE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, errorCode)

        /// \def THEKOGANS_UTIL_OSSTATUS_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Build an Exception from OSStatus errorCode.
        #define THEKOGANS_UTIL_OSSTATUS_ERROR_CODE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode)\
            thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, thekogans::util::os::osx::DescriptionFromOSStatus (errorCode).c_str ())
        /// \def THEKOGANS_UTIL_OSSTATUS_ERROR_CODE_EXCEPTION(errorCode)
        /// Build an Exception from OSStatus errorCode.
        #define THEKOGANS_UTIL_OSSTATUS_ERROR_CODE_EXCEPTION(errorCode)\
            THEKOGANS_UTIL_OSSTATUS_ERROR_CODE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, errorCode)

        /// \def THEKOGANS_UTIL_CFERRORREF_EXCEPTION_EX(
        ///          file, function, line, buildTime, error)
        /// Build an Exception from CFErrorRef error.
        #define THEKOGANS_UTIL_CFERRORREF_EXCEPTION_EX(\
                file, function, line, buildTime, error)\
            thekogans::util::Exception (file, function, line, buildTime,\
                (thekogans::util::i32)CFErrorGetCode (error),\
                thekogans::util::os::osx::DescriptionFromCFErrorRef (error).c_str ())
        /// \def THEKOGANS_UTIL_CFERRORREF_EXCEPTION(error)
        /// Build an Exception from CFErrorRef error.
        #define THEKOGANS_UTIL_CFERRORREF_EXCEPTION(error)\
            THEKOGANS_UTIL_CFERRORREF_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, error)

        /// \def THEKOGANS_UTIL_IORETURN_EXCEPTION_EX(
        ///          file, function, line, buildTime, error)
        /// Build an Exception from IOReturn error.
        #define THEKOGANS_UTIL_IORETURN_EXCEPTION_EX(\
                file, function, line, buildTime, error)\
            thekogans::util::Exception (file, function, line, buildTime,\
                error, thekogans::util::os::osx::DescriptionFromIOReturn (error).c_str ())
        /// \def THEKOGANS_UTIL_IORETURN_EXCEPTION(error)
        /// Build an Exception from IOReturn errorCode.
        #define THEKOGANS_UTIL_IORETURN_EXCEPTION(error)\
            THEKOGANS_UTIL_IORETURN_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, error)

        /// \def THEKOGANS_UTIL_SC_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Build an Exception from SCError errorCode.
        #define THEKOGANS_UTIL_SC_ERROR_CODE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode)\
            thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, SCErrorString (errorCode))
        /// \def THEKOGANS_UTIL_SC_ERROR_CODE_EXCEPTION(errorCode)
        /// Build an Exception from SCError errorCode.
        #define THEKOGANS_UTIL_SC_ERROR_CODE_EXCEPTION(errorCode)\
            THEKOGANS_UTIL_SC_ERROR_CODE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, errorCode)
    #endif // defined (TOOLCHAIN_OS_Windows)

        /// \def THEKOGANS_UTIL_POSIX_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Build an Exception from POSIX errorCode.
        #define THEKOGANS_UTIL_POSIX_ERROR_CODE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode)\
            thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, thekogans::util::Exception::FromPOSIXErrorCode (errorCode))
        /// \def THEKOGANS_UTIL_POSIX_ERROR_CODE_EXCEPTION(errorCode)
        /// Build an Exception from POSIX errorCode.
        #define THEKOGANS_UTIL_POSIX_ERROR_CODE_EXCEPTION(errorCode)\
            THEKOGANS_UTIL_POSIX_ERROR_CODE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, errorCode)

        /// \def THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Throw an Exception from system errorCode.
        #define THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, thekogans::util::Exception::FromErrorCode (errorCode))
        /// \def THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION(errorCode)
        /// Throw an Exception from system errorCode.
        #define THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION(errorCode)\
            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, errorCode)

        /// \def THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode, format, ...)
        /// Throw an Exception from system errorCode and a user message.
        #define THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode, format, ...)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, thekogans::util::Exception::FromErrorCodeAndMessage (\
                    errorCode, format, ##__VA_ARGS__))
        /// \def THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION(
        ///          errorCode, format, ...)
        /// Throw an Exception from system errorCode and a user message.
        #define THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION(\
                errorCode, format, ...)\
            THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__,\
                errorCode, format, ##__VA_ARGS__)

    #if defined (TOOLCHAIN_OS_Windows)
        /// \def THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Throw an Exception from HRESULT errorCode.
        #define THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, thekogans::util::Exception::FromHRESULTErrorCode (errorCode))
        /// \def THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION(errorCode)
        /// Throw an Exception from HRESULT errorCode.
        #define THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION(errorCode)\
            THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, errorCode)

        /// \def THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Throw an Exception from HRESULT errorCode.
        #define THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode, format, ...)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, thekogans::util::Exception::FromHRESULTErrorCodeAndMessage (\
                    errorCode, format, ##__VA_ARGS__))
        /// \def THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION(errorCode)
        /// Throw an Exception from HRESULT errorCode.
        #define THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_AND_MESSAGE_EXCEPTION(\
                errorCode, format, ...)\
            THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__,\
                errorCode, format, ##__VA_ARGS__)
    #elif defined (TOOLCHAIN_OS_OSX)
        /// \def THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Throw an Exception from mach_return_t errorCode.
        #define THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, mach_error_string (errorCode))
        /// \def THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_EXCEPTION(errorCode)
        /// Throw an Exception from mach_return_t errorCode.
        #define THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_EXCEPTION(errorCode)\
            THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, errorCode)

        /// \def THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Throw an Exception from mach_return_t errorCode.
        #define THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode, format, ...)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime, errorCode,\
                thekogans::util::FormatString ("%s%s",\
                    mach_error_string (errorCode),\
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str ()))
        /// \def THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_AND_MESSAGE_EXCEPTION(
        ///          errorCode, format, ...)
        /// Throw an Exception from mach_return_t errorCode.
        #define THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_AND_MESSAGE_EXCEPTION(\
                errorCode, format, ...)\
            THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__,\
                errorCode, format, ##__VA_ARGS__)

        /// \def THEKOGANS_UTIL_THROW_SEC_OSSTATUS_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Throw an Exception from Security framework OSStatus errorCode.
        #define THEKOGANS_UTIL_THROW_SEC_OSSTATUS_ERROR_CODE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, thekogans::util::os::osx::DescriptionFromSecOSStatus (errorCode).c_str ())
        /// \def THEKOGANS_UTIL_THROW_SEC_OSSTATUS_ERROR_CODE_EXCEPTION(errorCode)
        /// Throw an Exception from Security framework OSStatus errorCode.
        #define THEKOGANS_UTIL_THROW_SEC_OSSTATUS_ERROR_CODE_EXCEPTION(errorCode)\
            THEKOGANS_UTIL_THROW_SEC_OSSTATUS_ERROR_CODE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, errorCode)

        /// \def THEKOGANS_UTIL_THROW_SEC_OSSTATUS_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Throw an Exception from Security framework OSStatus errorCode.
        #define THEKOGANS_UTIL_THROW_SEC_OSSTATUS_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode, format, ...)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime, errorCode,\
                thekogans::util::FormatString ("%s%s",\
                    thekogans::util::os::osx::DescriptionFromSecOSStatus (errorCode).c_str (), \
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str ()))
        /// \def THEKOGANS_UTIL_THROW_SEC_OSSTATUS_ERROR_CODE_AND_MESSAGE_EXCEPTION(
        ///          errorCode, format, ...)
        /// Throw an Exception from Security framework OSStatus errorCode.
        #define THEKOGANS_UTIL_THROW_SEC_OSSTATUS_ERROR_CODE_AND_MESSAGE_EXCEPTION(\
                errorCode, format, ...)\
            THEKOGANS_UTIL_THROW_SEC_OSSTATUS_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__,\
                errorCode, format, ##__VA_ARGS__)

        /// \def THEKOGANS_UTIL_THROW_OSSTATUS_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Throw an Exception from OSStatus errorCode.
        #define THEKOGANS_UTIL_THROW_OSSTATUS_ERROR_CODE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, thekogans::util::DescriptionFromOSStatus (errorCode).c_str ())
        /// \def THEKOGANS_UTIL_THROW_OSSTATUS_ERROR_CODE_EXCEPTION(errorCode)
        /// Throw an Exception from OSStatus errorCode.
        #define THEKOGANS_UTIL_THROW_OSSTATUS_ERROR_CODE_EXCEPTION(errorCode)\
            THEKOGANS_UTIL_THROW_OSSTATUS_ERROR_CODE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, errorCode)

        /// \def THEKOGANS_UTIL_THROW_OSSTATUS_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Throw an Exception from OSStatus errorCode.
        #define THEKOGANS_UTIL_THROW_OSSTATUS_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode, format, ...)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime, errorCode,\
                thekogans::util::FormatString ("%s%s",\
                    thekogans::util::os::osx::DescriptionFromOSStatus (errorCode).c_str (), \
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str ()))
        /// \def THEKOGANS_UTIL_THROW_OSSTATUS_ERROR_CODE_AND_MESSAGE_EXCEPTION(
        ///          errorCode, format, ...)
        /// Throw an Exception from OSStatus errorCode.
        #define THEKOGANS_UTIL_THROW_OSSTATUS_ERROR_CODE_AND_MESSAGE_EXCEPTION(\
                errorCode, format, ...)\
            THEKOGANS_UTIL_THROW_OSSTATUS_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__,\
                errorCode, format, ##__VA_ARGS__)

        /// \def THEKOGANS_UTIL_THROW_CFERRORREF_EXCEPTION_EX(
        ///          file, function, line, buildTime, error)
        /// Throw an Exception from CFErrorRef error.
        #define THEKOGANS_UTIL_THROW_CFERRORREF_EXCEPTION_EX(\
                file, function, line, buildTime, error)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime,\
                (thekogans::util::i32)CFErrorGetCode (error),\
                thekogans::util::os::osx::DescriptionFromCFErrorRef (error).c_str ())
        /// \def THEKOGANS_UTIL_THROW_CFERRORREF_EXCEPTION(error)
        /// Throw an Exception from CFErrorRef error.
        #define THEKOGANS_UTIL_THROW_CFERRORREF_EXCEPTION(error)\
            THEKOGANS_UTIL_THROW_CFERRORREF_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, error)

        /// \def THEKOGANS_UTIL_THROW_CFERRORREF_EXCEPTION_EX(
        ///          file, function, line, buildTime, error)
        /// Throw an Exception from CFErrorRef error.
        #define THEKOGANS_UTIL_THROW_CFERRORREF_AND_MESSAGE_EXCEPTION_EX(\
                file, function, line, buildTime, error, format, ...)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime, CFErrorGetCode (error),\
                thekogans::util::FormatString ("%s%s",\
                    thekogans::util::os::osx::DescriptionFromCFErrorRef (error).c_str (), \
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str ()))
        /// \def THEKOGANS_UTIL_THROW_CFERRORREF_AND_MESSAGE_EXCEPTION(
        ///          errorCode, format, ...)
        /// Throw an Exception from CFErrorRef error.
        #define THEKOGANS_UTIL_THROW_CFERRORREF_AND_MESSAGE_EXCEPTION(\
                error, format, ...)\
            THEKOGANS_UTIL_THROW_CFERRORREF_AND_MESSAGE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__,\
                error, format, ##__VA_ARGS__)

        /// \def THEKOGANS_UTIL_THROW_IORETURN_EXCEPTION_EX(
        ///          file, function, line, buildTime, error)
        /// Throw an Exception from IOReturn error.
        #define THEKOGANS_UTIL_THROW_IORETURN_EXCEPTION_EX(\
                file, function, line, buildTime, error)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime,\
                error, thekogans::util::os::osx::DescriptionFromIOReturn (error).c_str ())
        /// \def THEKOGANS_UTIL_THROW_IORETURN_EXCEPTION(error)
        /// Throw an Exception from IOReturn error.
        #define THEKOGANS_UTIL_THROW_IORETURN_EXCEPTION(error)\
            THEKOGANS_UTIL_THROW_IORETURN_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, error)

        /// \def THEKOGANS_UTIL_THROW_IORETURN_EXCEPTION_EX(
        ///          file, function, line, buildTime, error)
        /// Throw an Exception from IOReturn error.
        #define THEKOGANS_UTIL_THROW_IORETURN_AND_MESSAGE_EXCEPTION_EX(\
                file, function, line, buildTime, error, format, ...)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime, error,\
                thekogans::util::FormatString ("%s%s",\
                    thekogans::util::os::osx::DescriptionFromIOReturn (error).c_str (), \
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str ()))
        /// \def THEKOGANS_UTIL_THROW_IORETURN_AND_MESSAGE_EXCEPTION(
        ///          errorCode, format, ...)
        /// Throw an Exception from Ioreturn error.
        #define THEKOGANS_UTIL_THROW_IORETURN_AND_MESSAGE_EXCEPTION(\
                error, format, ...)\
            THEKOGANS_UTIL_THROW_IORETURN_AND_MESSAGE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__,\
                error, format, ##__VA_ARGS__)

        /// \def THEKOGANS_UTIL_THROW_SC_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Throw an Exception from SCError errorCode.
        #define THEKOGANS_UTIL_THROW_SC_ERROR_CODE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, SCErrorString (errorCode))
        /// \def THEKOGANS_UTIL_THROW_SC_ERROR_CODE_EXCEPTION(errorCode)
        /// Throw an Exception from SCError errorCode.
        #define THEKOGANS_UTIL_THROW_SC_ERROR_CODE_EXCEPTION(errorCode)\
            THEKOGANS_UTIL_THROW_SC_ERROR_CODE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, errorCode)

        /// \def THEKOGANS_UTIL_THROW_SC_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Throw an Exception from SCError errorCode.
        #define THEKOGANS_UTIL_THROW_SC_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode, format, ...)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime, errorCode,\
                thekogans::util::FormatString ("%s%s",\
                    SCErrorString (errorCode),\
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str ()))
        /// \def THEKOGANS_UTIL_THROW_SC_ERROR_CODE_AND_MESSAGE_EXCEPTION(
        ///          errorCode, format, ...)
        /// Throw an Exception from SCError errorCode.
        #define THEKOGANS_UTIL_THROW_SC_ERROR_CODE_AND_MESSAGE_EXCEPTION(\
                errorCode, format, ...)\
            THEKOGANS_UTIL_THROW_SC_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__,\
                errorCode, format, ##__VA_ARGS__)
    #endif // defined (TOOLCHAIN_OS_Windows)

        /// \def THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode)
        /// Throw an Exception from POSIX errorCode.
        #define THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, thekogans::util::Exception::FromPOSIXErrorCode (errorCode))
        /// \def THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION(errorCode)
        /// Throw an Exception from POSIX errorCode.
        #define THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION(errorCode)\
            THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, errorCode)

        /// \def THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX(
        ///          file, function, line, buildTime, errorCode, format, ...)
        /// Throw an Exception from POSIX errorCode.
        #define THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX(\
                file, function, line, buildTime, errorCode, format, ...)\
            THEKOGANS_UTIL_DEBUG_BREAK\
            throw thekogans::util::Exception (file, function, line, buildTime,\
                errorCode, thekogans::util::Exception::FromPOSIXErrorCodeAndMessage (\
                    errorCode, format, ##__VA_ARGS__))
        /// \def THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_AND_MESSAGE_EXCEPTION(errorCode)
        /// Throw an Exception from POSIX errorCode.
        #define THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_AND_MESSAGE_EXCEPTION(\
                errorCode, format, ...)\
            THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_AND_MESSAGE_EXCEPTION_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__,\
                errorCode, format, ##__VA_ARGS__)

        /// \def THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION_EX(exception)
        /// Use this macro to note a location in the exception traceback.
        #define THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION_EX(\
                exception, file, function, line, buildTime)\
            exception.NoteLocation (file, function, line, buildTime)
        /// \def THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION(exception)
        /// Use this macro to note the current location in the exception traceback.
        #define THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION(exception)\
            THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION_EX (exception,\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__)

        /// \def THEKOGANS_UTIL_RETHROW_EXCEPTION(exception)
        /// Note location and rethrow the exception.
        #define THEKOGANS_UTIL_RETHROW_EXCEPTION(exception)\
            (THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION (exception), throw)

        /// \def THEKOGANS_UTIL_TRY
        /// try
        #define THEKOGANS_UTIL_TRY try
        /// \def THEKOGANS_UTIL_CATCH(type)
        /// Catch exception of type.
        #define THEKOGANS_UTIL_CATCH(type) catch (const type &exception)
        /// \def THEKOGANS_UTIL_CATCH_ANY
        /// Catch any exception.
        #define THEKOGANS_UTIL_CATCH_ANY catch (...)
        /// \def THEKOGANS_UTIL_CATCH_AND_RETHROW
        /// Catch and rethrow.
        #define THEKOGANS_UTIL_CATCH_AND_RETHROW\
            THEKOGANS_UTIL_CATCH (thekogans::util::Exception) {\
                if (thekogans::util::Exception::FilterException (exception)) {\
                    THEKOGANS_UTIL_DEBUG_BREAK\
                    THEKOGANS_UTIL_RETHROW_EXCEPTION (exception);\
                }\
            }\
            THEKOGANS_UTIL_CATCH (std::exception) {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                throw;\
            }\
            THEKOGANS_UTIL_CATCH_ANY {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                throw;\
            }

        /// \def THEKOGANS_UTIL_CATCH_AND_LOG
        /// Catch and log error.
        #define THEKOGANS_UTIL_CATCH_AND_LOG\
            THEKOGANS_UTIL_CATCH (thekogans::util::Exception) {\
                if (thekogans::util::Exception::FilterException (exception)) {\
                    THEKOGANS_UTIL_DEBUG_BREAK\
                    THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION (exception);\
                    THEKOGANS_UTIL_LOG_ERROR ("%s\n", exception.Report ().c_str ());\
                }\
            }\
            THEKOGANS_UTIL_CATCH (std::exception) {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_ERROR ("%s\n", exception.what ());\
            }\
            THEKOGANS_UTIL_CATCH_ANY {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_ERROR ("%s\n", "Caught unknown exception!");\
            }
        /// \def THEKOGANS_UTIL_CATCH_AND_LOG_WITH_MESSAGE(format, ...)
        /// Catch and log error with message.
        #define THEKOGANS_UTIL_CATCH_AND_LOG_WITH_MESSAGE(format, ...)\
            THEKOGANS_UTIL_CATCH (thekogans::util::Exception) {\
                if (thekogans::util::Exception::FilterException (exception)) {\
                    THEKOGANS_UTIL_DEBUG_BREAK\
                    THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION (exception);\
                    THEKOGANS_UTIL_LOG_ERROR ("%s\n%s\n",\
                        thekogans::util::FormatString (format, ##__VA_ARGS__).c_str (),\
                        exception.Report ().c_str ());\
                }\
            }\
            THEKOGANS_UTIL_CATCH (std::exception) {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_ERROR ("%s\n%s\n",\
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str (),\
                    exception.what ());\
            }\
            THEKOGANS_UTIL_CATCH_ANY {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_ERROR ("%s\n%s\n",\
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str (),\
                    "Caught unknown exception!");\
            }
        /// \def THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM(subsystem)
        /// Catch and log a subsystem error.
        #define THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM(subsystem)\
            THEKOGANS_UTIL_CATCH (thekogans::util::Exception) {\
                if (thekogans::util::Exception::FilterException (exception)) {\
                    THEKOGANS_UTIL_DEBUG_BREAK\
                    THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION (exception);\
                    THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (\
                        subsystem, "%s\n", exception.Report ().c_str ());\
                }\
            }\
            THEKOGANS_UTIL_CATCH (std::exception) {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (\
                    subsystem, "%s\n", exception.what ());\
            }\
            THEKOGANS_UTIL_CATCH_ANY {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (\
                    subsystem, "%s\n", "Caught unknown exception!");\
            }
        /// \def THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM_WITH_MESSAGE(subsystem, format, ...)
        /// Catch and log a subsystem error with message.
        #define THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM_WITH_MESSAGE(subsystem, format, ...)\
            THEKOGANS_UTIL_CATCH (thekogans::util::Exception) {\
                if (thekogans::util::Exception::FilterException (exception)) {\
                    THEKOGANS_UTIL_DEBUG_BREAK\
                    THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION (exception);\
                    THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (subsystem, "%s\n%s\n",\
                        thekogans::util::FormatString (format, ##__VA_ARGS__).c_str (),\
                        exception.Report ().c_str ());\
                }\
            }\
            THEKOGANS_UTIL_CATCH (std::exception) {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (subsystem, "%s\n%s\n",\
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str (),\
                    exception.what ());\
            }\
            THEKOGANS_UTIL_CATCH_ANY {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (subsystem, "%s\n%s\n",\
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str (),\
                    "Caught unknown exception!");\
            }

        /// \def THEKOGANS_UTIL_CATCH_LOG_AND_RETHROW
        /// Catch, log error, and rethrow the exception.
        #define THEKOGANS_UTIL_CATCH_LOG_AND_RETHROW\
            THEKOGANS_UTIL_CATCH (thekogans::util::Exception) {\
                if (thekogans::util::Exception::FilterException (exception)) {\
                    THEKOGANS_UTIL_DEBUG_BREAK\
                    THEKOGANS_UTIL_LOG_ERROR ("%s\n", exception.Report ().c_str ());\
                    THEKOGANS_UTIL_RETHROW_EXCEPTION (exception);\
                }\
            }\
            THEKOGANS_UTIL_CATCH (std::exception) {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_ERROR ("%s\n", exception.what ());\
                throw;\
            }\
            THEKOGANS_UTIL_CATCH_ANY {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_ERROR ("%s\n", "Caught unknown exception!");\
                throw;\
            }
        /// \def THEKOGANS_UTIL_CATCH_LOG_WITH_MESSAGE_AND_RETHROW(format, ...)
        /// Catch, log error with message, and rethrow the exception.
        #define THEKOGANS_UTIL_CATCH_LOG_WITH_MESSAGE_AND_RETHROW(format, ...)\
            THEKOGANS_UTIL_CATCH (thekogans::util::Exception) {\
                if (thekogans::util::Exception::FilterException (exception)) {\
                    THEKOGANS_UTIL_DEBUG_BREAK\
                    THEKOGANS_UTIL_LOG_ERROR ("%s\n%s\n",\
                        thekogans::util::FormatString (format, ##__VA_ARGS__).c_str (),\
                        exception.Report ().c_str ());\
                    THEKOGANS_UTIL_RETHROW_EXCEPTION (exception);\
                }\
            }\
            THEKOGANS_UTIL_CATCH (std::exception) {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_ERROR ("%s\n%s\n",\
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str (),\
                    exception.what ());\
                throw;\
            }\
            THEKOGANS_UTIL_CATCH_ANY {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_ERROR ("%s\n%s\n",\
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str (),\
                    "Caught unknown exception!");\
                throw;\
            }
        /// \def THEKOGANS_UTIL_CATCH_LOG_SUBSYSTEM_AND_RETHROW(subsystem)
        /// Catch, log subsystem error, and rethrow the exception.
        #define THEKOGANS_UTIL_CATCH_LOG_SUBSYSTEM_AND_RETHROW(subsystem)\
            THEKOGANS_UTIL_CATCH (thekogans::util::Exception) {\
                if (thekogans::util::Exception::FilterException (exception)) {\
                    THEKOGANS_UTIL_DEBUG_BREAK\
                    THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (\
                        subsystem, "%s\n", exception.Report ().c_str ());\
                    THEKOGANS_UTIL_RETHROW_EXCEPTION (exception);\
                }\
            }\
            THEKOGANS_UTIL_CATCH (std::exception) {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (\
                    subsystem, "%s\n", exception.what ());\
                throw;\
            }\
            THEKOGANS_UTIL_CATCH_ANY {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (\
                    subsystem, "%s\n", "Caught unknown exception!");\
                throw;\
            }
        /// \def THEKOGANS_UTIL_CATCH_LOG_SUBSYSTEM_WITH_MESSAGE_AND_RETHROW(subsystem, format, ...)
        /// Catch, log subsystem error with message, and rethrow the exception.
        #define THEKOGANS_UTIL_CATCH_LOG_SUBSYSTEM_WITH_MESSAGE_AND_RETHROW(subsystem, format, ...)\
            THEKOGANS_UTIL_CATCH (thekogans::util::Exception) {\
                if (thekogans::util::Exception::FilterException (exception)) {\
                    THEKOGANS_UTIL_DEBUG_BREAK\
                    THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (subsystem, "%s\n%s\n",\
                        thekogans::util::FormatString (format, ##__VA_ARGS__).c_str (),\
                        exception.Report ().c_str ());\
                    THEKOGANS_UTIL_RETHROW_EXCEPTION (exception);\
                }\
            }\
            THEKOGANS_UTIL_CATCH (std::exception) {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (subsystem, "%s\n%s\n",\
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str (),\
                    exception.what ());\
                throw;\
            }\
            THEKOGANS_UTIL_CATCH_ANY {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (subsystem, "%s\n%s\n",\
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str (),\
                    "Caught unknown exception!");\
                throw;\
            }

        /// \def THEKOGANS_UTIL_LOG_EXCEPTION
        /// Log error.
        #define THEKOGANS_UTIL_LOG_EXCEPTION(exception)\
            if (thekogans::util::Exception::FilterException (exception)) {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION (exception);\
                THEKOGANS_UTIL_LOG_ERROR ("%s\n", exception.Report ().c_str ());\
            }
        /// \def THEKOGANS_UTIL_LOG_EXCEPTION_WITH_MESSAGE(exception, format, ...)
        /// Log error with message.
        #define THEKOGANS_UTIL_LOG_EXCEPTION_WITH_MESSAGE(exception, format, ...)\
            if (thekogans::util::Exception::FilterException (exception)) {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION (exception);\
                THEKOGANS_UTIL_LOG_ERROR ("%s\n%s\n",\
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str (),\
                    exception.Report ().c_str ());\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_EXCEPTION(subsystem, exception)
        /// Log an subsystem error.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_EXCEPTION(subsystem, exception)\
            if (thekogans::util::Exception::FilterException (exception)) {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION (exception);\
                THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (\
                    subsystem, "%s\n", exception.Report ().c_str ());\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_EXCEPTION_WITH_MESSAGE(subsystem, exception, format, ...)
        /// Log an subsystem error with message.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_EXCEPTION_WITH_MESSAGE(subsystem, exception, format, ...)\
            if (thekogans::util::Exception::FilterException (exception)) {\
                THEKOGANS_UTIL_DEBUG_BREAK\
                THEKOGANS_UTIL_EXCEPTION_NOTE_LOCATION (exception);\
                THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (subsystem, "%s\n%s\n",\
                    thekogans::util::FormatString (format, ##__VA_ARGS__).c_str (),\
                    exception.Report ().c_str ());\
            }

        /// \brief
        /// Write the given Exception::Location to the given serializer.
        /// \param[in] serializer Where to write the given exception.
        /// \param[in] location Exception::Location to write.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
            Serializer &serializer,
            const Exception::Location &location);

        /// \brief
        /// Read an Exception::Location from the given serializer.
        /// \param[in] serializer Where to read the exception from.
        /// \param[out] location Exception::Location to read.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
            Serializer &serializer,
            Exception::Location &location);

        /// \brief
        /// Write the given exception to the given serializer.
        /// \param[in] serializer Where to write the given exception.
        /// \param[in] exception Exception to write.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
            Serializer &serializer,
            const Exception &exception);

        /// \brief
        /// Read an Exception from the given serializer.
        /// \param[in] serializer Where to read the exception from.
        /// \param[out] exception Exception to read.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
            Serializer &serializer,
            Exception &exception);

        /// \brief
        /// Write the given Exception::Location to the given node.
        /// \param[in] node Where to write the given exception.
        /// \param[in] location Exception::Location to write.
        /// \return node.
        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
            pugi::xml_node &node,
            const Exception::Location &location);

        /// \brief
        /// Read an Exception::Location from the given node.
        /// \param[in] node Where to read the exception from.
        /// \param[out] location Exception::Location to read.
        /// \return node.
        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
            pugi::xml_node &node,
            Exception::Location &location);

        /// \brief
        /// Write the given exception to the given node.
        /// The node syntax looks like this:
        /// <Exception ErrorCode = ""
        ///            Message = "">
        ///     <Location File = ""
        ///               Function = ""
        ///               Line = ""
        ///               BuildTime = ""/>
        ///     ...
        /// </Exception>
        /// \param[in] node Where to write the given exception.
        /// \param[in] exception Exception to write.
        /// \return node.
        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
            pugi::xml_node &node,
            const Exception &exception);

        /// \brief
        /// Read an Exception from the given node.
        /// The node syntax looks like this:
        /// <Exception ErrorCode = ""
        ///            Message = "">
        ///     <Location File = ""
        ///               Function = ""
        ///               Line = ""
        ///               BuildTime = ""/>
        ///     ...
        /// </Exception>
        /// \param[in] node Where to read the exception from.
        /// \param[out] exception Exception to read.
        /// \return node.
        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
            pugi::xml_node &node,
            Exception &exception);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Exception_h)
