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

#if !defined (__thekogans_util_Point_h)
#define __thekogans_util_Point_h

#include <cassert>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        struct Rectangle;

        /// \struct Point Point.h thekogans/util/Point.h
        ///
        /// \brief
        /// A simple 2D integer based point. Useful for handling mouse
        /// and window coordinates.

        struct _LIB_THEKOGANS_UTIL_DECL Point {
            /// \brief
            /// x coordinate.
            i32 x;
            /// \brief
            /// y coordinate.
            i32 y;

            /// \brief
            /// ctor.
            /// \param[in] x_ x coordinate.
            /// \param[in] y_ y coordinate.
            Point (
                i32 x_ = 0,
                i32 y_ = 0) :
                x (x_),
                y (y_) {}

            /// \brief
            /// Point (0, 0);
            static Point Empty;

            enum {
                /// \brief
                /// Serialized point size.
                SIZE = I32_SIZE + I32_SIZE
            };

            /// \brief
            /// Unary minus operator.
            /// \return Negated point.
            inline Point operator - () const {
                return Point (-x, -y);
            }

            /// \brief
            /// Unary plus operator.
            /// \return Copy of point.
            inline Point operator + () const {
                return Point (x, y);
            }

            /// \brief
            /// Add the given point to this one.
            /// \param[in] point Point to add.
            /// \return *this.
            inline Point &operator += (const Point &point) {
                x += point.x;
                y += point.y;
                return *this;
            }

            /// \brief
            /// Subtract the given point from this one.
            /// \param[in] point Point to subtract.
            /// \return *this.
            inline Point &operator -= (const Point &point) {
                x -= point.x;
                y -= point.y;
                return *this;
            }

            /// \brief
            /// Scale the point by the given factor.
            /// \param[in] scale Factor to scale the point by.
            /// \return *this.
            inline Point &operator *= (f32 scale) {
                x = (i32)ROUND ((f32)x * scale);
                y = (i32)ROUND ((f32)y * scale);
                return *this;
            }

            /// \brief
            /// Point in \see{Rectangle} test.
            /// \param[in] rectangle \see{Rectangle} to test for point containment.
            /// \return true = Point in rectangle, false = Point is outside.
            bool InRectangle (const Rectangle &rectangle) const;
        };

        /// \brief
        /// Check two points for equality.
        /// \param[in] point1 First point to check.
        /// \param[in] point2 Second point to check.
        /// \return true = point1 == point2, false = point1 != point2.
        inline bool operator == (
                const Point &point1,
                const Point &point2) {
            return point1.x == point2.x && point1.y == point2.y;
        }

        /// \brief
        /// Check two points for inequality.
        /// \param[in] point1 First point to check.
        /// \param[in] point2 Second point to check.
        /// \return true = point1 != point2, false = point1 == point2.
        inline bool operator != (
                const Point &point1,
                const Point &point2) {
            return point1.x != point2.x || point1.y != point2.y;
        }

        /// \brief
        /// Return the sum of two points.
        /// \param[in] point1 First point in sum.
        /// \param[in] point2 Second point in sum.
        /// \return point1 + point2.
        inline Point operator + (
                const Point &point1,
                const Point &point2) {
            return Point (point1.x + point2.x, point1.y + point2.y);
        }

        /// \brief
        /// Return the difference of two points.
        /// \param[in] point1 First point in difference.
        /// \param[in] point2 Second point in difference.
        /// \return point1 - point2.
        inline Point operator - (
                const Point &point1,
                const Point &point2) {
            return Point (point1.x - point2.x, point1.y - point2.y);
        }

        /// \brief
        /// Scale the given point by the given factor.
        /// \param[in] point Point to scale.
        /// \param[in] scale Factor to scale the point by.
        /// \return point * scale.
        inline Point operator * (
                const Point &point,
                f32 scale) {
            return Point (
                (i32)ROUND ((f32)point.x * scale),
                (i32)ROUND ((f32)point.y * scale));
        }

        /// \brief
        /// Serialize the given point to the given stream.
        /// \param[in] serializer Where to write the point.
        /// \param[in] point Point to serialize.
        /// \return serializer.
        inline Serializer &operator << (
                Serializer &serializer,
                const Point &point) {
            serializer << point.x << point.y;
            return serializer;
        }
        /// \brief
        /// Deserialize the given point from the given stream.
        /// \param[in] serializer Where to read the point.
        /// \param[in] point Point to deserialize.
        /// \return serializer.
        inline Serializer &operator >> (
                Serializer &serializer,
                Point &point) {
            serializer >> point.x >> point.y;
            return serializer;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Point_h)
