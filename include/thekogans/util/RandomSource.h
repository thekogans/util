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

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
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
        /// recommended that you use \see{SecureBuffer} for this task:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        /// util::SecureBuffer randomBytes (util::HostEndian, randomBytesNeeded);
        /// randomBytes.AdvanceWriteOffset (
        ///     util::GlobalRandomSource::Instance ()->GetBytes (
        ///         randomBytes.GetWritePtr (),
        ///         randomBytes.GetDataAvailableForWriting ()));
        /// \endcode
        ///
        /// The use of \see{SecureBuffer} will guarantee that buffer will be
        /// properly cleared when it goes out of scope and that it won't be
        /// swapped out to disc in an event of a core dump.

        struct _LIB_THEKOGANS_UTIL_DECL RandomSource : public Singleton<RandomSource> {
        private:
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Handle to Windows crypto context.
            HCRYPTPROV cryptProv;
        #elif defined (TOOLCHAIN_OS_Linux)
            /// \brief
            /// "/dev/urandom"
            ReadOnlyFile urandom;
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Every call to get random bits has to be atomic.
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
            /// Use a system specific entropy source to return a bufferLength of random bytes.
            /// NOTE: There is a very small but > 0 chance that the number
            /// of bytes returned will be less than what you asked for. You
            /// should check the return value and act accordingly.
            /// NOTE: The code implementing hardware random bytes
            /// requires the passed in buffer to be aligned on a ui32
            /// byte boundary. If you pass a misaligned buffer the
            /// function will allocate an aligned internal buffer, get
            /// bytes in to it, and copy the result in to passed in
            /// buffer incurring a performance penalty.
            /// \param[out] buffer Buffer where random bytes will be placed.
            /// \param[in] bufferLength Length of buffer.
            /// \return Actual count of random bytes placed in the buffer.
            std::size_t GetBytes (
                void *buffer,
                std::size_t bufferLength);

            /// \brief
            /// Use a hardware entropy source to return a bufferLength of seed bytes.
            /// NOTE: As per Intel's guidance here:
            /// https://software.intel.com/en-us/blogs/2012/11/17/the-difference-between-rdrand-and-rdseed,
            /// use of rdseed should be limited to seeding a prng.
            /// NOTE: The code implementing hardware random bytes
            /// requires the passed in buffer to be aligned on a ui32
            /// byte boundary. If you pass a misaligned buffer the
            /// function will allocate an aligned internal buffer, get
            /// bytes in to it, and copy the result in to passed in
            /// buffer incurring a performance penalty.
            /// IMPORTANT: Unlike GetBytes above, this method will not fall back
            /// on a software implementation and will only deliver true random
            /// bytes. Depending on your use case, there is a very good chance
            /// that the number of bytes returned will be less than what you
            /// asked for. This is on purpose. If you're using this routine,
            /// it's because you have a need for true randomness and I will
            /// not lie and tell you that I have it when I don't. It's up to
            /// you to decide how to proceed as you know your code better then
            /// I do.
            /// \param[out] buffer Buffer where seed bytes will be placed.
            /// \param[in] bufferLength Length of buffer.
            /// \return Actual count of seed bytes placed in the buffer.
            std::size_t GetSeed (
                void *buffer,
                std::size_t bufferLength);

            /// \brief
            /// This is a convenience method.
            /// Try to get bufferLength of seed. If not enough available, backfill with bytes.
            /// \param[out] buffer Buffer where seed or bytes will be placed.
            /// \param[in] bufferLength Length of buffer.
            /// \return Actual count of seed and bytes placed in the buffer.
            std::size_t GetSeedOrBytes (
                void *buffer,
                std::size_t bufferLength);

            /// \brief
            /// Substitute for system rand function.
            /// NOTE: This method is implemented in terms of GetSeedOrBytes.
            /// \return A random ui32.
            ui32 Getui32 ();

            /// \brief
            /// Substitute for system rand function.
            /// NOTE: This method is implemented in terms of GetSeedOrBytes.
            /// \return A random ui64.
            ui64 Getui64 ();

            /// \brief
            /// Substitute for system rand function.
            /// NOTE: This method is implemented in terms of GetSeedOrBytes.
            /// \return A random std::size_t.
            std::size_t Getsize_t ();

            /// \brief
            /// RandomSource is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (RandomSource)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RandomSource_h)
