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
            static Serializable::Map map;
            return map;
        }

        Serializable::Ptr Serializable::Get (util::Serializer &serializer) {
            Serializable::Header header;
            serializer >> header;
            if (header.magic == MAGIC32) {
                Map::iterator it = GetMap ().find (header.type);
                return it != GetMap ().end () ?
                    it->second (header, serializer) : Serializable::Ptr ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s is already registered.", header.type.c_str ());
            }
        }

        Serializable::MapInitializer::MapInitializer (
                const std::string &type,
                Factory factory) {
            std::pair<Map::iterator, bool> result =
                GetMap ().insert (Map::value_type (type, factory));
            assert (result.second);
            if (!result.second) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s is already registered.", type.c_str ());
            }
        }

    } // namespace util
} // namespace thekogans
