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

#if !defined (__thekogans_util_Types_h)
#define __thekogans_util_Types_h

#if defined (TOOLCHAIN_OS_Windows)
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
        #include <windows.h>
    #endif // !defined (_WINDOWS_)
    #include <cstdio>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <cstddef>
#include <cstdlib>
#include <cstdarg>
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// One of thekogans util strengths is it's support for binary
        /// serialization. To achieve this in platform independent manner,
        /// we need well defined sizes for basic types. This type system,
        /// together with the static_assert below makes sure that our
        /// assumptions are rooted in reality. If you ever get compiler
        /// errors that lead you here, chances are that thekogans util
        /// (at least it's serialization machinery) is not suited for
        /// your platform.

        /// \brief
        /// Signed 8 bit type.
        typedef signed char i8;
        /// \brief
        /// Unsigned 8 bit type.
        typedef unsigned char ui8;
        /// \brief
        /// Signed 16 bit type.
        typedef signed short i16;
        /// \brief
        /// Unsigned 16 bit type.
        typedef unsigned short ui16;
        /// \brief
        /// Signed 32 bit type.
        typedef signed int i32;
        /// \brief
        /// Unsigned 32 bit type.
        typedef unsigned int ui32;
    #if defined (TOOLCHAIN_OS_Windows)
        /// \brief
        /// Signed 64 bit type.
        typedef signed __int64 i64;
        /// \brief
        /// Unsigned 64 bit type.
        typedef unsigned __int64 ui64;
    #else // defined (TOOLCHAIN_OS_Windows)
        /// \brief
        /// Signed 64 bit type.
        typedef signed long long i64;
        /// \brief
        /// Unsigned 64 bit type.
        typedef unsigned long long ui64;
    #endif // defined (TOOLCHAIN_OS_Windows)
        /// \brief
        /// 32 bit float type.
        typedef float f32;
        /// \brief
        /// 64 bit float type.
        typedef double f64;
        /// \brief
        /// Architecture dependent natural word type.
    #if defined (TOOLCHAIN_ARCH_i386)
        typedef ui32 MachineWord;
    #elif defined (TOOLCHAIN_ARCH_x86_64)
        typedef ui64 MachineWord;
    #else // defined (TOOLCHAIN_ARCH_x86_64)
        #error "Unknown MachineWord."
    #endif // defined (TOOLCHAIN_ARCH_i386)

        /// \brief
        /// Validate assumptions about integral types sizes.
        static_assert (
            sizeof (i8) == 1 && sizeof (ui8) == 1 &&
            sizeof (i16) == 2 && sizeof (ui16) == 2 &&
            sizeof (i32) == 4 && sizeof (ui32) == 4 &&
            sizeof (i64) == 8 && sizeof (ui64) == 8 &&
            sizeof (f32) == 4 && sizeof (f64) == 8,
            "Invalid assumption about integral types sizes.");

        /// \enum
        /// OS constants.
        enum OS {
            /// \brief
            /// Windows.
            Windows,
            /// \brief
            /// Linux.
            Linux,
            /// \brief
            /// OS X.
            OSX,
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Host OS is Windows.
            HostOS = Windows
        #elif defined (TOOLCHAIN_OS_Linux)
            /// \brief
            /// Host OS is Linux.
            HostOS = Linux
        #elif defined (TOOLCHAIN_OS_OSX)
            /// \brief
            /// Host OS is OS X.
            HostOS = OSX
        #else // defined (TOOLCHAIN_ENDIAN_Big)
            #error "Unable to determine host OS."
        #endif // defined (TOOLCHAIN_ENDIAN_Little)
        };

        /// \enum
        /// Arch constants.
        enum Arch {
            /// \brief
            /// i386.
            i386,
            /// \brief
            /// x86_86.
            x86_86,
        #if defined (TOOLCHAIN_ARCH_i386)
            /// \brief
            /// Host Arch is i386.
            HostArch = i386
        #elif defined (TOOLCHAIN_ARCH_x86_64)
            /// \brief
            /// Host Arch is x86_86.
            HostArch = x86_86
        #else // defined (TOOLCHAIN_ENDIAN_Big)
            #error "Unable to determine host Arch."
        #endif // defined (TOOLCHAIN_ENDIAN_Little)
        };

        /// \brief
        /// Error code type.
        #define THEKOGANS_UTIL_POSIX_ERROR_CODE thekogans::util::i32
        /// \brief
        /// Error code.
        #define THEKOGANS_UTIL_POSIX_OS_ERROR_CODE errno
        /// \brief
        /// Handle type.
        #define THEKOGANS_UTIL_POSIX_HANDLE thekogans::util::i32
        /// \brief
        /// Invalid handle value.
        #define THEKOGANS_UTIL_POSIX_INVALID_HANDLE_VALUE -1
    #if defined (TOOLCHAIN_OS_Windows)
        /// \brief
        /// Error code type.
        /// NOTE: We don't use DWORD because M$ declares DWORD
        /// as unsigned long?!? Since util is heavily focused
        /// on serialization, and since we don't recognize long,
        /// we use unsigned int (ui32).
        #define THEKOGANS_UTIL_ERROR_CODE thekogans::util::ui32
        /// \brief
        /// Error code.
        #define THEKOGANS_UTIL_OS_ERROR_CODE GetLastError ()
        /// \brief
        /// Handle type.
        #define THEKOGANS_UTIL_HANDLE HANDLE
        /// \brief
        /// Invalid handle value.
        #define THEKOGANS_UTIL_INVALID_HANDLE_VALUE INVALID_HANDLE_VALUE
        /// \brief
        /// Process id type.
        /// See NOTE for THEKOGANS_UTIL_ERROR_CODE.
        #define THEKOGANS_UTIL_PROCESS_ID thekogans::util::ui32
        /// \brief
        /// Invalid process id value.
        #define THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE 0xffffffff
        /// \brief
        /// For compatibility only.
        #if defined (TOOLCHAIN_ARCH_i386)
            typedef long ssize_t;
        #elif defined (TOOLCHAIN_ARCH_x86_64)
            typedef __int64 ssize_t;
        #else // defined (TOOLCHAIN_ARCH_x86_64)
            #error "Unknown ssize_t"
        #endif // defined (TOOLCHAIN_ARCH_i386)
    #else // defined (TOOLCHAIN_OS_Windows)
        /// \brief
        /// Error code type.
        #define THEKOGANS_UTIL_ERROR_CODE THEKOGANS_UTIL_POSIX_ERROR_CODE
        /// \brief
        /// Error code.
        #define THEKOGANS_UTIL_OS_ERROR_CODE THEKOGANS_UTIL_POSIX_OS_ERROR_CODE
        /// \brief
        /// Handle type.
        #define THEKOGANS_UTIL_HANDLE THEKOGANS_UTIL_POSIX_HANDLE
        /// \brief
        /// Invalid handle value.
        #define THEKOGANS_UTIL_INVALID_HANDLE_VALUE THEKOGANS_UTIL_POSIX_INVALID_HANDLE_VALUE
        /// \brief
        /// Process id type.
        #define THEKOGANS_UTIL_PROCESS_ID pid_t
        /// \brief
        /// Invalid process id value.
        #define THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE -1
    #endif // defined (TOOLCHAIN_OS_Windows)

        /// \brief
        /// bool type size.
        const std::size_t BOOL_SIZE = sizeof (ui8);
        /// \brief
        /// Signed 8 bit type size.
        const std::size_t I8_SIZE = sizeof (i8);
        /// \brief
        /// Unsigned 8 bit type size.
        const std::size_t UI8_SIZE = sizeof (ui8);
        /// \brief
        /// Signed 16 bit type size.
        const std::size_t I16_SIZE = sizeof (i16);
        /// \brief
        /// Unsigned 16 bit type size.
        const std::size_t UI16_SIZE = sizeof (ui16);
        /// \brief
        /// Signed 32 bit type size.
        const std::size_t I32_SIZE = sizeof (i32);
        /// \brief
        /// Unsigned 32 bit type size.
        const std::size_t UI32_SIZE = sizeof (ui32);
        /// \brief
        /// Signed 64 bit type size.
        const std::size_t I64_SIZE = sizeof (i64);
        /// \brief
        /// Unsigned 64 bit type size.
        const std::size_t UI64_SIZE = sizeof (ui64);
        /// \brief
        /// 32 bit float type size.
        const std::size_t F32_SIZE = sizeof (f32);
        /// \brief
        /// 64 bit float type size.
        const std::size_t F64_SIZE = sizeof (f64);
        /// \brief
        /// Natural machine word size.
        const std::size_t MACHINE_WORD_SIZE = sizeof (MachineWord);
        /// \brief
        /// ssize_t type size.
        const std::size_t SSIZE_T_SIZE = sizeof (ssize_t);
        /// \brief
        /// std::size_t type size.
        const std::size_t SIZE_T_SIZE = sizeof (std::size_t);

        /// \def THEKOGANS_UTIL_IS_ODD(value)
        /// Evaluates to true if the given value is odd.
        /// \param[in] value Value to test for odd.
        #define THEKOGANS_UTIL_IS_ODD(value) (((value) & 1) == 1)
        /// \def THEKOGANS_UTIL_IS_EVEN(value)
        /// Evaluates to true if the given value is even.
        /// \param[in] value Value to test for even.
        #define THEKOGANS_UTIL_IS_EVEN(value) (((value) & 1) == 0)

        /// \brief
        /// VERY IMPORTANT: index = 0 - most significant byte/word/doubleword

        /// \def THEKOGANS_UTIL_UI16_GET_UI8_AT_INDEX(value, index)
        /// Extract a ui8 from a ui16.
        /// \param[in] value ui16 value to extract from.
        /// \param[in] index 0 (msb) or 1 (lsb).
        #define THEKOGANS_UTIL_UI16_GET_UI8_AT_INDEX(value, index)\
            ((thekogans::util::ui8)((thekogans::util::ui16)(value) >> ((1 - index) << 3)))

        /// \def THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX(value, index)
        /// Extract a ui8 from a ui32.
        /// \param[in] value ui32 value to extract from.
        /// \param[in] index 0 (msb) or 1 or 2 or 3 (lsb).
        #define THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX(value, index)\
            ((thekogans::util::ui8)((thekogans::util::ui32)(value) >> ((3 - index) << 3)))
        /// \def THEKOGANS_UTIL_UI32_GET_UI16_AT_INDEX(value, index)
        /// Extract a ui16 from a ui32.
        /// \param[in] value ui32 value to extract from.
        /// \param[in] index 0 (mss) or 1 (lss).
        #define THEKOGANS_UTIL_UI32_GET_UI16_AT_INDEX(value, index)\
            ((thekogans::util::ui16)((thekogans::util::ui32)(value) >> ((1 - index) << 4)))

        /// \def THEKOGANS_UTIL_UI64_GET_UI8_AT_INDEX(value, index)
        /// Extract a ui8 from a ui64.
        /// \param[in] value ui64 value to extract from.
        /// \param[in] index 0 (msb) or 1 or 2 or 3 or 4 or 5 or 6 or 7 (lsb).
        #define THEKOGANS_UTIL_UI64_GET_UI8_AT_INDEX(value, index)\
            ((thekogans::util::ui8)((thekogans::util::ui64)(value) >> ((7 - index) << 3)))
        /// \def THEKOGANS_UTIL_UI64_GET_UI16_AT_INDEX(value, index)
        /// Extract a ui16 from a ui64.
        /// \param[in] value ui64 value to extract from.
        /// \param[in] index 0 (mss) or 1 or 2 or 3 (lss).
        #define THEKOGANS_UTIL_UI64_GET_UI16_AT_INDEX(value, index)\
            ((thekogans::util::ui16)((thekogans::util::ui64)(value) >> ((3 - index) << 4)))
        /// \def THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX(value, index)
        /// Extract a ui32 from a ui64.
        /// \param[in] value ui64 value to extract from.
        /// \param[in] index 0 (msw) or 1 (lsw).
        #define THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX(value, index)\
            ((thekogans::util::ui32)((thekogans::util::ui64)(value) >> ((1 - index) << 5)))

        /// \def THEKOGANS_UTIL_MK_UI16(h8, l8)
        /// Make an ui16 from two ui8s.
        /// \param[in] h8 msb
        /// \param[in] l8 lsb
        #define THEKOGANS_UTIL_MK_UI16(h8, l8)\
            ((thekogans::util::ui16)(((thekogans::util::ui16)(h8) << 8) | (l8)))
        /// \def THEKOGANS_UTIL_MK_UI32(h16, l16)
        /// Make an ui32 from two ui16s.
        /// \param[in] h16 mss
        /// \param[in] l16 lss
        #define THEKOGANS_UTIL_MK_UI32(h16, l16)\
            ((thekogans::util::ui32)(((thekogans::util::ui32)(h16) << 16) | (l16)))
        /// \def THEKOGANS_UTIL_MK_UI64(h32, l32)
        /// Make an ui64 from two ui32s.
        /// \param[in] h32 msw
        /// \param[in] l32 lsw
        #define THEKOGANS_UTIL_MK_UI64(h32, l32)\
            ((thekogans::util::ui64)(((thekogans::util::ui64)(h32) << 32) | (l32)))

        /// \def THEKOGANS_UTIL_ARRAY_SIZE(array)
        /// Calculate the number of elements in an array.
        /// \param[in] array Array whos size to calculate.
        #define THEKOGANS_UTIL_ARRAY_SIZE(array)\
            (sizeof (array) / sizeof (array[0]))

        /// \def THEKOGANS_UTIL_MERGE(prefix, lineNumber)
        /// Concatenate a prefix and line number to for a unique name.
        /// \param[in] prefix Name prefix.
        /// \param[in] lineNumber Line number.
        #define THEKOGANS_UTIL_MERGE(prefix, lineNumber) prefix##lineNumber
        /// \def THEKOGANS_UTIL_LABEL(prefix, lineNumber)
        /// Create a unique label using a prefix and a line number.
        /// \param[in] prefix Label prefix.
        /// \param[in] lineNumber Line number.
        #define THEKOGANS_UTIL_LABEL(prefix, lineNumber) THEKOGANS_UTIL_MERGE(prefix, lineNumber)
        /// \def THEKOGANS_UTIL_UNIQUE_NAME(prefix)
        /// Given a prefix, create a unique name using __LINE__.
        /// \param[in] prefix Name prefix.
        #define THEKOGANS_UTIL_UNIQUE_NAME(prefix) THEKOGANS_UTIL_LABEL(prefix, __LINE__)

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Types_h)
