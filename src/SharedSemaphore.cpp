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
#include "thekogans/util/SharedSemaphore.h"

namespace thekogans {
    namespace util {

    #if !defined (TOOLCHAIN_OS_Windows)
        struct SharedSemaphore::SharedSemaphoreImpl : public SharedObject<SharedSemaphoreImpl> {
            ui32 maxCount;
            volatile ui32 count;
            struct Mutex {
                pthread_mutex_t mutex;

                Mutex () {
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

                ~Mutex () {
                    pthread_mutex_destroy (&mutex);
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
            } mutex;
            struct Condition {
                Mutex &mutex;
                pthread_cond_t condition;

                Condition (Mutex &mutex_) :
                        mutex (mutex_) {
                    struct Attribute {
                        pthread_condattr_t attribute;
                        Attribute () {
                            {
                                THEKOGANS_UTIL_ERROR_CODE errorCode =
                                    pthread_condattr_init (&attribute);
                                if (errorCode != 0) {
                                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                                }
                            }
                            {
                                THEKOGANS_UTIL_ERROR_CODE errorCode =
                                    pthread_condattr_setpshared (&attribute, PTHREAD_PROCESS_SHARED);
                                if (errorCode != 0) {
                                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                                }
                            }
                        }
                        ~Attribute () {
                            pthread_condattr_destroy (&attribute);
                        }
                    } attribute;
                    THEKOGANS_UTIL_ERROR_CODE errorCode =
                        pthread_cond_init (&condition, &attribute.attribute);
                    if (errorCode != 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }

                ~Condition () {
                    pthread_cond_destroy (&condition);
                }

                void Wait () {
                    THEKOGANS_UTIL_ERROR_CODE errorCode =
                        pthread_cond_wait (&condition, &mutex.mutex);
                    if (errorCode != 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }

                void SignalAll () {
                    THEKOGANS_UTIL_ERROR_CODE errorCode =
                        pthread_cond_broadcast (&condition);
                    if (errorCode != 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
            } condition;

            SharedSemaphoreImpl (
                    const char *name,
                    ui32 maxCount_,
                    ui32 initialCount) :
                    SharedObject (name),
                    maxCount (maxCount_),
                    count (0),
                    condition (mutex) {
                if (initialCount != 0) {
                    Release (initialCount);
                }
            }

            void Acquire () {
                LockGuard<Mutex> guard (mutex);
                while (count == 0) {
                    condition.Wait ();
                }
                if (count > 0) {
                    --count;
                }
            }

            void Release (ui32 count_) {
                if (count_ != 0) {
                    LockGuard<Mutex> guard (mutex);
                    if (count + count_ <= maxCount) {
                        count += count_;
                        condition.SignalAll ();
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "count_ (%u) > available (%u)",
                            count_, maxCount - count);
                    }
                }
            }
        };

        struct SharedSemaphore::SharedSemaphoreImplCreator :
                public SharedObject<SharedSemaphore::SharedSemaphoreImpl>::Creator {
            ui32 maxCount;
            ui32 initialCount;

            SharedSemaphoreImplCreator (
                ui32 maxCount_,
                ui32 initialCount_) :
                maxCount (maxCount_),
                initialCount (initialCount_) {}

            virtual SharedSemaphore::SharedSemaphoreImpl *operator () (
                    void *ptr,
                    const char *name) const {
                return new (ptr) SharedSemaphore::SharedSemaphoreImpl (
                    name, maxCount, initialCount);
            }
        };
    #endif // !defined (TOOLCHAIN_OS_Windows)

        SharedSemaphore::SharedSemaphore (
                const char *name,
                ui32 maxCount,
                ui32 initialCount) :
            #if defined (TOOLCHAIN_OS_Windows)
                handle (CreateSemaphore (0, initialCount, maxCount, name)) {
            #else // defined (TOOLCHAIN_OS_Windows)
                semaphore (
                    SharedSemaphoreImpl::Create (
                        name,
                        SharedSemaphoreImplCreator (maxCount, initialCount))) {
            #endif // defined (TOOLCHAIN_OS_Windows)
            if (maxCount < initialCount) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        #if defined (TOOLCHAIN_OS_Windows)
            if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            if (semaphore == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_ENOMEM);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        SharedSemaphore::~SharedSemaphore () {
        #if defined (TOOLCHAIN_OS_Windows)
            CloseHandle (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            SharedSemaphoreImpl::Destroy (semaphore);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void SharedSemaphore::Acquire () {
        #if defined (TOOLCHAIN_OS_Windows)
            if (WaitForSingleObject (handle, INFINITE) != WAIT_OBJECT_0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            semaphore->Acquire ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void SharedSemaphore::Release (ui32 count) {
            if (count != 0) {
            #if defined (TOOLCHAIN_OS_Windows)
                if (!ReleaseSemaphore (handle, count, 0)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                semaphore->Release (count);
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        }

    } // namespace util
} // namespace thekogans
