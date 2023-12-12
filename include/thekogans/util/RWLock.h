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

#if !defined (__thekogans_util_RWLock_h)
#define __thekogans_util_RWLock_h

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
    #include <synchapi.h>
#else // defined (TOOLCHAIN_OS_Windows)
    #include <pthread.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct RWLock RWLock.h thekogans/util/RWLock.h
        ///
        /// \brief
        /// RWLock wraps a Windows SRWLOCK and a POSIX pthread_rwlock_t
        /// so that they can be used with the rest of the util
        /// synchronization machinery.

        struct _LIB_THEKOGANS_UTIL_DECL RWLock {
        private:
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows read/write lock.
            SRWLOCK rwlock;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// POSIX read/write lock.
            pthread_rwlock_t rwlock;
        #endif // defined (TOOLCHAIN_OS_Windows)

        public:
            /// \brief
            /// Default ctor. Initialize to unlocked.
            RWLock ();
            /// \brief
            /// dtor.
            ~RWLock ();

            /// \brief
            /// Try to acquire the lock.
            /// \param[in] read true = acqure for reading, acquire for writing.
            /// \return true = acquierd, false = failed to acquire.
            bool TryAcquire (bool read);

            /// \brief
            /// Acquire the lock.
            /// \param[in] read true = acqure for reading, acquire for writing.
            void Acquire (bool read);

            /// \brief
            /// Release the lock.
            /// \param[in] read true = release for reading, release for writing.
            void Release (bool read);

            /// \brief
            /// RWLock is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (RWLock)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RWLock_h)
