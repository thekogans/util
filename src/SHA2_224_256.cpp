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
#include "thekogans/util/SHA2_224_256.h"

namespace thekogans {
    namespace util {

        namespace {
            #define R(b, x) ((x) >> (b))
            #define Ch(x, y, z) (((x) & (y)) ^ ((~(x)) & (z)))
            #define Maj(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
            #define S32(b, x) (((x) >> (b)) | ((x) << (32 - (b))))
            #define Sigma0_256(x) (S32 (2, (x)) ^ S32 (13, (x)) ^ S32 (22, (x)))
            #define Sigma1_256(x) (S32 (6, (x)) ^ S32 (11, (x)) ^ S32 (25, (x)))
            #define sigma0_256(x) (S32 (7, (x)) ^ S32 (18, (x)) ^ R (3, (x)))
            #define sigma1_256(x) (S32 (17, (x)) ^ S32 (19, (x)) ^ R (10, (x)))
            #define ROUND256_0_TO_15(a, b, c, d, e, f, g, h) {\
                W256[j] = ByteSwap<HostEndian, BigEndian> (W256[j]);\
                T1 = (h) + Sigma1_256 (e) + Ch ((e), (f), (g)) + K256[j] + W256[j];\
                (d) += T1;\
                (h) = T1 + Sigma0_256 (a) + Maj ((a), (b), (c));\
                ++j;\
            }
            #define ROUND256(a, b, c, d, e, f, g, h) {\
                s0 = W256[(j + 1) & 0x0f];\
                s0 = sigma0_256 (s0);\
                s1 = W256[(j + 14) & 0x0f];\
                s1 = sigma1_256 (s1);\
                T1 = (h) + Sigma1_256 (e) + Ch ((e), (f), (g)) + K256[j] +\
                    (W256[j & 0x0f] += s1 + W256[(j + 9) & 0x0f] + s0);\
                (d) += T1;\
                (h) = T1 + Sigma0_256 (a) + Maj ((a), (b), (c));\
                ++j;\
            }

            // Initial hash value H for SHA-224
            const ui32 initialHashValue224[8] = {
                0xc1059ed8,
                0x367cd507,
                0x3070dd17,
                0xf70e5939,
                0xffc00b31,
                0x68581511,
                0x64f98fa7,
                0xbefa4fa4
            };

            // Initial hash value H for SHA-256
            const ui32 initialHashValue256[8] = {
                0x6a09e667,
                0xbb67ae85,
                0x3c6ef372,
                0xa54ff53a,
                0x510e527f,
                0x9b05688c,
                0x1f83d9ab,
                0x5be0cd19
            };
        }

        void SHA2_224_256::Init (std::size_t digestSize_) {
            digestSize = digestSize_;
            memcpy (state,
                digestSize == 28 ? initialHashValue224 : initialHashValue256,
                sizeof (state));
            bitCount = 0;
            memset (buffer, 0, sizeof (buffer));
            bufferIndex = 0;
        }

        void SHA2_224_256::Update (
                const void *buffer_,
                std::size_t size) {
            if (buffer_ != nullptr && size != 0) {
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

        void SHA2_224_256::Final (Hash::Digest &digest) {
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
            digest.resize (digestSize);
        #if (HostEndian == LittleEndian)
            {
                // Convert TO host byte order
                for (std::size_t i = 0; i < 8; ++i) {
                    state[i] = ByteSwap<HostEndian, BigEndian> (state[i]);
                }
            }
        #endif // (HostEndian == LittleEndian)
            memcpy (digest.data (), state, digestSize);
            Reset ();
        }

        void SHA2_224_256::Reset () {
            digestSize = 0;
            memset (state, 0, sizeof (state));
            bitCount = 0;
            memset (buffer, 0, sizeof (buffer));
            bufferIndex = 0;
        }

        namespace {
            // Hash constant words K for SHA-256
            const ui32 K256[64] = {
                0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
                0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
                0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
                0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
                0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
                0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
                0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
                0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
                0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
            };
        }

    #define THEKOGANS_UTIL_SHA2_UNROLL_TRANSFORM
    #if defined (THEKOGANS_UTIL_SHA2_UNROLL_TRANSFORM)
        void SHA2_224_256::Transform () {
            ui32 *W256 = (ui32 *)buffer;
            // Initialize registers with the prev. intermediate value
            ui32 a = state[0];
            ui32 b = state[1];
            ui32 c = state[2];
            ui32 d = state[3];
            ui32 e = state[4];
            ui32 f = state[5];
            ui32 g = state[6];
            ui32 h = state[7];
            ui32 T1 = 0;
            ui32 s0 = 0;
            ui32 s1 = 0;
            ui32 j = 0;
            do {
                // Rounds 0 to 15 (unrolled)
                ROUND256_0_TO_15 (a, b, c, d, e, f, g, h);
                ROUND256_0_TO_15 (h, a, b, c, d, e, f, g);
                ROUND256_0_TO_15 (g, h, a, b, c, d, e, f);
                ROUND256_0_TO_15 (f, g, h, a, b, c, d, e);
                ROUND256_0_TO_15 (e, f, g, h, a, b, c, d);
                ROUND256_0_TO_15 (d, e, f, g, h, a, b, c);
                ROUND256_0_TO_15 (c, d, e, f, g, h, a, b);
                ROUND256_0_TO_15 (b, c, d, e, f, g, h, a);
            } while (j < 16);
            // Now for the remaining rounds to 64
            do {
                ROUND256 (a, b, c, d, e, f, g, h);
                ROUND256 (h, a, b, c, d, e, f, g);
                ROUND256 (g, h, a, b, c, d, e, f);
                ROUND256 (f, g, h, a, b, c, d, e);
                ROUND256 (e, f, g, h, a, b, c, d);
                ROUND256 (d, e, f, g, h, a, b, c);
                ROUND256 (c, d, e, f, g, h, a, b);
                ROUND256 (b, c, d, e, f, g, h, a);
            } while (j < 64);
            // Compute the current intermediate hash value
            state[0] += a;
            state[1] += b;
            state[2] += c;
            state[3] += d;
            state[4] += e;
            state[5] += f;
            state[6] += g;
            state[7] += h;
            // Done processing this block
            bufferIndex = 0;
            // Clean up
            a = b = c = d = e = f = g = h = T1 = s0 = s1 = 0;
        }
    #else // defined (THEKOGANS_UTIL_SHA2_UNROLL_TRANSFORM)
        void SHA2_224_256::Transform () {
            ui32 *W256 = (ui32 *)buffer;
            // Initialize registers with the prev. intermediate value
            ui32 a = state[0];
            ui32 b = state[1];
            ui32 c = state[2];
            ui32 d = state[3];
            ui32 e = state[4];
            ui32 f = state[5];
            ui32 g = state[6];
            ui32 h = state[7];
            ui32 T1 = 0;
            ui32 T2 = 0;
            ui32 s0 = 0;
            ui32 s1 = 0;
            ui32 j = 0;
            do {
                W256[j] = ByteSwap<HostEndian, BigEndian> (W256[j]);
                // Apply the SHA-256 compression function to update a..h
                T1 = h + Sigma1_256 (e) + Ch (e, f, g) + K256[j] + W256[j];
                T2 = Sigma0_256 (a) + Maj (a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + T1;
                d = c;
                c = b;
                b = a;
                a = T1 + T2;
                ++j;
            } while (j < 16);
            do {
                // Part of the message block expansion
                s0 = W256[(j + 1) & 0x0f];
                s0 = sigma0_256 (s0);
                s1 = W256[(j + 14) & 0x0f];
                s1 = sigma1_256 (s1);
                // Apply the SHA-256 compression function to update a..h
                T1 = h + Sigma1_256 (e) + Ch (e, f, g) + K256[j] +
                    (W256[j & 0x0f] += s1 + W256[(j + 9) & 0x0f] + s0);
                T2 = Sigma0_256 (a) + Maj (a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + T1;
                d = c;
                c = b;
                b = a;
                a = T1 + T2;
                ++j;
            } while (j < 64);
            // Compute the current intermediate hash value
            state[0] += a;
            state[1] += b;
            state[2] += c;
            state[3] += d;
            state[4] += e;
            state[5] += f;
            state[6] += g;
            state[7] += h;
            // Done processing this block
            bufferIndex = 0;
            // Clean up
            a = b = c = d = e = f = g = h = T1 = T2 = s0 = s1 = 0;
        }
    #endif // defined (THEKOGANS_UTIL_SHA2_UNROLL_TRANSFORM)

    } // namespace util
} // namespace thekogans
