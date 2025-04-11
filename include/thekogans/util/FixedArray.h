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

#include "thekogans/util/Constants.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/SecureAllocator.h"

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
            std::size_t capacity>
        struct FixedArray {
            /// \brief
            /// FixedArray length.
            std::size_t length;
            /// \brief
            /// FixedArray elements.
            T array[capacity];

            /// \brief
            /// ctor.
            /// \param[in] array_ Elements to initialize the array with.
            /// \param[in] length_ Number of elements in array_.
            /// \param[in] clearUnused Clear unused array elements to 0.
            FixedArray (
                    const T *array_ = nullptr,
                    std::size_t length_ = 0,
                    bool clearUnused = false) :
                    length (array_ != nullptr ? length_ : 0) {
                if (length <= capacity) {
                    for (std::size_t i = 0; i < length; ++i) {
                        array[i] = array_[i];
                    }
                    if (clearUnused) {
                        SecureZeroMemory (array + length, (capacity - length) * sizeof (T));
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
            explicit FixedArray (
                    const T &value,
                    std::size_t length_ = capacity) :
                    length (MIN (length_, capacity)) {
                for (std::size_t i = 0; i < length; ++i) {
                    array[i] = value;
                }
            }

            /// \brief
            /// Return the number of elements the array can hold.
            /// \return Number of elements in the array.
            inline std::size_t GetCapacity () const {
                return capacity;
            }

            /// \brief
            /// Return the number of elements in the array.
            /// \return Number of elements in the array.
            inline std::size_t GetLength () const {
                return length;
            }
            /// \brief
            /// Set the length of the array.
            /// \param[in] length_ New array length.
            inline void SetLength (std::size_t length_) {
                if (length_ <= capacity) {
                    length = length_;
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }

            /// \brief
            /// Return the size of the array in bytes.
            /// NOTE: This is the same Size used by all objects
            /// to return the binary serialized size on disk.
            /// \return Serialized binary size, in bytes, of the array.
            inline std::size_t Size () const {
                return length * sizeof (T);
            }

            /// \brief
            /// const (rvalue) element accessor.
            /// \param[in] index Element index to return.
            /// \return reference to element at index.
            inline const T &operator [] (std::size_t index) const {
                if (index < capacity) {
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
                if (index < capacity) {
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
        };


        /// \struct SecureFixedArray FixedArray.h thekogans/util/FixedArray.h
        ///
        /// \brief
        /// SecureFixedArray is a specialization of \see{FixedArray}.
        /// Adds extra protection by zeroing out the uninitilized space
        /// in the ctor and zeroing out the whole memory block in the dtor.

        template<
            typename T,
            std::size_t capacity>
        struct SecureFixedArray : public FixedArray<T, capacity> {
            /// \brief
            /// ctor.
            /// \param[in] array Elements to initialize the array with.
            /// \param[in] length Number of elements in array.
            SecureFixedArray (
                const T *array = nullptr,
                std::size_t length = 0) :
                FixedArray<T, capacity> (array, length, true) {}
            /// \brief
            /// dtor.
            /// Zero out the sensitive memory block.
            ~SecureFixedArray () {
                SecureZeroMemory (this->array, capacity * sizeof (T));
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FixedArray_h)
