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

#include <cstddef>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/SizeT.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        /// \struct BitSet BitSet.h thekogans/util/BitSet.h
        ///
        /// \brief
        /// BitSet is meant to be an extension of \see{Flags}. If your needs are
        /// simple (basically an array of flags), BitSet will do the trick. It's
        /// designed to have an interface compatible with std::bitset but it's
        /// not a template. You can resize BitSet at runtime.

        struct _LIB_THEKOGANS_UTIL_DECL BitSet {
            /// \brief
            /// The bit set.
            std::vector<ui32> bits;
            /// \brief
            /// Size of the bit set.
            SizeT size;

            /// \brief
            /// ctor.
            /// \param[in] size_ Number of bits in the bit set.
            BitSet (std::size_t size_ = 0) :
                    size (0) {
                Resize (size_);
            }

            /// \brief
            /// Return the size of the bit set.
            /// \return Size of the bit set.
            inline std::size_t Size () const {
                return size;
            }

            /// \brief
            /// Resize and clear the bit set.
            /// \param[in] size_ Number of bits in the new bit set.
            /// VERY IMPORTANT: Resize does not preserve the old
            /// contents. In fact, it clears the new bit set to 0.
            void Resize (std::size_t size_);

            /// \brief
            /// Return true if the bit at the given position is set.
            /// \param[in] bit The bit position to test.
            /// \return true = bit == 1, false = bit == 0.
            bool Test (std::size_t bit) const;
            /// \brief
            /// Set or clear the bit at the given position.
            /// \param[in] bit The bit position to set/clear.
            /// \param[in] on true = set, false = clear.
            /// \return Previous bit value.
            bool Set (
                std::size_t bit,
                bool on);
            /// \brief
            /// Flip the bit at the given position.
            /// \param[in] bit The bit position to flip.
            /// \return Previous bit value.
            bool Flip (std::size_t bit);

            /// \brief
            /// Set all bits to 1.
            void Set ();
            /// \brief
            /// Set all bits to 0.
            void Clear ();
            /// \brief
            /// Flip all bits (0 -> 1, 1 -> 0).
            void Flip ();

            /// \brief
            /// Return count of 1 bits.
            /// \return count of 1 bits.
            std::size_t Count () const;
            /// \brief
            /// Return true if any bits are set.
            /// \return true if any bits are set.
            bool Any () const;
            /// \brief
            /// Return true if no bits are set.
            /// \return true if no bits are set.
            bool None () const;
            /// \brief
            /// Return true if all bits are set.
            /// \return true if all bits are set.
            bool All () const;

            /// \brief
            /// r-value [] operator.
            /// \param[in] bit Which bit to test.
            /// \return true == bit == 1, false == bit == 0.
            inline bool operator [] (std::size_t bit) const {
                return Test (bit);
            }
            /// \struct BitSet::Proxy BitSet.h thekogans/util/BitSet.h
            ///
            /// \brief
            /// Proxy class for l-value [] operator.
            struct Proxy {
                /// \brief
                /// Set the bit to on.
                /// \param[in] on true == set, false == reset.
                /// \return *this.
                Proxy &operator = (bool on) {
                    bitSet->Set (bit, on);
                    return *this;
                }

                /// \brief
                /// Set the bit to the value of another proxy.
                /// \param[in] proxy Proxy to test.
                /// \return *this.
                Proxy &operator = (const Proxy &proxy) {
                    bitSet->Set (bit, bool (proxy));
                    return *this;
                }

                /// \brief
                /// Flip the bit from 0 -> 1, 1 -> 0.
                /// \return *this.
                Proxy &Flip () {
                    bitSet->Flip (bit);
                    return *this;
                }

                /// \brief
                /// Return true if bit is not set, false if set.
                /// \return true if bit is not set, false if set.
                bool operator ~ () const {
                    return !bitSet->Test (bit);
                }

                /// \brief
                /// Return true if the given bit is set.
                /// \return true if the given bit is set.
                operator bool () const {
                    return bitSet->Test (bit);
                }

            private:
                /// \brief
                /// ctor.
                Proxy () :
                    bitSet (0),
                    bit (0) {}

                /// \brief
                /// ctor.
                /// \param[in] bitSet_ BitSet to proxy.
                /// \param[in] bit_ Bit position to proxy.
                Proxy (
                    BitSet *bitSet_,
                    std::size_t bit_) :
                    bitSet (bitSet_),
                    bit (bit_) {}

                /// \brief
                /// BitSet to proxy.
                BitSet *bitSet;
                /// \brief
                /// Bit position to proxy.
                std::size_t bit;

                /// \brief
                /// Needs access to provate ctors.
                friend struct BitSet;
            };
            /// \brief
            /// l-value [] operator.
            /// \param[in] bit Which bit to set.
            /// \return Proxy to set the bit.
            Proxy operator [] (std::size_t bit);

            /// \brief
            /// And the given bit set to this one.
            /// \param[in] bitSet BitSet to and with this one.
            /// \return *this.
            BitSet &operator &= (const BitSet &bitSet);
            /// \brief
            /// Or the given bit set to this one.
            /// \param[in] bitSet BitSet to or with this one.
            /// \return *this.
            BitSet &operator |= (const BitSet &bitSet);
            /// \brief
            /// Xor the given bit set to this one.
            /// \param[in] bitSet BitSet to xor with this one.
            /// \return *this.
            BitSet &operator ^= (const BitSet &bitSet);

            /// \brief
            /// Shift the entire bit set to the left.
            /// \param[in] count Number of bits to shift.
            /// \return *this
            BitSet &operator <<= (std::size_t count);
            /// \brief
            /// Shift the entire bit set to the right.
            /// \param[in] count Number of bits to shift.
            /// \return *this
            BitSet &operator >>= (std::size_t count);

            /// \brief
            /// Return the Not bit set.
            /// \return Not (0 bits become 1, and 1 bits become 0) of the bit set.
            BitSet operator ~ () const;

            /// \brief
            /// Return a copy of the bit set shifted left by the given count.
            /// \param[in] count Number of bits to shift left.
            /// \return A copy of the bit set shifted left by the given count.
            inline BitSet operator << (std::size_t count) const {
                return BitSet (*this) <<= count;
            }

            /// \brief
            /// Return a copy of the bit set shifted right by the given count.
            /// \param[in] count Number of bits to shift right.
            /// \return A copy of the bit set shifted right by the given count.
            inline BitSet operator >> (std::size_t count) const {
                return BitSet (*this) >>= count;
            }

        private:
            /// \brief
            /// Per the standard, the unused bits need to be cleared to 0.
            /// This function accomplishes that and is called by various methods.
            void Trim ();
        };

        /// \brief
        /// And two bit sets together.
        /// \param[in] bitSet1 First bit set to and.
        /// \param[in] bitSet2 Second bit set to and.
        /// \return bitSet1 & bitSet2.
        inline BitSet operator & (
                const BitSet &bitSet1,
                const BitSet &bitSet2) {
            BitSet temp = bitSet1;
            return temp &= bitSet2;
		}

        /// \brief
        /// Or two bit sets together.
        /// \param[in] bitSet1 First bit set to or.
        /// \param[in] bitSet2 Second bit set to or.
        /// \return bitSet1 | bitSet2.
        inline BitSet operator | (
                const BitSet &bitSet1,
                const BitSet &bitSet2) {
            BitSet temp = bitSet1;
            return temp |= bitSet2;
		}

        /// \brief
        /// Xor two bit sets together.
        /// \param[in] bitSet1 First bit set to xor.
        /// \param[in] bitSet2 Second bit set to xor.
        /// \return bitSet1 ^ bitSet2.
        inline BitSet operator ^ (
                const BitSet &bitSet1,
                const BitSet &bitSet2) {
            BitSet temp = bitSet1;
            return temp ^= bitSet2;
		}

        /// \brief
        /// Return true if bitSet1 == bitSet2.
        /// \return true if bitSet1 == bitSet2.
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator == (
            const BitSet &bitSet1,
            const BitSet &bitSet2);

        /// \brief
        /// Return true if bitSet1 != bitSet2.
        /// \return true if bitSet1 != bitSet2.
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator != (
            const BitSet &bitSet1,
            const BitSet &bitSet2);

        /// \brief
        /// Write the given bit set to the given serializer.
        /// \param[in] serializer Where to write the given bit set.
        /// \param[in] bitSet BitSet to write.
        /// \return serializer.
        inline Serializer &operator << (
                Serializer &serializer,
                const BitSet &bitSet) {
            serializer << bitSet.bits << bitSet.size;
            return serializer;
        }

        /// \brief
        /// Read an bit set from the given serializer.
        /// \param[in] serializer Where to read the bit set from.
        /// \param[out] bitSet BitSet to read.
        /// \return serializer.
        inline Serializer &operator >> (
                Serializer &serializer,
                BitSet &bitSet) {
            serializer >> bitSet.bits >> bitSet.size;
            return serializer;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_BitSet_h)
