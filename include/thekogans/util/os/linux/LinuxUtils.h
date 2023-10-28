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

#if !defined (__thekogans_util_os_linux_LinuxUtils_h)
#define __thekogans_util_os_linux_LinuxUtils_h

#include "thekogans/util/Environment.h"

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

#if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_ppc32) ||\
    defined (TOOLCHAIN_ARCH_arm32) || defined (TOOLCHAIN_ARCH_mips32)
    #define STAT_STRUCT struct stat
    #define STAT_FUNC stat
    #define LSTAT_FUNC lstat
    #define FSTAT_FUNC fstat
    #define LSEEK_FUNC lseek
    #define FTRUNCATE_FUNC ftruncate
#elif defined (TOOLCHAIN_ARCH_x86_64) || defined (TOOLCHAIN_ARCH_ppc64) ||\
    defined (TOOLCHAIN_ARCH_arm64) || defined (TOOLCHAIN_ARCH_mips64)
    #define STAT_STRUCT struct stat64
    #define STAT_FUNC stat64
    #define LSTAT_FUNC lstat64
    #define FSTAT_FUNC fstat64
    #define LSEEK_FUNC lseek64
    #define FTRUNCATE_FUNC ftruncate64
#else // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_ppc32) ||
      // defined (TOOLCHAIN_ARCH_arm32) || defined (TOOLCHAIN_ARCH_mips32)
    #error Unknown TOOLCHAIN_ARCH.
#endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_ppc32) ||
       // defined (TOOLCHAIN_ARCH_arm32) || defined (TOOLCHAIN_ARCH_mips32)

#endif // defined (TOOLCHAIN_OS_Linux)

#endif // !defined (__thekogans_util_os_linux_LinuxUtils_h)
