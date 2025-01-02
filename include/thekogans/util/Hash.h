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

#if !defined (__thekogans_util_Hash_h)
#define __thekogans_util_Hash_h

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <list>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/DynamicCreatable.h"

namespace thekogans {
    namespace util {

        /// \struct Hash Hash.h thekogans/util/Hash.h
        ///
        /// \brief
        /// Base class used to represent an abstract hash generator.

        struct _LIB_THEKOGANS_UTIL_DECL Hash : public DynamicCreatable {
            /// \brief
            /// Declare \see{DynamicCreatable} boilerplate.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE (Hash)

        #if defined (THEKOGANS_UTIL_TYPE_Static)
            /// \brief
            /// Because Hash uses dynamic initialization, when using
            /// it in static builds call this method to have the Hash
            /// explicitly include all internal hash types. Without
            /// calling this api, the only hashers that will be available
            /// to your application are the ones you explicitly link to.
            static void StaticInit ();
        #endif // defined (THEKOGANS_UTIL_TYPE_Static)

            /// \brief
            /// Digest type.
            using Digest = std::vector<ui8>;
            /// \brief
            /// Convert a given digest to it's string representation.
            /// \param[in] digest Digest to convert.
            /// \return Digest's string representation.
            static std::string DigestTostring (const Digest &digest);
            /// \brief
            /// Convert a given digest string representation to digest.
            /// \param[in] digest Digest string to convert.
            /// \return Digest.
            static Digest stringToDigest (const std::string &digest);

            /// \brief
            /// Given it's size, return the digest name.
            /// \return Digest name based on the given size.
            virtual std::string GetDigestName (std::size_t /*digestSize*/) const = 0;
            /// \brief
            /// Return hasher supported digest sizes.
            /// \param[out] digestSizes List of supported digest sizes.
            virtual void GetDigestSizes (std::list<std::size_t> & /*digestSizes*/) const = 0;

            /// This API is used in streaming situations. Call Init at the
            /// beginning of the stream. As data comes in, call Update with
            /// each successive chunk. Once all data has been received,
            /// call Final to get the digest.
            /// \brief
            /// Initialize the hasher.
            /// \param[in] digestSize digest size.
            virtual void Init (std::size_t /*digestSize*/) = 0;
            /// \brief
            /// Hash a buffer.
            /// \param[in] buffer Buffer to hash.
            /// \param[in] size Size of buffer in bytes.
            virtual void Update (
                const void * /*buffer*/,
                std::size_t /*size*/) = 0;
            /// \brief
            /// Finalize the hashing operation, and retrieve the digest.
            /// \param[out] digest Result of the hashing operation.
            virtual void Final (Digest & /*digest*/) = 0;

            /// \brief
            /// Create a digest from a given buffer.
            /// \param[in] buffer Beginning of buffer.
            /// \param[in] size Size of buffer in bytes.
            /// \param[in] digestSize Size of difest in bytes.
            /// \param[out] digest Where to store the generated digest.
            void FromBuffer (
                const void *buffer,
                std::size_t size,
                std::size_t digestSize,
                Digest &digest);
            /// \brief
            /// Create a digest from a given file.
            /// \param[in] path File from which to generate the digest.
            /// \param[in] digestSize Size of difest in bytes.
            /// \param[out] digest Where to store the generated digest.
            void FromFile (
                const std::string &path,
                std::size_t digestSize,
                Digest &digest);
        };

        /// \brief
        /// Compare two digests for equality.
        /// \param[in] digest1 First digest to compare.
        /// \param[in] digest2 Second digest to compare.
        /// \return true = equal, false = not equal.
        inline bool _LIB_THEKOGANS_UTIL_API operator == (
                const Hash::Digest &digest1,
                const Hash::Digest &digest2) {
            return digest1.size () == digest2.size () &&
                memcmp (&digest1[0], &digest2[0], digest1.size ()) == 0;
        }

        /// \brief
        /// Compare two digests for inequality.
        /// \param[in] digest1 First digest to compare.
        /// \param[in] digest2 Second digest to compare.
        /// \return true = not equal, false = equal.
        inline bool _LIB_THEKOGANS_UTIL_API operator != (
                const Hash::Digest &digest1,
                const Hash::Digest &digest2) {
            return digest1.size () != digest2.size () ||
                memcmp (&digest1[0], &digest2[0], digest1.size ()) != 0;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Hash_h)
