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

/*
 * Copyright (c) 2008
 * Evan Teran
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appears in all copies and that both the
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the same name not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission. We make no representations about the
 * suitability this software for any purpose. It is provided "as is"
 * without express or implied warranty.
 */
#if !defined (__thekogans_util_ui128_h)
#define __thekogans_util_ui128_h

#include <stdexcept>
#include <string>
#include <climits>
#include <type_traits>

namespace thekogans {
    namespace util {

        namespace {
            template <typename T>
            static void divide (
                    const T &numerator,
                    const T &denominator,
                    T &quotient,
                    T &remainder) {
                static const int bits = sizeof (T) * CHAR_BIT;
                if (denominator == 0) {
                    throw std::domain_error ("divide by zero");
                }
                else {
                    T n = numerator;
                    T d = denominator;
                    T x = 1;
                    T answer = 0;
                    while ((n >= d) && (((d >> (bits - 1)) & 1) == 0)) {
                        x <<= 1;
                        d <<= 1;
                    }
                    while (x != 0) {
                        if (n >= d) {
                            n -= d;
                            answer |= x;
                        }
                        x >>= 1;
                        d >>= 1;
                    }
                    quotient = answer;
                    remainder = n;
                }
            }
        }

        template <typename T>
        struct less_than_comparable {
            friend bool operator > (
                    const T &x,
                    const T &y) {
                return y < x;
            }
            friend bool operator <= (
                    const T &x,
                    const T &y) {
                return !static_cast<bool> (y < x);
            }
            friend bool operator >= (
                    const T &x,
                    const T &y) {
                return !static_cast<bool> (x < y);
            }
        };

        template <typename T>
        struct equality_comparable {
            friend bool operator != (
                    const T &x,
                    const T &y) {
                return !static_cast<bool> (x == y);
            }
        };

        #define THEKOGANS_UTIL_BINARY_OPERATOR(NAME, OP)\
        template <typename T>\
        struct NAME {\
            friend T operator OP (\
                    const T& lhs,\
                    const T& rhs) {\
                T nrv (lhs);\
                nrv OP##= rhs;\
                return nrv;\
            }\
        };

        THEKOGANS_UTIL_BINARY_OPERATOR (multipliable, *)
        THEKOGANS_UTIL_BINARY_OPERATOR (addable, +)
        THEKOGANS_UTIL_BINARY_OPERATOR (subtractable, -)
        THEKOGANS_UTIL_BINARY_OPERATOR (dividable, /)
        THEKOGANS_UTIL_BINARY_OPERATOR (modable, %)
        THEKOGANS_UTIL_BINARY_OPERATOR (xorable, ^)
        THEKOGANS_UTIL_BINARY_OPERATOR (andable, &)
        THEKOGANS_UTIL_BINARY_OPERATOR (orable, |)
        THEKOGANS_UTIL_BINARY_OPERATOR (left_shiftable, <<)
        THEKOGANS_UTIL_BINARY_OPERATOR (right_shiftable, >>)

        #undef THEKOGANS_UTIL_BINARY_OPERATOR

        template <typename T>
        struct incrementable {
            friend T operator ++ (T &x, int) {
                T nrv (x);
                ++x;
                return nrv;
            }
        };

        template <typename T>
        struct decrementable {
            friend T operator -- (T &x, int) {
                T nrv (x);
                --x;
                return nrv;
            }
        };

        struct ui128 :
                less_than_comparable<ui128>,
                equality_comparable<ui128>,
                multipliable<ui128>,
                addable<ui128>,
                subtractable<ui128>,
                dividable<ui128>,
                modable<ui128>,
                xorable<ui128>,
                andable<ui128>,
                orable<ui128>,
                left_shiftable<ui128>,
                right_shiftable<ui128>,
                incrementable<ui128>,
                decrementable<ui128> {
            using BaseType = unsigned long long;

            static const std::size_t BIT_COUNT = (sizeof (BaseType) + sizeof (BaseType)) * CHAR_BIT;

            BaseType lo;
            BaseType hi;

            // constructors for all basic types
            ui128 () :
                lo (0),
                hi (0) {}
            ui128 (
                BaseType lo_,
                BaseType hi_) :
                lo (lo_),
                hi (lo_) {}
            ui128 (int value) :
                    lo (static_cast<BaseType> (value)),
                    hi (0) {
                if (value < 0) {
                    hi = static_cast<BaseType> (-1);
                }
            }
            ui128 (unsigned int value) :
                lo (static_cast<BaseType> (value)),
                hi (0) {}
            ui128 (float value) :
                lo (static_cast<BaseType> (value)),
                hi (0) {}
            ui128 (double value) :
                lo (static_cast<BaseType> (value)),
                hi (0) {}
            ui128 (const ui128 &value) :
                lo (value.lo),
                hi (value.hi) {}
            ui128 (BaseType value) :
                lo (value),
                hi (0) {}
            ui128 (const std::string &sz) :
                    lo (0),
                    hi (0) {
                // do we have at least one character?
                if (!sz.empty ()) {
                    // make some reasonable assumptions
                    int radix = 10;
                    bool minus = false;
                    std::string::const_iterator i = sz.begin ();
                    // check for minus sign, i suppose technically this should only apply
                    // to base 10, but who says that -0x1 should be invalid?
                    if (*i == '-') {
                        ++i;
                        minus = true;
                    }
                    // check if there is radix changing prefix (0 or 0x)
                    if (i != sz.end ()) {
                        if (*i == '0') {
                            radix = 8;
                            ++i;
                            if (i != sz.end ()) {
                                if (*i == 'x') {
                                    radix = 16;
                                    ++i;
                                }
                            }
                        }
                        while (i != sz.end ()) {
                            unsigned int n;
                            const char ch = *i;
                            if (ch >= 'A' && ch <= 'Z') {
                                if (((ch - 'A') + 10) < radix) {
                                    n = (ch - 'A') + 10;
                                }
                                else {
                                    break;
                                }
                            }
                            else if (ch >= 'a' && ch <= 'z') {
                                if (((ch - 'a') + 10) < radix) {
                                    n = (ch - 'a') + 10;
                                }
                                else {
                                    break;
                                }
                            }
                            else if (ch >= '0' && ch <= '9') {
                                if ((ch - '0') < radix) {
                                    n = (ch - '0');
                                }
                                else {
                                    break;
                                }
                            }
                            else {
                                // completely invalid character
                                break;
                            }
                            (*this) *= radix;
                            (*this) += n;
                            ++i;
                        }
                    }
                    // if this was a negative number, do that two's compliment madness :-P
                    if (minus) {
                        *this = -*this;
                    }
                }
            }

            ui128 &operator = (const ui128 &other) {
                if (&other != this) {
                    lo = other.lo;
                    hi = other.hi;
                }
                return *this;
            }

            // comparison operators
            bool operator == (const ui128 &o) const {
                return lo == o.lo && hi == o.hi;
            }

            bool operator < (const ui128 &o) const {
                return (hi == o.hi) ? lo < o.lo : hi < o.hi;
            }

            // unary operators
            bool operator ! () const {
                return !(lo != 0 || hi != 0);
            }

            ui128 operator - () const {
                // standard 2's compliment negation
                return ~ui128 (*this) + 1;
            }

            ui128 operator ~ () const {
                ui128 t (*this);
                t.lo = ~t.lo;
                t.hi = ~t.hi;
                return t;
            }

            ui128 &operator ++ () {
                if (++lo == 0) {
                    ++hi;
                }
                return *this;
            }

            ui128 &operator -- () {
                if (lo-- == 0) {
                    --hi;
                }
                return *this;
            }

            // basic math operators
            ui128 &operator += (const ui128 &b) {
                const BaseType old_lo = lo;
                lo += b.lo;
                hi += b.hi;
                if (lo < old_lo) {
                    ++hi;
                }
                return *this;
            }

            ui128 &operator -= (const ui128 &b) {
                // it happens to be way easier to write it
                // this way instead of make a subtraction algorithm
                return *this += -b;
            }

            ui128 &operator *= (const ui128 &b) {
                // check for multiply by 0
                // result is always 0 :-P
                if (b == 0) {
                    lo = 0;
                    hi = 0;
                }
                else if (b != 1) {
                    // check we aren't multiplying by 1
                    ui128 a (*this);
                    ui128 t = b;
                    lo = 0;
                    hi = 0;
                    for (unsigned int i = 0; i < BIT_COUNT; ++i) {
                        if ((t & 1) != 0) {
                            *this += (a << i);
                        }
                        t >>= 1;
                    }
                }
                return *this;
            }

            ui128 &operator |= (const ui128 &b) {
                lo |= b.lo;
                hi |= b.hi;
                return *this;
            }

            ui128 &operator &= (const ui128 &b) {
                lo &= b.lo;
                hi &= b.hi;
                return *this;
            }

            ui128 &operator ^= (const ui128 &b) {
                lo ^= b.lo;
                hi ^= b.hi;
                return *this;
            }

            ui128 &operator /= (const ui128 &b) {
                ui128 remainder;
                divide (*this, b, *this, remainder);
                return *this;
            }

            ui128 &operator %= (const ui128 &b) {
                ui128 quotient;
                divide (*this, b, quotient, *this);
                return *this;
            }

            ui128 &operator <<= (const ui128 &rhs) {
                unsigned int n = rhs.ToInteger ();
                if (n >= BIT_COUNT) {
                    lo = 0;
                    hi = 0;
                }
                else {
                    const unsigned int halfSize = BIT_COUNT / 2;
                    if (n >= halfSize) {
                        n -= halfSize;
                        lo = 0;
                        hi = lo;
                    }
                    if (n != 0) {
                        // shift high half
                        hi <<= n;
                        const BaseType mask (~(BaseType (-1) >> n));
                        // and add them to high half
                        hi |= (lo & mask) >> (halfSize - n);
                        // and finally shift also low half
                        lo <<= n;
                    }
                }
                return *this;
            }

            ui128 &operator >>= (const ui128 &rhs) {
                unsigned int n = rhs.ToInteger ();
                if (n >= BIT_COUNT) {
                    lo = 0;
                    hi = 0;
                }
                else {
                    const unsigned int halfSize = BIT_COUNT / 2;
                    if (n >= halfSize) {
                        n -= halfSize;
                        lo = hi;
                        hi = 0;
                    }
                    if (n != 0) {
                        // shift low half
                        lo >>= n;
                        // get lower N bits of high half
                        const BaseType mask (~(BaseType (-1) << n));
                        // and add them to low qword
                        lo |= (hi & mask) << (halfSize - n);
                        // and finally shift also high half
                        hi >>= n;
                    }
                }
                return *this;
            }

            int ToInteger () const {
                return static_cast<int> (lo);
            }

            BaseType ToBaseType () const {
                return lo;
            }

            std::string ToString (unsigned int radix = 10) const {
                if (*this == 0) {
                    return "0";
                }
                if (radix < 2 || radix > 37) {
                    return "(invalid radix)";
                }
                // at worst it will be BIT_COUNT digits (base 2) so make our buffer
                // that plus room for null terminator
                char sz[BIT_COUNT + 1];
                sz[sizeof (sz) - 1] = '\0';
                ui128 ii (*this);
                int i = BIT_COUNT - 1;
                while (ii != 0 && i) {
                    ui128 remainder;
                    divide (ii, ui128 (radix), ii, remainder);
                    sz[--i] = "0123456789abcdefghijklmnopqrstuvwxyz"[remainder.ToInteger ()];
                }
                return &sz[i];
            }
        };

        #define UI128_C(s) thekogans::util::ui128 (#s)

    } // namespace util
} // namespace thekogans

namespace std {
    template<>
    struct is_integral<thekogans::util::ui128> : true_type {};
    template<>
    struct is_arithmetic<thekogans::util::ui128> : true_type {};
}

#endif // __thekogans_util_ui128_h
