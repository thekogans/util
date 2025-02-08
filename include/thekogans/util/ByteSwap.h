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

#if !defined (__thekogans_util_ByteSwap_h)
#define __thekogans_util_ByteSwap_h

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_OSX)
    #include <libkern/OSByteOrder.h>
#endif // defined (TOOLCHAIN_OS_OSX)
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#include <type_traits>
#include "thekogans/util/Types.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Implements a ByteSwap template to deal with host endianess.
        ///
        /// This code came from: http://www.cplusplus.com/forum/general/27544/\n
        /// I cleaned it up a bit to use my type system, and style.
        /// Thanks goes to the original author: Steve Lorimer
        ///
        /// Little-endian operating systems:\n
        /// --------------------------------\n
        /// Linux on x86, x64, Alpha and Itanium\n
        /// Mac OS on x86, x64\n
        /// Solaris on x86, x64, PowerPC\n
        /// Tru64 on Alpha\n
        /// Windows on x86, x64 and Itanium
        ///
        /// Big-endian operating systems:\n
        /// -----------------------------\n
        /// AIX on POWER\n
        /// AmigaOS on PowerPC and 680x0\n
        /// HP-UX on Itanium and PA-RISC\n
        /// Linux on MIPS, SPARC, PA-RISC, POWER, PowerPC, and 680x0\n
        /// Mac OS on PowerPC and 680x0\n
        /// Solaris on SPARC\n

        namespace detail {
            /// \brief
            /// Swap bytes template.
            template<typename T, std::size_t size>
            struct SwapBytes {
                inline T operator () (T /*value*/) {
                    throw std::out_of_range ("data size");
                }
            };

            /// \brief
            /// Swap bytes template specialization for 1 byte values.
            template<typename T>
            struct SwapBytes<T, 1> {
                /// \brief
                /// Function call operator to perform the swap.
                /// \param[in] value Value whose bytes to swap.
                /// \return Byte swapped value.
                inline T operator () (T value) {
                    return value;
                }
            };

            /// \brief
            /// Swap bytes template specialization for 2 byte values.
            template<typename T>
            struct SwapBytes<T, 2> { // for 16 bit
                /// \brief
                /// Function call operator to perform the swap.
                /// \param[in] value Value whose bytes to swap.
                /// \return Byte swapped value.
                inline T operator () (T value) {
                #if defined (TOOLCHAIN_OS_Windows)
                    return _byteswap_ushort (value);
                #elif defined (__GNUC__) &&\
                        ((__GNUC__ == 4 && __GNUC_MINOR__ >= 8) || __GNUC__ > 4)
                    return __builtin_bswap16 (value);
                #elif defined (TOOLCHAIN_OS_OSX)
                    return OSSwapInt16 (value);
                #else
                    return ((((value) >> 8) & 0xff) | (((value) & 0xff) << 8));
                #endif // defined (TOOLCHAIN_OS_Windows)
                }
            };

            /// \brief
            /// Swap bytes template specialization for 4 byte values.
            template<typename T>
            struct SwapBytes<T, 4> { // for 32 bit
                /// \brief
                /// Function call operator to perform the swap.
                /// \param[in] value Value whose bytes to swap.
                /// \return Byte swapped value.
                inline T operator () (T value) {
                #if defined (TOOLCHAIN_OS_Windows)
                    return _byteswap_ulong (value);
                #elif defined (__GNUC__) &&\
                        ((__GNUC__ == 4 && __GNUC_MINOR__ >= 3) || __GNUC__ > 4)
                    return __builtin_bswap32 (value);
                #elif defined (TOOLCHAIN_OS_OSX)
                    return OSSwapInt32 (value);
                #else
                    return ((((value) & 0xff000000) >> 24) |
                        (((value) & 0x00ff0000) >> 8) |
                        (((value) & 0x0000ff00) << 8) |
                        (((value) & 0x000000ff) << 24));
                #endif // defined (TOOLCHAIN_OS_Windows)
                }
            };

            /// \brief
            /// Swap bytes template specialization for 4 byte floats.
            template<>
            struct SwapBytes<f32, 4> {
                /// \brief
                /// Function call operator to perform the swap.
                /// \param[in] value Value whose bytes to swap.
                /// \return Byte swapped value.
                inline f32 operator () (f32 value) {
                    ui32 *ptr = (ui32 *)&value;
                    *ptr = SwapBytes<ui32, 4> () (*ptr);
                    return value;
                }
            };

            /// \brief
            /// Swap bytes template specialization for 8 byte values.
            template<typename T>
            struct SwapBytes<T, 8> { // for 64 bit
                /// \brief
                /// Function call operator to perform the swap.
                /// \param[in] value Value whose bytes to swap.
                /// \return Byte swapped value.
                inline T operator () (T value) {
                #if defined (TOOLCHAIN_OS_Windows)
                    return _byteswap_uint64 (value);
                #elif defined (__GNUC__) &&\
                        ((__GNUC__ == 4 && __GNUC_MINOR__ >= 3) || __GNUC__ > 4)
                    return __builtin_bswap64 (value);
                #elif defined (TOOLCHAIN_OS_OSX)
                    return OSSwapInt64 (value);
                #else
                    return ((((value) & 0xff00000000000000ull) >> 56) |
                        (((value) & 0x00ff000000000000ull) >> 40) |
                        (((value) & 0x0000ff0000000000ull) >> 24) |
                        (((value) & 0x000000ff00000000ull) >> 8) |
                        (((value) & 0x00000000ff000000ull) << 8) |
                        (((value) & 0x0000000000ff0000ull) << 24) |
                        (((value) & 0x000000000000ff00ull) << 40) |
                        (((value) & 0x00000000000000ffull) << 56));
                #endif // defined (TOOLCHAIN_OS_Windows)
                }
            };

            /// \brief
            /// Swap bytes template specialization for 8 byte floats.
            template<>
            struct SwapBytes<f64, 8> {
                /// \brief
                /// Function call operator to perform the swap.
                /// \param[in] value Value whose bytes to swap.
                /// \return Byte swapped value.
                inline f64 operator () (f64 value) {
                    ui64 *ptr = (ui64 *)&value;
                    *ptr = SwapBytes<ui64, 8> () (*ptr);
                    return value;
                }
            };

            /// \brief
            /// Swap bytes template.
            template<Endianness from, Endianness to, typename T>
            struct DoSwapBytes {
                /// \brief
                /// Function call operator to perform the swap.
                /// \param[in] value Value whose bytes to swap.
                /// \return Byte swapped value.
                inline T operator () (T value) {
                    return SwapBytes<T, sizeof (T)> () (value);
                }
            };

            /// \brief
            /// Specialisation when attempting to swap to the same
            /// endianess (LittleEndian).
            template<typename T>
            struct DoSwapBytes<LittleEndian, LittleEndian, T> {
                /// \brief
                /// Function call operator to perform the swap.
                /// \param[in] value Value whose bytes to swap.
                /// \return Byte swapped value.
                inline T operator () (T value) {
                    return value;
                }
            };
            /// \brief
            /// Specialisation when attempting to swap to the same
            /// endianess (BigEndian).
            template<typename T>
            struct DoSwapBytes<BigEndian, BigEndian, T> {
                /// \brief
                /// Function call operator to perform the swap.
                /// \param[in] value Value whose bytes to swap.
                /// \return Byte swapped value.
                inline T operator () (T value) {
                    return value;
                }
            };
        } // namespace detail

        /// \brief
        /// Convert a given value from byte order to byte order.
        /// \param[in] value Value to convert.
        /// \return Converted value.
        template<
            Endianness from,
            Endianness to,
            typename T>
        inline T ByteSwap (T value) {
            // Ensure value is 1, 2, 4 or 8 bytes.
            static_assert (
                sizeof (T) == I8_SIZE || sizeof (T) == UI8_SIZE ||
                sizeof (T) == I16_SIZE || sizeof (T) == UI16_SIZE ||
                sizeof (T) == I32_SIZE || sizeof (T) == UI32_SIZE ||
                sizeof (T) == I64_SIZE || sizeof (T) == UI64_SIZE ||
                sizeof (T) == F32_SIZE || sizeof (T) == F64_SIZE,
                "Template parameter must be the size of an integral type.");
            // Ensure value is an arithmetic type.
            static_assert (
                std::is_arithmetic<T>::value,
                "Template parameter must be an arithmetic type.");
            return detail::DoSwapBytes<from, to, T> () (value);
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_ByteSwap_h)
