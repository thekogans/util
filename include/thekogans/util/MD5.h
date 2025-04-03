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

#if !defined (__thekogans_util_MD5_h)
#define __thekogans_util_MD5_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Hash.h"

namespace thekogans {
    namespace util {

        /// \struct MD5 MD5.h thekogans/util/MD5.h
        ///
        /// \brief
        /// Use instances of this class to create MD5 hashes.

        struct _LIB_THEKOGANS_UTIL_DECL MD5 : public Hash {
            /// \brief
            /// MD5 participates in the Hash dynamic discovery and creation.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (MD5)

            /// \brief
            /// MD5 hash size in bytes.
            static const std::size_t DIGEST_SIZE_128 = 16;

        private:
            /// \brief
            /// Size of the state vector in ui32.
            static const std::size_t STATE_SIZE = 4;
            /// \brief
            /// Block size in bytes.
            static const std::size_t BLOCK_SIZE = 64;
            /// \brief
            /// Block size, without the 8 bytes for bitCount, in bytes.
            static const std::size_t SHORT_BLOCK_SIZE = BLOCK_SIZE - 8;
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
            /// ctor. Reset the hasher.
            MD5 () {
                Reset ();
            }

            /// \brief
            /// Given it's size, return the digest name.
            /// \return Digest name based on the given size.
            virtual std::string GetDigestName (std::size_t /*digestSize*/) const override {
                return "MD5";
            }
            /// \brief
            /// Return hasher supported digest sizes.
            /// \param[out] digestSizes List of supported digest sizes.
            virtual void GetDigestSizes (std::list<std::size_t> &digestSizes) const override {
                digestSizes.push_back (DIGEST_SIZE_128);
            }

            /// \brief
            /// Initialize the hasher.
            virtual void Init (std::size_t digestSize) override;
            /// \brief
            /// Hash a buffer. Call multiple times before
            /// Finalize to process incremental data.
            /// \param[in] buffer Buffer to hash.
            /// \param[in] size Size of buffer in bytes.
            virtual void Update (
                const void *buffer,
                std::size_t length) override;
            /// \brief
            /// Finalize the hashing operation, and retrieve the digest.
            /// \param[out] digest Result of the hashing operation.
            virtual void Final (Digest &digest) override;

        private:
            /// \brief
            /// Called internally after finalize to reset
            /// the hasher and prevent secret leaking.
            void Reset ();
            /// \brief
            /// Called internally to transform (hash) the input block.
            void Transform ();
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_MD5_h)
