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
    #include "thekogans/util/Mutex.h"
    #include "thekogans/util/Condition.h"
    #include "thekogans/util/POSIXUtils.h"
    #include "thekogans/util/LockGuard.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Exception.h"
#include "thekogans/util/Semaphore.h"

namespace thekogans {
    namespace util {

    #if !defined (TOOLCHAIN_OS_Windows)
        struct Semaphore::SemaphoreImpl {
            const ui32 maxCount;
            volatile ui32 count;
            bool shared;
            Mutex mutex;
            Condition condition;

            SemaphoreImpl (
                    ui32 maxCount_,
                    ui32 initialCount,
                    bool shared_) :
                    maxCount (maxCount_),
                    count (0),
                    shared (shared_),
                    mutex (shared),
                    condition (mutex, shared) {
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

        struct Semaphore::SharedSemaphoreImpl :
                public SharedObject<SharedSemaphoreImpl>,
                public SemaphoreImpl {
            SharedSemaphoreImpl (
                ui32 maxCount,
                ui32 initialCount,
                const char *name) :
                SharedObject (name),
                SemaphoreImpl (maxCount, initialCount, true) {}
        };

        struct Semaphore::SharedSemaphoreImplConstructor :
                public SharedObject<Semaphore::SharedSemaphoreImpl>::Constructor {
            ui32 maxCount;
            ui32 initialCount;

            SharedSemaphoreImplConstructor (
                ui32 maxCount_,
                ui32 initialCount_) :
                maxCount (maxCount_),
                initialCount (initialCount_) {}

            virtual Semaphore::SharedSemaphoreImpl *operator () (
                    void *ptr,
                    const char *name) const {
                return new (ptr) SharedSemaphoreImpl (maxCount, initialCount, name);
            }
        };

        struct Semaphore::SharedSemaphoreImplDestructor :
                public SharedObject<SharedSemaphoreImpl>::Destructor {
            virtual void operator () (SharedSemaphoreImpl *semaphore) const {
                semaphore->~SharedSemaphoreImpl ();
            }
        };
    #endif // !defined (TOOLCHAIN_OS_Windows)

        Semaphore::Semaphore (
                ui32 maxCount,
                ui32 initialCount,
                const char *name) :
            #if defined (TOOLCHAIN_OS_Windows)
                handle (CreateSemaphore (0, initialCount, maxCount, name)) {
            #else // defined (TOOLCHAIN_OS_Windows)
                semaphore (name == 0 ?
                    new SemaphoreImpl (maxCount, initialCount, name) :
                    SharedSemaphoreImpl::Create (
                        name,
                        SharedSemaphoreImplConstructor (maxCount, initialCount))) {
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

        Semaphore::~Semaphore () {
        #if defined (TOOLCHAIN_OS_Windows)
            CloseHandle (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            if (!semaphore->shared) {
                delete semaphore;
            }
            else {
                SharedSemaphoreImpl::Destroy (
                    (SharedSemaphoreImpl *)semaphore,
                    SharedSemaphoreImplDestructor ());
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Semaphore::Acquire () {
        #if defined (TOOLCHAIN_OS_Windows)
            if (WaitForSingleObject (handle, INFINITE) != WAIT_OBJECT_0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            semaphore->Acquire ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Semaphore::Release (ui32 count) {
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
