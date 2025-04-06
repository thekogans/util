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

        const char * const Serializable::Header::ATTR_TYPE = "Type";
        const char * const Serializable::Header::ATTR_VERSION = "Version";

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

        std::size_t Serializable::GetSize () const noexcept {
            Header header (Type (), Version (), Size ());
            return header.Size () + header.size;
        }

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (thekogans::util::Blob)

        /*
        const char *Blob::Type () const noexcept {
            return header.type.c_str ();
        }
        */

        ui16 Blob::Version () const noexcept {
            return header.version;
        }

        std::size_t Blob::Size () const noexcept {
            return header.size;
        }

        void Blob::Read (
                const Header &header_,
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

        void Blob::Read (
                const Header &header_,
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
                const Header &header_,
                const JSON::Object &object_) {
            header = header_;
            object = object_;
        }

        void Blob::Write (JSON::Object &object_) const {
            object_ = object;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Serializable &serializable) {
            Serializable::Header header;
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
            Serializable::Header header;
            node >> header;
            if (header.type == serializable.Type ()) {
                serializable.Read (header, node);
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
            Serializable::Header header;
            object >> header;
            if (header.type == serializable.Type ()) {
                serializable.Read (header, object);
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
            Serializable::Header header;
            serializer >> header;
            serializable = Serializable::CreateType (header.type.c_str ());
            if (serializable == nullptr) {
                serializable.Reset (new Blob);
            }
            serializable->Read (header, serializer);
            return serializer;
        }

        _LIB_THEKOGANS_UTIL_DECL const pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
                const pugi::xml_node &node,
                Serializable::SharedPtr &serializable) {
            Serializable::Header header;
            node >> header;
            serializable = Serializable::CreateType (header.type.c_str ());
            if (serializable == nullptr) {
                serializable.Reset (new Blob);
            }
            serializable->Read (header, node);
            return node;
        }

        _LIB_THEKOGANS_UTIL_DECL const JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (
                const JSON::Object &object,
                Serializable::SharedPtr &serializable) {
            Serializable::Header header;
            object >> header;
            serializable = Serializable::CreateType (header.type.c_str ());
            if (serializable == nullptr) {
                serializable.Reset (new Blob);
            }
            serializable->Read (header, object);
            return object;
        }

        void ValueParser<Serializable::Header>::Reset () {
            magic = 0;
            magicParser.Reset ();
            typeParser.Reset ();
            versionParser.Reset ();
            sizeParser.Reset ();
            state = STATE_MAGIC;
        }

        bool ValueParser<Serializable::Header>::ParseValue (Serializer &serializer) {
            if (state == STATE_MAGIC) {
                if (magicParser.ParseValue (serializer)) {
                    if (magic == MAGIC32) {
                        state = STATE_TYPE;
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Corrupt serializable header: %u.",
                            magic);
                    }
                }
            }
            if (state == STATE_TYPE) {
                if (typeParser.ParseValue (serializer)) {
                    if (Serializable::IsType (value.type.c_str ())) {
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

        void ValueParser<Serializable>::Reset () {
            payload.Resize (0);
            headerParser.Reset ();
            state = STATE_BIN_HEADER;
        }

        bool ValueParser<Serializable>::ParseValue (Serializer &serializer) {
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
                    value.Read (header, payload);
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
                    value = Serializable::CreateType (header.type.c_str ());
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
