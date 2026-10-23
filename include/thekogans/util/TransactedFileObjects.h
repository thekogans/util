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

#if !defined (__thekogans_util_TransactedFileObjects_h)
#define __thekogans_util_TransactedFileObjects_h

#include <string>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/TransactedFile.h"
#include "thekogans/util/SerializableValues.h"

namespace thekogans {
    namespace util {

        /// \struct StringObject TransactedFileObjects.h thekogans/util/TransactedFileObjects.h
        ///
        /// \brief
        /// Variable size string object.
        struct _LIB_THEKOGANS_UTIL_DECL StringObject :
                public virtual TransactedFile::Object,
                public StringValue {
            /// \brief
            /// StringObject is a \see{TransactedFile}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (StringObject)

            /// \brief
            /// The actual string.
            std::string value;

            /// \brief
            /// ctor.
            /// \param[in] value_ std::string to initialize this value with.
            StringObject (const std::string &value_ = std::string ()) :
                value (value_) {}

            // TransactedFile
            /// \brief
            /// Return the serialized value size.
            /// \return Serialized value size.
            virtual std::size_t Size () const noexcept override {
                return Serializer::Size (value);
            }
            /// \brief
            /// Read the value from the given serializer.
            /// \param[in] serializer \see{Serializer} to read the value from.
            virtual void Read (Serializer &serializer) override {
                serializer >> value;
            }
            /// \brief
            /// Write the value to the given serializer.
            /// \param[out] serializer \see{Serializer} to write the value to.
            virtual void Write (Serializer &serializer) override {
                serializer << value;
            }
        };

        /// \struct ArrayObject TransactedFileObjects.h thekogans/util/TransactedFileObjects.h
        ///
        /// \brief
        /// ArrayObject is a template for storing arrays of types.
        template<typename T>
        struct ArrayObject :
                public virtual TransactedFile::Object,
                public ArrayValue<T> {
            /// \brief
            /// The actual array.
            std::vector<T> value;

            /// \brief
            /// ctor.
            /// \param[in] value_ Value to initialize with.
            ArrayObject (const std::vector<T> &value_ = std::vector<T> {}) :
                value (value_) {}

            /// \brief
            /// Return array length.
            /// \return Array length.
            inline std::size_t GetLength () const {
                return value.size ();
            }

            /// \brief
            /// const (rvalue) element accessor.
            /// \param[in] index Element index to return.
            /// \return reference to element at index.
            inline const T &operator [] (std::size_t index) const {
                if (index < value.size ()) {
                    return value[index];
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
                if (index < value.size ()) {
                    return value[index];
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW);
                }
            }

            // TransactedFile
            /// \brief
            /// Return the serialized value size.
            /// \return Serialized value size.
            virtual std::size_t Size () const noexcept override {
                return Serializer::Size (value);
            }
            /// \brief
            /// Read the value from the given serializer.
            /// \param[in] serializer \see{Serializer} to read the value from.
            virtual void Read (Serializer &serializer) override {
                serializer >> value;
            }
            /// \brief
            /// Write the value to the given serializer.
            /// \param[out] serializer \see{Serializer} to write the value to.
            virtual void Write (Serializer &serializer) override {
                serializer << value;
            }
        };

        /// \struct StringArrayObject TransactedFileObjects.h thekogans/util/TransactedFileObjects.h
        ///
        /// \brief
        /// Specialization of \see{ArrayObject} for std::string.
        struct _LIB_THEKOGANS_UTIL_DECL StringArrayObject : public ArrayObject<std::string> {
            /// \brief
            /// StringArrayObject is a \see{TransactedFile}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (StringArrayObject)

            /// \brief
            /// ctor.
            /// \param[in] object Optional std::string array to initialize with.
            StringArrayObject (const std::vector<std::string> &value = std::vector<std::string> ()) :
                ArrayObject<std::string> (value) {}
        };

        /// \struct StringArrayObject TransactedFileObjects.h thekogans/util/TransactedFileObjects.h
        ///
        /// \brief
        /// Specialization of \see{ArrayObject} for TransactedFile::AddressType.
        struct _LIB_THEKOGANS_UTIL_DECL AddressArrayObject : public ArrayObject<TransactedFile::AddressType> {
            /// \brief
            /// AddressArrayObject is a \see{TransactedFile}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (AddressArrayObject)

            /// \brief
            /// ctor.
            /// \param[in] object Optional TransactedFile::AddressType array to initialize with.
            AddressArrayObject (const std::vector<TransactedFile::AddressType> &value = std::vector<TransactedFile::AddressType> ()) :
                ArrayObject<TransactedFile::AddressType> (value) {}
        };

        /// \struct GUIDArrayObject TransactedFileObjects.h thekogans/util/TransactedFileObjects.h
        ///
        /// \brief
        /// Specialization of \see{ArrayObject} for \see{GUID}.
        struct _LIB_THEKOGANS_UTIL_DECL GUIDArrayObject : public ArrayObject<GUID> {
            /// \brief
            /// GUIDArrayObject is a \see{TransactedFile}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (GUIDArrayObject)

            /// \brief
            /// ctor.
            /// \param[in] object Optional \see{GUID} array to initialize with.
            GUIDArrayObject (const std::vector<GUID> &value = std::vector<GUID> ()) :
                ArrayObject<GUID> (value) {}
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_TransactedFileObjects_h)
