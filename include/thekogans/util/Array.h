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
#include <algorithm>
#include "thekogans/util/Config.h"
#include "thekogans/util/Exception.h"

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
        /// one and only ctor it either allocates a fixed block or wraps
        /// the one provided. Want resizable, growable containers? That's
        /// what the stl is for!

        template<typename T>
        struct Array {
            /// \brief
            /// Array length.
            std::size_t length;
            /// \brief
            /// Array elements.
            T *array;
            /// \brief
            /// Alias for std::function<void (T * /*array*/)>.
            using Deleter = std::function<void (T * /*array*/)>;
            /// \brief
            /// Deleter used to deallocate the array pointer.
            Deleter deleter;

            /// \brief
            /// ctor. Create (or wrap) Array of length elements.
            /// \param[in] length_ Number of elements in the array.
            /// \param[in] array_ Optional array pointer to wrap.
            /// \param[in] deleter_ Deleter used to deallocate the array pointer.
            Array (
                std::size_t length_,
                T *array_ = nullptr,
                const Deleter &deleter_ = [] (T * /*array*/) {}) :
                length (length_),
                array (array_ == nullptr ? new T[length] : array_),
                deleter (array_ == nullptr ? [] (T *array) {delete [] array;} : deleter_) {}
            /// \brief
            /// Move ctor.
            /// \param[in,out] other Array to move.
            Array (Array<T> &&other) :
                    length (0),
                    array (nullptr),
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
                if (index < length) {
                    return array[index];
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW);
                }
            }
            /// \brief
            /// lvalue element accessor.
            /// \param[in] index Element index to return.
            /// \return Reference to element at index.
            inline T &operator [] (std::size_t index) {
                if (index < length) {
                    return array[index];
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW);
                }
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

        /// \struct SortedArray Array.h thekogans/util/Array.h
        ///
        /// \brief
        /// Extends the capabilities of Array by imposing order on it's elements.
        /// It's a seperate class because it imposes additional properties on T.

        template<typename T>
        struct SortedArray : public Array<T> {
            /// \brief
            /// ctor. Create (or wrap) Array of length elements.
            /// \param[in] length Number of elements in the array.
            /// \param[in] array Optional array pointer to wrap.
            /// \param[in] deleter Deleter used to deallocate the array pointer.
            SortedArray (
                std::size_t length,
                T *array = nullptr,
                const typename Array<T>::Deleter &deleter = [] (T * /*array*/) {}) :
                Array<T> (length, array, deleter) {}
            /// \brief
            /// Move ctor.
            /// \param[in,out] other Array to move.
            SortedArray (Array<T> &&other) :
                Array<T> (other) {}

            /// \brief
            /// Sort the array elements in assending order.
            inline void Sort () {
                std::sort (this->array, this->array + this->length);
            }

            /// \brief
            /// Uses a binary search to locate elements in an ordered (Sort)ed array.
            /// WARNING: If you don't want garbage answers call this method
            /// only after you called Sort or you know a priori the array
            /// elements are sorted in assending order.
            /// \param[in] t Element to find.
            /// \param[out] index If found, return the index of the element.
            /// If not found, return the index where the element should be
            /// inserted to maintain assending order.
            /// \return true == found a match. false == no matching element found.
            bool Find (
                    const T &t,
                    std::size_t &index) {
                index = 0;
                std::size_t last = this->length;
                while (index < last) {
                    std::size_t middle = (index + last) / 2;
                    if (t == this->array[middle]) {
                        index = middle;
                        return true;
                    }
                    if (t < this->array[middle]) {
                        last = middle;
                    }
                    else {
                        index = middle + 1;
                    }
                }
                return false;
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Array_h)
