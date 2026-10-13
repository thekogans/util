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
    #include <synchapi.h>
#else // defined (TOOLCHAIN_OS_Windows)
    #include <pthread.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Exception.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/MutexImpl.h"
#include "thekogans/util/Condition.h"

namespace thekogans {
    namespace util {

        struct Condition::Impl {
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows condition variable.
            CONDITION_VARIABLE cv;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// POSIX condition variable.
            pthread_cond_t cv;
        #endif // defined (TOOLCHAIN_OS_Windows)

            /// \brief
            /// ctor.
            Impl () {
            #if defined (TOOLCHAIN_OS_Windows)
                InitializeConditionVariable (&cv);
            #else // defined (TOOLCHAIN_OS_Windows)
                struct Attribute {
                    pthread_condattr_t attribute;
                    Attribute () {
                        THEKOGANS_UTIL_ERROR_CODE errorCode = pthread_condattr_init (&attribute);
                        if (errorCode != 0) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                        }
                    #if defined (TOOLCHAIN_OS_Linux)
                        errorCode = pthread_condattr_setclock (&attribute, CLOCK_MONOTONIC);
                        if (errorCode != 0) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                        }
                    #endif // defined (TOOLCHAIN_OS_Linux)
                    }
                    ~Attribute () {
                        pthread_condattr_destroy (&attribute);
                    }
                } attribute;
                THEKOGANS_UTIL_ERROR_CODE errorCode = pthread_cond_init (&cv, &attribute.attribute);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            /// \brief
            /// dtor.
            ~Impl () {
            #if !defined (TOOLCHAIN_OS_Windows)
                pthread_cond_destroy (&cv);
            #endif // !defined (TOOLCHAIN_OS_Windows)
            }

            bool Wait (
                    Mutex &mutex,
                    const TimeSpec &timeSpec) {
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD timeout = timeSpec == TimeSpec::Infinite ? INFINITE : timeSpec.ToMilliseconds ();
                return mutex.impl->type == Mutex::Normal ?
                    SleepConditionVariableSRW (&cv, &mutex.impl->lock.srwLock, timeout, 0) :
                    SleepConditionVariableCS (&cv, &mutex.impl->lock.criticalSection, timeout);
            #else // defined (TOOLCHAIN_OS_Windows)
                if (timeSpec == TimeSpec::Infinite) {
                    THEKOGANS_UTIL_ERROR_CODE errorCode =
                        pthread_cond_wait (&cv, &mutex.impl->mutex);
                    if (errorCode == 0) {
                        return true;
                    }
                }
                else {
                #if defined (TOOLCHAIN_OS_Linux)
                    timespec absolute = (GetMonotonicTime () + timeSpec).Totimespec ();
                #elif defined (TOOLCHAIN_OS_OSX)
                    timespec relative = timeSpec.Totimespec ();
                #endif // defined (TOOLCHAIN_OS_Linux)
                    THEKOGANS_UTIL_ERROR_CODE errorCode =
                    #if defined (TOOLCHAIN_OS_Linux)
                        pthread_cond_timedwait (&cv, &mutex.impl->mutex, &absolute);
                    #elif defined (TOOLCHAIN_OS_OSX)
                        pthread_cond_timedwait_relative_np (&cv, &mutex.impl->mutex, &relative);
                    #endif // defined (TOOLCHAIN_OS_Linux)
                    if (errorCode == 0) {
                        return true;
                    }
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
                return false;
            }

            void Signal () {
            #if defined (TOOLCHAIN_OS_Windows)
                WakeConditionVariable (&cv);
            #else // defined (TOOLCHAIN_OS_Windows)
                THEKOGANS_UTIL_ERROR_CODE errorCode = pthread_cond_signal (&cv);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }

            void SignalAll () {
            #if defined (TOOLCHAIN_OS_Windows)
                WakeAllConditionVariable (&cv);
            #else // defined (TOOLCHAIN_OS_Windows)
                THEKOGANS_UTIL_ERROR_CODE errorCode = pthread_cond_broadcast (&cv);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        };

        Condition::Condition () :
            impl (new Impl) {}

        Condition::~Condition () = default;

        bool Condition::Wait (
                Mutex &mutex,
                const TimeSpec &timeSpec) {
            THEKOGANS_UTIL_ASSERT (impl != nullptr, "Calling Condition::Wait on a moved object.");
            if (impl != nullptr) {
                return impl->Wait (mutex, timeSpec);
            }
            return false;
        }

        void Condition::Signal () {
            THEKOGANS_UTIL_ASSERT (impl != nullptr, "Calling Condition::Signal on a moved object.");
            if (impl != nullptr) {
                impl->Signal ();
            }
        }

        void Condition::SignalAll () {
            THEKOGANS_UTIL_ASSERT (impl != nullptr, "Calling Condition::SignalAll on a moved object.");
            if (impl != nullptr) {
                impl->SignalAll ();
            }
        }

    } // namespace util
} // namespace thekogans
