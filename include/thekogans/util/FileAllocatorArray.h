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

#if !defined (__thekogans_util_FileAllocatorArray_h)
#define __thekogans_util_FileAllocatorArray_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/SerializableArray.h"
#include "thekogans/util/FileAllocatorHeader.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        /// \struct FileAllocatorArray FileAllocatorArray.h thekogans/util/FileAllocatorArray.h
        ///
        /// \brief
        /// FileAllocatorArray aggregates \see{Serializable} derived types in to an array
        /// container. FileAllocatorArray uses the type \see{Serializable} information to
        /// create a \see{SerializableHeader} context so that the array elements are packed
        /// without wasting space writting the same header information.
        template<typename T>
        struct FileAllocatorArray : public FileAllocator::Object {
            /// \brief
            /// FileAllocatorArray<T> is a \see{FileAllocator::Object}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (FileAllocatorArray<T>)

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
            FileAllocatorArray (
                    FileAllocator::SharedPtr fileAllocator,
                    FileAllocator::PtrType offset,
                    std::size_t length = 0,
                    T *array = nullptr,
                    Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                    FileAllocator::Object (fileAllocator, offset),
                    array (length, array_, allocator) {
                if (GetOffset () != 0) {
                    Reload ();
                }
            }

            // BufferedFile::TransactionParticipant
            /// \brief
            /// Reset internal state.
            virtual void Reset () override {
                SerializableArray<T> temp;
                array.swap (temp);
            }

            // FileAllocator::Object
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

        /// \struct FileAllocatorSharedPtrArray FileAllocatorArray.h thekogans/util/FileAllocatorArray.h
        ///
        /// \brief
        /// FileAllocatorSharedPtrArray aggregates \see{Serializable}::SharedPtr types
        /// in to an array container. Unlike FileAllocatorArray, FileAllocatorSharedPtrArray
        /// cannot deduce the context based on template type as it itself can be an abstract
        /// base (see \see{BTree::Key}). You must therefore pass in a ctor context so that
        /// the array elements are packed without wasting space writting the same header
        /// information.
        template<typename T>
        struct FileAllocatorSharedPtrArray : public FileAllocator::Object {
            /// \brief
            /// FileAllocatorSharedPtrArray<typename T::SharedPtr> is a \see{FileAllocator::Object}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (FileAllocatorSharedPtrArray<typename T::SharedPtr>)

            /// \brief
            /// \see{SerializableSharedPtrArray} of T::SharedPtr elements.
            SerializableSharedPtrArray<typename T::SharedPtr> array;

            /// \brief
            /// ctor. Create (or wrap) an array of length elements.
            /// \param[in] fileAllocator
            /// \param[in] offset
            /// \param[in] context
            /// \param[in] length Number of elements in the array.
            /// \param[in] array_ Optional array pointer to wrap.
            /// \param[in] allocator \see{Allocator} used for memory management.
            FileAllocatorSharedPtrArray (
                    FileAllocator::SharedPtr fileAllocator,
                    FileAllocator::PtrType offset,
                    const FileAllocatorHeader &context = FileAllocatorHeader (),
                    std::size_t length = 0,
                    typename T::SharedPtr *array_ = nullptr,
                    Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                    FileAllocator::Object (fileAllocator, offset),
                    array (context, length, array_, allocator) {
                if (GetOffset () != 0) {
                    Reload ();
                }
            }

            // BufferedFile::TransactionParticipant
            /// \brief
            /// Reset internal state.
            virtual void Reset () override {
                SerializableSharedPtrArray<typename T::SharedPtr> temp;
                array.swap (temp);
            }

            // FileAllocator::Object
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

#endif // !defined (__thekogans_util_FileAllocatorArray_h)
