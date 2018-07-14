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

#if !defined (__thekogans_util_SHA3_h)
#define __thekogans_util_SHA3_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Hash.h"

namespace thekogans {
    namespace util {

        /// \struct SHA3 SHA3.h thekogans/util/SHA3.h
        ///
        /// \brief
        /// Use instances of this class to create SHA3 hashes.

        struct _LIB_THEKOGANS_UTIL_DECL SHA3 : public Hash {
            /// \brief
            /// SHA3 participates in the Hash dynamic discovery and creation.
            THEKOGANS_UTIL_DECLARE_HASH (SHA3)

            enum {
                /// \brief
                /// SHA3 224 bit hash size in bytes.
                DIGEST_SIZE_224 = 28,
                /// \brief
                /// SHA3 256 bit hash size in bytes.
                DIGEST_SIZE_256 = 32,
                /// \brief
                /// SHA3 384 bit hash size in bytes.
                DIGEST_SIZE_384 = 48,
                /// \brief
                /// SHA3 512 bit hash size in bytes.
                DIGEST_SIZE_512 = 64
            };

        private:
            enum {
                /// \brief
                /// Max state size regardless of hash size.
                MAX_STATE_SIZE = 1600 / (8 * 8),
                /// \brief
                /// Max block size regardless of hash size.
                MAX_BLOCK_SIZE = 200 - 2 * (224 / 8)
            };
            /// \brief
            /// The size of digest the hash is being created for.
            std::size_t digestSize;
            /// \brief
            /// Block size for current digest size.
            std::size_t blockSize;
            /// \brief
            /// Incremental state used during hashing.
            ui64 state[MAX_STATE_SIZE];
            /// \brief
            /// Number of input bytes processed.
            ui64 byteCount;
            /// \brief
            /// Current data being hashed.
            ui8 buffer[MAX_BLOCK_SIZE];
            /// \brief
            /// Index in to the buffer where next write will occur.
            std::size_t bufferIndex;

        public:
            /// \brief
            /// ctor.
            SHA3 () {
                Reset ();
            }

            /// \brief
            /// Return hasher name.
            /// \return Hasher name.
            virtual std::string GetName (std::size_t digestSize) const {
                return FormatString ("SHA3-%u", digestSize * 8);
            }
            /// \brief
            /// Return hasher supported digest sizes.
            /// \param[out] digestSizes List of supported digest sizes.
            virtual void GetDigestSizes (std::list<std::size_t> &digestSizes) const {
                digestSizes.push_back (DIGEST_SIZE_224);
                digestSizes.push_back (DIGEST_SIZE_256);
                digestSizes.push_back (DIGEST_SIZE_384);
                digestSizes.push_back (DIGEST_SIZE_512);
            }
            /// \brief
            /// Initialize the hasher.
            virtual void Init (std::size_t digestSize);
            /// \brief
            /// Hash a buffer.
            /// \param[in] buffer Buffer to hash.
            /// \param[in] size Size of buffer in bytes.
            virtual void Update (
                const void *buffer,
                std::size_t size);
            /// \brief
            /// Finalize the hashing operation, and retrieve the digest.
            /// \param[out] digest Result of the hashing operation.
            virtual void Final (Digest &digest);

        private:
            /// \brief
            /// Called internally after finalize to reset
            /// the hasher and prevent secret leaking.
            void Reset ();
            /// \brief
            /// Called internally to transform (hash) the input block.
            void Transform ();

            /// \brief
            /// SHA3 is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SHA3)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SHA3_h)
