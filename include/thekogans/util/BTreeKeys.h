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

#if !defined (__thekogans_util_BTreeKeys_h)
#define __thekogans_util_BTreeKeys_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/BTree.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/Serializable.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        /// \struct StringKey BTreeKeys.h thekogans/util/BTreeKeys.h
        ///
        /// \brief
        /// Variable size string key.
        struct _LIB_THEKOGANS_UTIL_DECL StringKey : public BTree::Key {
            /// \brief
            /// StringKey is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (StringKey)

            /// \brief
            /// The actual key.
            std::string key;

            /// \brief
            /// ctor.
            /// \param[in] key_ std::string to initialize this key with.
            StringKey (const std::string &key_ = std::string ()) :
                key (key_) {}

            // BTree::Key
            /// \brief
            /// Used to find keys with matching prefixs.
            /// \param[in] prefix Key representing the prefix to compare against.
            /// \return -1 == this is < key, 0 == this == key, 1 == this is greater than key.
            virtual i32 PrefixCompare (const BTree::Key &prefix) const override;
            /// \brief
            /// Used to order keys.
            /// \param[in] key Key to compare against.
            /// \return -1 == this is < key, 0 == this == key, 1 == this is greater than key.
            virtual i32 Compare (const BTree::Key &key) const override;
            /// \brief
            /// This method is only used in Dump for debugging purposes.
            /// \return String representation of the key.
            virtual std::string ToString () const {
                return key;
            }

            // Serializable
            /// \brief
            /// Return the serialized key size.
            /// \return Serialized key size.
            virtual std::size_t Size () const noexcept override {
                return Serializer::Size (key);
            }

            /// \brief
            /// Read the key from the given serializer.
            /// \param[in] header \see{Serializable::Header}.
            /// \param[in] serializer \see{Serializer} to read the key from.
            virtual void Read (
                    const Header & /*header*/,
                    Serializer &serializer) override {
                serializer >> key;
            }
            /// \brief
            /// Write the key to the given serializer.
            /// \param[out] serializer \see{Serializer} to write the key to.
            virtual void Write (Serializer &serializer) const override {
                serializer << key;
            }
        };

        /// \struct GUIDKey BTree.h thekogans/util/BTreeKeys.h
        ///
        /// \brief
        /// GUID key.
        struct _LIB_THEKOGANS_UTIL_DECL GUIDKey : public BTree::Key {
            /// \brief
            /// GUIDKey is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (GUIDKey)

            /// \brief
            /// The actual key.
            GUID key;

            /// \brief
            /// ctor.
            /// \param[in] key_ \see{GUID} to initialize this key with.
            GUIDKey (const GUID &key_ = GUID {}) :
                key (key_) {}

            // BTree::Key
            /// \brief
            /// Used to find keys with matching prefixs.
            /// \param[in] prefix Key representing the prefix to compare against.
            /// \return -1 == this is < key, 0 == this == key, 1 == this is greater than key.
            virtual i32 PrefixCompare (const BTree::Key &prefix) const override;
            /// \brief
            /// Used to order keys.
            /// \param[in] key Key to compare against.
            /// \return -1 == this is < key, 0 == this == key, 1 == this is greater than key.
            virtual i32 Compare (const BTree::Key &key) const override;
            /// \brief
            /// This method is only used in Dump for debugging purposes.
            /// \return String representation of the key.
            virtual std::string ToString () const {
                return key.ToHexString ();
            }

            // Serializable
            /// \brief
            /// Return the serialized key size.
            /// \return Serialized key size.
            virtual std::size_t Size () const noexcept override {
                return Serializer::Size (key);
            }

            /// \brief
            /// Read the key from the given serializer.
            /// \param[in] header \see{Serializable::Header}.
            /// \param[in] serializer \see{Serializer} to read the key from.
            virtual void Read (
                    const Header & /*header*/,
                    Serializer &serializer) override {
                serializer >> key;
            }
            /// \brief
            /// Write the key to the given serializer.
            /// \param[out] serializer \see{Serializer} to write the key to.
            virtual void Write (Serializer &serializer) const override {
                serializer << key;
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_BTreeKeys_h)
