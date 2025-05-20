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

#include "thekogans/util/SerializableString.h"

namespace thekogans {
    namespace util {

        std::size_t SerializableString::Size () const {
            static const std::size_t bytesPerType[] = {
                UI8_SIZE, UI16_SIZE, UI32_SIZE, UI64_SIZE, 0
            };
            std::size_t lengthSize = lengthType == ValueParser<SizeT>::TYPE_SIZE_T ?
                (std::size_t)SizeT (size ()) : bytesPerType[lengthType];
            return lengthSize + size ();
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const SerializableString &serializableString) {
            switch (serializableString.lengthType) {
                case ValueParser<SizeT>::TYPE_UI8:
                    serializer << (ui8)serializableString.size ();
                    break;
                case ValueParser<SizeT>::TYPE_UI16:
                    serializer << (ui16)serializableString.size ();
                    break;
                case ValueParser<SizeT>::TYPE_UI32:
                    serializer << (ui32)serializableString.size ();
                    break;
                case ValueParser<SizeT>::TYPE_UI64:
                    serializer << (ui64)serializableString.size ();
                    break;
                case ValueParser<SizeT>::TYPE_SIZE_T:
                    serializer << SizeT (serializableString.size ());
                    break;
            }
            if (!serializableString.empty ()) {
                serializer.Write (serializableString.data (), serializableString.size ());
            }
            return serializer;
        }

        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                SerializableString &serializableString) {
            switch (serializableString.lengthType) {
                case ValueParser<SizeT>::TYPE_UI8: {
                    ui8 value;
                    serializer >> value;
                    serializableString.resize ((std::size_t)value);
                    break;
                }
                case ValueParser<SizeT>::TYPE_UI16: {
                    ui16 value;
                    serializer >> value;
                    serializableString.resize ((std::size_t)value);
                    break;
                }
                case ValueParser<SizeT>::TYPE_UI32: {
                    ui32 value;
                    serializer >> value;
                    serializableString.resize ((std::size_t)value);
                    break;
                }
                case ValueParser<SizeT>::TYPE_UI64: {
                    ui64 value;
                    serializer >> value;
                    serializableString.resize ((std::size_t)value);
                    break;
                }
                case ValueParser<SizeT>::TYPE_SIZE_T: {
                    SizeT value;
                    serializer >> value;
                    serializableString.resize ((std::size_t)value);
                    break;
                }
            }
            if (!serializableString.empty ()) {
                serializer.Read (&serializableString[0], serializableString.size ());
            }
            return serializer;
        }

        void ValueParser<SerializableString>::Reset () {
            length = 0;
            offset = 0;
            state = STATE_LENGTH;
        }

        bool ValueParser<SerializableString>::ParseValue (Serializer &serializer) {
            if (state == STATE_LENGTH) {
                if (lengthParser.ParseValue (serializer)) {
                    value.resize (length);
                    state = STATE_STRING;
                }
            }
            if (state == STATE_STRING) {
                offset += serializer.Read (&value[offset], length - offset);
                if (offset == length) {
                    Reset ();
                    return true;
                }
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
