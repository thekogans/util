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
            size = 0;
            offset = 1;
            state = STATE_SIZE;
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
                    valueBuffer[0] = firstByte;
                    state = STATE_VALUE;
                }
            }
            if (state == STATE_VALUE) {
                offset += serializer.Read (valueBuffer + offset, size - offset);
                if (offset == size) {
                    TenantReadBuffer buffer (serializer.endianness, valueBuffer, size);
                    buffer >> value;
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
            state = STATE_LENGTH;
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
