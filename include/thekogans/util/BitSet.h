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

#if !defined (__thekogans_util_BitSet_h)
#define __thekogans_util_BitSet_h

#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"

namespace thekogans {
    namespace util {

        /// \struct BitSet BitSet.h thekogans/util/BitSet.h
        ///
        /// \brief
        /// BitSet is meant to be an extension of \see{Flags}. If your needs are
        /// simple (basically an array of flags), BitSet will do the trick.

        struct _LIB_THEKOGANS_UTIL_DECL BitSet {
            /// \brief
            /// The bit set.
            /// Most significant bit of bits[0] is bit 0.
            std::vector<ui32> bits;

            /// \brief
            /// ctor.
            /// \param[in] size Number of bits in the bit set.
            BitSet (ui32 size = 0) {
                Resize (size);
            }

            /// \brief
            /// Resize and clear the bit set.
            /// \param[in] size Number of bits in the new bit set.
            /// VERY IMPORTANT: Resize does not preserve the old
            /// contents. In fact, it clears the new bit set to 0.
            void Resize (ui32 size);

            /// \brief
            /// Return true if the bit at the given position is set.
            /// \param[in] bit The bit position to test.
            /// \return true = bit == 1, false = bit == 0.
            bool Test (ui32 bit) const;
            /// \brief
            /// Set or clear the bit at the given position.
            /// \param[in] bit The bit position to set/clear.
            /// \param[in] on true = set, false = clear.
            /// \return Previous bit value.
            bool Set (
                ui32 bit,
                bool on);
            /// \brief
            /// Flip the bit at the given position.
            /// \param[in] bit The bit position to flip.
            /// \return Previous bit value.
            bool Flip (ui32 bit);

            /// \brief
            /// Set all bits to 0.
            void Clear ();
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_BitSet_h)
