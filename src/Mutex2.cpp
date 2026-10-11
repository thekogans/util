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
#include "thekogans/util/Exception.h"
#include "thekogans/util/Mutex2.h"

namespace thekogans {
    namespace util {

        Mutex2::Impl::Impl () {
        #if defined (TOOLCHAIN_OS_Windows)
            InitializeCriticalSection (&cs);
        #else // defined (TOOLCHAIN_OS_Windows)
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
                            pthread_mutexattr_settype (&attribute, PTHREAD_MUTEX_RECURSIVE);
                        if (errorCode != 0) {
                            pthread_mutexattr_destroy(&attribute);
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION(errorCode);
                        }
                    }
                }
                ~Attribute () {
                    pthread_mutexattr_destroy (&attribute);
                }
            } attribute;
            THEKOGANS_UTIL_ERROR_CODE errorCode = pthread_mutex_init (&mutex, &attribute.attribute);
            if (errorCode != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        Mutex2::Impl::~Impl () {
        #if defined (TOOLCHAIN_OS_Windows)
            DeleteCriticalSection (&cs);
        #else // defined (TOOLCHAIN_OS_Windows)
            pthread_mutex_destroy (&mutex);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        Mutex2 &Mutex2::operator = (Mutex2 &&other) noexcept {
            if (this != &other) {
                impl = std::move (other.impl);
            }
            return *this;
        }

        bool Mutex2::TryAcquire () {
            if (impl != nullptr) {
            #if defined (TOOLCHAIN_OS_Windows)
                return TryEnterCriticalSection (&impl->cs) == TRUE;
            #else // defined (TOOLCHAIN_OS_Windows)
                return pthread_mutex_trylock (&impl->mutex) == 0;
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            return false;
        }

        void Mutex2::Acquire () {
            if (impl != nullptr) {
            #if defined (TOOLCHAIN_OS_Windows)
                EnterCriticalSection (&impl->cs);
            #else // defined (TOOLCHAIN_OS_Windows)
                THEKOGANS_UTIL_ERROR_CODE errorCode =
                    pthread_mutex_lock (&impl->mutex);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        }

        void Mutex2::Release () {
            if (impl != nullptr) {
            #if defined (TOOLCHAIN_OS_Windows)
                LeaveCriticalSection (&impl->cs);
            #else // defined (TOOLCHAIN_OS_Windows)
                THEKOGANS_UTIL_ERROR_CODE errorCode =
                    pthread_mutex_unlock (&impl->mutex);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        }

    } // namespace util
} // namespace thekogans
