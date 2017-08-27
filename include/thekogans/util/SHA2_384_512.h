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

#if !defined (__thekogans_util_SHA2_384_512_h)
#define __thekogans_util_SHA2_384_512_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Hash.h"

namespace thekogans {
    namespace util {

        /// \struct SHA2_384_512 SHA2_384_512.h thekogans/util/SHA2_384_512.h
        ///
        /// \brief
        /// Use instances of this class to create SHA2_384_512 hashes.

        struct _LIB_THEKOGANS_UTIL_DECL SHA2_384_512 {
        private:
            enum {
                /// \brief
                /// Size of the state vector in ui32.
                STATE_SIZE = 8,
                /// \brief
                /// Block size in bytes.
                BLOCK_SIZE = 128,
                /// \brief
                /// Block size, without the 16 bytes for bitCount, in bytes.
                SHORT_BLOCK_SIZE = BLOCK_SIZE - 16
            };
            /// \brief
            /// Digest size (DIGEST_SIZE_384 or DIGEST_SIZE_512).
            ui32 digestSize;
            /// \brief
            /// Incremental state used during hashing.
            ui64 state[STATE_SIZE];
            /// \brief
            /// Number of input bits processed.
            ui64 bitCount[2];
            /// \brief
            /// Current data being hashed.
            ui8 buffer[BLOCK_SIZE];
            /// \brief
            /// Index in to the buffer where next write will occur.
            ui32 bufferIndex;

        public:
            /// \brief
            /// ctor.
            /// Initialize the hasher.
            SHA2_384_512 () {
                Reset ();
            }

            /// \brief
            /// Initialize the hasher.
            /// \param[in] digestSize digest size.
            void Init (ui32 digestSize);
            /// \brief
            /// Hash a buffer. Call multiple times before
            /// Finalize to process incremental data.
            /// \param[in] buffer Buffer to hash.
            /// \param[in] size Size of buffer in bytes.
            void Update (
                const void *buffer,
                std::size_t size);
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
            /// SHA2_384_512 is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SHA2_384_512)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SHA2_384_512_h)
