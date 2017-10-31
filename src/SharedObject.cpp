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

#if !defined (TOOLCHAIN_OS_Windows)
    #include <sys/mman.h>
    #include <fcntl.h>
#endif // !defined (TOOLCHAIN_OS_Windows)
#include <cstring>
#include "thekogans/util/Exception.h"
#if defined (TOOLCHAIN_OS_Linux)
    #include "thekogans/util/LinuxUtils.h"
#elif defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/OSXUtils.h"
#endif // defined (TOOLCHAIN_OS_Linux)
#include "thekogans/util/SharedObject.h"

namespace thekogans {
    namespace util {

        namespace {
            struct SharedObjectHeader {
                char name[NAME_MAX];
                ui64 size;
                bool secure;
                ui32 refCount;

                SharedObjectHeader (
                        const char *name_,
                        ui64 size_,
                        bool secure_) :
                        size (size_),
                        secure (secure_),
                        refCount (1) {
                    if (name_ != 0 && size > 0) {
                        strncpy (name, name_, NAME_MAX);
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }
            };
        }

    #if !defined (TOOLCHAIN_OS_Windows)
        void SharedObject::Cleanup (const char *name) {
            if (name != 0) {
                shm_unlink (name);
                shm_unlink (Lock::GetName (name).c_str ());
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        namespace {
            bool LockRegion (
                    void *ptr,
                    ui64 size) {
                if (mlock (ptr, size) == 0) {
                #if defined (MADV_DONTDUMP)
                    if (madvise (ptr, size, MADV_DONTDUMP) == 0) {
                #endif // defined (MADV_DONTDUMP)
                        return true;
                #if defined (MADV_DONTDUMP)
                    }
                    else {
                        THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                        munlock (ptr, size);
                        THEKOGANS_UTIL_OS_ERROR_CODE = errorCode;
                    }
                #endif // defined (MADV_DONTDUMP)
                }
                return false;
            }
        }
    #endif // !defined (TOOLCHAIN_OS_Windows)

        void *SharedObject::Create (
                const char *name,
                ui64 size,
                bool secure,
                const Constructor &constructor,
            #if defined (TOOLCHAIN_OS_Windows)
                LPSECURITY_ATTRIBUTES securityAttributes,
            #else // defined (TOOLCHAIN_OS_Windows)
                mode_t mode,
            #endif // defined (TOOLCHAIN_OS_Windows)
                const TimeSpec &timeSpec) {
            if (name != 0 && size > 0) {
                size += sizeof (SharedObjectHeader);
            #if defined (TOOLCHAIN_OS_Windows)
                Lock lock (name, securityAttributes, timeSpec);
                struct SharedMemory {
                    THEKOGANS_UTIL_HANDLE handle;
                    THEKOGANS_UTIL_ERROR_CODE errorCode;
                    bool created;
                    SharedMemory (
                            const char *name,
                            ui64 size,
                            LPSECURITY_ATTRIBUTES securityAttributes) :
                            handle (
                                CreateFileMapping (
                                    INVALID_HANDLE_VALUE,
                                    securityAttributes,
                                    PAGE_READWRITE,
                                    THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (size, 0),
                                    THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (size, 1),
                                    name)),
                            errorCode (THEKOGANS_UTIL_OS_ERROR_CODE),
                            created (errorCode != ERROR_ALREADY_EXISTS) {
                        if (handle == 0) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                        }
                    }
                    ~SharedMemory () {
                        CloseHandle (handle);
                    }
                } sharedMemory (name, size, securityAttributes);
                void *ptr = MapViewOfFile (sharedMemory.handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
                if (ptr != 0) {
                    if (!secure || VirtualLock (ptr, size)) {
                        if (sharedMemory.created) {
                            THEKOGANS_UTIL_TRY {
                                ptr = constructor (new (ptr) SharedObjectHeader (name, size, secure) + 1);
                            }
                            THEKOGANS_UTIL_CATCH_ANY {
                                UnmapViewOfFile (ptr);
                                throw;
                            }
                        }
                        else {
                            SharedObjectHeader *sharedObject = (SharedObjectHeader *)ptr;
                            ++sharedObject->refCount;
                            ptr = sharedObject + 1;
                        }
                        return ptr;
                    }
                    else {
                        THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                        UnmapViewOfFile (ptr);
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                Lock lock (name, mode, timeSpec);
                struct SharedMemory {
                    const char *name;
                    THEKOGANS_UTIL_HANDLE handle;
                    bool created;
                    SharedMemory (
                            const char *name_,
                            ui64 size,
                            mode_t mode) :
                            name (name_),
                            handle (shm_open (name, O_RDWR | O_CREAT | O_EXCL, mode)),
                            created (handle != THEKOGANS_UTIL_POSIX_INVALID_HANDLE_VALUE) {
                        if (created) {
                            if (FTRUNCATE_FUNC (handle, size) == -1) {
                                THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                                close (handle);
                                shm_unlink (name);
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                            }
                        }
                        else {
                            THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                            if (errorCode == EEXIST) {
                                handle = shm_open (name, O_RDWR);
                                if (handle == THEKOGANS_UTIL_POSIX_INVALID_HANDLE_VALUE) {
                                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                        THEKOGANS_UTIL_OS_ERROR_CODE);
                                }
                            }
                            else {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                            }
                        }
                    }
                    ~SharedMemory () {
                        close (handle);
                    }

                    void Destroy () {
                        if (created) {
                            shm_unlink (name);
                        }
                    }
                } sharedMemory (name, size, mode);
                void *ptr = mmap (0, size, PROT_READ | PROT_WRITE,
                    MAP_SHARED, sharedMemory.handle, 0);
                if (ptr != MAP_FAILED) {
                    if (!secure || LockRegion (ptr, size)) {
                        if (sharedMemory.created) {
                            THEKOGANS_UTIL_TRY {
                                ptr = constructor (new (ptr) SharedObjectHeader (name, size, secure) + 1);
                            }
                            THEKOGANS_UTIL_CATCH_ANY {
                                munmap (ptr, size);
                                sharedMemory.Destroy ();
                                throw;
                            }
                        }
                        else {
                            SharedObjectHeader *sharedObject = (SharedObjectHeader *)ptr;
                            ++sharedObject->refCount;
                            ptr = sharedObject + 1;
                        }
                        return ptr;
                    }
                    else {
                        THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                        munmap (ptr, size);
                        sharedMemory.Destroy ();
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
                else {
                    THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                    sharedMemory.Destroy ();
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void SharedObject::Destroy (
                void *ptr,
                const Destructor &destructor,
            #if defined (TOOLCHAIN_OS_Windows)
                LPSECURITY_ATTRIBUTES securityAttributes,
            #else // defined (TOOLCHAIN_OS_Windows)
                mode_t mode,
            #endif // defined (TOOLCHAIN_OS_Windows)
                const TimeSpec &timeSpec) {
            SharedObjectHeader *sharedObject = (SharedObjectHeader *)ptr - 1;
            ui64 size = sharedObject->size;
            bool secure = sharedObject->secure;
        #if defined (TOOLCHAIN_OS_Windows)
            Lock lock (sharedObject->name, securityAttributes, timeSpec);
            if (--sharedObject->refCount == 0) {
                destructor (sharedObject);
            }
            if (secure) {
                VirtualUnlock (sharedObject, size);
            }
            UnmapViewOfFile (sharedObject);
        #else // defined (TOOLCHAIN_OS_Windows)
            Lock lock (sharedObject->name, mode, timeSpec);
            if (--sharedObject->refCount == 0) {
                destructor (sharedObject);
                shm_unlink (sharedObject->name);
            }
            if (secure) {
                munlock (sharedObject, size);
            }
            munmap (sharedObject, size);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        SharedObject::Lock::Lock (
                const char *name_,
            #if defined (TOOLCHAIN_OS_Windows)
                LPSECURITY_ATTRIBUTES securityAttributes,
            #else // defined (TOOLCHAIN_OS_Windows)
                mode_t mode,
            #endif // defined (TOOLCHAIN_OS_Windows)
                const TimeSpec &timeSpec) :
            #if defined (TOOLCHAIN_OS_Windows)
                handle (
                    CreateFileMapping (
                        INVALID_HANDLE_VALUE, securityAttributes,
                        PAGE_READWRITE, 0, 1, GetName (name_).c_str ())) {
            #else // defined (TOOLCHAIN_OS_Windows)
                name (GetName (name_)),
                handle (shm_open (name.c_str (), O_RDWR | O_CREAT | O_EXCL, mode)) {
            #endif // defined (TOOLCHAIN_OS_Windows)
        #if defined (TOOLCHAIN_OS_Windows)
            for (THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                    errorCode != ERROR_SUCCESS;
                    errorCode = THEKOGANS_UTIL_OS_ERROR_CODE) {
                if (errorCode == ERROR_ALREADY_EXISTS) {
                    CloseHandle (handle);
                    Sleep (timeSpec);
                    handle = CreateFileMapping (
                        INVALID_HANDLE_VALUE, securityAttributes,
                        PAGE_READWRITE, 0, 1, GetName (name_).c_str ());
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            while (handle == THEKOGANS_UTIL_POSIX_INVALID_HANDLE_VALUE) {
                THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                if (errorCode == EEXIST) {
                    Sleep (timeSpec);
                    handle = shm_open (name.c_str (), O_RDWR | O_CREAT | O_EXCL, mode);
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        SharedObject::Lock::~Lock () {
        #if defined (TOOLCHAIN_OS_Windows)
            CloseHandle (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            close (handle);
            shm_unlink (name.c_str ());
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        std::string SharedObject::Lock::GetName (const char *name) {
            return std::string (name) + "_lock";
        }

    } // namespace util
} // namespace thekogans
