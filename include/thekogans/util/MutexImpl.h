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

#if !defined (__thekogans_util_MutexImpl_h)
#define __thekogans_util_MutexImpl_h

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
#else // defined (TOOLCHAIN_OS_Windows)
    #include <atomic>
    #include <pthread.h>
#endif // defined (TOOLCHAIN_OS_Windows)

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

            explicit Impl (Type type_);
            ~Impl ();

            bool TryAcquire ();
            void Acquire ();
            void Release ();
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_MutexImpl_h)
