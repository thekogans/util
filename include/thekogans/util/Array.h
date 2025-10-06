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
#include "thekogans/util/Constants.h"
#include "thekogans/util/SizeT.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/DefaultAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct Array Array.h thekogans/util/Array.h
        ///
        /// \brief
        /// Unlike \see{Buffer}, which models an array of ui8, Array
        /// represents an array of first class objects. Also, unlike \see{Buffer},
        /// which is meant to be a \see{Serializer}, Array has a completelly
        /// different mission. Arrays are meant to be lightweight containers
        /// with some first class properties (order and serialization).
        template<typename T>
        struct Array {
            /// \brief
            /// Array length.
            SizeT length;
            /// \brief
            /// Array elements.
            T *array;
            /// \brief
            /// \see{Allocator} used for memory management.
            Allocator::SharedPtr allocator;

            /// \brief
            /// ctor. Create (or wrap) array of length elements.
            /// \param[in] length_ Number of elements in the array.
            /// \param[in] array_ Optional array pointer to wrap.
            /// \param[in] allocator_ \see{Allocator} used for memory management.
            Array (
                    std::size_t length_ = 0,
                    T *array_ = nullptr,
                    Allocator::SharedPtr allocator_ = DefaultAllocator::Instance ()) :
                    length (length_),
                    array (length_ > 0 && array_ == nullptr ?
                        (T *)allocator_->Alloc (length_ * sizeof (T)) :
                        array_),
                    allocator (allocator_) {
                if (allocator != nullptr) {
                    for (std::size_t i = 0; i < length; ++i) {
                        new (&array[i]) T ();
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }
            /// \brief
            /// Move ctor.
            /// \param[in,out] other Array to move.
            Array (Array<T> &&other) :
                    length (0),
                    array (nullptr) {
                swap (other);
            }
            /// \brief
            /// dtor. Release the memory held by Array.
            ~Array () {
                Resize (0);
            }

            /// \brief
            /// Move assignment operator.
            /// \param[in,out] other Array to move.
            /// \return *this;
            Array<T> &operator = (Array<T> &&other) {
                if (this != &other) {
                    Array<T> temp (std::move (*this));
                    swap (other);
                }
                return *this;
            }

            /// \brief
            /// Return array length.
            /// \return Array length.
            inline std::size_t GetLength () const {
                return length;
            }

            /// \brief
            /// std::swap for Array.
            inline void swap (Array<T> &other) {
                std::swap (length, other.length);
                std::swap (array, other.array);
                std::swap (allocator, other.allocator);
            }

            /// \brief
            /// Resize the array.
            /// \param[in] length_ New array length.
            /// \param[in] allocator_ New \see{Allocator} (nullptr == use the existing allocator).
            void Resize (
                    std::size_t length_,
                    Allocator::SharedPtr allocator_ = nullptr) {
                if (length != length_) {
                    if (allocator_ == nullptr) {
                        allocator_ = allocator;
                    }
                    T *array_ = nullptr;
                    if (length_ > 0) {
                        array_ = (T *)allocator_->Alloc (length_ * sizeof (T));
                        if (array != nullptr) {
                            std::size_t i = 0;
                            for (std::size_t count =
                                    MIN (length_, (std::size_t)length.value); i < count; ++i) {
                                new (&array_[i]) T (std::move (array[i]));
                            }
                            for (; i < length_; ++i) {
                                new (&array_[i]) T ();
                            }
                            for (; i < length; ++i) {
                                array[i].~T ();
                            }
                        }
                    }
                    else {
                        for (std::size_t i = 0; i < length; ++i) {
                            array[i].~T ();
                        }
                    }
                    allocator->Free (array, length);
                    array = array_;
                    length = length_;
                    allocator = allocator_;
                }
            }

            /// \brief
            /// Return the size of the array in bytes.
            /// NOTE: This is the same Size used by all objects
            /// to return the binary serialized size on disk.
            /// \return Serialized binary size, in bytes, of the array.
            inline std::size_t Size () const noexcept {
                std::size_t size = Serializer::Size (length);
                for (std::size_t i = 0; i < length; ++i) {
                    size += Serializer::Size (array[i]);
                }
                return size;
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
            /// Sort the array elements in assending order.
            inline void Sort () {
                std::sort (array, array + length);
            }

            /// \brief
            /// Uses a binary search to locate elements in an ordered array.
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
                std::size_t last = length;
                while (index < last) {
                    std::size_t middle = (index + last) / 2;
                    if (t == array[middle]) {
                        index = middle;
                        return true;
                    }
                    if (t < array[middle]) {
                        last = middle;
                    }
                    else {
                        index = middle + 1;
                    }
                }
                return false;
            }

            /// \brief
            /// Array is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Array)
        };

        /// \brief
        /// Serialize a Array<T>.
        /// \param[in] serializer Where to write the given Array<T>.
        /// \param[in] array Array<T> to serialize.
        /// \return serializer.
        template<typename T>
        Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const Array<T> &array) {
            serializer << array.length;
            for (std::size_t i = 0; i < array.length; ++i) {
                serializer << array[i];
            }
            return serializer;
        }

        /// \brief
        /// Extract a Array<T> from the given \see{Serializer}.
        /// \param[in] serializer Where to read the Array<T> from.
        /// \param[out] Array Where to place the extracted Array<T>.
        /// \return serializer.
        template<typename T>
        Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Array<T> &array) {
            SizeT length;
            serializer >> length;
            Array<T> temp (length);
            for (std::size_t i = 0; i < length; ++i) {
                serializer >> temp[i];
            }
            array.swap (temp);
            return serializer;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Array_h)
