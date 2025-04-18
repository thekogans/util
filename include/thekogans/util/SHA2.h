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

#if !defined (__thekogans_util_SHA2_h)
#define __thekogans_util_SHA2_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Hash.h"
#include "thekogans/util/SHA2_224_256.h"
#include "thekogans/util/SHA2_384_512.h"

namespace thekogans {
    namespace util {

        /// \struct SHA2 SHA2.h thekogans/util/SHA2.h
        ///
        /// \brief
        /// Use instances of this class to create SHA2 hashes.

        struct _LIB_THEKOGANS_UTIL_DECL SHA2 : public Hash {
            /// \brief
            /// SHA2 participates in the Hash dynamic discovery and creation.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (SHA2)

            /// \brief
            /// SHA2 224 bit hash size in bytes.
            static const std::size_t DIGEST_SIZE_224 = 28;
            /// \brief
            /// SHA2 256 bit hash size in bytes.
            static const std::size_t DIGEST_SIZE_256 = 32;
            /// \brief
            /// SHA2 384 bit hash size in bytes.
            static const std::size_t DIGEST_SIZE_384 = 48;
            /// \brief
            /// SHA2 512 bit hash size in bytes.
            static const std::size_t DIGEST_SIZE_512 = 64;

        private:
            /// \brief
            /// The size of digest the hash is being created for.
            std::size_t digestSize;
            /// \brief
            /// Hasher for DIGEST_SIZE_224 and DIGEST_SIZE_256 hash sizes.
            SHA2_224_256 hasher_224_256;
            /// \brief
            /// Hasher for DIGEST_SIZE_384 and DIGEST_SIZE_512 hash sizes.
            SHA2_384_512 hasher_384_512;

        public:
            /// \brief
            /// ctor. Reset the hasher.
            SHA2 () :
                digestSize (0) {}

            /// \brief
            /// Return hasher name.
            /// \return Hasher name.
            virtual std::string GetDigestName (std::size_t digestSize) const override {
                return FormatString ("SHA2-" THEKOGANS_UTIL_SIZE_T_FORMAT, digestSize * 8);
            }
            /// \brief
            /// Return hasher supported digest sizes.
            /// \param[out] digestSizes List of supported digest sizes.
            virtual void GetDigestSizes (std::list<std::size_t> &digestSizes) const override;

            /// Initialize the hasher.
            /// \param[in] digestSize Digest size.
            virtual void Init (std::size_t digestSize) override;
            /// \brief
            /// Hash a buffer.
            /// \param[in] buffer Buffer to hash.
            /// \param[in] length Length of buffer in bytes.
            virtual void Update (
                const void *buffer,
                std::size_t length) override;
            /// \brief
            /// Finalize the hashing operation, and retrieve the digest.
            /// \param[out] digest Result of the hashing operation.
            virtual void Final (Digest &digest) override;

            /// \brief
            /// SHA2 is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SHA2)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SHA2_h)
