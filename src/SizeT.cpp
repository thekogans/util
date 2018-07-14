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

#if defined (TOOLCHAIN_OS_Windows)
    #include <intrin.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/ByteSwap.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/SizeT.h"

namespace thekogans {
    namespace util {

        // This code was inspired by:
        // https://github.com/stoklund/varint/blob/master/prefix_varint.cpp

    #if defined (TOOLCHAIN_OS_Windows)
        namespace {
            inline std::size_t __builtin_clzll (ui64 value) {
                unsigned long leadingZero = 0;
                _BitScanReverse64 (&leadingZero, value);
                return 63 - leadingZero;
            }

            inline std::size_t __builtin_ctz (ui32 value) {
                unsigned long trailingZero = 0;
                _BitScanForward (&trailingZero, value);
                return trailingZero;
            }
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        std::size_t SizeT::Size () const {
            std::size_t bits = 64 - __builtin_clzll (value | 1);
            return bits > 56 ? 9 : 1 + (bits - 1) / 7;
        }

        std::size_t SizeT::Size (ui8 firstByte) {
            return (THEKOGANS_UTIL_IS_ODD (firstByte) ? 0 :
                firstByte == 0 ? 8 : __builtin_ctz ((ui32)firstByte | 0x100)) + 1;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const SizeT &sizeT) {
            std::size_t bytes = sizeT.Size ();
            if (bytes < 9) {
                ui64 value = (2 * sizeT.value + 1) << (bytes - 1);
                for (std::size_t i = 0; i < bytes; ++i) {
                    serializer << (ui8)(value & 0xff);
                    value >>= 8;
                }
            }
            else {
                serializer << (ui8)0 << sizeT.value;
            }
            return serializer;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                SizeT &sizeT) {
            ui8 value;
            serializer >> value;
            std::size_t bytes = SizeT::Size (value);
            if (bytes < 9) {
                sizeT.value = value;
                for (std::size_t i = 1, shift = 8; i < bytes; ++i, shift += 8) {
                    serializer >> value;
                    sizeT.value |= (ui64)value << shift;
                }
                sizeT.value >>= bytes;
            }
            else {
                serializer >> sizeT.value;
            }
            return serializer;
        }

    } // namespace util
} // namespace thekogans
