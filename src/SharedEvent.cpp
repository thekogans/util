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
    #include "thekogans/util/LockGuard.h"
    #include "thekogans/util/POSIXUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Exception.h"
#include "thekogans/util/SharedEvent.h"

namespace thekogans {
    namespace util {

    #if !defined (TOOLCHAIN_OS_Windows)
        struct SharedEvent::SharedEventImpl : public SharedObject<SharedEventImpl> {
            bool manualReset;
            volatile SharedEvent::State state;
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

                bool Wait (const TimeSpec &timeSpec) {
                    timespec absolute = (GetCurrentTime () + timeSpec).Totimespec ();
                    THEKOGANS_UTIL_ERROR_CODE errorCode =
                        pthread_cond_timedwait (&condition, &mutex.mutex, &absolute);
                    if (errorCode != 0) {
                        if (errorCode != ETIMEDOUT) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                        }
                        return false;
                    }
                    return true;
                }

                void Signal () {
                    THEKOGANS_UTIL_ERROR_CODE errorCode =
                        pthread_cond_signal (&condition);
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

            SharedEventImpl (
                    const char *name,
                    bool manualReset_,
                    SharedEvent::State initialState) :
                    SharedObject (name),
                    manualReset (manualReset_),
                    state (SharedEvent::Free),
                    condition (mutex) {
                if (initialState == SharedEvent::Signalled) {
                    Signal ();
                }
            }

            void Signal () {
                LockGuard<Mutex> guard (mutex);
                state = SharedEvent::Signalled;
                if (manualReset) {
                    condition.SignalAll ();
                }
                else {
                    condition.Signal ();
                }
            }

            void SignalAll () {
                LockGuard<Mutex> guard (mutex);
                state = SharedEvent::Signalled;
                condition.SignalAll ();
            }

            void Reset () {
                LockGuard<Mutex> guard (mutex);
                state = SharedEvent::Free;
            }

            void Wait () {
                LockGuard<Mutex> guard (mutex);
                while (state == SharedEvent::Free) {
                    condition.Wait ();
                }
                if (!manualReset) {
                    state = SharedEvent::Free;
                }
            }

            bool Wait (const TimeSpec &timeSpec) {
                LockGuard<Mutex> guard (mutex);
                TimeSpec now = GetCurrentTime ();
                TimeSpec deadline = now + timeSpec;
                while (state == SharedEvent::Free && deadline > now) {
                    if (!condition.Wait (deadline - now)) {
                        return false;
                    }
                    now = GetCurrentTime ();
                }
                if (!manualReset) {
                    state = SharedEvent::Free;
                }
                return true;
            }
        };

        struct SharedEvent::SharedEventImplCreator :
                public SharedObject<SharedEvent::SharedEventImpl>::Creator {
            bool manualReset;
            SharedEvent::State initialState;

            SharedEventImplCreator (
                bool manualReset_,
                SharedEvent::State initialState_) :
                manualReset (manualReset_),
                initialState (initialState_) {}

            virtual SharedEvent::SharedEventImpl *operator () (
                    void *ptr,
                    const char *name) const {
                return new (ptr) SharedEvent::SharedEventImpl (
                    name, manualReset, initialState);
            }
        };
    #endif // !defined (TOOLCHAIN_OS_Windows)

        SharedEvent::SharedEvent (
                const char *name,
                bool manualReset,
                State initialState) :
        #if defined (TOOLCHAIN_OS_Windows)
                handle (CreateEvent (0, manualReset ? TRUE : FALSE,
                    initialState == Signalled ? TRUE : FALSE, name)) {
            if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
                event (
                    SharedEventImpl::Create (
                        name,
                        SharedEventImplCreator (manualReset, initialState))) {
            if (event == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_ENOMEM);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        SharedEvent::~SharedEvent () {
        #if defined (TOOLCHAIN_OS_Windows)
            CloseHandle (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            SharedEventImpl::Destroy (event);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void SharedEvent::Signal () {
        #if defined (TOOLCHAIN_OS_Windows)
            SetEvent (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            event->Signal ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void SharedEvent::SignalAll () {
        #if defined (TOOLCHAIN_OS_Windows)
            PulseEvent (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            event->SignalAll ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void SharedEvent::Reset () {
        #if defined (TOOLCHAIN_OS_Windows)
            ResetEvent (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            event->Reset ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void SharedEvent::Wait () {
        #if defined (TOOLCHAIN_OS_Windows)
            if (WaitForSingleObject (handle, INFINITE) != WAIT_OBJECT_0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            event->Wait ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        bool SharedEvent::Wait (const TimeSpec &timeSpec) {
        #if defined (TOOLCHAIN_OS_Windows)
            DWORD result = WaitForSingleObject (
                handle, (DWORD)timeSpec.ToMilliseconds ());
            if (result != WAIT_OBJECT_0) {
                if (result != WAIT_TIMEOUT) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                return false;
            }
            return true;
        #else // defined (TOOLCHAIN_OS_Windows)
            return event->Wait (timeSpec);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
