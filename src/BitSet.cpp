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

#include <cstring>
#include "thekogans/util/BitSet.h"

namespace thekogans {
    namespace util {

        void BitSet::Resize (ui32 size) {
            if (size == 0) {
                bits.clear ();
            }
            else {
                bits.resize ((size + 31) >> 5);
                Clear ();
            }
        }

        bool BitSet::Test (ui32 bit) const {
            return ((bits[bit >> 5] >> (31 - (bit & 31))) & 1) != 0;
        }

        bool BitSet::Set (
                ui32 bit,
                bool on) {
            bool old = Test (bit);
            static const ui32 mask = 0x80000000;
            if (on) {
                bits[bit >> 5] |= mask >> (bit & 31);
            }
            else {
                bits[bit >> 5] &= ~(mask >> (bit & 31));
            }
            return old;
        }

        bool BitSet::Flip (ui32 bit) {
            return Set (bit, !Test (bit));
        }

        void BitSet::Clear () {
            memset (&bits[0], 0, bits.size () * UI32_SIZE);
        }

    } // namespace util
} // namespace thekogans
