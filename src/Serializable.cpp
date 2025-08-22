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
    #include "thekogans/util/BTree.h"
    #include "thekogans/util/SerializableArray.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (thekogans::util::Serializable)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void Serializable::StaticInit () {
            Directory::Entry::StaticInit ();
            Fraction::StaticInit ();
            HRTimerMgr::TimerInfoBase::StaticInit ();
            HRTimerMgr::StaticInit ();
            RunLoop::Stats::Job::StaticInit ();
            RunLoop::Stats::StaticInit ();
            TimeSpec::StaticInit ();
            BTree::Key::StaticInit ();
            BTree::Value::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        SerializableHeader Serializable::GetHeader (
                const SerializableHeader &context) const noexcept {
            SerializableHeader header;
            if (context.NeedType ()) {
                header.type = Type ();
            }
            if (context.NeedVersion ()) {
                header.version = Version ();
            }
            if (context.NeedSize ()) {
                header.size = Size ();
            }
            return header;
        }

        std::size_t Serializable::GetSize (const SerializableHeader &context) const noexcept {
            SerializableHeader header = GetHeader (context);
            return header.Size () + (context.NeedSize () ? header.size : context.size);
        }

        const char *Blob::Type () const noexcept {
            return header.type.c_str ();
        }

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_BEGIN (Blob)
            DynamicCreatable::TYPE,
        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_END (Blob)

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_Bases (Blob)

        ui16 Blob::Version () const noexcept {
            return header.version;
        }

        std::size_t Blob::ClassSize () const noexcept {
            return header.size;
        }

        void Blob::Read (
                const SerializableHeader &header_,
                Serializer &serializer) {
            header = header_;
            buffer.Resize (header.size);
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

        std::size_t Blob::SizeXML () const noexcept {
            return header.size;
        }

        void Blob::ReadXML (
                const SerializableHeader &header_,
                const pugi::xml_node &node_) {
            header = header_;
            for (pugi::xml_attribute attribute = node_.first_attribute ();
                    attribute; attribute = attribute.next_attribute ()) {
                node.append_copy (attribute);
            }
            for (pugi::xml_node child = node_.first_child ();
                    child; child = child.next_sibling ()) {
                node.append_copy (child);
            }
        }

        void Blob::WriteXML (pugi::xml_node &node_) const {
            for (pugi::xml_attribute attribute = node.first_attribute ();
                    attribute; attribute = attribute.next_attribute ()) {
                node_.append_copy (attribute);
            }
            for (pugi::xml_node child = node.first_child ();
                    child; child = child.next_sibling ()) {
                node_.append_copy (child);
            }
        }

        std::size_t Blob::SizeJSON () const noexcept {
            return header.size;
        }

        void Blob::ReadJSON (
                const SerializableHeader &header_,
                const JSON::Object &object_) {
            header = header_;
            object = object_;
        }

        void Blob::WriteJSON (JSON::Object &object_) const {
            object_ = object;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const Serializable &serializable) {
            serializer << serializable.GetHeader (serializer.context);
            serializable.Write (serializer);
            return serializer;
        }

        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
                pugi::xml_node &node,
                const Serializable &serializable) {
            node <<
                SerializableHeader (
                    serializable.Type (),
                    serializable.Version (),
                    serializable.SizeXML ());
            serializable.WriteXML (node);
            return node;
        }

        _LIB_THEKOGANS_UTIL_DECL JSON::Object & _LIB_THEKOGANS_UTIL_API operator << (
                JSON::Object &object,
                const Serializable &serializable) {
            object <<
                SerializableHeader (
                    serializable.Type (),
                    serializable.Version (),
                    serializable.SizeJSON ());
            serializable.WriteJSON (object);
            return object;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Serializable &serializable) {
            SerializableHeader header;
            serializer >> header;
            if (header.type == serializable.Type ()) {
                serializable.Read (header, serializer);
                return serializer;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Header type (%s) is not the same as Serializable type (%s).",
                    header.type.c_str (),
                    serializable.Type ());
            }
        }

        _LIB_THEKOGANS_UTIL_DECL const pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
                const pugi::xml_node &node,
                Serializable &serializable) {
            SerializableHeader header;
            node >> header;
            if (header.type == serializable.Type ()) {
                serializable.ReadXML (header, node);
                return node;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Header type (%s) is not the same as Serializable type (%s).",
                    header.type.c_str (),
                    serializable.Type ());
            }
        }

        _LIB_THEKOGANS_UTIL_DECL const JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (
                const JSON::Object &object,
                Serializable &serializable) {
            SerializableHeader header;
            object >> header;
            if (header.type == serializable.Type ()) {
                serializable.ReadJSON (header, object);
                return object;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Header type (%s) is not the same as Serializable type (%s).",
                    header.type.c_str (),
                    serializable.Type ());
            }
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Serializable::SharedPtr &serializable) {
            SerializableHeader header;
            serializer >> header;
            serializable = serializer.factory ?
                serializer.factory (nullptr) :
                Serializable::CreateType (header.type.c_str ());
            if (serializable != nullptr) {
                serializable->Read (header, serializer);
                return serializer;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Unable to create s Serializable of type: %s.",
                    header.type.c_str ());
            }
        }

        _LIB_THEKOGANS_UTIL_DECL const pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
                const pugi::xml_node &node,
                Serializable::SharedPtr &serializable) {
            SerializableHeader header;
            node >> header;
            serializable = Serializable::CreateType (header.type.c_str ());
            if (serializable != nullptr) {
                serializable->ReadXML (header, node);
                return node;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Unable to create s Serializable of type: %s.",
                    header.type.c_str ());
            }
        }

        _LIB_THEKOGANS_UTIL_DECL const JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (
                const JSON::Object &object,
                Serializable::SharedPtr &serializable) {
            SerializableHeader header;
            object >> header;
            serializable = Serializable::CreateType (header.type.c_str ());
            if (serializable == nullptr) {
                serializable->ReadJSON (header, object);
                return object;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Unable to create s Serializable of type: %s.",
                    header.type.c_str ());
            }
        }

        void ValueParser<Serializable>::Reset (Serializer &serializer) {
            payload.Resize (0);
            headerParser.Reset (serializer);
            state = STATE_HEADER;
        }

        bool ValueParser<Serializable>::ParseValue (Serializer &serializer) {
            if (state == STATE_HEADER) {
                if (headerParser.ParseValue (serializer)) {
                    if (header.type == value.Type ()) {
                        if (header.size > 0 && header.size <= maxSerializableSize) {
                            THEKOGANS_UTIL_TRY {
                                payload.Resize (header.size);
                                state = STATE_SERIALIZABLE;
                            }
                            THEKOGANS_UTIL_CATCH (Exception) {
                                Reset (serializer);
                                THEKOGANS_UTIL_RETHROW_EXCEPTION (exception);
                            }
                        }
                        else {
                            Reset (serializer);
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "Invalid serializable length: %u.",
                                header.size);
                        }
                    }
                    else {
                        Reset (serializer);
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Header type (%s) is not the same as Serializable type (%s).",
                            header.type.c_str (),
                            value.Type ());
                    }
                }
            }
            if (state == STATE_SERIALIZABLE) {
                payload.AdvanceWriteOffset (
                    serializer.Read (
                        payload.GetWritePtr (),
                        payload.GetDataAvailableForWriting ()));
                if (payload.IsFull ()) {
                    value.Read (header, payload);
                    Reset (serializer);
                    return true;
                }
            }
            return false;
        }

        void ValueParser<Serializable::SharedPtr>::Reset (Serializer &serializer) {
            payload.Resize (0);
            headerParser.Reset (serializer);
            state = STATE_HEADER;
        }

        bool ValueParser<Serializable::SharedPtr>::ParseValue (Serializer &serializer) {
            if (state == STATE_HEADER) {
                if (headerParser.ParseValue (serializer)) {
                    if (header.size > 0 && header.size <= maxSerializableSize) {
                        THEKOGANS_UTIL_TRY {
                            payload.Resize (header.size);
                            state = STATE_SERIALIZABLE;
                        }
                        THEKOGANS_UTIL_CATCH (Exception) {
                            Reset (serializer);
                            THEKOGANS_UTIL_RETHROW_EXCEPTION (exception);
                        }
                    }
                    else {
                        Reset (serializer);
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
                    value = serializer.factory ?
                        serializer.factory (nullptr) :
                        Serializable::CreateType (header.type.c_str ());
                    if (value != nullptr) {
                        value->Read (header, payload);
                        Reset (serializer);
                        return true;
                    }
                    else {
                        Reset (serializer);
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Unable to create s Serializable of type: %s.",
                            header.type.c_str ());
                    }
                }
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
