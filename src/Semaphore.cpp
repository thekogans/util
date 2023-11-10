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
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
        #include <windows.h>
    #endif // !defined (_WINDOWS_)
    #include "thekogans/util/os/windows/WindowsUtils.h"
#else // defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/Heap.h"
    #include "thekogans/util/SpinLock.h"
    #include "thekogans/util/Mutex.h"
    #include "thekogans/util/Condition.h"
    #include "thekogans/util/SharedObject.h"
    #include "thekogans/util/LockGuard.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Exception.h"
#include "thekogans/util/Semaphore.h"

namespace thekogans {
    namespace util {

    #if !defined (TOOLCHAIN_OS_Windows)
        struct Semaphore::SemaphoreImpl {
            THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (SemaphoreImpl, SpinLock)

            const ui32 maxCount;
            volatile ui32 count;
            bool shared;
            Mutex mutex;
            Condition condition;

            SemaphoreImpl (
                    ui32 maxCount_,
                    ui32 initialCount,
                    bool shared_ = false) :
                    maxCount (maxCount_),
                    count (0),
                    shared (shared_),
                    mutex (shared),
                    condition (mutex, shared) {
                if (initialCount != 0) {
                    Release (initialCount);
                }
            }

            bool Acquire (const TimeSpec &timeSpec) {
                LockGuard<Mutex> guard (mutex);
                if (timeSpec == TimeSpec::Infinite) {
                    while (count == 0) {
                        condition.Wait ();
                    }
                }
                else {
                    TimeSpec now = GetCurrentTime ();
                    TimeSpec deadline = now + timeSpec;
                    while (count == 0 && deadline > now) {
                        if (!condition.Wait (deadline - now)) {
                            return false;
                        }
                        now = GetCurrentTime ();
                    }
                }
                if (count > 0) {
                    --count;
                    return true;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Unable to acquire the semaphore, count == %u.",
                        count);
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

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (Semaphore::SemaphoreImpl, SpinLock)

        struct Semaphore::SemaphoreImplConstructor : public SharedObject::Constructor {
            ui32 maxCount;
            ui32 initialCount;

            SemaphoreImplConstructor (
                ui32 maxCount_,
                ui32 initialCount_) :
                maxCount (maxCount_),
                initialCount (initialCount_) {}

            virtual void *operator () (void *ptr) const {
                return new (ptr) SemaphoreImpl (maxCount, initialCount, true);
            }
        };

        struct Semaphore::SemaphoreImplDestructor : public SharedObject::Destructor {
            virtual void operator () (void *ptr) const {
                ((SemaphoreImpl *)ptr)->~SemaphoreImpl ();
            }
        };
    #endif // !defined (TOOLCHAIN_OS_Windows)

        Semaphore::Semaphore (
                ui32 maxCount,
                ui32 initialCount,
                const std::string &name) :
            #if defined (TOOLCHAIN_OS_Windows)
                handle (CreateSemaphoreW (0, initialCount, maxCount,
                    !name.empty () ? UTF8ToUTF16 (name).c_str () : 0)) {
            #else // defined (TOOLCHAIN_OS_Windows)
                semaphore (name.empty () ?
                    new SemaphoreImpl (maxCount, initialCount) :
                    (SemaphoreImpl *)SharedObject::Create (
                        name.c_str (),
                        sizeof (SemaphoreImpl),
                        false,
                        SemaphoreImplConstructor (maxCount, initialCount))) {
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
                SharedObject::Destroy (semaphore, SemaphoreImplDestructor ());
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        bool Semaphore::Acquire (const TimeSpec &timeSpec) {
        #if defined (TOOLCHAIN_OS_Windows)
            if (timeSpec == TimeSpec::Infinite) {
                if (WaitForSingleObject (handle, INFINITE) != WAIT_OBJECT_0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
            else {
                DWORD result = WaitForSingleObject (
                    handle, (DWORD)timeSpec.ToMilliseconds ());
                if (result != WAIT_OBJECT_0) {
                    if (result != WAIT_TIMEOUT) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                    return false;
                }
            }
            return true;
        #else // defined (TOOLCHAIN_OS_Windows)
            return semaphore->Acquire (timeSpec);
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
