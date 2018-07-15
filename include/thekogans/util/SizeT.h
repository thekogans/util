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

#if !defined (__thekogans_util_SizeT_h)
#define __thekogans_util_SizeT_h

#if defined (TOOLCHAIN_OS_Windows)
    #include <intrin.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"

namespace thekogans {
    namespace util {

        struct Serializer;

        /// \struct SizeT SizeT.h thekogans/util/SizeT.h
        ///
        /// \brief
        /// SizeT implements an architecture independent, Prefix-encoded, variable length
        /// serialized std::szie_t.

        struct _LIB_THEKOGANS_UTIL_DECL SizeT {
            /// \brief
            /// Use ui64 to represent architecture independent size (i386, x86_64...).
            ui64 value;

            /// \brief
            /// ctor.
            /// \param[in] value_ Value to initialize to.
            SizeT (ui64 value_ = 0) :
                value (value_) {}

            /// \brief
            /// Return the serialized size of the current value.
            /// \return Serialized size of the current value.
            inline std::size_t Size () const {
                return (63 - __builtin_clzll (value | 1)) / 7 + 1;
            }

            /// \brief
            /// Given the first byte return the total size of the serialized SizeT
            /// (including the fisrt byte).
            /// \param[in] firstByte Encodes the total size of the serialized SizeT.
            /// \return Size of the serialized SizeT.
            inline static std::size_t Size (ui32 firstByte) {
                return __builtin_ctz (firstByte | 0x100) + 1;
            }

            /// \brief
            /// Implicit typecast operator.
            inline operator ui64 () const {
                return value;
            }

            /// \brief
            /// Assignment operator.
            /// \param[in] value_ Value to assign.
            /// \return *this;
            inline SizeT &operator = (ui64 value_) {
                value = value_;
                return *this;
            }
            /// \brief
            /// Addition operator.
            /// \param[in] value_ Value to add.
            /// \return *this.
            inline SizeT &operator += (ui64 value_) {
                value += value_;
                return *this;
            }
            /// \brief
            /// Subtractin operator.
            /// \param[in] value_ Value to subtract.
            /// \return *this.
            inline SizeT &operator -= (ui64 value_) {
                value -= value_;
                return *this;
            }
            /// \brief
            /// Multiplication operator.
            /// \param[in] value_ Value to multiply.
            /// \return *this.
            inline SizeT &operator *= (ui64 value_) {
                value *= value_;
                return *this;
            }
            /// \brief
            /// Division operator.
            /// \param[in] value_ Value to divide.
            /// \return *this.
            inline SizeT &operator /= (ui64 value_) {
                value /= value_;
                return *this;
            }
            /// \brief
            /// Bitwise and operator.
            /// \param[in] value_ Value to and.
            /// \return *this.
            inline SizeT &operator &= (ui64 value_) {
                value &= value_;
                return *this;
            }
            /// \brief
            /// Bitwise or operator.
            /// \param[in] value_ Value to or.
            /// \return *this.
            inline SizeT &operator |= (ui64 value_) {
                value |= value_;
                return *this;
            }
            /// \brief
            /// Exclusive or operator.
            /// \param[in] value_ Value to exclusive or.
            /// \return *this.
            inline SizeT &operator ^= (ui64 value_) {
                value ^= value_;
                return *this;
            }
            /// \brief
            /// Mod operator.
            /// \param[in] value_ Value to mod.
            /// \return *this.
            inline SizeT &operator %= (ui64 value_) {
                value %= value_;
                return *this;
            }
            /// \brief
            /// Shift left operator.
            /// \param[in] value_ Bitcount to shift.
            /// \return *this.
            inline SizeT &operator <<= (ui64 count) {
                value <<= count;
                return *this;
            }
            /// \brief
            /// Shift right operator.
            /// \param[in] value_ Bitcount to shift.
            /// \return *this.
            inline SizeT &operator >>= (ui64 count) {
                value >>= count;
                return *this;
            }
            /// \brief
            /// Pre-increment operator.
            /// \return *this.
            inline SizeT &operator ++ () {
                ++value;
                return *this;
            }
            /// \brief
            /// Post-increment operator.
            /// \return Copy of SizeT before the increment.
            inline SizeT operator ++ (int) {
                SizeT copy (value);
                ++value;
                return copy;
            }
            /// \brief
            /// Pre-decrement operator.
            /// \return *this;
            inline SizeT &operator -- () {
                --value;
                return *this;
            }
            /// \brief
            /// Post-decrement operator.
            /// \return Copy of SizeT before the decrement.
            inline SizeT operator -- (int) {
                SizeT copy (value);
                --value;
                return copy;
            }

    #if defined (TOOLCHAIN_OS_Windows)
        private:
            /// \brief
            /// Emulate __builtin_clzll on Windows.
            /// \param[in] value Value whose leading zero count to return.
            /// \return Leading zero count of the given value.
            inline std::size_t __builtin_clzll (ui64 value) {
                unsigned long leadingZero = 0;
                _BitScanReverse64 (&leadingZero, value);
                return 63 - leadingZero;
            }

            /// \brief
            /// Emulate __builtin_ctz on Windows.
            /// \param[in] value Value whose trailing zero count to return.
            /// \return Trailing zero count of the given value.
            inline std::size_t __builtin_ctz (ui32 value) {
                unsigned long trailingZero = 0;
                _BitScanForward (&trailingZero, value);
                return trailingZero;
            }
    #endif // defined (TOOLCHAIN_OS_Windows)
        };

        /// \brief
        /// Write the given SizeT to the given \see{Serializer}.
        /// \param[in] serializer Where to write the given SizeT.
        /// \param[in] sizeT SizeT to write.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
            Serializer &serializer,
            const SizeT &sizeT);
        /// \brief
        /// Read a SizeT from the given \see{Serializer}.
        /// \param[in] serializer Where to read the SizeT from.
        /// \param[out] sizeT SizeT to read.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
            Serializer &serializer,
            SizeT &sizeT);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SizeT_h)
