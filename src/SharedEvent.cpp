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
    #include "thekogans/util/LockGuard.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Exception.h"
#include "thekogans/util/SharedEvent.h"

namespace thekogans {
    namespace util {

    #if !defined (TOOLCHAIN_OS_Windows)
        struct SharedEvent::SharedEventImpl {
            char name[NAME_MAX];
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
            ui32 refCount;

            SharedEventImpl (
                    const char *name_,
                    bool manualReset_) :
                    manualReset (manualReset_),
                    state (SharedEvent::Free),
                    condition (mutex),
                    refCount (1) {
                strncpy (name, name_, NAME_MAX);
            }

            ui32 AddRef () {
                LockGuard<Mutex> guard (mutex);
                return ++refCount;
            }

            ui32 Release () {
                LockGuard<Mutex> guard (mutex);
                return --refCount;
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

            struct Lock {
                std::string name;
                int fd;
                Lock (const char *name_) :
                        name (std::string ("/tmp/") + name_),
                        fd (open (name.c_str (), O_RDWR | O_CREAT | O_EXCL, 0666/*S_IRUSR | S_IWUSR*/)) {
                    while (fd == -1) {
                        THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                        if (errorCode != EEXIST) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                        }
                        Sleep (TimeSpec::FromMilliseconds (100));
                        fd = open (name.c_str (), O_RDWR | O_CREAT | O_EXCL, 0666/*S_IRUSR | S_IWUSR*/);
                    }
                }
                ~Lock () {
                    close (fd);
                    unlink (name.c_str ());
                }
            };

            static SharedEventImpl *Create (
                    const char *name,
                    bool manualReset,
                    SharedEvent::State initialState) {
                if (name != 0) {
                    Lock lock (name);
                    struct File {
                        int fd;
                        bool owner;
                        File (const char *name) :
                                fd (shm_open (name, O_RDWR | O_CREAT | O_EXCL, 0666/*S_IRUSR | S_IWUSR*/)),
                                owner (fd != -1) {
                            if (fd != -1) {
                                if (FTRUNCATE_FUNC (fd, sizeof (SharedEventImpl)) == -1) {
                                    THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                                    close (fd);
                                    shm_unlink (name);
                                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                                }
                            }
                            else {
                                THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                                if (errorCode == EEXIST) {
                                    fd = shm_open (name, O_RDWR);
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
                    void *ptr = mmap (0, sizeof (SharedEventImpl),
                        PROT_READ | PROT_WRITE, MAP_SHARED, file.fd, 0);
                    if (ptr != 0) {
                        SharedEventImpl *event;
                        if (file.owner) {
                            event = new (ptr) SharedEventImpl (name, manualReset);
                            if (initialState == SharedEvent::Signalled) {
                                event->Signal ();
                            }
                        }
                        else {
                            event = (SharedEventImpl *)ptr;
                            event->AddRef ();
                        }
                        return event;
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

            static void Destroy (SharedEventImpl *event) {
                Lock lock (event->name);
                if (event->Release () == 0) {
                    event->~SharedEventImpl ();
                    shm_unlink (event->name);
                }
                munmap (event, sizeof (SharedEventImpl));
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
                event (SharedEventImpl::Create (name, manualReset, initialState)) {
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
