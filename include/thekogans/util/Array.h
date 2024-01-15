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
#include <functional>
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
        /// IMPORTANT: Arrays are not resizable, and do not grow. In its
        /// one and only ctor either allocates a fixed block or wraps the
        /// one provided. Want resizable, growable containers? That's what
        /// the stl is for!

        template<typename T>
        struct Array {
            /// \brief
            /// Array length.
            std::size_t length;
            /// \brief
            /// Array elements.
            T *array;
            /// \brief
            /// Convenient typedef for std::function<void (T * /*array*/)>.
            typedef std::function<void (T * /*array*/)> Deleter;
            /// \brief
            /// Deleter used to deallocate the array pointer.
            Deleter deleter;

            /// \brief
            /// ctor. Create Array of length elements.
            /// \param[in] length_ Number of elements in the array.
            /// \param[in] array_ Optional array pointer to wrap.
            /// \param[in] deleter_ Deleter used to deallocate the array pointer.
            Array (
                std::size_t length_,
                T *array_ = 0,
                const Deleter &deleter_ = [] (T * /*array*/) {}) :
                length (length_),
                array (array_ == 0 ? new T[length] : array_),
                deleter (array_ == 0 ? [] (T *array) {delete [] array;} : deleter_) {}
            /// \brief
            /// Move ctor.
            /// \param[in,out] other Array to move.
            Array (Array<T> &&other) :
                    length (0),
                    array (0),
                    deleter ([] (T * /*array*/) {}) {
                swap (other);
            }
            /// \brief
            /// dtor. Release the memory held by Array.
            ~Array () {
                deleter (array);
            }

            /// \brief
            /// Move assignment operator.
            /// \param[in,out] other Array to move.
            /// \return *this;
            Array<T> &operator = (Array<T> &&other) {
                if (this != &other) {
                    Array<T> temp (std::move (other));
                    swap (other);
                }
                return *this;
            }

            /// \brief
            /// std::swap for Array.
            void swap (Array<T> &other) {
                std::swap (length, other.length);
                std::swap (array, other.array);
                std::swap (deleter, other.deleter);
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
