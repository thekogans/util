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

#if !defined (__thekogans_util_internal_h)

#if defined (TOOLCHAIN_OS_Windows)
    #if defined (_MSC_VER)
        #include <direct.h>
        #include <cstdlib>
        #include <cstring>
        #include <cstdio>

        #define atoll(str) _strtoui64 (str, 0, 10)
        #define strncpy strncpy_s
        #define strcpy strcpy_s
        #define strtoll _strtoi64
        #define strtoull _strtoui64
        #define strtof strtod
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

    /// \brief
    /// Create both ends of an anonymous pipe. Useful
    /// if you're planning on using it for overlapped io.
    /// Adapted from: http://www.davehart.net/remote/PipeEx.c
    /// \param[out] fildes fildes[0] = read end of the pipe,
    /// fildes[1] = write end of the pipe.
    /// \return 0 = success, -1 = errno contains the error.
    _LIB_THEKOGANS_UTIL_DECL int _LIB_THEKOGANS_UTIL_API pipe (
        THEKOGANS_UTIL_HANDLE fildes[2]);
#else // defined (TOOLCHAIN_OS_Windows)
    #include <cstdarg>
    #include <cstdlib>

    /// \brief
    /// Return the formatted string length.
    /// \param[in] format printf style format string.
    /// \param[in] argptr Pointer to variable argument list.
    /// \return Formatted string length.
    int _vscprintf (
        const char *format,
        va_list argptr);
    /// \brief
    /// Clear data.
    /// \param[in] data Start of block to clear.
    /// \param[in] length Length of data block.
    void SecureZeroMemory (
        void *data,
        size_t length);
#if defined (TOOLCHAIN_OS_Linux)
    #if !defined (_LARGEFILE64_SOURCE)
        #define _LARGEFILE64_SOURCE 1
    #endif // !defined (_LARGEFILE64_SOURCE)
    #if !defined (_FILE_OFFSET_BITS)
        #define _FILE_OFFSET_BITS 64
    #endif // !defined (_FILE_OFFSET_BITS)
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>


    #if defined (TOOLCHAIN_ARCH_i386)
        #define STAT_STRUCT struct stat
        #define STAT_FUNC stat
        #define LSTAT_FUNC lstat
        #define FSTAT_FUNC fstat
        #define LSEEK_FUNC lseek
        #define FTRUNCATE_FUNC ftruncate
    #elif defined (TOOLCHAIN_ARCH_x86_64)
        #define STAT_STRUCT struct stat64
        #define STAT_FUNC stat64
        #define LSTAT_FUNC lstat64
        #define FSTAT_FUNC fstat64
        #define LSEEK_FUNC lseek64
        #define FTRUNCATE_FUNC ftruncate64
    #endif // defined (TOOLCHAIN_ARCH_i386)
#elif defined (TOOLCHAIN_OS_OSX)
    #include <sys/stat.h>
    #include <pthread.h>

    int pthread_timedjoin_np (
        pthread_t thread,
        void **result,
        timespec *timeSpec);

    #define STAT_STRUCT struct stat
    #define STAT_FUNC stat
    #define LSTAT_FUNC lstat
    #define FSTAT_FUNC fstat
    #define LSEEK_FUNC lseek
    #define FTRUNCATE_FUNC ftruncate

    #if defined (TOOLCHAIN_ARCH_i386)
        #define keventStruct kevent
        #define keventFunc(kq, changelist, nchanges, eventlist, nevents, timeout)\
            kevent (kq, changelist, nchanges, eventlist, nevents, timeout)
        #define keventSet(kev, ident, filter, flags, fflags, data, udata)\
            EV_SET (kev, ident, filter, flags, fflags, data, (void *)udata)
    #elif defined (TOOLCHAIN_ARCH_x86_64)
        #define keventStruct kevent64_s
        #define keventFunc(kq, changelist, nchanges, eventlist, nevents, timeout)\
            kevent64 (kq, changelist, nchanges, eventlist, nevents, 0, timeout)
        #define keventSet(kev, ident, filter, flags, fflags, data, udata)\
            EV_SET64 (kev, ident, filter, flags, fflags, data, (uint64_t)udata, 0, 0)
    #else // defined (TOOLCHAIN_ARCH_i386)
        #error "Unknown architecture!"
    #endif // defined (TOOLCHAIN_ARCH_i386)
#endif // defined (TOOLCHAIN_OS_Linux)
#endif // defined (TOOLCHAIN_OS_Windows)

#endif // !defined (__thekogans_util_internal_h)
