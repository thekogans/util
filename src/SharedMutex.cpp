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
    #include "thekogans/util/TimeSpec.h"
    #include "thekogans/util/POSIXUtils.h"
    #include "thekogans/util/LockGuard.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Exception.h"
#include "thekogans/util/SharedMutex.h"

namespace thekogans {
    namespace util {

    #if !defined (TOOLCHAIN_OS_Windows)
        struct SharedMutex::SharedMutexImpl : public SharedObject<SharedMutexImpl> {
            pthread_mutex_t mutex;

            SharedMutexImpl (const char *name) :
                    SharedObject (name) {
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
        };

        struct SharedMutex::SharedMutexImplCreator :
                public SharedObject<SharedMutex::SharedMutexImpl>::Creator {
            virtual SharedMutex::SharedMutexImpl *operator () (
                    void *ptr,
                    const char *name) const {
                return new (ptr) SharedMutex::SharedMutexImpl (name);
            }
        };

        struct SharedMutex::SharedMutexImplDestructor :
                public SharedObject<SharedMutex::SharedMutexImpl>::Destructor {
            virtual void operator () (SharedMutex::SharedMutexImpl *mutex) const {
                mutex->~SharedMutexImpl ();
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
                mutex (
                    SharedMutexImpl::Create (
                        name,
                        SharedMutexImplCreator ())) {
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
            SharedMutexImpl::Destroy (
                mutex,
                SharedMutexImplDestructor ());
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
