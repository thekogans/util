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

#if !defined (__thekogans_util_SharedSemaphore_h)
#define __thekogans_util_SharedSemaphore_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"

namespace thekogans {
    namespace util {

        /// \struct SharedSemaphore SharedSemaphore.h thekogans/util/SharedSemaphore.h
        ///
        /// \brief
        /// SharedSemaphore implements a cross process semaphore. Use the same name
        /// when creating the semaphore to signal across process boundaries.

        struct _LIB_THEKOGANS_UTIL_DECL SharedSemaphore {
        private:
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows semaphore handle.
            THEKOGANS_UTIL_HANDLE handle;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Forward declaration of a private class.
            struct SharedSemaphoreImpl;
            /// \brief
            /// POSIX shared semaphore implementation.
            SharedSemaphoreImpl *semaphore;
        #endif // defined (TOOLCHAIN_OS_Windows)

        public:
            /// \brief
            /// ctor.
            /// \param[in] name Shared semaphore name.
            /// \param[in] maxCount Maximum number of concurent threads.
            /// \param[in] initialCount Initial state of the semaphore.
            SharedSemaphore (
                const char *name,
                ui32 maxCount = 1,
                ui32 initialCount = 1);
            /// \brief
            /// dtor.
            ~SharedSemaphore ();

            /// \brief
            /// Wait for the semaphore to become signaled.
            void Acquire ();

            /// \brief
            /// Put the semaphore in to Signaled state. If any
            /// threads are waiting for the semaphore to become
            /// signaled, one (or more) will be woken up,
            /// and given a chance to execute.
            /// \param[in] count Number of threads to release.
            void Release (ui32 count = 1);

            /// \brief
            /// SharedSemaphore is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SharedSemaphore)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SharedSemaphore_h)
