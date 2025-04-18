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

#if !defined (__thekogans_util_GUID_h)
#define __thekogans_util_GUID_h

#include <cstddef>
#include <memory.h>
#include <functional>
#include <iostream>
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/MD5.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        /// \struct GUID GUID.h thekogans/util/GUID.h
        ///
        /// \brief
        /// A nice 128 bit globally unique id class. GUIDs can be
        /// created from file/buffer contents, and even randomly
        /// generated. GUID::FromRandom uses the \sse{RandomSource}
        /// to gather platform specific entropy.
        /// NOTE: GUID::FromFile, GUID::FromBuffer and GUID::FromRandom
        /// use \see{MD5} to hash the data in to a guid sized digest.
        /// While deprecated for cryptographic work, it's perfectly
        /// safe to use in low security situations. Keep that in mind
        /// when using GUID.
        /// PRO TIP: For crypto work I highly recommend \see{crypto::ID}.
        /// It has practically identical interface as GUID and uses SHA2_256
        /// to generate the hash.

        struct _LIB_THEKOGANS_UTIL_DECL GUID {
            /// \brief
            /// GUID size.
            static const std::size_t SIZE = MD5::DIGEST_SIZE_128;

            /// \brief
            /// GUID data.
            ui8 data[SIZE];

            /// \brief
            /// ctor. Initialize to a given value.
            /// \param[in] data_ Value to initialize to.
            /// If nullptr, initialize to all 0.
            GUID (const ui8 data_[SIZE] = nullptr);

            /// \brief
            /// Return the serialized size of this guid.
            /// \return Serialized size of this guid.
            inline constexpr std::size_t Size () const {
                return SIZE;
            }

            /// \brief
            /// Convert the guid to a string representation.
            /// \return String representation of the guid.
            std::string ToHexString (
                bool windows = false,
                bool upperCase = false) const;

            /// \brief
            /// Parse the GUID out of a hex string representation.
            /// NOTE: The guid must only contain hexadecimal (0 - 9, a - f) digits.
            /// \param[in] guid String representation of a guid to parse.
            /// guid can be in Windows GUID format (4-2-2-2-6),
            /// or a string of SIZE * 2 hexadecimal digits.
            static GUID FromHexString (const std::string &guid);
            /// \brief
            /// Create a guid for a given file. Uses \see{MD5::FromFile}.
            /// \param[in] path file to create a guid from (MD5 hash).
            /// \return MD5 hash of the file.
            static GUID FromFile (const std::string &path);
            /// \brief
            /// Create a guid for a given buffer. Uses \see{MD5::FromBuffer}.
            /// \param[in] buffer Pointer to the beginning of the buffer.
            /// \param[in] length Length of the buffer.
            /// \return MD5 hash of the buffer.
            static GUID FromBuffer (
                const void *buffer,
                std::size_t length);
            /// \brief
            /// Create a random guid. Uses \see{MD5::FromRandom}.
            /// \param[in] length Length of random bytes.
            /// \return MD5 hash of the random bytes.
            static GUID FromRandom (std::size_t length = SIZE);
        };

        /// \brief
        /// Compare two guids for order.
        /// \param[in] guid1 GUID to test for less than.
        /// \param[in] guid2 GUID to test for against.
        /// \return true = guid1 < guid2, false guid1 >= guid2
        inline bool _LIB_THEKOGANS_UTIL_API operator < (
                const GUID &guid1,
                const GUID &guid2) {
            return memcmp (guid1.data, guid2.data, GUID::SIZE) < 0;
        }

        /// \brief
        /// Compare two guids for order.
        /// \param[in] guid1 GUID to test for greater than.
        /// \param[in] guid2 GUID to test for against.
        /// \return true = guid1 > guid2, false guid1 <= guid2
        inline bool _LIB_THEKOGANS_UTIL_API operator > (
                const GUID &guid1,
                const GUID &guid2) {
            return memcmp (guid1.data, guid2.data, GUID::SIZE) > 0;
        }

        /// \brief
        /// Compare two guids for equality.
        /// \param[in] guid1 First GUID to compare.
        /// \param[in] guid2 Second GUID to compare.
        /// \return true = guid1 == guid2, false guid1 != guid2
        inline bool _LIB_THEKOGANS_UTIL_API operator == (
                const GUID &guid1,
                const GUID &guid2) {
            return memcmp (guid1.data, guid2.data, GUID::SIZE) == 0;
        }

        /// \brief
        /// Compare two guids for inequality.
        /// \param[in] guid1 First GUID to compare.
        /// \param[in] guid2 Second GUID to compare.
        /// \return true = guid1 != guid2, false guid1 == guid2
        inline bool _LIB_THEKOGANS_UTIL_API operator != (
                const GUID &guid1,
                const GUID &guid2) {
            return memcmp (guid1.data, guid2.data, GUID::SIZE) != 0;
        }

        /// \brief
        /// Dump guid to stream.
        /// \param[in] stream Stream to dump to.
        /// \param[in] guid GIUD to dump.
        /// \return stream
        inline std::ostream & _LIB_THEKOGANS_UTIL_API operator << (
                std::ostream &stream,
                const GUID &guid) {
            return stream << guid.ToHexString ();
        }

        /// \brief
        /// Write the given guid to the given serializer.
        /// \param[in] serializer Where to write the given guid.
        /// \param[in] guid GUID to write.
        /// \return serializer.
        inline Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const GUID &guid) {
            if (serializer.Write (guid.data, GUID::SIZE) != GUID::SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Write (guid.data, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    GUID::SIZE,
                    GUID::SIZE);
            }
            return serializer;
        }

        /// \brief
        /// Read an guid from the given serializer.
        /// \param[in] serializer Where to read the guid from.
        /// \param[out] guid GUID to read.
        /// \return serializer.
        inline Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                GUID &guid) {
            if (serializer.Read (guid.data, GUID::SIZE) != GUID::SIZE) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Read (guid.data, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    GUID::SIZE,
                    GUID::SIZE);
            }
            return serializer;
        }

    } // namespace util
} // namespace thekogans

namespace std {

    /// \struct hash<thekogans::util::GUID> GUID.h thekogans/util/GUID.h
    ///
    /// \brief
    /// Implementation of std::hash for thekogans::util::GUID.

    template <>
    struct hash<thekogans::util::GUID> {
        size_t operator () (const thekogans::util::GUID &guid) const {
            return thekogans::util::HashBuffer32 (
                (const thekogans::util::ui32 *)guid.data,
                thekogans::util::GUID::SIZE >> 2);
        }
    };

} // namespace std

#endif // !defined (__thekogans_util_GUID_h)
