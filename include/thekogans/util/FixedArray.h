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

#if !defined (__thekogans_util_FixedArray_h)
#define __thekogans_util_FixedArray_h

#include <cstddef>
#include <cstring>
#include "thekogans/util/Config.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        /// \struct FixedArray FixedArray.h thekogans/util/FixedArray.h
        ///
        /// \brief
        /// This little template acts as an interface between C++ and
        /// various OS C APIs.
        ///
        /// Helps with life cycle management, and keeps the code
        /// clutter down.
        ///
        /// IMPORTANT: FixedArrays are not resizable, and do not grow. This
        /// is the reason for a single explicit ctor. Want resizable,
        /// growable containers? That's what the stl is for!

        template<
            typename T,
            std::size_t count>
        struct FixedArray {
            /// \brief
            /// FixedArray elements.
            T array[count];

            /// \brief
            /// ctor.
            FixedArray () {}
            /// \brief
            /// ctor.
            /// \param[in] array_ Elements to initialize the array with.
            /// \param[in] count_ Number of elements in array_.
            /// \param[in] clearUnused Clear unused array elements to 0.
            FixedArray (
                    const T *array_,
                    std::size_t count_,
                    bool clearUnused = false) {
                if (array_ != nullptr && count_ <= count) {
                    std::size_t usedBytes = sizeof (T) * count_;
                    memcpy (array, array_, usedBytes);
                    if (clearUnused) {
                        std::size_t unusedBytes = sizeof (T) * (count - count_);
                        memset (array + usedBytes, 0, unusedBytes);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }
            /// \brief
            /// ctor.
            /// \param[in] value Value to initialize every element of the array to.
            explicit FixedArray (const T &value) {
                for (std::size_t i = 0; i < count; ++i) {
                    array[i] = value;
                }
            }

            /// \brief
            /// const (rvalue) element accessor.
            /// \param[in] index Element index to return.
            /// \return reference to element at index.
            inline const T &operator [] (std::size_t index) const {
                return array[index];
            }
            /// \brief
            /// lvalue element accessor.
            /// \param[in] index Element index to return.
            /// \return Reference to element at index.
            inline T &operator [] (std::size_t index) {
                return array[index];
            }

            /// \brief
            /// Implicit conversion to const T *.
            /// \return const T *.
            inline operator const T * () const {
                return array;
            }
            /// \brief
            /// Implicit conversion to T *.
            /// \return T *.
            inline operator T * () {
                return array;
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FixedArray_h)
