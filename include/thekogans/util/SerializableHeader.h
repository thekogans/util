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

#if !defined (__thekogans_util_SerializableHeader_h)
#define __thekogans_util_SerializableHeader_h

#include <cstddef>
#include <string>
#include "pugixml/pugixml.hpp"
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/JSON.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Forward declaration for \see{Serializer}.
        struct Serializer;

        /// \struct SerializableHeader SerializableHeader.h thekogans/util/SerializableHeader.h
        ///
        /// \brief
        /// SerializableHeader is a variable size header containing the metadata
        /// needed to extract a \see{Serializable} instance from a \see{Serializer}
        /// without knowing it's concrete type. It's variable size because the members
        /// inserted in to or extracted out of a \see{Serializer} depend on the current
        /// context. This context commes in the form of \see{Serializer::context} which
        /// tells operators << and >> whats missing and needs to be inserted or extracted.
        /// The more members the contet has filled in, the fewer the header will need
        /// to insert or extract. This design allows for aggregation of like \see{Serializable}
        /// types and saves space (not to mention insertion/extraction time).
        struct _LIB_THEKOGANS_UTIL_DECL SerializableHeader {
            /// \brief
            /// Serializable type (see \see{DynamicCreatable::Type}).
            std::string type;
            /// \brief
            /// \see{Serializable} version.
            ui16 version;
            /// \brief
            /// \see{Serializable} size in bytes (not including the header).
            SizeT size;

            /// \brief
            /// ctor.
            /// \param[in] type_ \see{Serializable} type (see \see{DynamicCreatable::Type}).
            /// \param[in] version_ \see{Serializable} version.
            /// \param[in] size_ \see{Serializable} size in bytes (not including the header).
            SerializableHeader (
                const std::string &type_ = std::string (),
                ui16 version_ = 0,
                std::size_t size_ = 0) :
                type (type_),
                version (version_),
                size (size_) {}

            /// \brief
            /// "Type"
            static const char * const ATTR_TYPE;
            /// \brief
            /// "Version"
            static const char * const ATTR_VERSION;
            /// \brief
            /// "Size"
            static const char * const ATTR_SIZE;

            inline bool NeedType () const {
                return type.empty ();
            }
            inline bool NeedVersion () const {
                return version == 0;
            }
            inline bool NeedSize () const {
                return size == 0;
            }

            inline bool IsEmpty () const {
                return NeedType () && NeedVersion () && NeedSize ();
            }
            inline bool IsFull () const {
                return !NeedType () && !NeedVersion () && !NeedSize ();
            }

            /// \brief
            /// Return the binary header size.
            /// \return SerializableHeader binary size.
            std::size_t Size () const;
        };

        /// \brief
        /// SerializableHeader insertion operator.
        /// \param[in] serializer Where to serialize the serializable header.
        /// \param[in] header SerializableHeader to serialize.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
            Serializer &serializer,
            const SerializableHeader &header);
        /// \brief
        /// SerializableHeader extraction operator.
        /// \param[in] serializer Where to deserialize the serializable header.
        /// \param[in] header SerializableHeader to deserialize.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
            Serializer &serializer,
            SerializableHeader &header);

        /// \brief
        /// SerializableHeader insertion operator.
        /// \param[in] node Where to serialize the serializable header.
        /// \param[in] header SerializableHeader to serialize.
        /// \return node.
        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
            pugi::xml_node &node,
            const SerializableHeader &header);
        /// \brief
        /// SerializableHeader extraction operator.
        /// \param[in] node Where to deserialize the serializable header.
        /// \param[in] header SerializableHeader to deserialize.
        /// \return node.
        _LIB_THEKOGANS_UTIL_DECL const pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
            const pugi::xml_node &node,
            SerializableHeader &header);

        /// \brief
        /// SerializableHeader insertion operator.
        /// \param[in] object Where to serialize the serializable header.
        /// \param[in] header SerializableHeader to serialize.
        /// \return object.
        _LIB_THEKOGANS_UTIL_DECL JSON::Object & _LIB_THEKOGANS_UTIL_API operator << (
            JSON::Object &object,
            const SerializableHeader &header);
        /// \brief
        /// SerializableHeader extraction operator.
        /// \param[in] object \see{JSON::Object} containing the serializable header.
        /// \param[in] header SerializableHeader to deserialize.
        /// \return object.
        _LIB_THEKOGANS_UTIL_DECL const JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (
            const JSON::Object &object,
            SerializableHeader &header);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SerializableHeader_h)
