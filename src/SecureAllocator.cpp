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

#include "thekogans/util/Environment.h"

#if defined (TOOLCHAIN_OS_Windows) || \
    defined (THEKOGANS_UTIL_HAVE_MMAP) || \
    defined (THEKOGANS_UTIL_USE_DEFAULT_SECURE_ALLOCATOR)

#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
#else // defined (TOOLCHAIN_OS_Windows)
    #if defined (TOOLCHAIN_OS_Linux)
        #include <sys/resource.h>
    #endif // defined (TOOLCHAIN_OS_Linux)
    #include <sys/mman.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Exception.h"
#if !defined (THEKOGANS_UTIL_HAVE_MMAP)
    #include "thekogans/util/DefaultAllocator.h"
#endif // !defined (THEKOGANS_UTIL_HAVE_MMAP)
#include "thekogans/util/SecureAllocator.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_ALLOCATOR (SecureAllocator)

        SecureAllocator &SecureAllocator::Instance () {
            static SecureAllocator *instance = new SecureAllocator;
            return *instance;
        }

        void SecureAllocator::ReservePages (
                ui64 minWorkingSetSize,
                ui64 maxWorkingSetSize) {
        #if defined (TOOLCHAIN_OS_Windows)
            if (!SetProcessWorkingSetSize (GetCurrentProcess (),
                    (SIZE_T)minWorkingSetSize, (SIZE_T)maxWorkingSetSize)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
        #if defined (THEKOGANS_UTIL_HAVE_MMAP)
            rlimit limit;
            limit.rlim_cur = (rlim_t)minWorkingSetSize;
            limit.rlim_max = (rlim_t)maxWorkingSetSize;
            if (setrlimit (RLIMIT_MEMLOCK, &limit) != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #endif // defined (THEKOGANS_UTIL_HAVE_MMAP)
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void *SecureAllocator::Alloc (std::size_t size) {
            void *ptr = 0;
            if (size > 0) {
            #if defined (TOOLCHAIN_OS_Windows)
                ptr = VirtualAlloc (0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                if (ptr != 0) {
                    if (!VirtualLock (ptr, size)) {
                        // Grab the error code in case VirtualFree clears it.
                        THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                        VirtualFree (ptr, 0, MEM_RELEASE);
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #else // defined (TOOLCHAIN_OS_Windows)
            #if defined (THEKOGANS_UTIL_HAVE_MMAP)
                ptr = mmap (0, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE,
                    THEKOGANS_UTIL_INVALID_HANDLE_VALUE, 0);
                if (ptr != MAP_FAILED) {
                    if (mlock (ptr, size) == 0) {
                    #if defined (MADV_DONTDUMP)
                        if (madvise (ptr, size, MADV_DONTDUMP) != 0) {
                            // Grab the error code in case munlock/munmap clears it.
                            THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                            munlock (ptr, size);
                            munmap (ptr, size);
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                        }
                    #endif // defined (MADV_DONTDUMP)
                    }
                    else {
                        // Grab the error code in case munmap clears it.
                        THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                        munmap (ptr, size);
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #elif defined (THEKOGANS_UTIL_USE_DEFAULT_SECURE_ALLOCATOR)
                ptr = DefaultAllocator::Instance ().Alloc (size);
            #endif // defined (THEKOGANS_UTIL_HAVE_MMAP)
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            return ptr;
        }

    #if !defined (TOOLCHAIN_OS_Windows)
        namespace {
            void SecureZeroMemory (
                    void *data,
                    std::size_t size) {
                volatile ui8 *ptr = (volatile ui8 *)data;
                while (size-- != 0) {
                    *ptr++ = 0;
                }
            }
        }
    #endif // !defined (TOOLCHAIN_OS_Windows)

        void SecureAllocator::Free (
                void *ptr,
                std::size_t size) {
            if (ptr != 0) {
                SecureZeroMemory (ptr, size);
            #if defined (TOOLCHAIN_OS_Windows)
                if (!VirtualUnlock (ptr, size) || !VirtualFree (ptr, 0, MEM_RELEASE)) {
            #else // defined (TOOLCHAIN_OS_Windows)
            #if defined (THEKOGANS_UTIL_HAVE_MMAP)
                if (munlock (ptr, size) != 0 || munmap (ptr, size) != 0) {
            #elif defined (THEKOGANS_UTIL_USE_DEFAULT_SECURE_ALLOCATOR)
                DefaultAllocator::Instance ().Free (ptr, size);
                if (0) {
            #else // defined (THEKOGANS_UTIL_USE_DEFAULT_SECURE_ALLOCATOR)
                // At this point we know that ptr could not possibly
                // have been allocated by the SecureAllocator.
                THEKOGANS_UTIL_OS_ERROR_CODE = EINVAL;
                if (1) {
            #endif // defined (THEKOGANS_UTIL_HAVE_MMAP)
            #endif // defined (TOOLCHAIN_OS_Windows)
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
        }

    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Windows) ||
       // defined (THEKOGANS_UTIL_HAVE_MMAP) ||
       // defined (THEKOGANS_UTIL_USE_DEFAULT_SECURE_ALLOCATOR)
