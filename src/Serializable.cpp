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
#if defined (THEKOGANS_UTIL_TYPE_Static)
    #include "thekogans/util/Directory.h"
    #include "thekogans/util/Fraction.h"
    #include "thekogans/util/HRTimerMgr.h"
    #include "thekogans/util/RunLoop.h"
    #include "thekogans/util/TimeSpec.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE (thekogans::util::Serializable)

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

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void Serializable::StaticInit () {
            Directory::Entry::StaticInit ();
            Fraction::StaticInit ();
            HRTimerMgr::TimerInfoBase::StaticInit ();
            HRTimerMgr::StaticInit ();
            RunLoop::Stats::Job::StaticInit ();
            RunLoop::Stats::StaticInit ();
            TimeSpec::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        std::size_t Serializable::GetSize () const {
            BinHeader header (Type (), Version (), Size ());
            return header.Size () + header.size;
        }

        const std::string &Blob::Type () const {
            return type;
        }

        const std::string &Blob::Base () const {
            return Serializable::TYPE;
        }

        ui16 Blob::Version () const {
            return version;
        }

        std::size_t Blob::Size () const {
            return size;
        }

        void Blob::Read (
                const BinHeader &header,
                Serializer &serializer) {
            type = header.type;
            version = header.version;
            size = header.size;
            buffer.Resize (size);
            buffer.Rewind ();
            buffer.AdvanceWriteOffset (
                serializer.Read (
                    buffer.GetWritePtr (),
                    buffer.GetDataAvailableForWriting ()));
        }

        void Blob::Write (Serializer &serializer) const {
            serializer.Write (
                buffer.GetReadPtr (),
                buffer.GetDataAvailableForReading ());
        }

        void Blob::Read (
                const TextHeader &header,
                const pugi::xml_node &node_) {
            type = header.type;
            version = header.version;
            size = 0;
            for (pugi::xml_attribute attribute = node_.first_attribute ();
                    attribute; attribute = attribute.next_attribute ()) {
                node.append_copy (attribute);
            }
            for (pugi::xml_node child = node_.first_child ();
                    child; child = child.next_sibling ()) {
                node.append_copy (child);
            }
        }

        void Blob::Write (pugi::xml_node &node_) const {
            for (pugi::xml_attribute attribute = node.first_attribute ();
                    attribute; attribute = attribute.next_attribute ()) {
                node_.append_copy (attribute);
            }
            for (pugi::xml_node child = node.first_child ();
                    child; child = child.next_sibling ()) {
                node_.append_copy (child);
            }
        }

        void Blob::Read (
                const TextHeader &header,
                const JSON::Object &object_) {
            type = header.type;
            version = header.version;
            size = 0;
            object = object_;
        }

        void Blob::Write (JSON::Object &object_) const {
            object_ = object;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Serializable::SharedPtr &serializable) {
            Serializable::BinHeader header;
            serializer >> header;
            if (header.magic == MAGIC32) {
                serializable = Serializable::CreateType (header.type);
                if (serializable == nullptr) {
                    serializable.Reset (new Blob);
                }
                serializable->Read (header, serializer);
                return serializer;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt serializable '%s' header.",
                    header.type.c_str ());
            }
        }

        _LIB_THEKOGANS_UTIL_DECL const pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
                const pugi::xml_node &node,
                Serializable::SharedPtr &serializable) {
            Serializable::TextHeader header;
            node >> header;
            serializable = Serializable::CreateType (header.type);
            if (serializable == nullptr) {
                serializable.Reset (new Blob);
            }
            serializable->Read (header, node);
            return node;
        }

        _LIB_THEKOGANS_UTIL_DECL const JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (
                const JSON::Object &object,
                Serializable::SharedPtr &serializable) {
            Serializable::TextHeader header;
            object >> header;
            serializable = Serializable::CreateType (header.type);
            if (serializable == nullptr) {
                serializable.Reset (new Blob);
            }
            serializable->Read (header, object);
            return object;
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
                    if (Serializable::IsType (value.type)) {
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

        void ValueParser<Serializable::SharedPtr>::Reset () {
            payload.Resize (0);
            headerParser.Reset ();
            state = STATE_BIN_HEADER;
        }

        bool ValueParser<Serializable::SharedPtr>::ParseValue (Serializer &serializer) {
            if (state == STATE_BIN_HEADER) {
                if (headerParser.ParseValue (serializer)) {
                    if (header.size > 0 && header.size <= maxSerializableSize) {
                        THEKOGANS_UTIL_TRY {
                            payload.Resize (header.size);
                            state = STATE_SERIALIZABLE;
                        }
                        THEKOGANS_UTIL_CATCH (Exception) {
                            Reset ();
                            THEKOGANS_UTIL_RETHROW_EXCEPTION (exception);
                        }
                    }
                    else {
                        Reset ();
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Invalid serializable length: %u.",
                            header.size);
                    }
                }
            }
            if (state == STATE_SERIALIZABLE) {
                payload.AdvanceWriteOffset (
                    serializer.Read (
                        payload.GetWritePtr (),
                        payload.GetDataAvailableForWriting ()));
                if (payload.IsFull ()) {
                    value = Serializable::CreateType (header.type);
                    if (value == nullptr) {
                        value.Reset (new Blob);
                    }
                    value->Read (header, payload);
                    Reset ();
                    return true;
                }
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
