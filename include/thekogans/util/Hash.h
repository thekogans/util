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
#include "thekogans/util/Environment.h"
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
            /// \param[in] length Length of buffer in bytes.
            virtual void Update (
                const void * /*buffer*/,
                std::size_t /*length*/) = 0;
            /// \brief
            /// Finalize the hashing operation, and retrieve the digest.
            /// \param[out] digest Result of the hashing operation.
            virtual void Final (Digest & /*digest*/) = 0;

            /// \brief
            /// Create a digest from random bytes (\see{RandomSource}).
            /// \param[in] length Length of buffer of random in bytes.
            /// \param[in] digestSize Size of difest in bytes.
            /// \param[out] digest Where to store the generated digest.
            void FromRandom (
                std::size_t length,
                std::size_t digestSize,
                Digest &digest);
            /// \brief
            /// Create a digest from a given buffer.
            /// \param[in] buffer Beginning of buffer.
            /// \param[in] length Length of buffer in bytes.
            /// \param[in] digestSize Size of difest in bytes.
            /// \param[out] digest Where to store the generated digest.
            void FromBuffer (
                const void *buffer,
                std::size_t length,
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

        /// NOTE: If your looking to hash a char string look at
        /// \see{HashString} in \see{StringUtils.h}.

        /// By Bob Jenkins, 2006. bob_jenkins@burtleburtle.net. You may use this
        /// code any way you wish, private, educational, or commercial. It's free.
        /// Use for hash table lookup, or anything where one collision in 2^^32 is
        /// acceptable. Do NOT use for cryptographic purposes.

        /// \brief
        /// This works on all machines. To be useful, it requires the buffer be an
        /// array of ui32's, and  the length be the number of ui32's in the buffer.
        /// Its identical to HashBufferLittle on little-endian machines, and identical
        /// to HashBufferBig on big-endian machines, except that the length has to be
        /// measured in ui32s rather than in bytes. HashBufferLittle is more complicated
        /// than HashBuffer32 only because HashBufferLittle has to dance around fitting
        /// the buffer bytes into registers.
        /// \param[in] buffer An array of ui32 values.
        /// \param[in] length Length of the buffer in ui32.
        /// \param[in] seed Previous hash, or an arbitrary seed value.
        /// \return 32-bit value. Every bit of the buffer affects every bit of
        /// the return value. Two buffers differing by one or two bits will have
        /// totally different hash values.
        _LIB_THEKOGANS_UTIL_DECL ui32 _LIB_THEKOGANS_UTIL_API HashBuffer32 (
            const ui32 *buffer,
            std::size_t length,
            ui32 seed = 0);
        /// \brief
        /// Hash a variable-length buffer into a 32-bit value. The best hash table
        /// sizes are powers of 2. There is no need to do mod a prime (mod is sooo slow!).
        /// If you need less than 32 bits, use a bitmask. For example, if you need only
        /// 10 bits, do h = h & hashmask (10);  In which case, the hash table should
        /// have hashsize (10) elements. If you are hashing n strings (ui8 **)buffer,
        /// do it like this:
        /// \code{.cpp}
        /// util::ui32 hash = 0;
        /// for (std::size_t i = 0; i < n; ++i) {
        ///     hash = HashBufferLittle (buffer[i], length[i], hash);
        /// }
        /// \endcode
        /// \param[in] buffer The buffer (the unaligned variable-length array of bytes).
        /// \param[in] length The length of the buffer, counting by bytes.
        /// \param[in] seed Can be any 4-byte value.
        /// \return 32-bit value. Every bit of the buffer affects every bit of
        /// the return value. Two buffers differing by one or two bits will have
        /// totally different hash values.
        _LIB_THEKOGANS_UTIL_DECL ui32 _LIB_THEKOGANS_UTIL_API HashBufferLittle (
            const void *buffer,
            std::size_t length,
            ui32 seed = 0);
        /// \brief
        /// This is the same as HashBuffer32 on big-endian machines. It is different
        /// from HashBufferLittle on all machines. HashBufferBig takes advantage of
        /// big-endian byte ordering.
        /// \param[in] buffer The buffer (the unaligned variable-length array of bytes).
        /// \param[in] length The length of the buffer, counting by bytes.
        /// \param[in] seed Can be any 4-byte value.
        /// \return 32-bit value. Every bit of the buffer affects every bit of
        /// the return value. Two buffers differing by one or two bits will have
        /// totally different hash values.
        _LIB_THEKOGANS_UTIL_DECL ui32 _LIB_THEKOGANS_UTIL_API HashBufferBig (
            const void *buffer,
            std::size_t length,
            ui32 seed = 0);

    /// \def
    /// Define a buffer hasher based on the machine endianness.
    #if defined (TOOLCHAIN_ENDIAN_Little)
        #define HashBuffer HashBufferLittle
    #elif defined (TOOLCHAIN_ENDIAN_Big)
        #define HashBuffer HashBufferBig
    #else // defined (TOOLCHAIN_ENDIAN_Big)
        #error "Unable to determine system endianness."
    #endif // defined (TOOLCHAIN_ENDIAN_Little)

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Hash_h)
