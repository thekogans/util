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

#include "thekogans/util/ValueParser.h"

namespace thekogans {
    namespace util {

        void ValueParser<ui8 *>::Reset () {
            offset = 0;
        }

        void ValueParser<ui8 *>::Reset (
                ui8 *value_,
                std::size_t length_) {
            if (value_ != 0 && length_ > 0) {
                value = value_;
                length = length_;
                offset = 0;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool ValueParser<ui8 *>::ParseValue (Serializer &serializer) {
            offset += serializer.Read (value + offset, length - offset);
            if (offset == length) {
                Reset ();
                return true;
            }
            return false;
        }

        void ValueParser<SizeT>::Reset () {
            static const std::size_t bytesPerType[] = {
                UI8_SIZE, UI16_SIZE, UI32_SIZE, UI64_SIZE, 0
            };
            size = bytesPerType[type];
            offset = 0;
            state = type == TYPE_SIZE_T ? STATE_SIZE : STATE_VALUE;
        }

        bool ValueParser<SizeT>::ParseValue (Serializer &serializer) {
            if (state == STATE_SIZE) {
                ui8 firstByte;
                if (serializer.Read (&firstByte, 1) == 1) {
                    size = SizeT::Size (firstByte);
                    if (size == 1) {
                        value = firstByte >> 1;
                        Reset ();
                        return true;
                    }
                    valueBuffer[++offset] = firstByte;
                    state = STATE_VALUE;
                }
            }
            if (state == STATE_VALUE) {
                offset += serializer.Read (valueBuffer + offset, size - offset);
                if (offset == size) {
                    TenantReadBuffer buffer (serializer.endianness, valueBuffer, size);
                    switch (type) {
                        case TYPE_UI8: {
                            ui8 _value;
                            buffer >> _value;
                            value = _value;
                            break;
                        }
                        case TYPE_UI16: {
                            ui16 _value;
                            buffer >> _value;
                            value = _value;
                            break;
                        }
                        case TYPE_UI32: {
                            ui32 _value;
                            buffer >> _value;
                            value = _value;
                            break;
                        }
                        case TYPE_UI64: {
                            ui64 _value;
                            buffer >> _value;
                            value = _value;
                            break;
                        }
                        case TYPE_SIZE_T: {
                            buffer >> value;
                            break;
                        }
                    }
                    Reset ();
                    return true;
                }
            }
            return false;
        }

        void ValueParser<std::string>::Reset () {
            length = 0;
            lengthParser.Reset ();
            offset = 0;
            state = delimiter == 0 ? STATE_LENGTH : STATE_STRING;
        }

        bool ValueParser<std::string>::ParseValue (Serializer &serializer) {
            if (state == STATE_LENGTH) {
                if (lengthParser.ParseValue (serializer)) {
                    value.resize (length);
                    if (length == 0) {
                        Reset ();
                        return true;
                    }
                    state = STATE_STRING;
                }
            }
            if (state == STATE_STRING) {
                if (delimiter == 0) {
                    offset += serializer.Read (&value[offset], length - offset);
                    if (offset == length) {
                        Reset ();
                        return true;
                    }
                }
                else {
                    ui8 byte;
                    serializer >> byte;
                    if (*((const ui8 *)delimiter + offset) != byte) {
                        if (offset != 0) {
                            value += std::string (
                                (const ui8 *)delimiter,
                                (const ui8 *)delimiter + offset);
                            if (*(const ui8 *)delimiter == byte) {
                                offset = 1;
                            }
                            else {
                                offset = 0;
                                value += byte;
                            }
                        }
                        else {
                            value += byte;
                        }
                    }
                    else if (++offset == delimiterLength) {
                        Reset ();
                        return true;
                    }
                }
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
