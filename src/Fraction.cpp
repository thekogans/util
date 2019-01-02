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
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/Fraction.h"

namespace thekogans {
    namespace util {

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

        const char * const Fraction::TAG_FRACTION = "Fraction";
        const char * const Fraction::ATTR_NUMERATOR = "Numerator";
        const char * const Fraction::ATTR_DENOMINATOR = "Denominator";
        const char * const Fraction::ATTR_SIGN = "Sign";
        const char * const Fraction::VALUE_POSITIVE = "Positive";
        const char * const Fraction::VALUE_NEGATIVE = "Negative";

    #if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
        void Fraction::Parse (const pugi::xml_node &node) {
            numerator = stringToui32 (node.attribute (ATTR_NUMERATOR).value ());
            denominator = stringToui32 (node.attribute (ATTR_DENOMINATOR).value ());
            sign = stringTosign (node.attribute (ATTR_SIGN).value ());
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)

        std::string Fraction::ToString (
                std::size_t indentationLevel,
                const char *tagName) const {
            Attributes attributes;
            attributes.push_back (Attribute (ATTR_NUMERATOR, ui32Tostring (numerator)));
            attributes.push_back (Attribute (ATTR_DENOMINATOR, ui32Tostring (denominator)));
            attributes.push_back (Attribute (ATTR_SIGN, signTostring (sign)));
            return OpenTag (indentationLevel, tagName, attributes, true, true);
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
