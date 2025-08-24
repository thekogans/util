// Copyright 2016 Boris Kogan (boris@thekogans.net)
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

#if !defined (__thekogans_util_SerializableArray_h)
#define __thekogans_util_SerializableArray_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/Array.h"
#include "thekogans/util/SerializableHeader.h"
#include "thekogans/util/Serializable.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        /// \struct SerializableArray SerializableArray.h thekogans/util/SerializableArray.h
        ///
        /// \brief
        /// SerializableArray aggregates \see{Serializable} derived types in to an array
        /// container. SerializableArray uses the type \see{Serializable} information to
        /// create a \see{SerializableHeader} contex so that the array elements are packed
        /// without wasting space writting the same header information.
        template<typename T>
        struct SerializableArray : public Serializable {
            /// \brief
            /// SerializableArray<T> is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (SerializableArray<T>)

            /// \brief
            /// Context for the elements of the array.
            const SerializableHeader context;
            /// \brief
            /// \see{Array} of T elements.
            Array<T> array;

            /// \brief
            /// ctor. Create (or wrap) an array of length elements.
            /// \param[in] length_ Number of elements in the array.
            /// \param[in] array_ Optional array pointer to wrap.
            /// \param[in] allocator_ \see{Allocator} used for memory management.
            SerializableArray (
                std::size_t length = 0,
                T *array_ = nullptr,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                context (T::TYPE, T::VERSION, T::CLASS_SIZE),
                array (length, array_, allocator) {}

            // Serializable
            /// \brief
            /// Return the serialized array size.
            /// \return Serialized array size.
            virtual std::size_t Size () const noexcept override {
                std::size_t size = Serializer::Size (array.length);
                for (std::size_t i = 0; i < array.length; ++i) {
                    size += Serializer::Size (array[i], context);
                }
                return size;

            }
            /// \brief
            /// Read the array from the given \see{Serializer}.
            /// \param[in] header Serialized array header.
            /// \param[in] serializer \see{Serializer} to read the array from.
            virtual void Read (
                    const SerializableHeader & /*header*/,
                    Serializer &serializer) override {
                Serializer::ContextGuard guard (serializer, context);
                serializer >> array;
            }
            /// \brief
            /// Write the array to the given \see{Serializer}.
            /// \param[out] serializer \see{Serializer} to write the array to.
            virtual void Write (Serializer &serializer) const override {
                Serializer::ContextGuard guard (serializer, context);
                serializer << array;
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SerializableArray_h)
