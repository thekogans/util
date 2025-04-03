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

#if !defined (__thekogans_util_SHA2_224_256_h)
#define __thekogans_util_SHA2_224_256_h

#include <cstddef>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Hash.h"

namespace thekogans {
    namespace util {

        /// \struct SHA2_224_256 SHA2_224_256.h thekogans/util/SHA2_224_256.h
        ///
        /// \brief
        /// Use instances of this class to create SHA2_224_256 hashes.

        struct _LIB_THEKOGANS_UTIL_DECL SHA2_224_256 {
        private:
            /// \brief
            /// Size of the state vector in ui32.
            static const std::size_t STATE_SIZE = 8;
            /// \brief
            /// Block size in bytes.
            static const std::size_t BLOCK_SIZE = 64;
            /// \brief
            /// Block size, without the 8 bytes for bitCount, in bytes.
            static const std::size_t SHORT_BLOCK_SIZE = BLOCK_SIZE - 8;
            /// \brief
            /// Digest size (DIGEST_SIZE_224 or DIGEST_SIZE_256).
            std::size_t digestSize;
            /// \brief
            /// Incremental state used during hashing.
            ui32 state[STATE_SIZE];
            /// \brief
            /// Number of input bits processed.
            ui64 bitCount;
            /// \brief
            /// Current data being hashed.
            ui8 buffer[BLOCK_SIZE];
            /// \brief
            /// Index in to the buffer where next write will occur.
            std::size_t bufferIndex;

        public:
            /// \brief
            /// ctor.
            /// Initialize the hasher.
            SHA2_224_256 () {
                Reset ();
            }

            /// \brief
            /// Initialize the hasher.
            /// \param[in] digestSize digest size.
            void Init (std::size_t digestSize);
            /// \brief
            /// Hash a buffer. Call multiple times before
            /// Finalize to process incremental data.
            /// \param[in] buffer Buffer to hash.
            /// \param[in] length Length of buffer in bytes.
            void Update (
                const void *buffer,
                std::size_t length);
            /// \brief
            /// Finalize the hashing operation, and retrieve the digest.
            /// \param[out] digest Result of the hashing operation.
            void Final (Hash::Digest &digest);

        private:
            /// \brief
            /// Called internally after finalize to reset
            /// the hasher and prevent secret leaking.
            void Reset ();
            /// \brief
            /// Called internally to transform (hash) the input block.
            void Transform ();

            /// \brief
            /// SHA2_224_256 is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SHA2_224_256)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SHA2_224_256_h)
