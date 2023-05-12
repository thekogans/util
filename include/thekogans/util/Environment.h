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

// NOTE: This file is meant to be periodically updated by adding new
// defines for whatever os, arch and compiler we might want to support
// next.

// TOOLCHAIN_OS
#if !defined (TOOLCHAIN_OS_Windows) && !defined (TOOLCHAIN_OS_Linux) && !defined (TOOLCHAIN_OS_OSX)
    #if defined (_WINDOWS_)
        #define TOOLCHAIN_OS_Windows
        #define TOOLCHAIN_OS "Windows"
    #elif defined (__linux__)
        #define TOOLCHAIN_OS_Linux
        #define TOOLCHAIN_OS "Linux"
    #elif defined (__APPLE__)
        #define TOOLCHAIN_OS_OSX
        #define TOOLCHAIN_OS "OSX"
    #else // defined (_WINDOWS_)
        #error Unknown TOOLCHAIN_OS.
    #endif // defined (_WINDOWS_)
#endif // !defined (TOOLCHAIN_OS_Windows) && !defined (TOOLCHAIN_OS_Linux) && !defined (TOOLCHAIN_OS_OSX)

// TOOLCHAIN_ARCH
#if !defined (TOOLCHAIN_ARCH_i386) && !defined (TOOLCHAIN_ARCH_x86_64) &&\
    !defined (TOOLCHAIN_ARCH_ppc32) && !defined (TOOLCHAIN_ARCH_ppc64) &&\
    !defined (TOOLCHAIN_ARCH_arm32) && !defined (TOOLCHAIN_ARCH_arm64) &&\
    !defined (TOOLCHAIN_ARCH_mips32) && !defined (TOOLCHAIN_ARCH_mips64)
    #if defined (_M_X64) || defined (__x86_64__) || defined (__x86_64)
        #define TOOLCHAIN_ARCH_x86_64
        #define TOOLCHAIN_ARCH "x86_64"
    #elif defined (_M_IX86) || defined (__i386__)
        #define TOOLCHAIN_ARCH_i386
        #define TOOLCHAIN_ARCH "i386"
    #elif defined (_M_ARM64) || defined (__aarch64__) || defined (__arm64)
        #define TOOLCHAIN_ARCH_arm64
        #define TOOLCHAIN_ARCH "arm64"
    #elif defined (_M_ARM) || defined (__arm__)
        #define TOOLCHAIN_ARCH_arm32
        #define TOOLCHAIN_ARCH "arm32"
    #elif defined (__powerpc64__) || defined (__ppc64)
        #define TOOLCHAIN_ARCH_ppc64
        #define TOOLCHAIN_ARCH "ppc64"
    #else
        #error Unknown TOOLCHAIN_ARCH.
    #endif // defined (_M_X64) || defined (__x86_64__) || defined (__x86_64)
#endif // !defined (TOOLCHAIN_ARCH_i386) && !defined (TOOLCHAIN_ARCH_x86_64) &&
       // !defined (TOOLCHAIN_ARCH_ppc32) && !defined (TOOLCHAIN_ARCH_ppc64) &&
       // !defined (TOOLCHAIN_ARCH_arm32) && !defined (TOOLCHAIN_ARCH_arm64) &&
       // !defined (TOOLCHAIN_ARCH_mips32) && !defined (TOOLCHAIN_ARCH_mips64)

// TOOLCHAIN_COMPILER
#if !defined (TOOLCHAIN_COMPILER_cl) && !defined (TOOLCHAIN_COMPILER_clang) &&\
    !defined (TOOLCHAIN_COMPILER_gcc) && !defined (TOOLCHAIN_COMPILER_icc)
    #if defined (_MSC_VER)
        #define TOOLCHAIN_COMPILER_cl
        #define TOOLCHAIN_COMPILER "cl"
    #elif defined (__clang__)
        #define TOOLCHAIN_COMPILER_clang
        #define TOOLCHAIN_COMPILER "clang"
    #elif defined (__GNUC__)
        #define TOOLCHAIN_COMPILER_gcc
        #define TOOLCHAIN_COMPILER "gcc"
    #elif defined (__INTEL_COMPILER)
        #define TOOLCHAIN_COMPILER_icc
        #define TOOLCHAIN_COMPILER "icc"
    #else
        #error Unknown TOOLCHAIN_COMPILER.
    #endif // defined (_MSC_VER)
#endif // !defined (TOOLCHAIN_COMPILER_cl) && !defined (TOOLCHAIN_COMPILER_clang) &&
       // !defined (TOOLCHAIN_COMPILER_gcc) && !defined (TOOLCHAIN_COMPILER_icc)

// TOOLCHAIN_BRANCH
#if !defined (TOOLCHAIN_BRANCH)
    #define TOOLCHAIN_BRANCH TOOLCHAIN_OS##"/"##TOOLCHAIN_ARCH##"/"##TOOLCHAIN_COMPILER
#endif // !defined (TOOLCHAIN_BRANCH)

// TOOLCHAIN_TRIPLET
#if !defined (TOOLCHAIN_TRIPLET)
    #define TOOLCHAIN_TRIPLET TOOLCHAIN_OS##"-"##TOOLCHAIN_ARCH##"-"##TOOLCHAIN_COMPILER
#endif // !defined (TOOLCHAIN_TRIPLET)

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
