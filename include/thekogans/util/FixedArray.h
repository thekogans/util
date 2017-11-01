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
            std::size_t size>
        struct FixedArray {
            /// \brief
            /// FixedArray elements.
            T array[size];

            /// \brief
            /// ctor.
            /// \param[in] buffer Elements to initialize the array with.
            /// \param[in] bufferSize Number of elements in buffer.
            FixedArray (
                    const T *buffer = 0,
                    std::size_t bufferSize = 0) {
                if (buffer != 0) {
                    if (bufferSize < size) {
                        memcpy (array, buffer, sizeof (T) * bufferSize);
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
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

            /// \brief
            /// FixedArray is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (FixedArray)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FixedArray_h)
