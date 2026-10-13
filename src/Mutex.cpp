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
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
#else // defined (TOOLCHAIN_OS_Windows)
    #include <pthread.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Exception.h"
#include "thekogans/util/Mutex.h"

namespace thekogans {
    namespace util {

        struct Mutex::Impl {
            Type type;

        #if defined (TOOLCHAIN_OS_Windows)
            union {
                SRWLOCK srwLock;
                CRITICAL_SECTION criticalSection;
            } lock;
        #else // defined (TOOLCHAIN_OS_Windows)
            pthread_mutex_t mutex;
            std::atomic<pthread_t> ownerThread{pthread_t{}};
        #endif // defined (TOOLCHAIN_OS_Windows)

            explicit Impl (Type type_) :
                    type (type_) {
            #if defined (TOOLCHAIN_OS_Windows)
                if (type == Mutex::Recursive) {
                    InitializeCriticalSection (&lock.criticalSection);
                }
                else {
                    InitializeSRWLock (&lock.srwLock);
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                pthread_mutexattr_t attributes;
                THEKOGANS_UTIL_ERROR_CODE errorCode = pthread_mutexattr_init (&attributes);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
                if (type == Mutex::Recursive) {
                    pthread_mutexattr_settype (&attributes, PTHREAD_MUTEX_RECURSIVE);
                }
                else {
                    pthread_mutexattr_settype (&attributes, PTHREAD_MUTEX_NORMAL);
                }
                errorCode = pthread_mutex_init (&mutex, &attributes);
                pthread_mutexattr_destroy (&attributes);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            ~Impl () {
            #if defined (TOOLCHAIN_OS_Windows)
                if (type == Mutex::Recursive) {
                    DeleteCriticalSection (&lock.criticalSection);
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                pthread_mutex_destroy (&mutex);
            #endif // defined (TOOLCHAIN_OS_Windows)
            }

            bool TryAcquire () {
            #if defined (TOOLCHAIN_OS_Windows)
                return type == Mutex::Recursive ?
                    TryEnterCriticalSection (&lock.criticalSection) == TRUE :
                    TryAcquireSRWLockExclusive (&lock.srwLock) == TRUE;
            #else // defined (TOOLCHAIN_OS_Windows)
                return pthread_mutex_trylock (&mutex) == 0;
            #endif // defined (TOOLCHAIN_OS_Windows)
            }

            void Acquire () {
            #if defined (TOOLCHAIN_OS_Windows)
                if (type == Mutex::Recursive) {
                    EnterCriticalSection (&lock.criticalSection);
                }
                else {
                    AcquireSRWLockExclusive (&lock.srwLock);
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                pthread_t currentThread = pthread_self ();
                // RECURSION CHECK GATE:
                // If this is a normal non-recursive mutex and the current thread already owns it,
                // intercept the deadlock immediately and throw a loud exception.
                if (type == Mutex::Normal &&
                    pthread_equal (ownerThread.load (std::memory_order_relaxed), currentThread)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (THEKOGANS_UTIL_OS_ERROR_CODE_EDEADLK);
                }
                THEKOGANS_UTIL_ERROR_CODE errorCode = pthread_mutex_lock (&mutex);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
                // Update ownership invariants tracking
                if (type == Mutex::Normal) {
                    ownerThread.store (currentThread, std::memory_order_relaxed);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }

            void Release () {
            #if defined (TOOLCHAIN_OS_Windows)
                if (type == Mutex::Recursive) {
                    LeaveCriticalSection (&lock.criticalSection);
                }
                else {
                    ReleaseSRWLockExclusive (&lock.srwLock);
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                if (type == Mutex::Normal) {
                    // Clear owner tracking right before unlocking
                    ownerThread.store (pthread_t{}, std::memory_order_relaxed);
                }
                THEKOGANS_UTIL_ERROR_CODE errorCode = pthread_mutex_unlock (&mutex);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        };

        Mutex::Mutex (Type type) :
            impl (new Impl (type)) {}

        Mutex::~Mutex () = default;

        bool Mutex::TryAcquire () {
            THEKOGANS_UTIL_ASSERT (impl != nullptr, "Calling Mutex::TryAcquire on a moved object.");
            if (impl != nullptr) {
                impl->TryAcquire ();
            }
            return false;
        }

        void Mutex::Acquire () {
            THEKOGANS_UTIL_ASSERT (impl != nullptr, "Calling Mutex::Acquire on a moved object.");
            if (impl != nullptr) {
                impl->Acquire ();
            }
        }

        void Mutex::Release () {
            THEKOGANS_UTIL_ASSERT (impl != nullptr, "Calling Mutex::Release on a moved object.");
            if (impl != nullptr) {
                impl->Release ();
            }
        }

    } // namespace util
} // namespace thekogans
