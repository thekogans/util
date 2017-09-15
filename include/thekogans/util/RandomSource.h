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

#if !defined (__thekogans_util_RandomSource_h)
#define __thekogans_util_RandomSource_h

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
    #include <wincrypt.h>
#else // defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/File.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Config.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Buffer.h"

namespace thekogans {
    namespace util {

        /// \struct RandomSource RandomSource.h thekogans/util/RandomSource.h
        ///
        /// \brief
        /// Uses system specific resources to provide a source of random bytes.
        /// NOTE: If your intended usage is for cryptography, it is very highly
        /// recommended that you use thekogans::util::SecureBuffer for this task:
        ///
        /// \code{.cpp}
        /// thekogans::util::RandomSource randomSource;
        /// thekogans::util::SecureBuffer entropy (thekogans::util::HostEndian, entropyNeeded);
        /// entropy.AdvanceWriteOffset (
        ///     randomSource.GetBytes (
        ///         entropy.GetWritePtr (),
        ///         entropy.GetDataAvailableForWriting ()));
        /// \endcode
        ///
        /// The use of thekogans::util::SecureBuffer will guarantee that
        /// buffer will be properly cleared when it goes out of scope and
        /// that it won't be swapped to disc in an event of a core dump.

        struct _LIB_THEKOGANS_UTIL_DECL RandomSource {
        private:
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Handle to Windows crypto context.
            HCRYPTPROV cryptProv;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// "/dev/urandom"
            ReadOnlyFile urandom;
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

        public:
            /// \brief
            /// ctor.
            /// Initialize system specific resources.
            RandomSource ();
            /// \brief
            /// dtor.
            /// Release system specific resources.
            ~RandomSource ();

            /// \brief
            /// Use a system specific entropy source to return a count of random bytes.
            /// \param[out] buffer Buffer where random bytes will be placed.
            /// \param[in] count Count of random bytes to place in the buffer.
            /// \return Actual count of random bytes placed in the buffer.
            ui32 GetBytes (
                void *buffer,
                std::size_t count);

            /// \brief
            /// Substitute for system rand function.
            /// \return A random ui32.
            ui32 Getui32 ();

            /// \brief
            /// RandomSource is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (RandomSource)
        };

        /// \struct GlobalRandomSource RandomSource.h thekogans/util/RandomSource.h
        ///
        /// \brief
        /// A global random source instance.
        struct _LIB_THEKOGANS_UTIL_DECL GlobalRandomSource :
            public Singleton<RandomSource, SpinLock> {};

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RandomSource_h)
