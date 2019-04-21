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

#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/Serializable.h"

namespace thekogans {
    namespace util {

        const char * const Serializable::BinHeader::TAG_BIN_HEADER = "BinHeader";
        const char * const Serializable::BinHeader::ATTR_MAGIC = "Magic";
        const char * const Serializable::BinHeader::ATTR_TYPE = "Type";
        const char * const Serializable::BinHeader::ATTR_VERSION = "Version";
        const char * const Serializable::BinHeader::ATTR_SIZE = "Size";

        void Serializable::BinHeader::Parse (const pugi::xml_node &node) {
            magic = stringToui32 (node.attribute (ATTR_MAGIC).value ());
            type = Decodestring (node.attribute (ATTR_TYPE).value ());
            version = stringToui16 (node.attribute (ATTR_VERSION).value ());
            size = stringToui64 (node.attribute (ATTR_SIZE).value ());
        }

        std::string Serializable::BinHeader::ToString (
                std::size_t indentationLevel,
                const char *tagName) const {
            Attributes attributes;
            attributes.push_back (Attribute (ATTR_MAGIC, ui32Tostring (magic)));
            attributes.push_back (Attribute (ATTR_TYPE, Encodestring (type)));
            attributes.push_back (Attribute (ATTR_VERSION, ui32Tostring (version)));
            attributes.push_back (Attribute (ATTR_SIZE, ui64Tostring (size)));
            return OpenTag (0, tagName, attributes, true, true);
        }

        const char * const Serializable::TextHeader::ATTR_TYPE = "Type";
        const char * const Serializable::TextHeader::ATTR_VERSION = "Version";

        Serializable::Map &Serializable::GetMap () {
            static Map map;
            return map;
        }

        Serializable::MapInitializer::MapInitializer (
                const std::string &type,
                Factories factories) {
            std::pair<Map::iterator, bool> result =
                GetMap ().insert (Map::value_type (type, factories));
            if (!result.second) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "'%s' is already registered.", type.c_str ());
            }
        }

        bool Serializable::ValidateType (const std::string &type) {
            return GetMap ().find (type) != GetMap ().end ();
        }

        std::size_t Serializable::Size (const Serializable &serializable) {
            BinHeader header (
                serializable.Type (),
                serializable.Version (),
                serializable.Size ());
            return header.Size () + header.size;
        }

        void ValueParser<Serializable::BinHeader>::Reset () {
            magicParser.Reset ();
            typeParser.Reset ();
            versionParser.Reset ();
            sizeParser.Reset ();
            state = STATE_MAGIC;
        }

        bool ValueParser<Serializable::BinHeader>::ParseValue (Serializer &serializer) {
            if (state == STATE_MAGIC) {
                if (magicParser.ParseValue (serializer)) {
                    if (value.magic == MAGIC32) {
                        state = STATE_TYPE;
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Corrupt serializable header: %u.",
                            value.magic);
                    }
                }
            }
            if (state == STATE_TYPE) {
                if (typeParser.ParseValue (serializer)) {
                    if (Serializable::ValidateType (value.type)) {
                        state = STATE_VERSION;
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Unknown serializable type: %s.",
                            value.type.c_str ());
                    }
                }
            }
            if (state == STATE_VERSION) {
                if (versionParser.ParseValue (serializer)) {
                    state = STATE_SIZE;
                }
            }
            if (state == STATE_SIZE) {
                if (sizeParser.ParseValue (serializer)) {
                    Reset ();
                    return true;
                }
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
