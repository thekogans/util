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

#include "thekogans/util/Serializable.h"

namespace thekogans {
    namespace util {

        Serializable::Map &Serializable::GetMap () {
            static Map map;
            return map;
        }

        Serializable::MapInitializer::MapInitializer (
                const std::string &type,
                Factory factory) {
            std::pair<Map::iterator, bool> result =
                GetMap ().insert (Map::value_type (type, factory));
            if (!result.second) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "'%s' is already registered.", type.c_str ());
            }
        }

        std::size_t Serializable::Size (const Serializable &serializable) {
            Header header (
                serializable.Type (),
                serializable.Version (),
                (ui32)serializable.Size ());
            return header.Size () + header.size;
        }

        std::size_t Serializable::Serialize (ui8 *buffer) const {
            if (buffer != 0) {
                Header header (Type (), Version (), (ui32)Size ());
                TenantWriteBuffer buffer_ (
                    NetworkEndian,
                    buffer,
                    (ui32)(header.Size () + header.size));
                buffer_ << header;
                Write (buffer_);
                return buffer_.GetDataAvailableForReading ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Buffer::UniquePtr Serializable::Serialize () const {
            Header header (Type (), Version (), (ui32)Size ());
            Buffer::UniquePtr buffer (
                new Buffer (
                    NetworkEndian,
                    (ui32)(header.Size () + header.size)));
            *buffer << header;
            Write (*buffer);
            return buffer;
        }

        Serializable::Ptr Serializable::Deserialize (
                const Header &header,
                Serializer &serializer) {
            Map::iterator it = GetMap ().find (header.type);
            if (it != GetMap ().end ()) {
                return it->second (header, serializer);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "No registered factory for serializable '%s'.",
                    header.type.c_str ());
            }
        }

        Serializable::Ptr Serializable::Deserialize (Serializer &serializer) {
            Header header;
            serializer >> header;
            if (header.magic == MAGIC32) {
                return Deserialize (header, serializer);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt serializable '%s' header.",
                    header.type.c_str ());
            }
        }

    } // namespace util
} // namespace thekogans
