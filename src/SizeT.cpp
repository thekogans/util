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

#include "thekogans/util/Serializer.h"
#include "thekogans/util/SizeT.h"

namespace thekogans {
    namespace util {

        // This code was inspired by:
        // https://github.com/stoklund/varint/blob/master/prefix_varint.cpp

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (SizeT, SpinLock)

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const SizeT &sizeT) {
            std::size_t bytes = sizeT.Size ();
            if (bytes < SizeT::MAX_SIZE) {
                ui64 value = ((sizeT.value << 1) | 1) << (bytes - 1);
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
            if (bytes < SizeT::MAX_SIZE) {
                sizeT.value = value;
                for (std::size_t i = 1; i < bytes; ++i) {
                    serializer >> value;
                    sizeT.value |= (ui64)value << (i << 3);
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
