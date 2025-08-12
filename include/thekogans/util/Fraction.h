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
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Serializable.h"

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

        struct _LIB_THEKOGANS_UTIL_DECL Fraction : public Serializable {
            /// \brief
            /// Fraction is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (Fraction)

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

            // Serializable
            /// \brief
            /// Return the serialized key size.
            /// \return Serialized key size.
            virtual std::size_t Size () const noexcept override;

            /// \brief
            /// Read the key from the given serializer.
            /// \param[in] header \see{SerializableHeader}.
            /// \param[in] serializer \see{Serializer} to read the key from.
            virtual void Read (
                const SerializableHeader & /*header*/,
                Serializer &serializer) override;
            /// \brief
            /// Write the key to the given serializer.
            /// \param[out] serializer \see{Serializer} to write the key to.
            virtual void Write (Serializer &serializer) const override;

            /// \brief
            /// Read the Serializable from an XML DOM.
            /// \param[in] header \see{SerializableHeader}.
            /// \param[in] node XML DOM representation of a Serializable.
            virtual void ReadXML (
                const SerializableHeader & /*header*/,
                const pugi::xml_node &node) override;
            /// \brief
            /// Write the Serializable to the XML DOM.
            /// \param[out] node Parent node.
            virtual void WriteXML (pugi::xml_node &node) const override;

            /// \brief
            /// Read the Serializable from an JSON DOM.
            /// \param[in] node JSON DOM representation of a Serializable.
            virtual void ReadJSON (
                const SerializableHeader & /*header*/,
                const JSON::Object &object) override;
            /// \brief
            /// Write the Serializable to the JSON DOM.
            /// \param[out] node Parent node.
            virtual void WriteJSON (JSON::Object &object) const override;
        };

        /// \brief
        /// Multiply the fraction by an int.
        /// \param[in] value int value to multiply the fraction by.
        /// \param[in] fraction Fraction to multiply.
        /// \return Integer part of the fraction.
        inline i32 _LIB_THEKOGANS_UTIL_API operator * (
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
        inline i32 _LIB_THEKOGANS_UTIL_API operator * (
                const Fraction &fraction,
                i32 value) {
            return value * fraction;
        }

        /// \brief
        /// Negate the fraction.
        /// \param[in] fraction Fraction to negate.
        /// \return Negated fraction.
        inline Fraction _LIB_THEKOGANS_UTIL_API operator - (const Fraction &fraction) {
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
        inline Fraction _LIB_THEKOGANS_UTIL_API operator * (
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
        inline Fraction _LIB_THEKOGANS_UTIL_API operator / (
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
        /// \return true == same, false == different.
        inline bool _LIB_THEKOGANS_UTIL_API operator == (
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
        /// \return true == different, false == same.
        inline bool _LIB_THEKOGANS_UTIL_API operator != (
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
        /// \return true == fraction1 < fraction2, false == fraction1 >= fraction2.
        inline bool _LIB_THEKOGANS_UTIL_API operator < (
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
        /// \return true == fraction1 <= fraction2, false == fraction1 > fraction2.
        inline bool _LIB_THEKOGANS_UTIL_API operator <= (
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
        /// \return true == fraction1 > fraction2, false == fraction1 <= fraction2.
        inline bool _LIB_THEKOGANS_UTIL_API operator > (
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
        /// \return true == fraction1 >= fraction2, false == fraction1 < fraction2.
        inline bool _LIB_THEKOGANS_UTIL_API operator >= (
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
        /// Implement Fraction extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EXTRACTION_OPERATORS (Fraction)

        /// \brief
        /// Implement Fraction value parser.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_VALUE_PARSER (Fraction)

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Fraction_h)
