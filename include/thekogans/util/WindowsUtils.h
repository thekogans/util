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

#if !defined (__thekogans_util_WindowsUtils_h)
#define __thekogans_util_WindowsUtils_h

#if !defined (_WINDOWS_)
    #if !defined (WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif // !defined (WIN32_LEAN_AND_MEAN)
    #if !defined (NOMINMAX)
        #define NOMINMAX
    #endif // !defined (NOMINMAX)
    #include <windows.h>
#endif // !defined (_WINDOWS_)
#if defined (_MSC_VER)
    #include <direct.h>
    #include <cstdlib>
    #include <cstring>
    #include <cstdio>

    #define atoll(str) _strtoui64 (str, 0, 10)
    #define strtoll _strtoi64
    #define strtoull _strtoui64
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
    #define snprintf sprintf_s
    #define vsnprintf vsprintf_s
    #define unlink _unlink
    #define rmdir _rmdir
    #define sscanf sscanf_s
#endif // defined (_MSC_VER)
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"

/// \brief
/// Create both ends of an anonymous pipe. Useful
/// if you're planning on using it for overlapped io.
/// Adapted from: http://www.davehart.net/remote/PipeEx.c
/// \param[out] fildes fildes[0] = read end of the pipe,
/// fildes[1] = write end of the pipe.
/// \return 0 = success, -1 = errno contains the error.
_LIB_THEKOGANS_UTIL_DECL int _LIB_THEKOGANS_UTIL_API pipe (
    THEKOGANS_UTIL_HANDLE fildes[2]);

namespace thekogans {
    namespace util {

        /// \brief
        /// Convert a given i64 value to Windows FILETIME.
        /// \param[in] value i64 value to convert.
        /// \return Converted FILETIME.
        _LIB_THEKOGANS_UTIL_DECL FILETIME _LIB_THEKOGANS_UTIL_API i64ToFILETIME (i64 value);
        /// \brief
        /// Convert a given Windows FILETIME value to i64.
        /// \param[in] value FILETIME value to convert.
        /// \return Converted i64.
        _LIB_THEKOGANS_UTIL_DECL i64 _LIB_THEKOGANS_UTIL_API FILETIMEToi64 (const FILETIME &value);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_WindowsUtils_h)

#endif // defined (TOOLCHAIN_OS_Windows)
