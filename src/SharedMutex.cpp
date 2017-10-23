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
#else // defined (TOOLCHAIN_OS_Windows)
    #include <sys/mman.h>
    #include "thekogans/util/TimeSpec.h"
    #include "thekogans/util/LockGuard.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Exception.h"
#include "thekogans/util/SharedMutex.h"

namespace thekogans {
    namespace util {

    #if !defined (TOOLCHAIN_OS_Windows)
        struct SharedMutex::SharedMutexImpl {
            char name[NAME_MAX];
            pthread_mutex_t mutex;
            ui32 refCount;

            SharedMutexImpl (const char *name_) :
                    refCount (1) {
                strncpy (name, name_, NAME_MAX);
                struct Attribute {
                    pthread_mutexattr_t attribute;
                    Attribute () {
                        {
                            THEKOGANS_UTIL_ERROR_CODE errorCode =
                                pthread_mutexattr_init (&attribute);
                            if (errorCode != 0) {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                            }
                        }
                        {
                            THEKOGANS_UTIL_ERROR_CODE errorCode =
                                pthread_mutexattr_setpshared (&attribute, PTHREAD_PROCESS_SHARED);
                            if (errorCode != 0) {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                            }
                        }
                    }
                    ~Attribute () {
                        pthread_mutexattr_destroy (&attribute);
                    }
                } attribute;
                THEKOGANS_UTIL_ERROR_CODE errorCode =
                    pthread_mutex_init (&mutex, &attribute.attribute);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            }

            ~SharedMutexImpl () {
                pthread_mutex_destroy (&mutex);
            }

            bool TryAcquire () {
                return pthread_mutex_trylock (&mutex) == 0;
            }

            void Acquire () {
                THEKOGANS_UTIL_ERROR_CODE errorCode =
                    pthread_mutex_lock (&mutex);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            }

            void Release () {
                THEKOGANS_UTIL_ERROR_CODE errorCode =
                    pthread_mutex_unlock (&mutex);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            }

            struct Lock {
                std::string name;
                int fd;
                Lock (const char *name_) :
                        name (std::string ("/tmp/") + name_),
                        fd (open (name.c_str (), O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) {
                    while (fd == -1) {
                        THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                        if (errorCode != EEXIST) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                        }
                        Sleep (TimeSpec::FromMilliseconds (100));
                        fd = open (name.c_str (), O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
                    }
                }
                ~Lock () {
                    close (fd);
                    unlink (name.c_str ());
                }
            };

            static SharedMutexImpl *Create (const char *name) {
                if (name != 0) {
                    Lock lock (name);
                    struct File {
                        int fd;
                        bool owner;
                        File (const char *name) :
                                fd (shm_open (name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)),
                                owner (fd != -1) {
                            if (fd != -1) {
                                if (FTRUNCATE_FUNC (fd, sizeof (SharedMutexImpl)) == -1) {
                                    THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                                    close (fd);
                                    shm_unlink (name);
                                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                                }
                            }
                            else {
                                THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                                if (errorCode == EEXIST) {
                                    fd = shm_open (name, O_RDWR, S_IRUSR | S_IWUSR);
                                    if (fd != -1) {
                                        owner = false;
                                    }
                                    else {
                                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                            THEKOGANS_UTIL_OS_ERROR_CODE);
                                    }
                                }
                                else {
                                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                                }
                            }
                        }
                        ~File () {
                            close (fd);
                        }
                    } file (name);
                    void *ptr = mmap (0, sizeof (SharedMutexImpl),
                        PROT_READ | PROT_WRITE, MAP_SHARED, file.fd, 0);
                    if (ptr != 0) {
                        SharedMutexImpl *mutex;
                        if (file.owner) {
                            mutex = new (ptr) SharedMutexImpl (name);
                        }
                        else {
                            mutex = (SharedMutexImpl *)ptr;
                            ++mutex->refCount;
                        }
                        return mutex;
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }

            static void Destroy (SharedMutexImpl *mutex) {
                Lock lock (mutex->name);
                if (--mutex->refCount == 0) {
                    mutex->~SharedMutexImpl ();
                    shm_unlink (mutex->name);
                }
                munmap (mutex, sizeof (SharedMutexImpl));
            }
        };
    #endif // !defined (TOOLCHAIN_OS_Windows)

        SharedMutex::SharedMutex (const char *name) :
        #if defined (TOOLCHAIN_OS_Windows)
                handle (CreateMutex (0, FALSE, name)) {
            if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
                mutex (SharedMutexImpl::Create (name)) {
            if (mutex == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_ENOMEM);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        SharedMutex::~SharedMutex () {
        #if defined (TOOLCHAIN_OS_Windows)
            CloseHandle (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            SharedMutexImpl::Destroy (mutex);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        bool SharedMutex::TryAcquire () {
        #if defined (TOOLCHAIN_OS_Windows)
            return TryEnterCriticalSection (&cs) == TRUE;
        #else // defined (TOOLCHAIN_OS_Windows)
            return mutex->TryAcquire ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void SharedMutex::Acquire () {
        #if defined (TOOLCHAIN_OS_Windows)
            EnterCriticalSection (&cs);
        #else // defined (TOOLCHAIN_OS_Windows)
            mutex->Acquire ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void SharedMutex::Release () {
        #if defined (TOOLCHAIN_OS_Windows)
            if (!ReleaseMutex (handle)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            mutex->Release ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
