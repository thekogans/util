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

#include "thekogans/util/Exception.h"
#include "thekogans/util/Mutex.h"

namespace thekogans {
    namespace util {

        Mutex::Mutex (bool shared) {
        #if defined (TOOLCHAIN_OS_Windows)
            InitializeCriticalSection (&cs);
        #else // defined (TOOLCHAIN_OS_Windows)
            THEKOGANS_UTIL_ERROR_CODE errorCode;
            if (shared) {
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
                errorCode = pthread_mutex_init (&mutex, &attribute.attribute);
            }
            else {
                errorCode = pthread_mutex_init (&mutex, 0);
            }
            if (errorCode != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        Mutex::~Mutex () {
        #if defined (TOOLCHAIN_OS_Windows)
            DeleteCriticalSection (&cs);
        #else // defined (TOOLCHAIN_OS_Windows)
            pthread_mutex_destroy (&mutex);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        bool Mutex::TryAcquire () {
        #if defined (TOOLCHAIN_OS_Windows)
            return TryEnterCriticalSection (&cs) == TRUE;
        #else // defined (TOOLCHAIN_OS_Windows)
            return pthread_mutex_trylock (&mutex) == 0;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Mutex::Acquire () {
        #if defined (TOOLCHAIN_OS_Windows)
            EnterCriticalSection (&cs);
        #else // defined (TOOLCHAIN_OS_Windows)
            THEKOGANS_UTIL_ERROR_CODE errorCode =
                pthread_mutex_lock (&mutex);
            if (errorCode != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Mutex::Release () {
        #if defined (TOOLCHAIN_OS_Windows)
            LeaveCriticalSection (&cs);
        #else // defined (TOOLCHAIN_OS_Windows)
            THEKOGANS_UTIL_ERROR_CODE errorCode =
                pthread_mutex_unlock (&mutex);
            if (errorCode != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
