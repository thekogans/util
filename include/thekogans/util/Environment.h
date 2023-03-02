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

#if !defined (__thekogans_util_Environment_h)
#define __thekogans_util_Environment_h

// thekogans libraries are used in environments that don't call $TOOLCHAIN_COMMON_BIN/setenvironment,
// and don't use thekogans_make. In that case try to deduce the environment we're running on top of.

// TOOLCHAIN_OS
#if !defined (TOOLCHAIN_OS_Windows) && !defined (TOOLCHAIN_OS_Linux) && !defined (TOOLCHAIN_OS_OSX)
    #if defined (_WIN32) || defined (_WIN64)
        #define TOOLCHAIN_OS_Windows
    #elif defined (__linux__)
        #define TOOLCHAIN_OS_Linux
    #elif defined (__APPLE__)
        #define TOOLCHAIN_OS_OSX
    #else // defined (_WIN32) || defined (_WIN64)
        #error Unknown TOOLCHAIN_OS.
    #endif // defined (_WIN32) || defined (_WIN64)
#endif // !defined (TOOLCHAIN_OS_Windows) && !defined (TOOLCHAIN_OS_Linux) && !defined (TOOLCHAIN_OS_OSX)

// TOOLCHAIN_ARCH
#if !defined (TOOLCHAIN_ARCH_i386) && !defined (TOOLCHAIN_ARCH_x86_64) &&\
    !defined (TOOLCHAIN_ARCH_ppc) && !defined (TOOLCHAIN_ARCH_ppc64) &&\
    !defined (TOOLCHAIN_ARCH_arm) && !defined (TOOLCHAIN_ARCH_arm64)
    #if defined (TOOLCHAIN_OS_Windows)
        #if defined (_M_IX86)
            #define TOOLCHAIN_ARCH_i386
        #elif defined (_M_X64)
            #define TOOLCHAIN_ARCH_x86_64
        #elif defined (_M_ARM)
            #if defined (_M_ARM64)
                #define TOOLCHAIN_ARCH_arm64
            #else // defined (_M_ARM64)
                #define TOOLCHAIN_ARCH_arm32
            #endif // defined (_M_ARM64)
        #else // defined (_M_IX86)
            #error Unknown TOOLCHAIN_ARCH.
        #endif // defined (_M_IX86)
    #elif defined (TOOLCHAIN_OS_Linux)
        #if defined (__i386__)
            #define TOOLCHAIN_ARCH_i386
        #elif defined (__x86_64__)
            #define TOOLCHAIN_ARCH_x86_64
        #elif defined (__arm__)
            #if defined (__aarch64__)
                #define TOOLCHAIN_ARCH_arm64
            #else // defined (__aarch64__)
                #define TOOLCHAIN_ARCH_arm32
            #endif // defined (__aarch64__)
        #elif defined (__powerpc64__)
            #define TOOLCHAIN_ARCH_ppc64
        #else // defined (__i386__)
            #error Unknown TOOLCHAIN_ARCH.
        #endif // defined (__i386__)
    #elif defined (TOOLCHAIN_OS_OSX)
        #if defined (__x86_64)
            #define TOOLCHAIN_ARCH_x86_64
        #elif defined (__ppc64)
            #define TOOLCHAIN_ARCH_ppc64
        #elif defined (__arm64)
            #define TOOLCHAIN_ARCH_arm64
        #else // defined (__x86_64)
            #error Unknown TOOLCHAIN_ARCH.
        #endif // defined (__x86_64)
    #endif // defined (TOOLCHAIN_OS_Windows)
#endif // !defined (TOOLCHAIN_ARCH_i386) && !defined (TOOLCHAIN_ARCH_x86_64) &&
       // !defined (TOOLCHAIN_ARCH_ppc) && !defined (TOOLCHAIN_ARCH_ppc64) &&
       // !defined (TOOLCHAIN_ARCH_arm) && !defined (TOOLCHAIN_ARCH_arm64)

// TOOLCHAIN_COMPILER
#if defined (TOOLCHAIN_OS_Windows)
    #if !defined (TOOLCHAIN_COMPILER_cl18) && !defined (TOOLCHAIN_COMPILER_cl19) &&\
        !defined (TOOLCHAIN_COMPILER_cl1910) && !defined (TOOLCHAIN_COMPILER_cl1924)
        #if _MSC_VER == 1800
            #define TOOLCHAIN_COMPILER_cl18
        #elif _MSC_VER == 1900
            #define TOOLCHAIN_COMPILER_cl19
        #elif _MSC_VER == 1910
            #define TOOLCHAIN_COMPILER_cl1910
        #elif _MSC_VER == 1924
            #define TOOLCHAIN_COMPILER_cl1924
        #else // _MSC_VER == 1800
            #error Unknown TOOLCHAIN_COMPILER.
        #endif // _MSC_VER == 1800
    #endif // !defined (TOOLCHAIN_COMPILER_cl18) && !defined (TOOLCHAIN_COMPILER_cl19) &&
           // !defined (TOOLCHAIN_COMPILER_cl1910) && !defined (TOOLCHAIN_COMPILER_cl1924)
#elif defined (TOOLCHAIN_OS_Linux)
    #if !defined (TOOLCHAIN_COMPILER_gcc)
        #if defined (__GNUC__)
            #define TOOLCHAIN_COMPILER_gcc
        #else // defined (__GNUC__)
            #error Unknown TOOLCHAIN_COMPILER.
        #endif // defined (__GNUC__)
    #endif // !defined (TOOLCHAIN_COMPILER_gcc)
#elif defined (TOOLCHAIN_OS_OSX)
    #if !defined (TOOLCHAIN_COMPILER_clang) && !defined (TOOLCHAIN_COMPILER_gcc)
        #if defined (__clang__)
            #define TOOLCHAIN_COMPILER_clang
        #elif defined (__GNUC__)
            #define TOOLCHAIN_COMPILER_gcc
        #else // defined (__clang__)
            #error Unknown TOOLCHAIN_COMPILER.
        #endif // defined (__clang__)
    #endif // !defined (TOOLCHAIN_COMPILER_clang) && !defined (TOOLCHAIN_COMPILER_gcc)
#endif // defined (TOOLCHAIN_OS_Windows)

// TOOLCHAIN_ENDIAN
#if !defined (TOOLCHAIN_ENDIAN_Little) && !defined (TOOLCHAIN_ENDIAN_Big)
    #define THEKOGANS_UTIL_LITTLE_ENDIAN 0x41424344UL
    #define THEKOGANS_UTIL_BIG_ENDIAN    0x44434241UL
    #define THEKOGANS_UTIL_ENDIAN_ORDER  ('ABCD')
    #if THEKOGANS_UTIL_ENDIAN_ORDER == THEKOGANS_UTIL_LITTLE_ENDIAN
        #define TOOLCHAIN_ENDIAN_Little
    #elif THEKOGANS_UTIL_ENDIAN_ORDER == THEKOGANS_UTIL_BIG_ENDIAN
        #define TOOLCHAIN_ENDIAN_Big
    #else // THEKOGANS_UTIL_ENDIAN_ORDER == THEKOGANS_UTIL_LITTLE_ENDIAN
        #error Unknown TOOLCHAIN_ENDIAN
    #endif // THEKOGANS_UTIL_ENDIAN_ORDER == THEKOGANS_UTIL_LITTLE_ENDIAN
    #undef THEKOGANS_UTIL_LITTLE_ENDIAN
    #undef THEKOGANS_UTIL_BIG_ENDIAN
    #undef THEKOGANS_UTIL_ENDIAN_ORDER
#endif // !defined (TOOLCHAIN_ENDIAN_Little) && !defined (TOOLCHAIN_ENDIAN_Big)

#endif // !defined (__thekogans_util_Environment_h)
