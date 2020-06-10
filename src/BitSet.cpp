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
#include "thekogans/util/Exception.h"
#include "thekogans/util/BitSet.h"

namespace thekogans {
    namespace util {

        void BitSet::Resize (std::size_t size_) {
            if (size != size_) {
                if (size_ == 0) {
                    bits.clear ();
                }
                else {
                    bits.resize ((size_ + 31) >> 5);
                    Clear ();
                }
                size = size_;
            }
        }

        bool BitSet::Test (std::size_t bit) const {
            if (bit < size) {
                return ((bits[bit >> 5] >> (31 - (bit & 31))) & 1) != 0;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool BitSet::Set (
                std::size_t bit,
                bool on) {
            if (bit < size) {
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
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool BitSet::Flip (std::size_t bit) {
            if (bit < size) {
                return Set (bit, !Test (bit));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void BitSet::Set () {
            if (size > 0) {
                memset (bits.data (), 0xff, bits.size () * UI32_SIZE);
                Trim ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s", "BitSet is empty.");
            }
        }

        void BitSet::Clear () {
            if (size > 0) {
                memset (bits.data (), 0, bits.size () * UI32_SIZE);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s", "BitSet is empty.");
            }
        }

        void BitSet::Flip () {
            for (std::size_t i = 0, count = bits.size (); i < count; ++i) {
                bits[i] = ~bits[i];
            }
            Trim ();
        }

        std::size_t BitSet::Count () const {
            std::size_t count = 0;
            if (size > 0) {
                static const ui8 bitsPerByte[] = {
                    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
                    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
                    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
                    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
                    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
                    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
                    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
                    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
                    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
                    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
                    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
                    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
                    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
                    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
                    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
                    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
                };
                const ui8 *begin = (const ui8 *)(const void *)bits.data ();
                const ui8 *end = begin + (bits.size () << 2);
                for (; begin != end; ++begin) {
                    count += bitsPerByte[*begin];
                }
            }
            return count;
        }

        bool BitSet::Any () const {
            for (std::size_t i = 0, count = bits.size (); i < count; ++i) {
                if (bits[i] != 0) {
                    return true;
                }
            }
            return false;
        }

        bool BitSet::None () const {
            return !Any ();
        }

        bool BitSet::All () const {
            return Count () == BitSize ();
        }

        BitSet::Proxy BitSet::operator [] (std::size_t bit) {
            if (bit < size) {
                return Proxy (this, bit);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        BitSet &BitSet::operator &= (const BitSet &bitSet) {
            if (size == bitSet.size) {
                for (std::size_t i = 0, count = bits.size (); i < count; ++i) {
                    bits[i] &= bitSet.bits[i];
                }
                return *this;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        BitSet &BitSet::operator |= (const BitSet &bitSet) {
            if (size == bitSet.size) {
                for (std::size_t i = 0, count = bits.size (); i < count; ++i) {
                    bits[i] |= bitSet.bits[i];
                }
                return *this;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        BitSet &BitSet::operator ^= (const BitSet &bitSet) {
            if (size == bitSet.size) {
                for (std::size_t i = 0, count = bits.size (); i < count; ++i) {
                    bits[i] ^= bitSet.bits[i];
                }
                return *this;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        BitSet &BitSet::operator <<= (std::size_t count) {
            if (size > 0) {
                std::ptrdiff_t wordCount = (std::ptrdiff_t)(count / 32);
                if (wordCount != 0) {
                    for (std::ptrdiff_t i = bits.size (); --i >= 0;) {
                        bits[i] = wordCount <= i ? bits[i - wordCount] : 0;
                    }
                }
                if ((count %= 32) != 0) {
                    for (std::ptrdiff_t i = bits.size (); --i > 0;) {
                        bits[i] = (bits[i] << count) | (bits[i - 1] >> (32 - count));
                    }
                    bits[0] <<= count;
                }
                Trim ();
                return *this;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s", "BitSet is empty.");
            }
        }

        BitSet &BitSet::operator >>= (std::size_t count) {
            if (size > 0) {
                std::ptrdiff_t wordCount = (std::ptrdiff_t)(count / 32);
                if (wordCount != 0) {
                    for (std::ptrdiff_t i = 0, last = bits.size () - 1; i <= last; ++i) {
                        bits[i] = wordCount <= last - i ? bits[i + wordCount] : 0;
                    }
                }
                if ((count %= 32) != 0) {
                    for (std::ptrdiff_t i = 0, last = bits.size () - 1; i < last; ++i) {
                        bits[i] = (bits[i] >> count) | (bits[i + 1] << (32 - count));
                    }
                    bits.back () >>= count;
                }
                return *this;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s", "BitSet is empty.");
            }
        }

        BitSet BitSet::operator ~ () const {
            BitSet bitSet (*this);
            bitSet.Flip ();
            return bitSet;
        }

        void BitSet::Trim () {
            if (size % 32 != 0) {
                bits.back () &= ((ui32)1 << size % 32) - 1;
            }
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator == (
                const BitSet &bitSet1,
                const BitSet &bitSet2) {
            if (bitSet1.size == bitSet2.size) {
                for (std::size_t i = 0, count = bitSet1.bits.size (); i < count; ++i) {
                    if (bitSet1.bits[i] != bitSet2.bits[i]) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator != (
                const BitSet &bitSet1,
                const BitSet &bitSet2) {
            if (bitSet1.size == bitSet2.size) {
                for (std::size_t i = 0, count = bitSet1.bits.size (); i < count; ++i) {
                    if (bitSet1.bits[i] != bitSet2.bits[i]) {
                        return true;
                    }
                }
                return false;
            }
            return true;
        }

    } // namespace util
} // namespace thekogans
