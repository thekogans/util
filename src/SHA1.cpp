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

/*
 * Copyright (c) 2000-2001, Aaron D. Gifford
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <cstring>
#include "thekogans/util/ByteSwap.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/SHA1.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HASH (SHA1)

        namespace {
            // Initial hash value H for SHA-256:
            const ui32 initialHashValue[5] = {
                0x67452301,
                0xEFCDAB89,
                0x98BADCFE,
                0x10325476,
                0xC3D2E1F0
            };
        }

        void SHA1::Init (std::size_t digestSize) {
            if (digestSize == DIGEST_SIZE_160) {
                memcpy (state, initialHashValue, sizeof (state));
                bitCount = 0;
                memset (buffer, 0, sizeof (buffer));
                bufferIndex = 0;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void SHA1::Update (
                const void *buffer_,
                std::size_t size) {
            if (buffer_ != 0 && size != 0) {
                bitCount += size << 3;
                const ui8 *ptr = (const ui8 *)buffer_;
                do {
                    std::size_t bytesWritten = BLOCK_SIZE - bufferIndex;
                    if (bytesWritten > size) {
                        bytesWritten = size;
                    }
                    memcpy (&buffer[bufferIndex], ptr, bytesWritten);
                    bufferIndex += (ui32)bytesWritten;
                    ptr += bytesWritten;
                    size -= bytesWritten;
                    if (bufferIndex == BLOCK_SIZE) {
                        Transform ();
                    }
                } while (size != 0);
            }
        }

        void SHA1::Final (Digest &digest) {
            // Check to see if the current message block is too small
            // to hold the initial padding bits and size. If so, we
            // will pad the block, process it, and then continue
            // padding into a second block.
            buffer[bufferIndex++] = 0x80;
            if (bufferIndex > SHORT_BLOCK_SIZE) {
                memset (&buffer[bufferIndex], 0, BLOCK_SIZE - bufferIndex);
                Transform ();
            }
            memset (&buffer[bufferIndex], 0, SHORT_BLOCK_SIZE - bufferIndex);
            {
                // Store the message size as the last 8 octets
                union {
                    ui8 *ui8Ptr;
                    ui64 *ui64Ptr;
                } unionBuffer;
                unionBuffer.ui8Ptr = &buffer[SHORT_BLOCK_SIZE];
                *unionBuffer.ui64Ptr = ByteSwap<HostEndian, BigEndian> (bitCount);
            }
            Transform ();
            digest.resize (DIGEST_SIZE_160);
        #if (HostEndian == LittleEndian)
            {
                // Convert TO host byte order
                for (std::size_t i = 0; i < 5; ++i) {
                    state[i] = ByteSwap<HostEndian, BigEndian> (state[i]);
                }
            }
        #endif // (HostEndian == LittleEndian)
            memcpy (digest.data (), state, DIGEST_SIZE_160);
            Reset ();
        }

        void SHA1::Reset () {
            memset (state, 0, sizeof (state));
            bitCount = 0;
            memset (buffer, 0, sizeof (buffer));
            bufferIndex = 0;
        }

        namespace {
            // Constants defined in SHA-1
            const ui32 K[] = {
                0x5A827999,
                0x6ED9EBA1,
                0x8F1BBCDC,
                0xCA62C1D6
            };

            // Define the SHA1 circular left shift macro
            #define CircularShift(bits, word)\
                (((word) << (bits)) | ((word) >> (32 - (bits))))
        }

        void SHA1::Transform () {
            // Word sequence
            ui32 W[80];
            // Initialize the first 16 words in the array W
            for (ui32 t = 0; t < 16; ++t) {
                W[t] = buffer[t * 4] << 24;
                W[t] |= buffer[t * 4 + 1] << 16;
                W[t] |= buffer[t * 4 + 2] << 8;
                W[t] |= buffer[t * 4 + 3];
            }
            for (ui32 t = 16; t < 80; ++t) {
                W[t] = CircularShift (1, W[t- 3 ] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16]);
            }
            ui32 A = state[0];
            ui32 B = state[1];
            ui32 C = state[2];
            ui32 D = state[3];
            ui32 E = state[4];
            for (ui32 t = 0; t < 20; ++t) {
                ui32 temp = CircularShift (5, A) +
                    ((B & C) | ((~B) & D)) + E + W[t] + K[0];
                E = D;
                D = C;
                C = CircularShift (30, B);
                B = A;
                A = temp;
                temp = 0;
            }
            for (ui32 t = 20; t < 40; ++t) {
                ui32 temp = CircularShift (5, A) +
                    (B ^ C ^ D) + E + W[t] + K[1];
                E = D;
                D = C;
                C = CircularShift (30, B);
                B = A;
                A = temp;
                temp = 0;
            }
            for (ui32 t = 40; t < 60; ++t) {
                ui32 temp = CircularShift (5, A) +
                    ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
                E = D;
                D = C;
                C = CircularShift (30, B);
                B = A;
                A = temp;
                temp = 0;
            }
            for (ui32 t = 60; t < 80; ++t) {
                ui32 temp = CircularShift (5, A) +
                    (B ^ C ^ D) + E + W[t] + K[3];
                E = D;
                D = C;
                C = CircularShift (30, B);
                B = A;
                A = temp;
                temp = 0;
            }
            state[0] += A;
            state[1] += B;
            state[2] += C;
            state[3] += D;
            state[4] += E;
            bufferIndex = 0;
            // Clean up.
            A = B = C = D = E = 0;
            memset (W, 0, sizeof (W));
        }

    } // namespace util
} // namespace thekogans
