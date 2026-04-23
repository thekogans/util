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

#if !defined (__thekogans_util_TransactedFileArray_h)
#define __thekogans_util_TransactedFileArray_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/SerializableArray.h"
#include "thekogans/util/SerializableHeader.h"
#include "thekogans/util/TransactedFile.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        /// \struct TransactedFileArray TransactedFileArray.h thekogans/util/TransactedFileArray.h
        ///
        /// \brief
        /// TransactedFileArray aggregates \see{Serializable} derived types in to an array
        /// container. TransactedFileArray uses the type \see{Serializable} information to
        /// create a \see{SerializableHeader} context so that the array elements are packed
        /// without wasting space writting the same header information.
        template<typename T>
        struct TransactedFileArray : public TransactedFile::Object {
            /// \brief
            /// TransactedFileArray<T> is a \see{TransactedFile::Object}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (TransactedFileArray<T>)

            /// \brief
            /// \see{SerializerArray} of T elements.
            SerializerArray<T> array;

            /// \brief
            /// ctor. Create (or wrap) an array of length elements.
            /// VERY IMPORTANT: Array does not make a copy of the array_
            /// passed in. It's up to you to make sure the pointer survives
            /// the Array lifetime.
            /// \param[in] fileAllocator
            /// \param[in] offset
            /// \param[in] length Number of elements in the array.
            /// \param[in] array_ Optional array pointer to wrap.
            /// \param[in] allocator \see{Allocator} used for memory management.
            TransactedFileArray (
                    TransactedFile::Allocator::SharedPtr allocator,
                    TransactedFile::Allocator::PtrType offset,
                    std::size_t length = 0,
                    T *array = nullptr,
                    util::Allocator::SharedPtr allocator_ = DefaultAllocator::Instance ()) :
                    TransactedFile::Object (allocator, offset),
                    array (length, array_, allocator_) {
                if (GetOffset () != 0 && GetLength () == 0) {
                    Reload ();
                }
            }

            // TransactedFile::TransactionParticipant
            /// \brief
            /// Reset internal state.
            virtual void Reset () override {
                SerializableArray<T> temp;
                array.swap (temp);
            }

            // TransactedFile::Object
            /// \brief
            /// Return the serialized array size.
            /// \return Serialized array size.
            virtual std::size_t Size () const noexcept override {
                return array.Size ();
            }
            /// \brief
            /// Read the array from the given \see{Serializer}.
            /// \param[in] header Serialized array header.
            /// \param[in] serializer \see{Serializer} to read the array from.
            virtual void Read (Serializer &serializer) override {
                array.Read (serializer);
            }
            /// \brief
            /// Write the array to the given \see{Serializer}.
            /// \param[out] serializer \see{Serializer} to write the array to.
            virtual void Write (Serializer &serializer) const override {
                array.Write (serializer);
            }
        };

        /// \struct TransactedFileSharedPtrArray TransactedFileArray.h thekogans/util/TransactedFileArray.h
        ///
        /// \brief
        /// TransactedFileSharedPtrArray aggregates \see{Serializable}::SharedPtr types
        /// in to an array container. Unlike TransactedFileArray, TransactedFileSharedPtrArray
        /// cannot deduce the context based on template type as it itself can be an abstract
        /// base (see \see{BTree::Key}). You must therefore pass in a ctor context so that
        /// the array elements are packed without wasting space writting the same header
        /// information.
        template<typename T>
        struct TransactedFileSharedPtrArray : public TransactedFile::Object {
            /// \brief
            /// TransactedFileSharedPtrArray<typename T::SharedPtr> is a \see{TransactedFile::Object}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (TransactedFileSharedPtrArray<typename T::SharedPtr>)

            /// \brief
            /// \see{SerializableSharedPtrArray} of T::SharedPtr elements.
            SerializableSharedPtrArray<typename T::SharedPtr> array;

            /// \brief
            /// ctor. Create (or wrap) an array of length elements.
            /// \param[in] allocator
            /// \param[in] offset
            /// \param[in] context
            /// \param[in] length Number of elements in the array.
            /// \param[in] array_ Optional array pointer to wrap.
            /// \param[in] allocator_ \see{Allocator} used for memory management.
            TransactedFileSharedPtrArray (
                    TransactedFile::Allocator::SharedPtr allocator,
                    TransactedFile::Allocator::PtrType offset,
                    const TransactedFileHeader &context = TransactedFileHeader (),
                    std::size_t length = 0,
                    typename T::SharedPtr *array_ = nullptr,
                    util::Allocator::SharedPtr allocator_ = DefaultAllocator::Instance ()) :
                    TransactedFile::Object (allocator, offset),
                    array (context, length, array_, allocator_) {
                if (GetOffset () != 0 && GetLength () == 0) {
                    Reload ();
                }
            }

            // TransactedFile::TransactionParticipant
            /// \brief
            /// Reset internal state.
            virtual void Reset () override {
                SerializableSharedPtrArray<typename T::SharedPtr> temp;
                array.swap (temp);
            }

            // TransactedFile::Object
            /// \brief
            /// Return the serialized array size.
            /// \return Serialized array size.
            virtual std::size_t Size () const noexcept override {
                return array.Size ();
            }
            /// \brief
            /// Read the array from the given \see{Serializer}.
            /// \param[in] header Serialized array header.
            /// \param[in] serializer \see{Serializer} to read the array from.
            virtual void Read (Serializer &serializer) override {
                array.Read (serializer);
            }
            /// \brief
            /// Write the array to the given \see{Serializer}.
            /// \param[out] serializer \see{Serializer} to write the array to.
            virtual void Write (Serializer &serializer) const override {
                array.Write (serializer);
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_TransactedFileArray_h)
