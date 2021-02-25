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

#if !defined (__thekogans_util_Constants_h)
#define __thekogans_util_Constants_h

#include <cstddef>
#include <climits>
#include <cfloat>
#include <type_traits>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"

namespace thekogans {
    namespace util {

        /// \def THEKOGANS_UTIL_I64_LITERAL(n)
        /// Macro to define a platform independent
        /// 64 bit integer literal.
        #if defined (_MSC_VER)
            #define THEKOGANS_UTIL_I64_LITERAL(n) n##I64
        #else // defined (_MSC_VER)
            #define THEKOGANS_UTIL_I64_LITERAL(n) n##LL
        #endif // defined (_MSC_VER)

        /// \def THEKOGANS_UTIL_UI64_LITERAL(n)
        /// Macro to define a platform independent
        /// 64 bit unsigned integer literal.
        #if defined (_MSC_VER)
            #define THEKOGANS_UTIL_UI64_LITERAL(n) n##UI64
        #else // defined (_MSC_VER)
            #define THEKOGANS_UTIL_UI64_LITERAL(n) n##ULL
        #endif // defined (_MSC_VER)

        /// \brief
        /// Smallest 64 bit integer.
    #if !defined (LONGLONG_MIN)
        #define LONGLONG_MIN THEKOGANS_UTIL_I64_LITERAL (0x8000000000000000)
    #endif // !defined (LONGLONG_MIN)

        /// \brief
        /// Largest 64 bit integer.
    #if !defined (LONGLONG_MAX)
        #define LONGLONG_MAX THEKOGANS_UTIL_I64_LITERAL (0x7FFFFFFFFFFFFFFF)
    #endif // !defined (LONGLONG_MAX)

        /// \brief
        /// Largest 64 bit unsigned integer.
    #if !defined (ULONGLONG_MAX)
        #define ULONGLONG_MAX THEKOGANS_UTIL_UI64_LITERAL (0xffffffffffffffff)
    #endif // !defined (ULONGLONG_MAX)

        /// \brief
        /// Smallest 8 bit integer.
        const i8 I8_MIN = (i8)0x80;
        /// \brief
        /// Largest 8 bit integer.
        const i8 I8_MAX = 0x7f;

        /// \brief
        /// Smallest 8 bit unsigned integer.
        const ui8 UI8_MIN = 0x00;
        /// \brief
        /// Largest 8 bit unsigned integer.
        const ui8 UI8_MAX = 0xffU;

        /// \brief
        /// Smallest 16 bit integer.
        const i16 I16_MIN = (i16)0x8000;
        /// \brief
        /// Largest 16 bit integer.
        const i16 I16_MAX = 0x7fff;

        /// \brief
        /// Smallest 16 bit unsigned integer.
        const ui16 UI16_MIN = 0x0000;
        /// \brief
        /// Largest 16 bit unsigned integer.
        const ui16 UI16_MAX = 0xffffU;

        /// \brief
        /// Smallest 32 bit integer.
        const i32 I32_MIN = 0x80000000;
        /// \brief
        /// Largest 32 bit integer.
        const i32 I32_MAX = 0x7fffffff;

        /// \brief
        /// Smallest 32 bit unsigned integer.
        const ui32 UI32_MIN = 0x00000000;
        /// \brief
        /// Largest 32 bit unsigned integer.
        const ui32 UI32_MAX = 0xffffffffU;

        /// \brief
        /// Smallest 64 bit integer.
        const i64 I64_MIN = LONGLONG_MIN;
        /// \brief
        /// Largest 64 bit integer.
        const i64 I64_MAX = LONGLONG_MAX;

        /// \brief
        /// Smallest 64 bit unsigned integer.
        const ui64 UI64_MIN = 0;
        /// \brief
        /// Largest 64 bit unsigned integer.
        const ui64 UI64_MAX = ULONGLONG_MAX;

        /// \brief
        /// Smallest 32 bit float.
        const f32 F32_MIN = FLT_MIN;
        /// \brief
        /// Largest 32 bit float.
        const f32 F32_MAX = FLT_MAX;

        /// \brief
        /// Smallest 64 bit float.
        const f64 F64_MIN = DBL_MIN;
        /// \brief
        /// Largest 64 bit float.
        const f64 F64_MAX = DBL_MAX;

        /// \brief
        /// 16 bit invalid index.
        const ui16 NIDX16 = UI16_MAX;
        /// \brief
        /// 32 bit invalid index.
        const ui32 NIDX32 = UI32_MAX;
        /// \brief
        /// 64 bit invalid index.
        const ui64 NIDX64 = UI64_MAX;

        /// \brief
        /// 16 bit magic number.
        const ui16 MAGIC16 = 0x4246; // "BF"
        /// \brief
        /// 32 bit magic number.
        const ui32 MAGIC32 = 0x46415253; // "FARS"
        /// \brief
        /// 64 bit magic number.
        const ui64 MAGIC64 = 0x4246415253544B4E; // "BFARSTKN"

        /// \brief
        /// Architecture word size invalid index, and magic number.
    #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_ppc) || defined (TOOLCHAIN_ARCH_arm)
        const std::size_t NIDX = NIDX32;
        const std::size_t MAGIC = MAGIC32;
    #if !defined (SIZE_T_MAX)
        #define SIZE_T_MAX thekogans::util::UI32_MAX
    #endif // !defined (SIZE_T_MAX)
    #elif defined (TOOLCHAIN_ARCH_x86_64) || defined (TOOLCHAIN_ARCH_ppc64) || defined (TOOLCHAIN_ARCH_arm64)
        const std::size_t NIDX = NIDX64;
        const std::size_t MAGIC = MAGIC64;
    #if !defined (SIZE_T_MAX)
        #define SIZE_T_MAX thekogans::util::UI64_MAX
    #endif // !defined (SIZE_T_MAX)
    #else // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_ppc) || defined (TOOLCHAIN_ARCH_arm)
        #error Unknown TOOLCHAIN_ARCH.
    #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_ppc) || defined (TOOLCHAIN_ARCH_arm)

        /// \brief
        /// Ethernet MAC length.
        const std::size_t MAC_LENGTH = 6;

        /// \brief
        /// Fudge factor. Every routine which compares
        /// two f32s takes an eps parameter. eps defaults
        /// to EPSILON.
    #if !defined (EPSILON)
        #define EPSILON 0.000001f
    #endif // !defined (EPSILON)

        // conversion factors

        /// \brief
        /// Centimeters to meters.
        const f32 CM2M = 0.01f;
        /// \brief
        /// Feet to meters.
        const f32 FT2M = 0.304878f;
        /// \brief
        /// Inches to meters.
        const f32 IN2M = 0.025381f;
        /// \brief
        /// Meters to feet.
        const f32 M2FT = 3.28f;
        /// \brief
        /// Centimeters to feet.
        const f32 CM2FT = 0.032787f;
        /// \brief
        /// Inches to feet.
        const f32 IN2FT = 0.083333f;

        /// \brief
        /// PI
    #if !defined (PI)
        #define PI 3.14159265358979323846f
    #endif // !defined (PI)
        /// \brief
        /// PI / 2
    #if !defined (HALFPI)
        #define HALFPI (0.5f * PI)
    #endif // !defined (HALFPI)
        /// \brief
        /// 2 * PI
    #if !defined (TWOPI)
        #define TWOPI (2.0f * PI)
    #endif // !defined (TWOPI)

        /// \brief
        /// PI / 180
        const f32 PIOVER180 = 1.74532925199433E-002f;
        /// \brief
        /// 180 / PI
        const f32 PIUNDER180 = 5.72957795130823E+001f;

        /// \brief
        /// Convert radians to degrees.
        /// \param[in] x Angle in radians.
        /// \return Angle in degrees.
        inline f32 DEG (f32 x) {
            return x * PIUNDER180;
        }
        /// \brief
        /// Convert degrees to radians.
        /// \param[in] x Angle in degrees.
        /// \return Angle in radians.
        inline f32 RAD (f32 x) {
            return x * PIOVER180;
        }
        /// \brief
        /// Round a float to the nearest integer.
        /// \param[in] x float to round.
        /// \return Nearest integer.
        inline i32 ROUND (f32 x) {
            return x > 0 ? (i32)(x + 0.5f) : -(i32)(0.5f - x);
        }
        /// \brief
        /// Return the sign of the given float.
        /// \param[in] x float whose sign to test.
        /// \return -1 = negative, 0 = zero, 1 = positive.
        inline i32 SIGN (f32 x) {
            return x < 0.0f ? -1 : x > 0.0f ? 1 : 0;
        }
        /// \brief
        /// Return the sign of the given float.
        /// \param[in] x float whose sign to test.
        /// \return -1 = negative, 1 = positive or zero.
        inline i32 SIGN2 (f32 x) {
            return x < 0.0f ? -1 : 1;
        }

        /// \brief
        /// Test for zero within a given tolerance.
        /// \param[in] x Value to test.
        /// \param[in] eps Tolerance to test to.
        /// \return true = zero, false = not zero.
        inline bool IS_ZERO (
                f32 x,
                f32 eps = EPSILON) {
            return x > -eps && x < eps;
        }
        /// \brief
        /// Test two values for equality within a given tolerance.
        /// \param[in] x1 First value to test.
        /// \param[in] x2 Second value to test.
        /// \param[in] eps Tolerance to test to.
        /// \return true = equal, false = not equal.
        inline bool IS_EQ (
                f32 x1,
                f32 x2,
                f32 eps = EPSILON) {
            return IS_ZERO (x1 - x2, eps);
        }
        /// \brief
        /// Test two values for difference within a given tolerance.
        /// \param[in] x1 First value to test.
        /// \param[in] x2 second value to test.
        /// \param[in] eps Tolerance to test to.
        /// \return true = not equal, false = equal.
        inline bool IS_NE (
                f32 x1,
                f32 x2,
                f32 eps = EPSILON) {
            return !IS_EQ (x1, x2, eps);
        }
        /// \brief
        /// Test two values for strict order within a given tolerance.
        /// \param[in] x1 First value to test.
        /// \param[in] x2 Second value to test.
        /// \param[in] eps Tolerance to test to.
        /// \return true = x1 < x2, false = x1 >= x2.
        inline bool IS_LT (
                f32 x1,
                f32 x2,
                f32 eps = EPSILON) {
            return x1 + eps < x2;
        }
        /// \brief
        /// Test two values for order within a given tolerance.
        /// \param[in] x1 First value to test.
        /// \param[in] x2 second value to test.
        /// \param[in] eps Tolerance to test to.
        /// \return true = x1 <= x2, false = x1 > x2.
        inline bool IS_LE (
                f32 x1,
                f32 x2,
                f32 eps = EPSILON) {
            return x1 < x2 || IS_EQ (x1, x2, eps);
        }
        /// \brief
        /// Test two values for strict order within a given tolerance.
        /// \param[in] x1 First value to test.
        /// \param[in] x2 Second value to test.
        /// \param[in] eps Tolerance to test to.
        /// \return true = x1 > x2, false = x1 <= x2.
        inline bool IS_GT (
                f32 x1,
                f32 x2,
                f32 eps = EPSILON) {
            return x1 > x2 + eps;
        }
        /// \brief
        /// Test two values for order within a given tolerance.
        /// \param[in] x1 First value to test.
        /// \param[in] x2 Second value to test.
        /// \param[in] eps Tolerance to test to.
        /// \return true = x1 >= x2, false = x1 < x2.
        inline bool IS_GE (
                f32 x1,
                f32 x2,
                f32 eps = EPSILON) {
            return x1 > x2 || IS_EQ (x1, x2, eps);
        }
        /// \brief
        /// Test a value for range (exclusive).
        /// \param[in] x Value to test.
        /// \param[in] min Minimum range value.
        /// \param[in] max Maximum range value.
        /// \return true = min < x < max, false = x <= min or x >= max.
        inline bool IS_BETWEEN (
                f32 x,
                f32 min,
                f32 max) {
            return (min < x && x < max) || (max < x && x < min);
        }
        /// \brief
        /// Test a value for range (inclusive).
        /// \param[in] x Value to test.
        /// \param[in] min Minimum range value.
        /// \param[in] max Maximum range value.
        /// \param[in] eps Tolerance to test to.
        /// \return true = min <= x <= max, false = x < min or x > max.
        inline bool IS_BETWEEN_EQ (
                f32 x,
                f32 min,
                f32 max,
                f32 eps = EPSILON) {
            return IS_BETWEEN (x, min, max) || IS_EQ (x, min, eps) || IS_EQ (x, max, eps);
        }
        /// \brief
        /// Compare two values.
        /// \param[in] x1 First value to compare.
        /// \param[in] x2 Second value to compare.
        /// \param[in] eps Tolerance to test to.
        /// \return true = x1 == x2, false = x1 != x2.
        inline i32 COMPARE (
                f32 x1,
                f32 x2,
                f32 eps = EPSILON) {
            return IS_EQ (x1, x2, eps) ? 0 : x1 > x2 ? 1 : -1;
        }

        /// \brief
        /// Clamp value to range.
        /// \param[in] x Value to clamp.
        /// \param[in] min Minimum range value.
        /// \param[in] max Maximum range value.
        /// \return min <= x <= max.
        template <typename T>
        inline T CLAMP (
                const T &x,
                const T &min,
                const T &max) {
            return x < min ? min : x > max ? max : x;
        }

        /// \brief
        /// Interpolate within a given range.
        /// \param[in] t Interpolation point (0 < t < 1).
        /// \param[in] x1 Minimum range value.
        /// \param[in] x2 Maximum range value.
        /// \return x1 <= x <= x2.
        template <typename T>
        inline T LERP (
                f32 t,
                const T &x1,
                const T &x2) {
            return (T)(x1 + t * (x2 - x1));
        }

        /// \brief
        /// Exchange the given object with the new value and return the old value.
        /// \param[in, out] object Object whose value to excahnge.
        /// \param[in] newValue Value to set the object to.
        /// \return Old value.
        template<typename T>
        T EXCHANGE (
                T &object,
                const T &newValue) {
            T oldValue = object;
            object = newValue;
            return oldValue;
        }

        /// \brief
        /// Return true if the given arithmetic value is even.
        /// \param[in] value Arithmetic value to test.
        /// \return true if the given arithmetic value is even.
        template<typename T>
        bool IS_EVEN (T value) {
            // Ensure value is an arithmetic type.
            static_assert (
                std::is_arithmetic<T>::value,
                "Template parameter must be an arithmetic type.");
            return (value % 2) == 0;
        }

        /// \brief
        /// Return true if the given arithmetic value is odd.
        /// \param[in] value Arithmetic value to test.
        /// \return true if the given arithmetic value is odd.
        template<typename T>
        bool IS_ODD (T value) {
            // Ensure value is an arithmetic type.
            static_assert (
                std::is_arithmetic<T>::value,
                "Template parameter must be an arithmetic type.");
            return (value % 2) == 1;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Constants_h)
