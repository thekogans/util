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
    #error libthekogans_util requires C++ compilation (use a .cpp suffix)
#endif // !defined (__cplusplus)

/// \brief
/// NOTE: Headers should be included in the following order:
///
/// - system/os specific
/// - std c
/// - std c++
/// - third party dependencies
/// - thekogans dependencies
///
/// The reason for this order is to prevent global namespace
/// collisions as much as posible.

#include <cstdlib>
#include <iostream>

#if defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
    #include <atomic>
    #define THEKOGANS_UTIL_ATOMIC std::atomic
#else // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
    #include <boost/atomic.hpp>
    #define THEKOGANS_UTIL_ATOMIC boost::atomic
#endif // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)

#if defined (TOOLCHAIN_OS_Windows)
    #define _LIB_THEKOGANS_UTIL_API __stdcall
    #if defined (TOOLCHAIN_TYPE_Shared)
        #if defined (_LIB_THEKOGANS_UTIL_BUILD)
            #define _LIB_THEKOGANS_UTIL_DECL __declspec (dllexport)
        #else // defined (_LIB_THEKOGANS_UTIL_BUILD)
            #define _LIB_THEKOGANS_UTIL_DECL __declspec (dllimport)
        #endif // defined (_LIB_THEKOGANS_UTIL_BUILD)
        #define THEKOGANS_UTIL_EXPORT __declspec (dllexport)
    #else // defined (TOOLCHAIN_TYPE_Shared)
        #define _LIB_THEKOGANS_UTIL_DECL
        #define THEKOGANS_UTIL_EXPORT
    #endif // defined (TOOLCHAIN_TYPE_Shared)
    #if defined (_MSC_VER) && (_MSC_VER <= 1200)
        #define THEKOGANS_UTIL_TYPENAME
    #else // defined (_MSC_VER) && (_MSC_VER <= 1200)
        #define THEKOGANS_UTIL_TYPENAME typename
    #endif // defined (_MSC_VER) && (_MSC_VER <= 1200)
    #if defined (_MSC_VER)
        #pragma warning (disable: 4251)  // using non-exported as public in exported
        #pragma warning (disable: 4786)
    #endif // defined (_MSC_VER)
    #define THEKOGANS_UTIL_PACKED(x) __pragma (pack (push, 1)) x __pragma (pack (pop))
#else // defined (TOOLCHAIN_OS_Windows)
    #define _LIB_THEKOGANS_UTIL_API
    #define _LIB_THEKOGANS_UTIL_DECL
    #define THEKOGANS_UTIL_EXPORT
    #define THEKOGANS_UTIL_TYPENAME typename
    #define THEKOGANS_UTIL_PACKED(x) x __attribute__ ((packed))
#endif // defined (TOOLCHAIN_OS_Windows)

#if defined (TOOLCHAIN_CONFIG_Debug)
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
#else // defined (TOOLCHAIN_CONFIG_Debug)
    #define THEKOGANS_UTIL_ASSERT(condition, message) do {} while (false)
    #define THEKOGANS_UTIL_DEBUG_BREAK
#endif // defined (TOOLCHAIN_CONFIG_Debug)

/// \def THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN(type)
/// A convenient macro to suppress copy construction and assignment.
#define THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN(type)\
private:\
    type (const type &);\
    type &operator = (const type &);

/// \def THEKOGANS_UTIL
/// Logging subsystem name.
#define THEKOGANS_UTIL "thekogans_util"

#endif // !defined (__thekogans_util_Config_h)
