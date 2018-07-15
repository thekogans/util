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

#if !defined (__thekogans_util_Base64_h)
#define __thekogans_util_Base64_h

#include <cstddef>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Buffer.h"

namespace thekogans {
    namespace util {

        /// \struct Base64 Base64.h thekogans/util/Base64.h
        ///
        /// \brief
        /// Base64 implements a standard base64 encoder/decoder.
        ///
        /// The API should be self explanatory.

        struct _LIB_THEKOGANS_UTIL_DECL Base64 {
            /// \brief
            /// Return the buffer length required to hold the result of Encode.
            /// \param[in] buffer Buffer to encode.
            /// \param[in] bufferLength Length of buffer in bytes.
            /// \param[in] lineLength Insert a '\n' after every
            /// lineLength characters of output.
            /// \param[in] linePad Pad the start of every line with ' '.
            /// \return Buffer length required to encode the given buffer.
            static std::size_t GetEncodedLength (
                const void *buffer,
                std::size_t bufferLength,
                std::size_t lineLength,
                std::size_t linePad);

            /// \brief
            /// Encode a buffer.
            /// \param[in] buffer Buffer to encode.
            /// \param[in] bufferLength Length of buffer in bytes.
            /// \param[in] lineLength Insert a '\n' after every
            /// lineLength characters of output.
            /// \param[in] linePad Pad the start of every line with ' '.
            /// \param[out] encoded Where to write encoded bytes.
            /// \return Number of bytes written to encoded.
            static std::size_t Encode (
                const void *buffer,
                std::size_t bufferLength,
                std::size_t lineLength,
                std::size_t linePad,
                ui8 *encoded);
            /// \brief
            /// Encode a buffer.
            /// \param[in] buffer Buffer to encode.
            /// \param[in] bufferLength Length of buffer in bytes.
            /// \param[in] lineLength Insert a '\n' after every
            /// lineLength characters of output.
            /// \param[in] linePad Pad the start of every line with ' '.
            /// \return Resulting base64 encoding.
            static Buffer Encode (
                const void *buffer,
                std::size_t bufferLength,
                std::size_t lineLength = SIZE_T_MAX,
                std::size_t linePad = 0);

            /// \brief
            /// Return the buffer length required to hold the result of Decode.
            /// \param[in] buffer Buffer to decode.
            /// \param[in] bufferLength Length of buffer in bytes.
            /// \return Buffer length required to decode the given buffer.
            static std::size_t GetDecodedLength (
                const void *buffer,
                std::size_t bufferLength);

            /// \brief
            /// Decode a buffer.
            /// \param[in] bufferLength Buffer to decode.
            /// \param[in] length Length of buffer in bytes.
            /// \param[out] decoded Where to write the resulting decoding.
            /// \return Number of bytes written to decoded.
            static std::size_t Decode (
                const void *buffer,
                std::size_t bufferLength,
                ui8 *decoded);
            /// \brief
            /// Decode a buffer.
            /// \param[in] buffer Buffer to decode.
            /// \param[in] bufferLength Length of buffer in bytes.
            /// \return Resulting decoding.
            static Buffer Decode (
                const void *buffer,
                std::size_t bufferLength);
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Base64_h)
