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
#include "thekogans/util/Serializer.h"
#include "thekogans/util/Serializable.h"
#include "thekogans/util/SerializableHeader.h"

namespace thekogans {
    namespace util {

        std::size_t SerializableHeader::Size () const {
            std::size_t totalSize = 0;
            if (!NeedType ()) {
                totalSize += Serializer::Size (type);
            }
            if (!NeedVersion ()) {
                totalSize += Serializer::Size (version);
            }
            if (!NeedSize ()) {
                totalSize += Serializer::Size (size);
            }
            return totalSize;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const SerializableHeader &header) {
            if (serializer.context.NeedType ()) {
                serializer << header.type;
            }
            if (serializer.context.NeedVersion ()) {
                serializer << header.version;
            }
            if (serializer.context.NeedSize ()) {
                serializer << header.size;
            }
            return serializer;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                SerializableHeader &header) {
            if (serializer.context.NeedType ()) {
                serializer >> header.type;
            }
            else {
                header.type = serializer.context.type;
            }
            if (serializer.context.NeedVersion ()) {
                serializer >> header.version;
            }
            else {
                header.version = serializer.context.version;
            }
            if (serializer.context.NeedSize ()) {
                serializer >> header.size;
            }
            else {
                header.size = serializer.context.size;
            }
            return serializer;
        }

        namespace {
            const char * const ATTR_TYPE = "Type";
            const char * const ATTR_VERSION = "Version";
            const char * const ATTR_SIZE = "Size";
        }

        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
                pugi::xml_node &node,
                const SerializableHeader &header) {
            node.append_attribute (ATTR_TYPE).set_value (header.type.c_str ());
            node.append_attribute (ATTR_VERSION).set_value (ui32Tostring (header.version).c_str ());
            node.append_attribute (ATTR_SIZE).set_value (ui64Tostring (header.size).c_str ());
            return node;
        }

        _LIB_THEKOGANS_UTIL_DECL const pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
                const pugi::xml_node &node,
                SerializableHeader &header) {
            header.type = node.attribute (ATTR_TYPE).value ();
            header.version = stringToui16 (node.attribute (ATTR_VERSION).value ());
            header.size = stringToui64 (node.attribute (ATTR_SIZE).value ());
            return node;
        }

        _LIB_THEKOGANS_UTIL_DECL JSON::Object & _LIB_THEKOGANS_UTIL_API operator << (
                JSON::Object &object,
                const SerializableHeader &header) {
            object.Add<const std::string &> (ATTR_TYPE, header.type);
            object.Add<ui16> (ATTR_VERSION, header.version);
            object.Add<ui64> (ATTR_SIZE, header.size);
            return object;
        }

        _LIB_THEKOGANS_UTIL_DECL const JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (
                const JSON::Object &object,
                SerializableHeader &header) {
            header.type = object.Get<JSON::String> (ATTR_TYPE)->value;
            header.version = object.Get<JSON::Number> (ATTR_VERSION)->To<ui16> ();
            header.size = object.Get<JSON::Number> (ATTR_SIZE)->To<ui64> ();
            return object;
        }

        void ValueParser<SerializableHeader>::Reset (Serializer &serializer) {
            typeParser.Reset ();
            versionParser.Reset ();
            sizeParser.Reset ();
            value = serializer.context;
            if (value.NeedType ()) {
                state = STATE_TYPE;
            }
            else if (value.NeedVersion ()) {
                state = STATE_VERSION;
            }
            else if (value.NeedSize ()) {
                state = STATE_SIZE;
            }
            else {
                state = STATE_NOTHING;
            }
        }

        bool ValueParser<SerializableHeader>::ParseValue (Serializer &serializer) {
            if (state == STATE_NOTHING) {
                Reset (serializer);
                return true;
            }
            if (state == STATE_TYPE) {
                if (typeParser.ParseValue (serializer)) {
                    if (Serializable::IsType (value.type.c_str ())) {
                        if (value.NeedVersion ()) {
                            state = STATE_VERSION;
                        }
                        else if (value.NeedSize ()) {
                            state = STATE_SIZE;
                        }
                        else {
                            Reset (serializer);
                            return true;
                        }
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
                    if (value.NeedSize ()) {
                        state = STATE_SIZE;
                    }
                    else {
                        Reset (serializer);
                        return true;
                    }
                }
            }
            if (state == STATE_SIZE) {
                if (sizeParser.ParseValue (serializer)) {
                    Reset (serializer);
                    return true;
                }
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
