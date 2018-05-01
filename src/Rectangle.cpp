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

#include <algorithm>
#include "thekogans/util/Exception.h"
#include "thekogans/util/Rectangle.h"

namespace thekogans {
    namespace util {

        Rectangle::Extents Rectangle::Extents::Empty (0, 0);

        f32 Rectangle::Extents::AspectFit (const Extents &extents) {
            f32 scale = width != 0 && height != 0 ? std::min (
                (f32)extents.width / (f32)width,
                (f32)extents.height / (f32)height) : 0.0f;
            width = ROUND (width * scale);
            height = ROUND (height * scale);
            return scale;
        }

        Rectangle Rectangle::Empty (0, 0, 0, 0);

        void Rectangle::Split (
                ui32 splitDirection,
                f32 t,
                Rectangle &half0,
                Rectangle &half1) const {
            if (splitDirection == SplitVertical) {
                ui32 half0Width = ROUND (extents.width * t);
                half0.origin = origin;
                half1.origin.x = origin.x + half0Width;
                half1.origin.y = origin.y;
                half0.extents.width = half0Width;
                half1.extents.width = extents.width - half0Width;
                half0.extents.height =
                half1.extents.height = extents.height;
            }
            else if (splitDirection == SplitHorizontal) {
                ui32 half0Height = ROUND (extents.height * t);
                half0.origin = origin;
                half1.origin.x = origin.x;
                half1.origin.y = origin.y + half0Height;
                half0.extents.width =
                half1.extents.width = extents.width;
                half0.extents.height = half0Height;
                half1.extents.height = extents.height - half0Height;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Unknown split direction: %u", splitDirection);
            }
        }

        Rectangle Rectangle::Intersection (const Rectangle &rectangle) const {
            // If the rectangles don't overlap, there can be no
            // intersection.
            if (rectangle.origin.x + (i32)rectangle.extents.width < origin.x ||
                    rectangle.origin.x > origin.x + (i32)extents.width ||
                    rectangle.origin.y + (i32)rectangle.extents.height < origin.y ||
                    rectangle.origin.y > origin.y + (i32)extents.height) {
                return Rectangle ();
            }
            i32 left = std::max (origin.x, rectangle.origin.x);
            i32 top = std::max (origin.y, rectangle.origin.y);
            i32 right = std::min (origin.x + (i32)extents.width,
                rectangle.origin.x + (i32)rectangle.extents.width);
            i32 bottom = std::min (origin.y + (i32)extents.height,
                rectangle.origin.y + (i32)rectangle.extents.height);
            return Rectangle (left, top, right - left, bottom - top);
        }

        Rectangle Rectangle::Union (const Rectangle &rectangle) const {
            if (IsDegenerate ()) {
                return rectangle;
            }
            else if (rectangle.IsDegenerate ()) {
                return *this;
            }
            i32 left = std::min (origin.x, rectangle.origin.x);
            i32 top = std::min (origin.y, rectangle.origin.y);
            i32 right = std::max (origin.x + extents.width,
                rectangle.origin.x + rectangle.extents.width);
            i32 bottom = std::max (origin.y + extents.height,
                rectangle.origin.y + rectangle.extents.height);
            return Rectangle (left, top, right - left, bottom - top);
        }

        bool Rectangle::CanMergeWith (const Rectangle &rectangle) const {
            return
                (origin.x == rectangle.origin.x &&
                extents.width == rectangle.extents.width &&
                (origin.y < rectangle.origin.y ?
                    origin.y + (i32)extents.height >= rectangle.origin.y :
                    rectangle.origin.y + (i32)rectangle.extents.height >= origin.y)) ||
                (origin.y == rectangle.origin.y &&
                extents.height == rectangle.extents.height &&
                (origin.x < rectangle.origin.x ?
                    origin.x + (i32)extents.width >= rectangle.origin.x :
                    rectangle.origin.x + (i32)rectangle.extents.width >= origin.x));
        }

        void Rectangle::MergeWith (const Rectangle &rectangle) {
            if (origin.x > rectangle.origin.x) {
                origin.x = rectangle.origin.x;
            }
            if (origin.y > rectangle.origin.y) {
                origin.y = rectangle.origin.y;
            }
            i32 right = origin.x + extents.width;
            i32 rectangleRight =
                rectangle.origin.x + rectangle.extents.width;
            if (right < rectangleRight) {
                extents.width = rectangleRight - origin.x;
            }
            i32 bottom = origin.y + extents.height;
            i32 rectangleBottom =
                rectangle.origin.y + rectangle.extents.height;
            if (bottom < rectangleBottom) {
                extents.height = rectangleBottom - origin.y;
            }
        }

        Rectangle Rectangle::AspectFitAndLetterbox (
                const Extents &frame,
                const Extents &window,
                f32 *scale) {
            Rectangle::Extents extents = frame;
            f32 scale_ = extents.AspectFit (window);
            if (scale != 0) {
                *scale = scale_;
            }
            Point origin (
                (std::max (window.width, extents.width) -
                    std::min (window.width, extents.width)) / 2,
                (std::max (window.height, extents.height) -
                    std::min (window.height, extents.height)) / 2);
            return Rectangle (origin, extents);
        }

    } // namespace util
} // namespace thekogans
