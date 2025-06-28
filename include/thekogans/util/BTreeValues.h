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

#if !defined (__thekogans_util_BTreeValues_h)
#define __thekogans_util_BTreeValues_h

#include <string>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/BTree.h"

namespace thekogans {
    namespace util {

        /// \struct StringValue BTreeValues.h thekogans/util/BTreeValues.h
        ///
        /// \brief
        /// Variable size string value.
        struct _LIB_THEKOGANS_UTIL_DECL StringValue : public BTree::Value {
            /// \brief
            /// StringValue is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (StringValue)

            /// \brief
            /// The actual string.
            std::string value;

            /// \brief
            /// ctor.
            /// \param[in] value_ std::string to initialize this value with.
            StringValue (const std::string &value_ = std::string ()) :
                value (value_) {}

            // BTree::Value
            /// \brief
            /// This method is only used in Dump for debugging purposes.
            /// \return String representation of the value.
            virtual std::string ToString () const override {
                return value;
            }

            // Serializable
            /// \brief
            /// Return the serialized value size.
            /// \return Serialized value size.
            virtual std::size_t Size () const noexcept override {
                return Serializer::Size (value);
            }

            /// \brief
            /// Read the value from the given serializer.
            /// \param[in] header \see{Serializable::Header}.
            /// \param[in] serializer \see{Serializer} to read the value from.
            virtual void Read (
                    const Header & /*header*/,
                    Serializer &serializer) override {
                serializer >> value;
            }
            /// \brief
            /// Write the value to the given serializer.
            /// \param[out] serializer \see{Serializer} to write the value to.
            virtual void Write (Serializer &serializer) const override {
                serializer << value;
            }
        };

        /// \struct PtrValue BTreeValues.h thekogans/util/BTreeValues.h
        ///
        /// \brief
        /// \see{FileAllocator::PtrType} value.
        struct _LIB_THEKOGANS_UTIL_DECL PtrValue : public BTree::Value {
            /// \brief
            /// PtrValue is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (PtrValue)

            /// \brief
            /// The actual ptr.
            FileAllocator::PtrType value;

            /// \brief
            /// ctor.
            /// \param[in] value_ FileAllocator::PtrType to initialize this value with.
            PtrValue (FileAllocator::PtrType value_ = 0) :
                value (value_) {}

            // BTree::Value
            /// \brief
            /// This method is only used in Dump for debugging purposes.
            /// \return String representation of the value.
            virtual std::string ToString () const override {
                return ui64Tostring (value);
            }

            // Serializable
            /// \brief
            /// Return the serialized value size.
            /// \return Serialized value size.
            virtual std::size_t Size () const noexcept override {
                return Serializer::Size (value);
            }

            /// \brief
            /// Read the value from the given serializer.
            /// \param[in] header \see{Serializable::Header}.
            /// \param[in] serializer \see{Serializer} to read the value from.
            virtual void Read (
                    const Header & /*header*/,
                    Serializer &serializer) override {
                serializer >> value;
            }
            /// \brief
            /// Write the value to the given serializer.
            /// \param[out] serializer \see{Serializer} to write the value to.
            virtual void Write (Serializer &serializer) const override {
                serializer << value;
            }
        };

        /// \struct ArrayValue BTreeValues.h thekogans/util/BTreeValues.h
        ///
        /// \brief
        /// ArrayValue is a template for storing arrays of types.
        template<typename T>
        struct ArrayValue : public BTree::Value {
            /// \brief
            /// The actual array.
            std::vector<T> value;

            /// \brief
            /// ctor.
            /// \param[in] value_ Value to initialize with.
            ArrayValue (const std::vector<T> &value_ = std::vector<T> {}) :
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

            // BTree::Value
            /// \brief
            /// This method is only used in Dump for debugging purposes.
            /// \return String representation of the value.
            virtual std::string ToString () const override {
                return TYPE;
            }

            // Serializable
            /// \brief
            /// Return the serialized value size.
            /// \return Serialized value size.
            virtual std::size_t Size () const noexcept override {
                return Serializer::Size (value);
            }

            /// \brief
            /// Read the value from the given serializer.
            /// \param[in] header \see{Serializable::Header}.
            /// \param[in] serializer \see{Serializer} to read the value from.
            virtual void Read (
                    const Header & /*header*/,
                    Serializer &serializer) override {
                serializer >> value;
            }
            /// \brief
            /// Write the value to the given serializer.
            /// \param[out] serializer \see{Serializer} to write the value to.
            virtual void Write (Serializer &serializer) const override {
                serializer << value;
            }
        };

        /// \struct StringArrayValue BTreeValues.h thekogans/util/BTreeValues.h
        ///
        /// \brief
        /// Specialization of \see{ArrayValue} for std::string.
        struct _LIB_THEKOGANS_UTIL_DECL StringArrayValue : public ArrayValue<std::string> {
            /// \brief
            /// StringArrayValue is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (StringArrayValue)

            /// \brief
            /// ctor.
            /// \param[in] value Optional std::string array to initialize with.
            StringArrayValue (const std::vector<std::string> &value = std::vector<std::string> ()) :
                ArrayValue<std::string> (value) {}
        };

        /// \struct GUIDArrayValue BTreeValues.h thekogans/util/BTreeValues.h
        ///
        /// \brief
        /// Specialization of \see{ArrayValue} for \see{GUID}.
        struct _LIB_THEKOGANS_UTIL_DECL GUIDArrayValue : public ArrayValue<GUID> {
            /// \brief
            /// GUIDArrayValue is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (GUIDArrayValue)

            /// \brief
            /// ctor.
            /// \param[in] value Optional \see{GUID} array to initialize with.
            GUIDArrayValue (const std::vector<GUID> &value = std::vector<GUID> ()) :
                ArrayValue<GUID> (value) {}
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_BTreeValues_h)
