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

#if !defined (__thekogans_util_SharedMutex_h)
#define __thekogans_util_SharedMutex_h

#include "thekogans/util/Config.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/Types.h"
#endif // defined (TOOLCHAIN_OS_Windows)

namespace thekogans {
    namespace util {

        /// \struct SharedMutex SharedMutex.h thekogans/util/SharedMutex.h
        ///
        /// \brief
        /// SharedMutex implements a cross process mutex. Use the same name
        /// when creating the mutex to signal across process boundaries.

        struct _LIB_THEKOGANS_UTIL_DECL SharedMutex {
        private:
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows mutex handle.
            THEKOGANS_UTIL_HANDLE handle;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Forward declaration of a private class.
            struct SharedMutexImpl;
            /// \brief
            /// POSIX shared mutex implementation.
            SharedMutexImpl *mutex;
            /// \brief
            /// Forward declaration of a private class.
            struct SharedMutexImplCreator;
        #endif // defined (TOOLCHAIN_OS_Windows)

        public:
            /// \brief
            /// Default ctor. Initialize to unacquired.
            /// \param[in] name Shared mutex name.
            SharedMutex (const char *name);
            /// \brief
            /// dtor.
            ~SharedMutex ();

            /// \brief
            /// Try to acquire the mutex without blocking.
            /// \return true = acquired, false = failed to acquire
            bool TryAcquire ();

            /// \brief
            /// Acquire the mutex.
            void Acquire ();

            /// \brief
            /// Release the mutex.
            void Release ();

            /// \brief
            /// SharedMutex is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SharedMutex)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SharedMutex_h)
