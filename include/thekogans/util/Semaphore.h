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

#if !defined (__thekogans_util_Semaphore_h)
#define __thekogans_util_Semaphore_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/TimeSpec.h"

namespace thekogans {
    namespace util {

        /// \struct Semaphore Semaphore.h thekogans/util/Semaphore.h
        ///
        /// \brief
        /// Wraps a Windows semaphore synchronization object.
        /// Emulates it on Linux/OS X.

        struct _LIB_THEKOGANS_UTIL_DECL Semaphore {
        private:
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Handle to Windows native semaphore object.
            THEKOGANS_UTIL_HANDLE handle;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Forward declaration of a private class.
            struct SemaphoreImpl;
            /// \brief
            /// POSIX shared semaphore implementation.
            SemaphoreImpl *semaphore;
            /// \brief
            /// Forward declaration of a private class.
            struct SemaphoreImplConstructor;
            /// \brief
            /// Forward declaration of a private class.
            struct SemaphoreImplDestructor;
        #endif // defined (TOOLCHAIN_OS_Windows)

        public:
            /// \brief
            /// ctor.
            /// \param[in] maxCount Maximum number of concurent threads.
            /// \param[in] initialCount Initial state of the semaphore.
            /// \param[in] name Shared semaphore name.
            Semaphore (
                ui32 maxCount = 1,
                ui32 initialCount = 1,
                const char *name = 0);
            /// \brief
            /// dtor.
            ~Semaphore ();

            /// \brief
            /// Wait for the semaphore to become signaled.
            /// \param[in] timeSpec Amount of time to wait before returning false.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true = succeeded, false = timed out.
            bool Acquire (const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Put the semaphore in to Signaled state. If any
            /// threads are waiting for the semaphore to become
            /// signaled, one (or more) will be woken up,
            /// and given a chance to execute.
            /// \param[in] count_ Number of threads to release.
            void Release (ui32 count = 1);

            /// \brief
            /// Semaphore is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Semaphore)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Semaphore_h)
