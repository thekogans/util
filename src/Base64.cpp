// Copyright 2011 Boris Kogan (boris@thekogans.net)
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

#include <cctype>
#include "thekogans/util/Exception.h"
#include "thekogans/util/Base64.h"

namespace thekogans {
    namespace util {

        void Base64::Encode (
                const void *buffer,
                std::size_t length,
                std::vector<ui8> &output,
                std::size_t lineLength,
                std::size_t linePad) {
            if (buffer != 0 && length > 0) {
                static const ui8 base64[] = {
                    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                    'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
                };
                const ui8 *bufferPtr = (const ui8 *)buffer;
                std::size_t lineCount = length / lineLength + 1;
                std::size_t extraSpace = lineCount * linePad + lineCount;
                output.resize ((length + 2 - ((length + 2) % 3)) / 3 * 4 + extraSpace);
                struct LineFormatter {
                    ui8 *output;
                    std::size_t lineLength;
                    std::size_t linePad;
                    std::size_t currChar;
                    LineFormatter (
                        ui8 *output_,
                        std::size_t lineLength_,
                        std::size_t linePad_) :
                        output (output_),
                        lineLength (lineLength_),
                        linePad (linePad_),
                        currChar (0) {}
                    void Format (ui8 ch) {
                        if (currChar == 0) {
                            for (std::size_t i = 0; i < linePad; ++i) {
                                *output++ = ' ';
                            }
                            currChar += linePad;
                        }
                        *output++ = ch;
                        ++currChar;
                        if (currChar == lineLength) {
                            *output++ = '\n';
                            currChar = 0;
                        }
                    }
                    void Finish () {
                        if (currChar != 0) {
                            *output++ = '\n';
                            currChar = 0;
                        }
                    }
                } lineFormatter (&output[0], lineLength, linePad);
                for (std::size_t i = 0, count = length / 3; i < count; ++i) {
                    ui8 buffer0 = *bufferPtr++;
                    ui8 buffer1 = *bufferPtr++;
                    ui8 buffer2 = *bufferPtr++;
                    lineFormatter.Format (base64[buffer0 >> 2]);
                    lineFormatter.Format (base64[(buffer0 << 4 | buffer1 >> 4) & 0x3f]);
                    lineFormatter.Format (base64[(buffer1 << 2 | buffer2 >> 6) & 0x3f]);
                    lineFormatter.Format (base64[buffer2 & 0x3f]);
                }
                switch (length % 3) {
                    case 1: {
                        ui8 buffer0 = *bufferPtr++;
                        lineFormatter.Format (base64[buffer0 >> 2]);
                        lineFormatter.Format (base64[buffer0 << 4 & 0x3f]);
                        lineFormatter.Format ('=');
                        lineFormatter.Format ('=');
                        break;
                    }
                    case 2: {
                        ui8 buffer0 = *bufferPtr++;
                        ui8 buffer1 = *bufferPtr++;
                        lineFormatter.Format (base64[buffer0 >> 2]);
                        lineFormatter.Format (base64[(buffer0 << 4 | buffer1 >> 4) & 0x3f]);
                        lineFormatter.Format (base64[buffer1 << 2 & 0x3f]);
                        lineFormatter.Format ('=');
                        break;
                    }
                }
                lineFormatter.Finish ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        namespace {
            const ui8 unbase64[] = {
                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,   62, 0xff, 0xff, 0xff,   63,
                  52,   53,   54,   55,   56,   57,   58,   59,   60,   61, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff,    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
                  15,   16,   17,   18,   19,   20,   21,   22,   23,   24,   25, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff,   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,   36,   37,   38,   39,   40,
                  41,   42,   43,   44,   45,   46,   47,   48,   49,   50,   51, 0xff, 0xff, 0xff, 0xff, 0xff
            };

            inline bool IsValidBase64 (ui8 value) {
                return value < 128 && unbase64[value] != 0xff;
            }

            std::size_t ValidateInput (
                    std::size_t index,
                    ui8 buffer0,
                    ui8 buffer1,
                    ui8 buffer2,
                    ui8 buffer3) {
                std::size_t equalCount = 0;
                bool valid = IsValidBase64 (buffer0) && IsValidBase64 (buffer1);
                if (valid) {
                    if (buffer3 == '=') {
                        ++equalCount;
                        if (buffer2 == '=') {
                            ++equalCount;
                        }
                    }
                    switch (equalCount) {
                        case 0:
                            valid = IsValidBase64 (buffer2) && IsValidBase64 (buffer3);
                            break;
                        case 1:
                            valid = IsValidBase64 (buffer2);
                            break;
                    }
                }
                if (!valid) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Invalid input @ %u (%u, %u, %u, %u)",
                        index, buffer0, buffer1, buffer2, buffer3);
                }
                return equalCount;
            }
        }

        void Base64::Decode (
                const void *buffer,
                std::size_t length,
                std::vector<ui8> &output) {
            if (buffer != 0 && length > 0) {
                output.resize (length);
                ui8 *outputPtr = &output[0];
                std::size_t index = 0;
                std::size_t charCount = 0;
                for (const ui8 *bufferPtr = (const ui8 *)buffer,
                        *endBufferPtr = bufferPtr + length; bufferPtr < endBufferPtr; ++index) {
                    if (!isspace (*bufferPtr)) {
                        if (endBufferPtr - bufferPtr >= 4) {
                            ui8 buffer0 = *bufferPtr++;
                            ui8 buffer1 = *bufferPtr++;
                            ui8 buffer2 = *bufferPtr++;
                            ui8 buffer3 = *bufferPtr++;
                            switch (ValidateInput (index, buffer0, buffer1, buffer2, buffer3)) {
                                case 0:
                                    *outputPtr++ = unbase64[buffer0] << 2 |
                                        (unbase64[buffer1] & 0x30) >> 4;
                                    *outputPtr++ = unbase64[buffer1] << 4 |
                                        (unbase64[buffer2] & 0x3c) >> 2;
                                    *outputPtr++ = (unbase64[buffer2] & 0x03) << 6 | unbase64[buffer3];
                                    charCount += 3;
                                    break;
                                case 1:
                                    *outputPtr++ = unbase64[buffer0] << 2 |
                                        (unbase64[buffer1] & 0x30) >> 4;
                                    *outputPtr++ = unbase64[buffer1] << 4 |
                                        (unbase64[buffer2] & 0x3c) >> 2;
                                    charCount += 2;
                                    break;
                                case 2:
                                    *outputPtr++ = unbase64[buffer0] << 2 |
                                        (unbase64[buffer1] & 0x30) >> 4;
                                    ++charCount;
                                    break;
                            }
                        }
                        else {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "Invalid input @ %u", index);
                        }
                    }
                    else {
                        ++bufferPtr;
                    }
                }
                output.resize (charCount);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    } // namespace util
} // namespace thekogans
