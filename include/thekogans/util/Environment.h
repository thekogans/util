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
#if !defined (TOOLCHAIN_OS)
    #if defined (_WIN32) || defined (_WIN64)
        #define TOOLCHAIN_OS "Windows"
        #if !defined (TOOLCHAIN_OS_Windows)
            #define TOOLCHAIN_OS_Windows
        #endif // !defined (TOOLCHAIN_OS_Windows)
    #elif defined (__linux__)
        #define TOOLCHAIN_OS "Linux"
        #if !defined (TOOLCHAIN_OS_Linux)
            #define TOOLCHAIN_OS_Linux
        #endif // !defined (TOOLCHAIN_OS_Linux)
    #elif defined (__APPLE__)
        #define TOOLCHAIN_OS "OSX"
        #if !defined (TOOLCHAIN_OS_OSX)
            #define TOOLCHAIN_OS_OSX
        #endif // !defined (TOOLCHAIN_OS_OSX)
    #elif defined (__sun)
        #define TOOLCHAIN_OS "Solaris"
        #if !defined (TOOLCHAIN_OS_Solaris)
            #define TOOLCHAIN_OS_Solaris
        #endif // !defined (TOOLCHAIN_OS_Solaris)
    #elif defined (_AIX)
        #define TOOLCHAIN_OS "AIX"
        #if !defined (TOOLCHAIN_OS_AIX)
            #define TOOLCHAIN_OS_AIX
        #endif // !defined (TOOLCHAIN_OS_AIX)
    #elif defined (_hpux)
        #define TOOLCHAIN_OS "HPUX"
        #if !defined (TOOLCHAIN_OS_HPUX)
            #define TOOLCHAIN_OS_HPUX
        #endif // !defined (TOOLCHAIN_OS_HPUX)
    #elif defined (_OS2)
        #define TOOLCHAIN_OS "OS2"
        #if !defined (TOOLCHAIN_OS_OS2)
            #define TOOLCHAIN_OS_OS2
        #endif // !defined (TOOLCHAIN_OS_OS2)
    #elif defined (_sgi)
        #define TOOLCHAIN_OS "IRIX"
        #if !defined (TOOLCHAIN_OS_IRIX)
            #define TOOLCHAIN_OS_IRIX
        #endif // !defined (TOOLCHAIN_OS_IRIX)
    #else // defined (_WINDOWS_)
        #error Unknown TOOLCHAIN_OS.
    #endif // defined (_WINDOWS_)
#endif // !defined (TOOLCHAIN_OS)

namespace thekogans {
    namespace util {

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
            /// \brief
            /// Solaris.
            Solaris,
            /// \brief
            /// AIX.
            AIX,
            /// \brief
            /// HPUX.
            HPUX,
            /// \brief
            /// OS 2.
            OS2,
            /// \brief
            /// IRIX.
            IRIX,
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
        #elif defined (TOOLCHAIN_OS_Solaris)
            /// \brief
            /// Host OS is Solaris.
            HostOS = Solaris
        #elif defined (TOOLCHAIN_OS_AIX)
            /// \brief
            /// Host OS is AIX.
            HostOS = AIX
        #elif defined (TOOLCHAIN_HPUX)
            /// \brief
            /// Host OS is HPUX.
            HostOS = HPUX
        #elif defined (TOOLCHAIN_OS_OS2)
            /// \brief
            /// Host OS is OS2.
            HostOS = OS2
        #elif defined (TOOLCHAIN_OS_IRIX)
            /// \brief
            /// Host OS is IRIX.
            HostOS = IRIX
        #else // defined (TOOLCHAIN_OS_Windows)
            #error "Unable to determine host OS."
        #endif // defined (TOOLCHAIN_OS_Windows)
        };

    } // namespace util
} // namespace thekogans

// TOOLCHAIN_ARCH
#if !defined (TOOLCHAIN_ARCH)
    #if defined (_M_X64) || defined (__x86_64__) || defined (__x86_64)
        #define TOOLCHAIN_ARCH "x86_64"
        #if !defined (TOOLCHAIN_ARCH_x86_64)
            #define TOOLCHAIN_ARCH_x86_64
        #endif // !defined (TOOLCHAIN_ARCH_x86_64)
    #elif defined (_M_IX86) || defined (__i386__)
        #define TOOLCHAIN_ARCH "i386"
        #if !defined (TOOLCHAIN_ARCH_i386)
            #define TOOLCHAIN_ARCH_i386
        #endif // !defined (TOOLCHAIN_ARCH_i386)
    #elif defined (_M_ARM64) || defined (__aarch64__) || defined (__arm64)
        #define TOOLCHAIN_ARCH "arm64"
        #if !defined (TOOLCHAIN_ARCH_arm64)
            #define TOOLCHAIN_ARCH_arm64
        #endif // !defined (TOOLCHAIN_ARCH_arm64)
    #elif defined (_M_ARM) || defined (__arm__)
        #define TOOLCHAIN_ARCH "arm32"
        #if !defined (TOOLCHAIN_ARCH_arm32)
            #define TOOLCHAIN_ARCH_arm32
        #endif // !defined (TOOLCHAIN_ARCH_arm32)
    #elif defined (__powerpc64__) || defined (__ppc64__)
        #define TOOLCHAIN_ARCH "ppc64"
        #if !defined (TOOLCHAIN_ARCH_ppc64)
            #define TOOLCHAIN_ARCH_ppc64
        #endif // !defined (TOOLCHAIN_ARCH_ppc64)
    #elif defined (__powerpc__) || defined (__ppc__)
        #define TOOLCHAIN_ARCH "ppc32"
        #if !defined (TOOLCHAIN_ARCH_ppc32)
            #define TOOLCHAIN_ARCH_ppc32
        #endif // !defined (TOOLCHAIN_ARCH_ppc32)
    #elif defined (__sparcv9)
        #define TOOLCHAIN_ARCH "sparc64"
        #if !defined (TOOLCHAIN_ARCH_sparc64)
            #define TOOLCHAIN_ARCH_sparc64
        #endif // !defined (TOOLCHAIN_ARCH_sparc64)
    #elif defined (__sparc)
        #define TOOLCHAIN_ARCH "sparc32"
        #if !defined (TOOLCHAIN_ARCH_sparc32)
            #define TOOLCHAIN_ARCH_sparc32
        #endif // !defined (TOOLCHAIN_ARCH_sparc32)
    #elif defined (__mips64)
        #define TOOLCHAIN_ARCH "mips64"
        #if !defined (TOOLCHAIN_ARCH_mips64)
            #define TOOLCHAIN_ARCH_mips64
        #endif // !defined (TOOLCHAIN_ARCH_mips64)
    #elif defined (__mips__)
        #define TOOLCHAIN_ARCH "mips32"
        #if !defined (TOOLCHAIN_ARCH_mips32)
            #define TOOLCHAIN_ARCH_mips32
        #endif // !defined (TOOLCHAIN_ARCH_mips32)
    #else
        #error Unknown TOOLCHAIN_ARCH.
    #endif // defined (_M_X64) || defined (__x86_64__) || defined (__x86_64)
#endif // !defined (TOOLCHAIN_ARCH)

namespace thekogans {
    namespace util {

        /// \enum
        /// Arch constants.
        enum Arch {
            /// \brief
            /// i386.
            i386,
            /// \brief
            /// x86_64.
            x86_64,
            /// \brief
            /// arm32.
            arm32,
            /// \brief
            /// arm64.
            arm64,
            /// \brief
            /// ppc32.
            ppc32,
            /// \brief
            /// ppc64.
            ppc64,
            /// \brief
            /// sparc32.
            sparc32,
            /// \brief
            /// sparc64.
            sparc64,
            /// \brief
            /// mips32.
            mips32,
            /// \brief
            /// mips64.
            mips64,
        #if defined (TOOLCHAIN_ARCH_i386)
            /// \brief
            /// Host Arch is i386.
            HostArch = i386
        #elif defined (TOOLCHAIN_ARCH_x86_64)
            /// \brief
            /// Host Arch is x86_64.
            HostArch = x86_64
        #elif defined (TOOLCHAIN_ARCH_arm32)
            /// \brief
            /// Host Arch is arm32.
            HostArch = arm32
        #elif defined (TOOLCHAIN_ARCH_arm64)
            /// \brief
            /// Host Arch is arm64.
            HostArch = arm64
        #elif defined (TOOLCHAIN_ARCH_ppc32)
            /// \brief
            /// Host Arch is ppc32.
            HostArch = ppc32
        #elif defined (TOOLCHAIN_ARCH_ppc64)
            /// \brief
            /// Host Arch is ppc64.
            HostArch = ppc64
        #elif defined (TOOLCHAIN_ARCH_sparc32)
            /// \brief
            /// Host Arch is sparc32.
            HostArch = sparc32
        #elif defined (TOOLCHAIN_ARCH_sparc64)
            /// \brief
            /// Host Arch is sparc64.
            HostArch = sparc64
        #elif defined (TOOLCHAIN_ARCH_mips32)
            /// \brief
            /// Host Arch is mips32.
            HostArch = mips32
        #elif defined (TOOLCHAIN_ARCH_mips64)
            /// \brief
            /// Host Arch is mips64.
            HostArch = mips64
        #else // defined (TOOLCHAIN_ARCH_i386)
            #error Unknown TOOLCHAIN_ARCH.
        #endif // defined (TOOLCHAIN_ARCH_i386)
        };

    } // namespace util
} // namespace thekogans

// TOOLCHAIN_ARCH_WORD_SIZE
#if !defined (TOOLCHAIN_ARCH_WORD_SIZE)
    #if defined (TOOLCHAIN_ARCH_x86_64) || defined (TOOLCHAIN_ARCH_arm64) ||\
        defined (TOOLCHAIN_ARCH_ppc64) || defined (TOOLCHAIN_ARCH_sparc64) ||\
        defined (TOOLCHAIN_ARCH_mips64)
        #define TOOLCHAIN_ARCH_WORD_SIZE 8
    #elif defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_arm32) ||\
        defined (TOOLCHAIN_ARCH_ppc32) || defined (TOOLCHAIN_ARCH_sparc32) ||\
        defined (TOOLCHAIN_ARCH_mips32)
        #define TOOLCHAIN_ARCH_WORD_SIZE 4
    #endif
#endif // !defined (TOOLCHAIN_ARCH_WORD_SIZE)

// TOOLCHAIN_COMPILER
#if !defined (TOOLCHAIN_COMPILER)
    #if defined (_MSC_VER)
        #define TOOLCHAIN_COMPILER "cl"
        #if !defined (TOOLCHAIN_COMPILER_cl)
            #define TOOLCHAIN_COMPILER_cl
        #endif // !defined (TOOLCHAIN_COMPILER_cl)
    #elif defined (__clang__)
        #define TOOLCHAIN_COMPILER "clang"
        #if !defined (TOOLCHAIN_COMPILER_clang)
            #define TOOLCHAIN_COMPILER_clang
        #endif // !defined (TOOLCHAIN_COMPILER_clang)
    #elif defined (__GNUC__)
        #define TOOLCHAIN_COMPILER "gcc"
        #if !defined (TOOLCHAIN_COMPILER_gcc)
            #define TOOLCHAIN_COMPILER_gcc
        #endif // !defined (TOOLCHAIN_COMPILER_gcc)
    #elif defined (__INTEL_COMPILER)
        #define TOOLCHAIN_COMPILER "icc"
        #if !defined (TOOLCHAIN_COMPILER_icc)
            #define TOOLCHAIN_COMPILER_icc
        #endif // !defined (TOOLCHAIN_COMPILER_icc)
    #else
        #error Unknown TOOLCHAIN_COMPILER.
    #endif // defined (_MSC_VER)
#endif // !defined (TOOLCHAIN_COMPILER)

// TOOLCHAIN_BRANCH
#if !defined (TOOLCHAIN_BRANCH)
    #define TOOLCHAIN_BRANCH TOOLCHAIN_OS "/" TOOLCHAIN_ARCH "/" TOOLCHAIN_COMPILER
#endif // !defined (TOOLCHAIN_BRANCH)

// TOOLCHAIN_TRIPLET
#if !defined (TOOLCHAIN_TRIPLET)
    #define TOOLCHAIN_TRIPLET TOOLCHAIN_OS "-" TOOLCHAIN_ARCH "-" TOOLCHAIN_COMPILER
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
