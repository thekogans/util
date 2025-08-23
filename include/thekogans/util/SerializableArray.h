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
#include "thekogans/util/Array.h"
#include "thekogans/util/SerializableHeader.h"
#include "thekogans/util/Serializable.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        /// \struct SerializableArray SerializableArray.h thekogans/util/SerializableArray.h
        ///
        /// \brief
        template<typename T>
        struct SerializableArray : public Serializable {
            /// \brief
            /// SerializableArray<T> is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (SerializableArray<T>)

            /// \brief
            /// Context for the elements of the array.
            const SerializableHeader context;
            /// \brief
            ///
            Array<T> array;

            SerializableArray (
                std::size_t length = 0,
                T *array_ = nullptr,
                const typename Array<T>::Deleter &deleter =
                    [] (T * /*array*/, std::size_t /*length*/) {}) :
                context (T::TYPE, T::VERSION, T::CLASS_SIZE),
                array (length, array_, deleter) {}

            // Serializable
            /// \brief
            /// Return the serializable binary size.
            /// \return Serializable binary size.
            virtual std::size_t Size () const noexcept {
                std::size_t size = SizeT (array.GetLength ()).Size ();
                for (std::size_t i = 0, count = array.GetLength (); i < count; ++i) {
                    size += array[i].GetSize (context);
                }
                return size;

            }
            /// \brief
            /// Write the serializable from the given serializer.
            /// \param[in] header
            /// \param[in] serializer Serializer to read the serializable from.
            virtual void Read (
                    const SerializableHeader & /*header*/,
                    Serializer &serializer) {
                Serializer::ContextGuard guard (serializer, context);
                serializer >> array;
            }
            /// \brief
            /// Write the serializable to the given serializer.
            /// \param[out] serializer Serializer to write the serializable to.
            virtual void Write (Serializer &serializer) const {
                Serializer::ContextGuard guard (serializer, context);
                serializer << array;
            }
        };

        /// \brief
        /// Serialize a Array<T>.
        /// \param[in] serializer Where to write the given Array<T>.
        /// \param[in] array Array<T> to serialize.
        /// \return serializer.
        template<typename T>
        Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const SerializableArray<T> &array) {
            serializer << array.GetHeader (serializer.context);
            array.Write (serializer);
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
                SerializableArray<T> &array) {
            SerializableHeader header;
            serializer >> header;
            if (header.type == array.Type ()) {
                serializable.Read (header, serializer);
                return serializer;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Header type (%s) is not the same as Array type (%s).",
                    header.type.c_str (),
                    array.Type ());
            }
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SerializableArray_h)
