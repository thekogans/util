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

#if !defined (__thekogans_util_Rectangle_h)
#define __thekogans_util_Rectangle_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Point.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        /// \struct Rectangle Rectangle.h thekogans/util/Rectangle.h
        ///
        /// \brief
        /// A simple 2D integer based rectangle. Useful for handling
        /// window and image rectangles. Coordinates are right handed
        /// Cartesian.

        struct _LIB_THEKOGANS_UTIL_DECL Rectangle {
            /// \brief
            /// Bottom/Left point.
            Point origin;
            /// \struct Rectangle::Extents Rectangle.h thekogans/util/Rectangle.h
            ///
            /// \brief
            /// Rectangle extents in both horizontal (x) and vertical (y) directions.
            struct _LIB_THEKOGANS_UTIL_DECL Extents {
                /// \brief
                /// Horizontal (x) extent.
                ui32 width;
                /// \brief
                /// Vertical (y) extent.
                ui32 height;

                /// \brief
                /// ctor.
                /// \param[in] width_ Horizontal (x) extent.
                /// \param[in] height_ Vertical (y) extent.
                Extents (
                    ui32 width_ = 0,
                    ui32 height_ = 0) :
                    width (width_),
                    height (height_) {}

                /// \brief
                /// Extents (0, 0);
                static const Extents Empty;

                enum {
                    /// \brief
                    /// Serialized extent size.
                    SIZE = UI32_SIZE + UI32_SIZE
                };

                /// \brief
                /// Return the size of Extents.
                /// \return Size of Extents.
                inline std::size_t Size () const {
                    return SIZE;
                }

                /// \brief
                /// Scale self to aspect fit in to given extents.
                /// Return the scale factor used to acomplish the feat.
                /// \param[in] extents Extents to aspect fit to.
                /// \return Scale factor used to aspect fit.
                f32 AspectFit (const Extents &extents);

                /// \brief
                /// Return true if width == 0 or height == 0.
                /// \return true if width == 0 or height == 0.
                inline bool IsDegenerate () const {
                    return width == 0 || height == 0;
                }

                /// \brief
                /// Return extents area.
                /// \return Extents area.
                inline ui32 GetArea () const {
                    return width * height;
                }
            } extents;

            /// \brief
            /// ctor.
            /// \param[in] x Origin x.
            /// \param[in] y Origin y.
            /// \param[in] width Extents width.
            /// \param[in] height Extents height.
            Rectangle (
                i32 x = 0,
                i32 y = 0,
                ui32 width = 0,
                ui32 height = 0) :
                origin (x, y),
                extents (width, height) {}
            /// \brief
            /// ctor.
            /// \param[in] origin_ Rectangle origin.
            /// \param[in] extents_ Rectangle extents.
            Rectangle (
                const Point &origin_,
                const Extents &extents_) :
                origin (origin_),
                extents (extents_) {}

            /// \brief
            /// Rectangle (0, 0, 0, 0);
            static const Rectangle Empty;

            enum {
                /// \brief
                /// Serialized rectangle size.
                SIZE = Point::SIZE + Extents::SIZE
            };

            /// \brief
            /// Return the size of Rectangle.
            /// \return Size of Rectangle.
            inline std::size_t Size () const {
                return SIZE;
            }

            /// \brief
            /// Return true if rectangle has zero area.
            /// \return true if rectangle has zero area.
            inline bool IsDegenerate () const {
                return extents.IsDegenerate ();
            }

            /// \brief
            /// Return true if this rectangle contains the given one.
            /// \param[in] rectangle Rectangle to check for containment.
            /// \return true if this rectangle contains the given one.
            inline bool IsInside (const Rectangle &rectangle) const {
                return
                    origin.x >= rectangle.origin.x &&
                    origin.x + extents.width <=
                        rectangle.origin.x + rectangle.extents.width &&
                    origin.y >= rectangle.origin.y &&
                    origin.y + extents.height <=
                        rectangle.origin.y + rectangle.extents.height;
            }

            enum {
                /// \brief
                /// Split in vertical direction.
                SplitVertical,
                /// \brief
                /// Split in horizontal direction.
                SplitHorizontal
            };
            /// \brief
            /// Split this rectangle in to two.
            /// \param[in] splitDirection Direction of split (horizontal or vertical).
            /// \param[in] t How far along the extent (width or height) to split.
            /// \param[out] half0 Left/Bottom part of the split rectangle.
            /// \param[out] half1 Right/Top part of the split rectangle.
            void Split (
                ui32 splitDirection,
                f32 t,
                Rectangle &half0,
                Rectangle &half1) const;

            /// \brief
            /// Return the intersection of this rectangle and a given one.
            /// \param[in] rectangle Rectangle to intersect with.
            /// \return Intersection of this rectangle and a given one.
            Rectangle Intersection (const Rectangle &rectangle) const;
            /// \brief
            /// Return the uion of this rectangle and a given one.
            /// \param[in] rectangle Rectangle to union with.
            /// \return Union of this rectangle and a given one.
            Rectangle Union (const Rectangle &rectangle) const;

            /// \brief
            /// Check if the given rectangle shares a side with this one.
            /// \param[in] rectangle Rectangle to check.
            /// \return true = Given rectangle shares a size with this one.
            bool CanMergeWith (const Rectangle &rectangle) const;
            /// \brief
            /// Merge with the given rectangle.
            /// NOTE: For this function to make sense, you should
            /// only call it if CanMergeWith returned true.
            /// \param[in] rectangle Rectangle to merge with.
            void MergeWith (const Rectangle &rectangle);

            /// \brief
            /// Return the area of this rectangle.
            /// \return Area of this rectangle.
            inline ui32 GetArea () const {
                return extents.GetArea ();
            }

            /// \brief
            /// Return a rectangle (and optionaly the scale) which will
            /// aspect fit, and letterbox the frame in the window.
            static Rectangle AspectFitAndLetterbox (
                const Extents &frame,
                const Extents &window,
                f32 *scale = 0);
        };

        /// \brief
        /// Compare rectangle extents for equality.
        /// \paran[in] extents1 First extents to compare.
        /// \paran[in] extents2 Second extents to compare.
        /// \return true = equal, false = not equal.
        inline bool operator == (
                const Rectangle::Extents &extents1,
                const Rectangle::Extents &extents2) {
            return extents1.width == extents2.width &&
                extents1.height == extents2.height;
        }

        /// \brief
        /// Compare rectangle extents for inequality.
        /// \paran[in] extents1 First extents to compare.
        /// \paran[in] extents2 Second extents to compare.
        /// \return true = not equal, false = equal.
        inline bool operator != (
                const Rectangle::Extents &extents1,
                const Rectangle::Extents &extents2) {
            return extents1.width != extents2.width ||
                extents1.height != extents2.height;
        }

        /// \brief
        /// Scale the given rectangle extents by the given factor.
        /// \paran[in] extents Rectangle extents to scale.
        /// \paran[in] scale Factor to scale the extents by.
        /// \return Scaled rectangle extents.
        inline Rectangle::Extents operator * (
                const Rectangle::Extents &extents,
                f32 scale) {
            return Rectangle::Extents (
                (ui32)ROUND (((f32)extents.width * scale)),
                (ui32)ROUND (((f32)extents.height * scale)));
        }

        /// \brief
        /// Compare rectangles for equality.
        /// \paran[in] rectangle1 First rectangle to compare.
        /// \paran[in] rectangle2 Second rectangle to compare.
        /// \return true = equal, false = not equal.
        inline bool operator == (
                const Rectangle &rectangle1,
                const Rectangle &rectangle2) {
            return rectangle1.origin == rectangle2.origin &&
                rectangle1.extents == rectangle2.extents;
        }

        /// \brief
        /// Compare rectangles for inequality.
        /// \paran[in] rectangle1 First rectangle to compare.
        /// \paran[in] rectangle2 Second rectangle to compare.
        /// \return true = not equal, false = equal.
        inline bool operator != (
                const Rectangle &rectangle1,
                const Rectangle &rectangle2) {
            return rectangle1.origin != rectangle2.origin ||
                rectangle1.extents != rectangle2.extents;
        }

        /// \brief
        /// Scale the given rectangle by the given factor.
        /// \paran[in] rectangle Rectangle to scale.
        /// \paran[in] scale Factor to scale the rectangle by.
        /// \return Scaled rectangle.
        inline Rectangle operator * (
                const Rectangle &rectangle,
                f32 scale) {
            return Rectangle (rectangle.origin, rectangle.extents * scale);
        }

        /// \brief
        /// Translate the given rectangle origin by the given offset.
        /// \paran[in] rectangle Rectangle to translate.
        /// \paran[in] offset How much to translate by.
        /// \return Translated rectangle.
        inline Rectangle operator + (
                const Rectangle &rectangle,
                const Point &offset) {
            return Rectangle (rectangle.origin + offset, rectangle.extents);
        }

        /// \brief
        /// Translate the given rectangle origin by the given offset.
        /// \paran[in] rectangle Rectangle to translate.
        /// \paran[in] offset How much to translate by.
        /// \return Translated rectangle.
        inline Rectangle operator - (
                const Rectangle &rectangle,
                const Point &offset) {
            return Rectangle (rectangle.origin - offset, rectangle.extents);
        }

        /// \brief
        /// Serialize the given rectangle extents.
        /// \param[in] serializer Where to write the given rectangle extents.
        /// \param[in] extents Rectangle extents to write.
        /// \return serializer.
        inline Serializer &operator << (
                Serializer &serializer,
                const Rectangle::Extents &extents) {
            serializer << extents.width << extents.height;
            return serializer;
        }
        /// \brief
        /// Deserialize rectangle extents.
        /// \param[in] serializer Where to read the rectangle extents.
        /// \param[in] extents Rectangle extents to read.
        /// \return serializer.
        inline Serializer &operator >> (
                Serializer &serializer,
                Rectangle::Extents &extents) {
            serializer >> extents.width >> extents.height;
            return serializer;
        }

        /// \brief
        /// Serialize the given rectangle.
        /// \param[in] serializer Where to write the given rectangle.
        /// \param[in] rectangle Rectangle to write.
        /// \return serializer.
        inline Serializer &operator << (
                Serializer &serializer,
                const Rectangle &rectangle) {
            serializer << rectangle.origin << rectangle.extents;
            return serializer;
        }
        /// \brief
        /// Deserialize a rectangle.
        /// \param[in] serializer Where to read the rectangle.
        /// \param[in] rectangle Rectangle to read.
        /// \return serializer.
        inline Serializer &operator >> (
                Serializer &serializer,
                Rectangle &rectangle) {
            serializer >> rectangle.origin >> rectangle.extents;
            return serializer;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Rectangle_h)
