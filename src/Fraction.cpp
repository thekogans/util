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

#include "thekogans/util/Types.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Fraction.h"

namespace thekogans {
    namespace util {

        #if !defined (THEKOGANS_UTIL_MIN_FRACTIONS_IN_PAGE)
            #define THEKOGANS_UTIL_MIN_FRACTIONS_IN_PAGE 64
        #endif // !defined (THEKOGANS_UTIL_MIN_FRACTIONS_IN_PAGE)

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (
            Fraction,
            1,
            SpinLock,
            THEKOGANS_UTIL_MIN_FRACTIONS_IN_PAGE,
            DefaultAllocator::Global)

        const Fraction Fraction::Zero (0, 1);
        const Fraction Fraction::One (1, 1);

        std::string Fraction::signTostring (Sign sign) {
            return sign == Positive ? VALUE_POSITIVE : VALUE_NEGATIVE;
        }

        Fraction::Sign Fraction::stringTosign (const std::string &sign) {
            return sign == VALUE_POSITIVE ? Positive : Negative;
        }

        namespace {
            // Euclid's greatest common divisor algorithm.
            ui32 GCD (
                    ui32 a,
                    ui32 b) {
                while (b > 0) {
                    ui32 t = b;
                    b = a % b;
                    a = t;
                }
                return a;
            }
        }

        Fraction &Fraction::operator = (const Fraction &fraction) {
            if (&fraction != this) {
                numerator = fraction.numerator;
                denominator = fraction.denominator;
                sign = fraction.sign;
            }
            return *this;
        }

        Fraction &Fraction::operator += (const Fraction &fraction) {
            ui32 numerator1 = numerator * fraction.denominator;
            ui32 numerator2 = fraction.numerator * denominator;
            if (sign == Fraction::Positive) {
                if (fraction.sign == Fraction::Positive) {
                    numerator = numerator1 + numerator2;
                }
                else if (numerator1 < numerator2) {
                    numerator = numerator2 - numerator1;
                    sign = Fraction::Negative;
                }
                else {
                    numerator = numerator1 - numerator2;
                }
            }
            else if (fraction.sign == Fraction::Positive) {
                if (numerator1 < numerator2) {
                    numerator = numerator2 - numerator1;
                    sign = Fraction::Positive;
                }
                else {
                    numerator = numerator1 - numerator2;
                }
            }
            else {
                numerator = numerator1 + numerator2;
            }
            denominator *= fraction.denominator;
            Reduce ();
            return *this;
        }

        Fraction &Fraction::operator -= (const Fraction &fraction) {
            ui32 numerator1 = numerator * fraction.denominator;
            ui32 numerator2 = fraction.numerator * denominator;
            if (sign == Fraction::Positive) {
                if (fraction.sign == Fraction::Positive) {
                    if (numerator1 < numerator2) {
                        numerator = numerator2 - numerator1;
                        sign = Fraction::Negative;
                    }
                    else {
                        numerator = numerator1 - numerator2;
                    }
                }
                else {
                    numerator = numerator1 + numerator2;
                }
            }
            else if (fraction.sign == Fraction::Positive) {
                numerator = numerator1 + numerator2;
            }
            else if (numerator1 < numerator2) {
                numerator = numerator2 - numerator1;
                sign = Fraction::Positive;
            }
            else {
                numerator = numerator1 - numerator2;
            }
            denominator *= fraction.denominator;
            Reduce ();
            return *this;
        }

        Fraction &Fraction::operator *= (const Fraction &fraction) {
            numerator *= fraction.numerator;
            denominator *= fraction.denominator;
            sign = sign == fraction.sign ? Fraction::Positive : Fraction::Negative;
            Reduce ();
            return *this;
        }

        Fraction &Fraction::operator /= (const Fraction &fraction) {
            numerator *= fraction.denominator;
            denominator *= fraction.numerator;
            sign = sign == fraction.sign ? Fraction::Positive : Fraction::Negative;
            Reduce ();
            return *this;
        }

        void Fraction::Reduce () {
            assert (denominator != 0);
            ui32 gcd = GCD (numerator, denominator);
            assert (gcd != 0);
            numerator /= gcd;
            denominator /= gcd;
        }

        std::size_t Fraction::Size () const {
            return UI32_SIZE + UI32_SIZE + UI8_SIZE;
        }

        void Fraction::Read (
                const BinHeader & /*header*/,
                Serializer &serializer) {
            serializer >> numerator >> denominator >> (ui8 &)sign;
        }

        void Fraction::Write (Serializer &serializer) const {
            serializer << numerator << denominator << (ui8)sign;
        }

        const char * const Fraction::TAG_FRACTION = "Fraction";
        const char * const Fraction::ATTR_NUMERATOR = "Numerator";
        const char * const Fraction::ATTR_DENOMINATOR = "Denominator";
        const char * const Fraction::ATTR_SIGN = "Sign";
        const char * const Fraction::VALUE_POSITIVE = "Positive";
        const char * const Fraction::VALUE_NEGATIVE = "Negative";

        void Fraction::Read (
                const TextHeader & /*header*/,
                const pugi::xml_node &node) {
            numerator = stringToui32 (node.attribute (ATTR_NUMERATOR).value ());
            denominator = stringToui32 (node.attribute (ATTR_DENOMINATOR).value ());
            sign = stringTosign (node.attribute (ATTR_SIGN).value ());
        }

        void Fraction::Write (pugi::xml_node &node) const {
            node.append_attribute (ATTR_NUMERATOR).set_value (ui32Tostring (numerator).c_str ());
            node.append_attribute (ATTR_DENOMINATOR).set_value (ui32Tostring (denominator).c_str ());
            node.append_attribute (ATTR_SIGN).set_value (signTostring (sign).c_str ());
        }

        void Fraction::Read (
                const TextHeader & /*header*/,
                const JSON::Object &object) {
            numerator = object.Get<JSON::Number> (ATTR_NUMERATOR)->To<ui32> ();
            denominator = object.Get<JSON::Number> (ATTR_DENOMINATOR)->To<ui32> ();
            sign = stringTosign (object.Get<JSON::String> (ATTR_SIGN)->value);
        }

        void Fraction::Write (JSON::Object &object) const {
            object.Add (ATTR_NUMERATOR, numerator);
            object.Add (ATTR_DENOMINATOR, denominator);
            object.Add<const std::string &> (ATTR_SIGN, signTostring (sign));
        }

        _LIB_THEKOGANS_UTIL_DECL Fraction _LIB_THEKOGANS_UTIL_API operator + (
                const Fraction &fraction1,
                const Fraction &fraction2) {
            ui32 numerator1 = fraction1.numerator * fraction2.denominator;
            ui32 numerator2 = fraction2.numerator * fraction1.denominator;
            ui32 denominator = fraction1.denominator * fraction2.denominator;
            return
                fraction1.sign == Fraction::Positive ?
                    fraction2.sign == Fraction::Positive ?
                        Fraction (numerator1 + numerator2,
                            denominator, Fraction::Positive) :
                        numerator1 < numerator2 ?
                            Fraction (numerator2 - numerator1,
                                denominator, Fraction::Negative) :
                            Fraction (numerator1 - numerator2,
                                denominator, Fraction::Positive) :
                    fraction2.sign == Fraction::Positive ?
                        numerator1 < numerator2 ?
                            Fraction (numerator2 - numerator1,
                                denominator, Fraction::Positive) :
                            Fraction (numerator1 - numerator2,
                                denominator, Fraction::Negative) :
                        Fraction (numerator1 + numerator2,
                            denominator, Fraction::Negative);
        }

        _LIB_THEKOGANS_UTIL_DECL Fraction _LIB_THEKOGANS_UTIL_API operator - (
                const Fraction &fraction1,
                const Fraction &fraction2) {
            ui32 numerator1 = fraction1.numerator * fraction2.denominator;
            ui32 numerator2 = fraction2.numerator * fraction1.denominator;
            ui32 denominator = fraction1.denominator * fraction2.denominator;
            return
                fraction1.sign == Fraction::Positive ?
                    fraction2.sign == Fraction::Positive ?
                        numerator1 < numerator2 ?
                            Fraction (numerator2 - numerator1,
                                denominator, Fraction::Negative) :
                            Fraction (numerator1 - numerator2,
                                denominator, Fraction::Positive) :
                        Fraction (numerator1 + numerator2,
                            denominator, Fraction::Positive) :
                    fraction2.sign == Fraction::Positive ?
                        Fraction (numerator1 + numerator2,
                            denominator, Fraction::Negative) :
                        numerator1 < numerator2 ?
                            Fraction (numerator2 - numerator1,
                                denominator, Fraction::Positive) :
                            Fraction (numerator1 - numerator2,
                                denominator, Fraction::Negative);
        }

    } // namespace util
} // namespace thekogans
