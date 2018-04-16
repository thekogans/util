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

#if !defined (__thekogans_util_CRC32_h)
#define __thekogans_util_CRC32_h

#include <cstddef>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Compute a 32 bit Cyclic Redundancy Check (CRC) over a given buffer.
        /// \param[in] buffer Buffer whose contents to use for crc calculation.
        /// \param[in] length Length of buffer.
        /// \param[in] crc CRC from previous buffer. Use it if you need to
        /// calculate a CRC of multiple disjoint buffers.
        /// \return A 32 bit CRC.
        _LIB_THEKOGANS_UTIL_DECL ui32 _LIB_THEKOGANS_UTIL_API CRC32 (
            const void *buffer,
            std::size_t length,
            ui32 crc = 0);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_CRC32_h)
