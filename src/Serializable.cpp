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
    #include "thekogans/util/TransactedFile.h"
    #include "thekogans/util/TransactedFileBTree.h"
    #include "thekogans/util/SerializableValues.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (thekogans::util::Serializable)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void Serializable::StaticInit () {
            HRTimerMgr::TimerInfoBase::StaticInit ();
            HRTimerMgr::StaticInit ();
            TransactedFile::Object::StaticInit ();
            TransactedFileBTree::Key::StaticInit ();
            StringValue::StaticInit ();
            StringArrayValue::StaticInit ();
            AddressArrayValue::StaticInit ();
            GUIDArrayValue::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        std::size_t Serializable::GetSize (Serializer &serializer) noexcept {
            SerializableHeader header{Type (), Version (), Size (serializer)};
            return serializer.Size (header) + (serializer.context.NeedSize () ? header.size : serializer.context.size);
        }

        const HashedString &Blob::Type () const noexcept {
            return header.type;
        }

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_BEGIN (Blob)
            DynamicCreatable::TYPE,
        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_END (Blob)

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_Bases (Blob)

        std::size_t Blob::Version () const noexcept {
            return header.version;
        }

        std::size_t Blob::ClassSize () const noexcept {
            return header.size;
        }

        void Blob::Read (Serializer &serializer) {
            header = serializer.context;
            buffer.Resize (header.size);
            buffer.Rewind ();
            buffer.AdvanceWriteOffset (
                serializer.Read (
                    buffer.GetWritePtr (),
                    buffer.GetDataAvailableForWriting ()));
        }

        void Blob::Write (Serializer &serializer) {
            serializer.Write (
                buffer.GetReadPtr (),
                buffer.GetDataAvailableForReading ());
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                Serializable &serializable) {
            serializer << SerializableHeader{
                serializable.Type (), serializable.Version (), serializable.Size (serializer)};
            serializable.Write (serializer);
            return serializer;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Serializable &serializable) {
            SerializableHeader header;
            serializer >> header;
            if (THEKOGANS_UTIL_UNLIKELY (header.type != serializable.Type ())) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Header type (%s) does not match Serializable type (%s).",
                    header.type,
                    serializable.Type ());
            }
            if (THEKOGANS_UTIL_UNLIKELY (header.version > serializable.Version ())) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Header type (%s) version (%u) is greater then Serializable version (%u).",
                    header.type,
                    header.version,
                    serializable.Version ());
            }
            Serializer::ContextGuard guard (serializer, header);
            serializable.Read (serializer);
            return serializer;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Serializable::SharedPtr &serializable) {
            SerializableHeader header;
            serializer >> header;
            if (serializer.factory != nullptr) {
                serializable = serializer.factory (serializer.parameters);
            }
            else {
                serializable = Serializable::CreateType (header.type, serializer.parameters);
            }
            if (serializable == nullptr) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Unable to create a Serializable of type: %s.",
                    header.type);
            }
            if (header.version > serializable->Version ()) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Header type (%s) version (%u) is greater then the Serializable version (%u).",
                    header.type,
                    header.version,
                    serializable->Version ());
            }
            Serializer::ContextGuard guard (serializer, header);
            serializable->Read (serializer);
            return serializer;
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
                            header.type,
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
                    Serializer::ContextGuard guard (payload, header);
                    value.Read (payload);
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
                            "Invalid serializable length: " THEKOGANS_UTIL_SIZE_T_FORMAT,
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
                    if (serializer.factory != nullptr) {
                        value = serializer.factory (serializer.parameters);
                    }
                    else {
                        value = Serializable::CreateType (header.type, serializer.parameters);
                    }
                    if (value != nullptr) {
                        Serializer::ContextGuard guard (payload, header);
                        value->Read (payload);
                        Reset (serializer);
                        return true;
                    }
                    else {
                        Reset (serializer);
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Unable to create a Serializable of type: %s.",
                            header.type);
                    }
                }
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
