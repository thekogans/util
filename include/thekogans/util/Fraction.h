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

#if !defined (__thekogans_util_Fraction_h)
#define __thekogans_util_Fraction_h

#include <cassert>
#if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
    #include <pugixml.hpp>
#endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        /// \struct Fraction Fraction.h thekogans/util/Fraction.h
        ///
        /// \brief
        /// Fraction implements a simple signed fraction type. Values
        /// like 1/3 are exactly representable as numerator and
        /// denominator are kept separate. Standard arithmetic
        /// operators are provided to manipulate fractions like ints.
        ///
        /// IMPORTANT: Most algorithms assume that fractions are
        /// reduced. If you use any of the three provided ctors, you should
        /// be fine. If you manipulate numerator and/or denominator directly,
        /// it is your responsibility to call Fraction::Reduce () after.

        struct _LIB_THEKOGANS_UTIL_DECL Fraction {
            /// \brief
            /// Fraction numerator.
            ui32 numerator;
            /// \brief
            /// Fraction denominator.
            ui32 denominator;
            /// \enum
            /// Fraction sign.
            enum Sign {
                /// \brief
                /// Positive.
                Positive,
                /// \brief
                /// Negative.
                Negative
            } sign;

            /// \brief
            /// Zero value.
            static const Fraction Zero;
            /// \brief
            /// One value.
            static const Fraction One;

            /// \brief
            /// Default ctor. Init to Zero.
            Fraction () :
                numerator (0),
                denominator (1),
                sign (Positive) {}
            /// \brief
            /// ctor.
            /// param[in] numerator_ Fraction numerator.
            /// param[in] denominator_ Fraction denominator.
            /// param[in] sign_ Fraction sign.
            /// NOTE: This ctor will reduce the fraction.
            Fraction (
                    ui32 numerator_,
                    ui32 denominator_,
                    Sign sign_ = Positive) :
                    numerator (numerator_),
                    denominator (denominator_),
                    sign (sign_) {
                assert (denominator > 0);
                Reduce ();
            }
            /// \brief
            /// Copy ctor.
            /// \param[in] fraction Fraction to copy.
            Fraction (const Fraction &fraction) :
                numerator (fraction.numerator),
                denominator (fraction.denominator),
                sign (fraction.sign) {}
        #if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
            /// \brief
            /// ctor.
            /// \param[in] node pugi::xml_node representing the Fraction.
            Fraction (const pugi::xml_node &node) {
                Parse (node);
            }
        #endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)

            /// \brief
            /// Convert an integral sign to it's string equivalent.
            /// \param[in] sign Positive, Negative.
            /// \return "Positive", "Negative".
            static std::string signTostring (Sign sign);
            /// \brief
            /// Convert a string representation of the fraction sign to it's integral form.
            /// \param[in] sign "Positive", "Negative".
            /// \return Positive, Negative.
            static Sign stringTosign (const std::string &sign);

            /// \brief
            /// Return the serialized size of this fraction.
            /// \return Serialized size of this fraction.
            inline std::size_t Size () const {
                return
                    Serializer::Size (numerator) +
                    Serializer::Size (denominator) +
                    UI8_SIZE;
            }

            /// \brief
            /// Assignment operator.
            /// \param[in] fraction Fraction to copy.
            /// \return *this
            Fraction &operator = (const Fraction &fraction);

            /// \brief
            /// Add assign operator.
            /// \param[in] fraction Fraction to add.
            /// \return *this
            Fraction &operator += (const Fraction &fraction);
            /// \brief
            /// Subtract assign operator.
            /// \param[in] fraction Fraction to subtract.
            /// \return *this
            Fraction &operator -= (const Fraction &fraction);
            /// \brief
            /// Multiply assign operator.
            /// \param[in] fraction Fraction to multiply.
            /// \return *this
            Fraction &operator *= (const Fraction &fraction);
            /// \brief
            /// Divide assign operator.
            /// \param[in] fraction Fraction to divide.
            /// \return *this
            Fraction &operator /= (const Fraction &fraction);

            /// \brief
            /// Reduce the fraction to it's proper form.
            void Reduce ();

            /// \brief
            /// Convert the fraction to f32.
            /// \return f32 representation of the fraction.
            inline f32 Tof32 () const {
                f32 value = (f32)numerator / (f32)denominator;
                return sign == Positive ? value : -value;
            }

            /// \brief
            /// "Fraction"
            static const char * const TAG_FRACTION;
            /// \brief
            /// "Numerator"
            static const char * const ATTR_NUMERATOR;
            /// \brief
            /// "Denominator"
            static const char * const ATTR_DENOMINATOR;
            /// \brief
            /// "Sign"
            static const char * const ATTR_SIGN;
            /// \brief
            /// "Positive"
            static const char * const VALUE_POSITIVE;
            /// \brief
            /// "Negative"
            static const char * const VALUE_NEGATIVE;


        #if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
            /// \brief
            /// Given an pugi::xml_node, parse the
            /// fraction it represents. The Fraction has
            /// the following format:
            /// <tagName Numerator = "Fraction numerator"
            ///          Denominator = "Fraction denominator"
            ///          Sign = "Fraction sign"/>
            /// \param[in] node pugi::xml_node representing the fraction.
            void Parse (const pugi::xml_node &node);
        #endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)
            /// \brief
            /// Serialize the Fraction parameters in to an XML string.
            /// \param[in] indentationLevel Pretty print parameter. If
            /// the resulting tag is to be included in a larger structure
            /// you might want to provide a value that will embed it in
            /// the structure.
            /// \param[in] tagName Openning tag name.
            /// \return The XML reprentation of the Fraction.
            std::string ToString (
                std::size_t indentationLevel,
                const char *tagName = TAG_FRACTION) const;
        };

        /// \brief
        /// Serialized Fraction size.
        const std::size_t FRACTION_SIZE = UI32_SIZE + UI32_SIZE + UI8_SIZE;

        /// \brief
        /// Multiply the fraction by an int.
        /// \param[in] value int value to multiply the fraction by.
        /// \param[in] fraction Fraction to multiply.
        /// \return Integer part of the fraction.
        inline i32 operator * (
                i32 value,
                const Fraction &fraction) {
            i32 result = value * fraction.numerator / fraction.denominator;
            // Don't get fooled. Negative value has been accounted for.
            // Do the logic table, if you don't believe me.
            return fraction.sign == Fraction::Positive ? result : -result;
        }

        /// \brief
        /// Multiply the fraction by an int.
        /// \param[in] fraction Fraction to multiply.
        /// \param[in] value int value to multiply the fraction by.
        /// \return Integer part of the fraction.
        inline i32 operator * (
                const Fraction &fraction,
                i32 value) {
            return value * fraction;
        }

        /// \brief
        /// Negate the fraction.
        /// \param[in] fraction Fraction to negate.
        /// \return Negated fraction.
        inline Fraction operator - (const Fraction &fraction) {
            // Because we call Reduce in the ctor above, and because this
            // operator is used heavily in the code, we eschew the RVO for
            // the performance gain of not calling Reduce.
            Fraction negative;
            negative.numerator = fraction.numerator;
            negative.denominator = fraction.denominator;
            negative.sign = fraction.sign == Fraction::Positive ?
                Fraction::Negative : Fraction::Positive;
            return negative;
        }

        /// \brief
        /// Add two fractions.
        /// \param[in] fraction1 First fraction to add.
        /// \param[in] fraction2 Second fraction to add.
        /// \return The sum of fraction1 and fraction2.
        _LIB_THEKOGANS_UTIL_DECL Fraction _LIB_THEKOGANS_UTIL_API operator + (
            const Fraction &fraction1,
            const Fraction &fraction2);
        /// \brief
        /// Subtract two fractions.
        /// \param[in] fraction1 Fraction to subtract from.
        /// \param[in] fraction2 Fraction to subtract.
        /// \return The difference of fraction1 and fraction2.
        _LIB_THEKOGANS_UTIL_DECL Fraction _LIB_THEKOGANS_UTIL_API operator - (
            const Fraction &fraction1,
            const Fraction &fraction2);

        /// \brief
        /// Multiply two fractions.
        /// \param[in] fraction1 First fraction to multiply.
        /// \param[in] fraction2 Second fraction to multiply.
        /// \return The product of fraction1 and fraction2.
        inline Fraction operator * (
                const Fraction &fraction1,
                const Fraction &fraction2) {
            return Fraction (fraction1.numerator * fraction2.numerator,
                fraction1.denominator * fraction2.denominator,
                fraction1.sign == fraction2.sign ?
                    Fraction::Positive : Fraction::Negative);
        }

        /// \brief
        /// Divide two fractions.
        /// \param[in] fraction1 Numerator fraction.
        /// \param[in] fraction2 Denominator fraction.
        /// \return The quotient of fraction1 and fraction2.
        inline Fraction operator / (
                const Fraction &fraction1,
                const Fraction &fraction2) {
            return Fraction (fraction1.numerator * fraction2.denominator,
                fraction1.denominator * fraction2.numerator,
                fraction1.sign == fraction2.sign ?
                    Fraction::Positive : Fraction::Negative);
        }

        /// \brief
        /// Compare two fraction for equality.
        /// \param[in] fraction1 First fraction to compare.
        /// \param[in] fraction2 Second fraction to compare.
        /// \return true = same, false = different.
        inline bool operator == (
                const Fraction &fraction1,
                const Fraction &fraction2) {
            return fraction1.numerator == fraction2.numerator &&
                fraction1.denominator == fraction2.denominator &&
                fraction1.sign == fraction2.sign;
        }

        /// \brief
        /// Compare two fraction for inequality.
        /// \param[in] fraction1 First fraction to compare.
        /// \param[in] fraction2 Second fraction to compare.
        /// \return true = different, false = same.
        inline bool operator != (
                const Fraction &fraction1,
                const Fraction &fraction2) {
            return fraction1.numerator != fraction2.numerator ||
                fraction1.denominator != fraction2.denominator ||
                fraction1.sign != fraction2.sign;
        }

        /// \brief
        /// Compare two fraction for order.
        /// \param[in] fraction1 First fraction to compare.
        /// \param[in] fraction2 Second fraction to compare.
        /// \return true = fraction1 < fraction2, false = fraction1 >= fraction2.
        inline bool operator < (
                const Fraction &fraction1,
                const Fraction &fraction2) {
            if (fraction1.sign == Fraction::Negative &&
                    fraction2.sign == Fraction::Positive) {
                return true;
            }
            if (fraction1.sign == Fraction::Positive &&
                    fraction2.sign == Fraction::Negative) {
                return false;
            }
            return fraction1.numerator * fraction2.denominator <
                fraction2.numerator * fraction1.denominator;
        }

        /// \brief
        /// Compare two fraction for order.
        /// \param[in] fraction1 First fraction to compare.
        /// \param[in] fraction2 Second fraction to compare.
        /// \return true = fraction1 <= fraction2, false = fraction1 > fraction2.
        inline bool operator <= (
                const Fraction &fraction1,
                const Fraction &fraction2) {
            if (fraction1.sign == Fraction::Negative &&
                    fraction2.sign == Fraction::Positive) {
                return true;
            }
            if (fraction1.sign == Fraction::Positive &&
                    fraction2.sign == Fraction::Negative) {
                return false;
            }
            return fraction1.numerator * fraction2.denominator <=
                fraction2.numerator * fraction1.denominator;
        }

        /// \brief
        /// Compare two fraction for order.
        /// \param[in] fraction1 First fraction to compare.
        /// \param[in] fraction2 Second fraction to compare.
        /// \return true = fraction1 > fraction2, false = fraction1 <= fraction2.
        inline bool operator > (
                const Fraction &fraction1,
                const Fraction &fraction2) {
            if (fraction1.sign == Fraction::Positive &&
                    fraction2.sign == Fraction::Negative) {
                return true;
            }
            if (fraction1.sign == Fraction::Negative &&
                    fraction2.sign == Fraction::Positive) {
                return false;
            }
            return fraction1.numerator * fraction2.denominator >
                fraction2.numerator * fraction1.denominator;
        }

        /// \brief
        /// Compare two fraction for order.
        /// \param[in] fraction1 First fraction to compare.
        /// \param[in] fraction2 Second fraction to compare.
        /// \return true = fraction1 >= fraction2, false = fraction1 < fraction2.
        inline bool operator >= (
                const Fraction &fraction1,
                const Fraction &fraction2) {
            if (fraction1.sign == Fraction::Positive &&
                    fraction2.sign == Fraction::Negative) {
                return true;
            }
            if (fraction1.sign == Fraction::Negative &&
                    fraction2.sign == Fraction::Positive) {
                return false;
            }
            return fraction1.numerator * fraction2.denominator >=
                fraction2.numerator * fraction1.denominator;
        }

        /// \brief
        /// Write the given fraction to the given serializer.
        /// \param[in] serializer Where to write the given guid.
        /// \param[in] fraction Fraction to write.
        /// \return serializer.
        inline Serializer &operator << (
                Serializer &serializer,
                const Fraction &fraction) {
            return serializer <<
                fraction.numerator <<
                fraction.denominator <<
                (ui8)fraction.sign;
        }

        /// \brief
        /// Read a fraction from the given serializer.
        /// \param[in] serializer Where to read the guid from.
        /// \param[in] fraction Fraction to read.
        /// \return serializer.
        inline Serializer &operator >> (
                Serializer &serializer,
                Fraction &fraction) {
            return serializer >>
                fraction.numerator >>
                fraction.denominator >>
                (ui8 &)fraction.sign;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Fraction_h)
