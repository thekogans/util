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
        template<typename T>
        struct _LIB_THEKOGANS_UTIL_DECL ArrayValue : public BTree::Value {
            /// \brief
            /// ArrayValue is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (ArrayValue)

            /// \brief
            /// The actual array.
            std::vector<T> value;

            /// \brief
            /// ctor.
            /// \param[in] value_ Value to initialize with.
            ArrayValue (const std::vector<T> &value_ = std::vector<T> {}) :
                value (value_) {}

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

        using StringArrayValue = ArrayValue<std::string>;
        using GUIDArrayValue = ArrayValue<GUID>;

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_BTreeValues_h)
