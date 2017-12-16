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
            /// Encode a buffer.
            /// \param[in] buffer Buffer to encode.
            /// \param[in] length Length of buffer in bytes.
            /// \param[in] lineLength Insert a '\n' after every
            /// lineLength characters of output.
            /// \param[in] linePad Pad the start of every line
            /// with ' '.
            /// \return Resulting base64 encoding.
            static Buffer::UniquePtr Encode (
                const void *buffer,
                std::size_t length,
                std::size_t lineLength = SIZE_T_MAX,
                std::size_t linePad = 0);
            /// \brief
            /// Decode a buffer.
            /// \param[in] buffer Buffer to decode.
            /// \param[in] length Length of buffer in bytes.
            /// \return Resulting decoding.
            static Buffer::UniquePtr Decode (
                const void *buffer,
                std::size_t length);
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Base64_h)
