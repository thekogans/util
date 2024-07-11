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

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
    #include <cstdio>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <cstddef>
#include <cstdlib>
#include <cstdarg>
#include "thekogans/util/Config.h"

/// \brief
/// For compatibility only.
#if defined (TOOLCHAIN_OS_Windows)
    #if defined (TOOLCHAIN_ARCH_i386)
        typedef long ssize_t;
    #elif defined (TOOLCHAIN_ARCH_x86_64)
        typedef __int64 ssize_t;
    #else // defined (TOOLCHAIN_ARCH_x86_64)
        #error Unknown TOOLCHAIN_ARCH.
    #endif // defined (TOOLCHAIN_ARCH_i386)
#endif // defined (TOOLCHAIN_OS_Windows)

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
        /// Validate assumptions about integral type sizes.
        static_assert (
            I8_SIZE == 1 && UI8_SIZE == 1 &&
            I16_SIZE == 2 && UI16_SIZE == 2 &&
            I32_SIZE == 4 && UI32_SIZE == 4 &&
            I64_SIZE == 8 && UI64_SIZE == 8 &&
            F32_SIZE == 4 && F64_SIZE == 8,
            "Invalid assumption about integral types sizes.");

        /// \brief
        /// Architecture dependent natural word (register) type (size).
        #if (TOOLCHAIN_ARCH_WORD_SIZE == 1)
            typedef ui8 MachineWord;
        #elif (TOOLCHAIN_ARCH_WORD_SIZE == 2)
            typedef ui16 MachineWord;
        #elif (TOOLCHAIN_ARCH_WORD_SIZE == 4)
            typedef ui32 MachineWord;
        #elif (TOOLCHAIN_ARCH_WORD_SIZE == 8)
            typedef ui64 MachineWord;
        #else // (TOOLCHAIN_ARCH_WORD_SIZE == 8)
            #error Unknown TOOLCHAIN_ARCH_WORD_SIZE.
        #endif // (TOOLCHAIN_ARCH_WORD_SIZE == 1)

        /// \brief
        /// Natural machine word size.
        const std::size_t MACHINE_WORD_SIZE = sizeof (MachineWord);
        /// \brief
        /// ssize_t type size.
        const std::size_t SSIZE_T_SIZE = sizeof (ssize_t);
        /// \brief
        /// std::size_t type size.
        const std::size_t SIZE_T_SIZE = sizeof (std::size_t);
        /// \brief
        /// wchar_t type size.
        const std::size_t WCHAR_T_SIZE = sizeof (wchar_t);

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
        /// Session id type.
        /// See NOTE for THEKOGANS_UTIL_ERROR_CODE.
        #define THEKOGANS_UTIL_SESSION_ID thekogans::util::ui32
        /// \brief
        /// Invalid session id value.
        #define THEKOGANS_UTIL_INVALID_SESSION_ID_VALUE 0xffffffff
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
        /// \brief
        /// Session id type.
        #define THEKOGANS_UTIL_SESSION_ID pid_t
        /// \brief
        /// Invalid session id value.
        #define THEKOGANS_UTIL_INVALID_SESSION_ID_VALUE -1
    #endif // defined (TOOLCHAIN_OS_Windows)

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
        /// \def THEKOGANS_UTIL_UI64_FROM_UI64_UI16_GET_UI64(ui64)
        /// Extract the top 48 bits.
        /// \param[in] ui64 Value to extract from.
        #define THEKOGANS_UTIL_UI64_FROM_UI64_UI16_GET_UI64(ui64)\
            (((thekogans::util::ui64)(ui64) >> 16) & 0x0000ffffffffffff)
        /// \def THEKOGANS_UTIL_UI64_FROM_UI64_UI8_GET_UI64(ui64)
        /// Extract the top 56 bits.
        /// \param[in] ui64 Value to extract from.
        #define THEKOGANS_UTIL_UI64_FROM_UI64_UI8_GET_UI64(ui64)\
            (((thekogans::util::ui64)(ui64) >> 8) & 0x00ffffffffffffff)
        /// \def THEKOGANS_UTIL_UI64_FROM_UI64_UI16_GET_UI16(ui64)
        /// Extract the low 16 bits.
        /// \param[in] ui64 Value to extract from.
        #define THEKOGANS_UTIL_UI64_FROM_UI64_UI16_GET_UI16(ui64)\
            ((thekogans::util::ui64)(ui64) & 0x000000000000ffff)
        /// \def THEKOGANS_UTIL_UI64_FROM_UI64_UI16_GET_UI8(ui64)
        /// Extract the low 8 bits.
        /// \param[in] ui64 Value to extract from.
        #define THEKOGANS_UTIL_UI64_FROM_UI64_UI16_GET_UI8(ui64)\
            ((thekogans::util::ui64)(ui64) & 0x00000000000000ff)

        /// \def THEKOGANS_UTIL_MK_UI16(h8, l8)
        /// Make an ui16 from two ui8s.
        /// \param[in] h8 msb
        /// \param[in] l8 lsb
        #define THEKOGANS_UTIL_MK_UI16(h8, l8)\
            ((thekogans::util::ui16)(((thekogans::util::ui16)(h8) << 8) |\
                (thekogans::util::ui16)(thekogans::util::ui8)(l8)))
        /// \def THEKOGANS_UTIL_MK_UI32(h16, l16)
        /// Make an ui32 from two ui16s.
        /// \param[in] h16 mss
        /// \param[in] l16 lss
        #define THEKOGANS_UTIL_MK_UI32(h16, l16)\
            ((thekogans::util::ui32)(((thekogans::util::ui32)(h16) << 16) |\
                (thekogans::util::ui32)(thekogans::util::ui16)(l16)))
        /// \def THEKOGANS_UTIL_MK_UI32_FROM_UI8_UI32(h8, l24)
        /// Make an ui32 from a ui8 and a ui32(Least significant 24 bits).
        /// \param[in] h8 msb
        /// \param[in] l24 lsdw
        #define THEKOGANS_UTIL_MK_UI32_FROM_UI8_UI32(h8, l24)\
            ((thekogans::util::ui32)(((thekogans::util::ui32)(h8) << 24) |\
                (thekogans::util::ui32)(l24) & 0x0fff)
        /// \def THEKOGANS_UTIL_MK_UI64(h32, l32)
        /// Make an ui64 from two ui32s.
        /// \param[in] h32 msw
        /// \param[in] l32 lsw
        #define THEKOGANS_UTIL_MK_UI64(h32, l32)\
            ((thekogans::util::ui64)(((thekogans::util::ui64)(h32) << 32) |\
                (thekogans::util::ui64)(thekogans::util::ui32)(l32)))
        /// \def THEKOGANS_UTIL_MK_UI64_FROM_UI8_UI64(h8, l56)
        /// Make an ui64 from a ui8 and a ui64(Least significant 56 bits).
        /// \param[in] h8 msb
        /// \param[in] l56 lsqw
        #define THEKOGANS_UTIL_MK_UI64_FROM_UI8_UI64(h8, l56)\
            ((thekogans::util::ui64)(((thekogans::util::ui64)(h8) << 56) |\
                ((thekogans::util::ui64)(l56) & 0x00ffffffffffffff))
        /// \def THEKOGANS_UTIL_MK_UI64_FROM_UI64_UI8(h56, l8)
        /// Make an ui64 from a ui64(Least significant 56 bits) and a ui8.
        /// \param[in] h56 msqw
        /// \param[in] l8 lsb
        #define THEKOGANS_UTIL_MK_UI64_FROM_UI64_UI8(h56, l8)\
            ((thekogans::util::ui64)(((thekogans::util::ui64)(h56) << 8) |\
                ((thekogans::util::ui64)(thekogans::util::ui8)(l8) & 0x00ff))
        /// \def THEKOGANS_UTIL_MK_UI64_FROM_UI16_UI64(h16, l48)
        /// Make an ui64 from a ui16 and a ui64(Least significant 48 bits).
        /// \param[in] h16 msb
        /// \param[in] l48 lsqw
        #define THEKOGANS_UTIL_MK_UI64_FROM_UI16_UI64(h16, l48)\
            ((thekogans::util::ui64)(((thekogans::util::ui64)(h8) << 48) |\
                ((thekogans::util::ui64)(l48) & 0x0000ffffffffffff))
        /// \def THEKOGANS_UTIL_MK_UI64_FROM_UI64_UI16(h48, l16)
        /// Make an ui64 from a ui16 and a ui64(Least significant 48 bits).
        /// \param[in] h48 msqw
        /// \param[in] l16 lsb
        #define THEKOGANS_UTIL_MK_UI64_FROM_UI64_UI16(h48, l16)\
            ((thekogans::util::ui64)(((thekogans::util::ui64)(h48) << 16) |\
                ((thekogans::util::ui64)(thekogans::util::ui16)(l16) & 0x0000ffff))

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
        #define THEKOGANS_UTIL_UNIQUE_NAME(prefix) THEKOGANS_UTIL_LABEL (prefix, __LINE__)
        /// \undef THEKOGANS_UTIL_LABEL
        /// THEKOGANS_UTIL_LABEL is private to Types.h and should not polute the global namespace.
        #undef THEKOGANS_UTIL_LABEL
        /// \undef THEKOGANS_UTIL_MERGE
        /// THEKOGANS_UTIL_MERGE is private to Types.h and should not polute the global namespace.
        #undef THEKOGANS_UTIL_MERGE

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Types_h)
