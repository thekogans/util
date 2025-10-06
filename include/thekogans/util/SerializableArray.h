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
        /// create a \see{SerializableHeader} context so that the array elements are packed
        /// without wasting space writting the same header information.
        template<typename T>
        struct SerializableArray : public Serializable {
            /// \brief
            /// SerializableArray<T> is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (SerializableArray<T>)

            /// \brief
            /// Context for the elements of the array.
            SerializableHeader context;
            /// \brief
            /// \see{Array} of T elements.
            Array<T> array;

            /// \brief
            /// ctor. Create (or wrap) an array of length elements.
            /// VERY IMPORTANT: Array does not make a copy of the array_
            /// passed in. It's up to you to make sure the pointer survives
            /// the Array lifetime.
            /// \param[in] length_ Number of elements in the array.
            /// \param[in] array_ Optional array pointer to wrap.
            /// \param[in] allocator_ \see{Allocator} used for memory management.
            SerializableArray (
                std::size_t length = 0,
                T *array_ = nullptr,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                context (T::TYPE, T::VERSION, T::CLASS_SIZE),
                array (length, array_, allocator) {}

            /// \brief
            /// std::swap for Array.
            inline void swap (SerializableArray<T> &other) {
                std::swap (context, other.context);
                array.swap (other.array);
            }

            // Serializable
            /// \brief
            /// Return the serialized array size.
            /// \return Serialized array size.
            virtual std::size_t Size () const noexcept override {
                std::size_t size = Serializer::Size (array.length);
                if (context.NeedSize ()) {
                    for (std::size_t i = 0; i < array.length; ++i) {
                        size += Serializer::Size (array[i], context);
                    }
                }
                else {
                    size += array.length *
                        ((context.NeedVersion () ? Serializer::Size (context.version) : 0) +
                            context.size);
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

        /// \struct SerializableSharedPtrArray SerializableArray.h thekogans/util/SerializableArray.h
        ///
        /// \brief
        /// SerializableSharedPtrArray aggregates \see{Serializable}::SharedPtr types
        /// in to an array container. Unlike SerializableArray, SerializableSharedPtrArray
        /// cannot deduce the context based on template type as it itself can be an abstract
        /// base (see \see{BTree::Key}). You must therefore pass in a ctor context so that
        /// the array elements are packed without wasting space writting the same header
        /// information.
        template<typename T>
        struct SerializableSharedPtrArray : public Serializable {
            /// \brief
            /// SerializableSharedPtrArray<typename T::SharedPtr> is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (SerializableSharedPtrArray<typename T::SharedPtr>)

            /// \brief
            /// Context for the elements of the array.
            SerializableHeader context;
            /// \brief
            /// Default \see{Serializable} factory.
            DynamicCreatable::FactoryType factory;
            /// \brief
            /// \see{Array} of T::SharedPtr elements.
            Array<typename T::SharedPtr> array;

            /// \brief
            /// ctor. Create (or wrap) an array of length elements.
            /// \param[in] length_ Number of elements in the array.
            /// \param[in] array_ Optional array pointer to wrap.
            /// \param[in] allocator_ \see{Allocator} used for memory management.
            SerializableSharedPtrArray (
                const SerializableHeader &context_ = SerializableHeader (),
                std::size_t length = 0,
                typename T::SharedPtr *array_ = nullptr,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                context (context_),
                factory (Serializable::GetTypeFactory (context.type.c_str ())),
                array (length, array_, allocator) {}

            /// \brief
            /// std::swap for Array.
            inline void swap (SerializableSharedPtrArray<T> &other) {
                std::swap (context, other.context);
                std::swap (factory, other.factory);
                array.swap (other.array);
            }

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
                Serializer::ContextGuard guard (serializer, context, factory);
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
