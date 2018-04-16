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

#if !defined (__thekogans_util_Array_h)
#define __thekogans_util_Array_h

#include <cstddef>
#include <memory>
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct Array Array.h thekogans/util/Array.h
        ///
        /// \brief
        /// This little template acts as an interface between C++ and
        /// various OS C APIs.
        ///
        /// Helps with life cycle management, and keeps the code
        /// clutter down.
        ///
        /// IMPORTANT: Arrays are not resizable, and do not grow. This
        /// is the reason for a single explicit ctor. Want resizable,
        /// growable containers? That's what the stl is for!

        template<typename T>
        struct Array {
            /// \brief
            /// Array length.
            std::size_t length;
            /// \brief
            /// Array elements.
            T *array;

            /// \brief
            /// ctor. Create Array of length elements.
            /// \param[in] length_ Number of elements in the array.
            explicit Array (std::size_t length_) :
                length (length_),
                array (new T[length]) {}
            /// \brief
            /// dtor. Release the memory held by Array.
            ~Array () {
                delete [] array;
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
            /// Array is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Array)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Array_h)
