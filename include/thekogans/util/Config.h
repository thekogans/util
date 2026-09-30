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

#if !defined (__thekogans_util_Config_h)
#define __thekogans_util_Config_h

#if !defined (__cplusplus)
    #error libthekogans_util requires C++ compilation (use a .cpp suffix).
#endif // !defined (__cplusplus)

/// \brief
/// NOTE: Headers should be included in the following order:
///
/// - system/os specific
/// - std c
/// - std c++
/// - third party dependencies
/// - thekogans dependencies
/// - project
///
/// The reason for this order is to prevent global namespace
/// collisions as much as posible.
///
/// The only exception is Environment.h. That header defines
/// the toolchain triplet (TOOLCHAIN_OS, TOOLCHAIN_ARCH and
/// TOOLCHAIN_COMP) and should be included before the use of
/// any of the TOOLCHAIN_... macros as it will try to deduce
/// them if they were not provided during compilation.

#include <cstdlib>
#include <new>
#include <iostream>
#include "thekogans/util/Environment.h"

#if defined (TOOLCHAIN_OS_Windows)
    #define _LIB_THEKOGANS_UTIL_API __stdcall
    #if defined (THEKOGANS_UTIL_TYPE_Shared)
        #if defined (_LIB_THEKOGANS_UTIL_BUILD)
            #define _LIB_THEKOGANS_UTIL_DECL __declspec (dllexport)
        #else // defined (_LIB_THEKOGANS_UTIL_BUILD)
            #define _LIB_THEKOGANS_UTIL_DECL __declspec (dllimport)
        #endif // defined (_LIB_THEKOGANS_UTIL_BUILD)
        #define THEKOGANS_UTIL_EXPORT __declspec (dllexport)
    #else // defined (THEKOGANS_UTIL_TYPE_Shared)
        #define _LIB_THEKOGANS_UTIL_DECL
        #define THEKOGANS_UTIL_EXPORT
    #endif // defined (THEKOGANS_UTIL_TYPE_Shared)
    #if defined (TOOLCHAIN_COMPILER_cl) && (_MSC_VER <= 1200)
        #define THEKOGANS_UTIL_TYPENAME
    #else // defined (TOOLCHAIN_COMPILER_cl) && (_MSC_VER <= 1200)
        #define THEKOGANS_UTIL_TYPENAME typename
    #endif // defined (TOOLCHAIN_COMPILER_cl) && (_MSC_VER <= 1200)
    #define THEKOGANS_UTIL_PACKED(x) __pragma (pack (push, 1)) x __pragma (pack (pop))
    #if defined (TOOLCHAIN_COMPILER_cl)
        #pragma warning (disable: 4251)  // using non-exported as public in exported
        #pragma warning (disable: 4786)
    #endif // defined (TOOLCHAIN_COMPILER_cl)
#else // defined (TOOLCHAIN_OS_Windows)
    #define _LIB_THEKOGANS_UTIL_API
    #define _LIB_THEKOGANS_UTIL_DECL
    #define THEKOGANS_UTIL_EXPORT
    #define THEKOGANS_UTIL_TYPENAME typename
    #define THEKOGANS_UTIL_PACKED(x) x __attribute__ ((packed))
#endif // defined (TOOLCHAIN_OS_Windows)

#if defined (THEKOGANS_UTIL_CONFIG_Debug)
    /// \def THEKOGANS_UTIL_ASSERT(condition, message)
    /// A more capable replacement for assert.
    #define THEKOGANS_UTIL_ASSERT(condition, message)\
        do {\
            if (!(condition)) {\
                std::cerr << "Assertion `" #condition "` failed in " << __FILE__ <<\
                    " line " << __LINE__ << ": " << message << std::endl;\
                std::exit (EXIT_FAILURE);\
            }\
        } while (false)
    /// \def THEKOGANS_UTIL_DEBUG_BREAK
    /// Trigger a debug break.
    #if defined (THEKOGANS_UTIL_DEBUG_BREAK_ON_THROW)
        #if defined (TOOLCHAIN_OS_Windows)
            #define THEKOGANS_UTIL_DEBUG_BREAK __debugbreak ();
        #else // defined (TOOLCHAIN_OS_Windows)
            #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
                #define THEKOGANS_UTIL_DEBUG_BREAK __asm__ ("int $3");
            #else // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
                #define THEKOGANS_UTIL_DEBUG_BREAK
            #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
        #endif // defined (TOOLCHAIN_OS_Windows)
    #else // defined (THEKOGANS_UTIL_BREAK_ON_THROW)
        #define THEKOGANS_UTIL_DEBUG_BREAK
    #endif // defined (THEKOGANS_UTIL_BREAK_ON_THROW)
#else // defined (THEKOGANS_UTIL_CONFIG_Debug)
    #define THEKOGANS_UTIL_ASSERT(condition, message) do {} while (false)
    #define THEKOGANS_UTIL_DEBUG_BREAK
#endif // defined (THEKOGANS_UTIL_CONFIG_Debug)

/// \def THEKOGANS_UTIL
/// Logging subsystem name.
#define THEKOGANS_UTIL "thekogans_util"

/// \def THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS
/// Macro to declare std allocator functions.
#define THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS  \
public:\
    static void *operator new (std::size_t);\
    static void *operator new (std::size_t, const std::nothrow_t &) noexcept;\
    static void *operator new (std::size_t, void *) noexcept;\
    static void operator delete (void *) noexcept;\
    static void operator delete (void *, const std::nothrow_t &) noexcept;\
    static void operator delete (void *, void *) noexcept;

/// \def THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN(_T)
/// A convenient macro to suppress copy construction and assignment.
#define THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN(_T)\
    _T (const _T &) = delete;\
    _T &operator = (const _T &) = delete;

/// \def THEKOGANS_UTIL_DISALLOW_MOVE_AND_ASSIGN(_T)
/// A convenient macro to suppress move construction and assignment.
#define THEKOGANS_UTIL_DISALLOW_MOVE_AND_ASSIGN(_T)\
    _T (_T &&) = delete;\
    _T &operator = (_T &&) = delete;

/// \def THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN(_T)
/// A convenient macro to suppress copy and move construction and assignment.
#define THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN(_T)\
    THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN(_T)\
    THEKOGANS_UTIL_DISALLOW_MOVE_AND_ASSIGN(_T)

#if defined (__clang__) || defined (__GNUC__)
    #define THEKOGANS_UTIL_LIKELY(x)   (__builtin_expect (!!(x), 1))
    #define THEKOGANS_UTIL_UNLIKELY(x) (__builtin_expect (!!(x), 0))
#elif defined (_MSC_VER) && (_MSVC_LANG >= 202002L) // C++20 attribute fallback for newer MSVC
    #define THEKOGANS_UTIL_LIKELY(x)   (x) [[likely]]
    #define THEKOGANS_UTIL_UNLIKELY(x) (x) [[unlikely]]
#else
    #define THEKOGANS_UTIL_LIKELY(x)   (x)
    #define THEKOGANS_UTIL_UNLIKELY(x) (x)
#endif

namespace thekogans {
    namespace util {

        /// \brief
        /// System cache line size.
        /// It goes without saying that this value must be a power of 2.
    #if defined (__cpp_lib_hardware_interference_size)
        static constexpr std::size_t SYSTEM_CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
    #else // defined (__cpp_lib_hardware_interference_size)
        static constexpr std::size_t SYSTEM_CACHE_LINE_SIZE = 64; // Safe, rock-solid industry fallback.
    #endif // defined (__cpp_lib_hardware_interference_size)

        /// \enum
        /// Log levels. Each successive level builds on the previous ones.
        /// IMPORTANT: These constants are mirrored in Config.h. If we
        /// change this list, we need to update that one too.
        enum {
            /// \brief
            /// Log nothing.
            Invalid,
            /// \brief
            /// Log only errors.
            Error,
            /// \brief
            /// Log errors and warnings.
            Warning,
            /// \brief
            /// Log errors, warnings and info.
            Info,
            /// \brief
            /// Log errors, warnings, info and debug.
            Debug,
            /// \brief
            /// Log errors, warnings, info, debug and development.
            Development,
            /// \brief
            /// Highest log level we support.
            MaxLevel = Development
        };

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        /// \brief
        /// If you're linking to thekogans_util statically, call this
        /// method early on in main to initialize dynamically creatable
        /// (\see{DynamicCreatable}) types. If you don't call this method
        /// the only types that will be available to your application are
        /// the ones you explicitly link to.
        /// NOTE: This is the root and the only StaticInit that needs to
        /// be called. It takes care of the rest.
        void StaticInit ();
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        /// \brief
        /// Zero out the given memory block.
        /// Its volatile so that the optimizer leaves it alone.
        /// \param[in] data Block t zero out.
        /// \param[in] size Block size (in bytes).
        /// \return if data != nullptr && size > 0, size oterwise 0.
        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API SecureZeroMemory (
            volatile void *data,
            std::size_t size);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Config_h)
