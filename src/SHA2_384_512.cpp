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
#include "thekogans/util/Constants.h"
#include "thekogans/util/ByteSwap.h"
#include "thekogans/util/SHA2_384_512.h"

namespace thekogans {
    namespace util {

        namespace {
            #define R(b, x) ((x) >> (b))
            #define Ch(x, y, z) (((x) & (y)) ^ ((~(x)) & (z)))
            #define Maj(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
            #define S64(b, x) (((x) >> (b)) | ((x) << (64 - (b))))
            #define Sigma0_512(x) (S64 (28, (x)) ^ S64 (34, (x)) ^ S64 (39, (x)))
            #define Sigma1_512(x) (S64 (14, (x)) ^ S64 (18, (x)) ^ S64 (41, (x)))
            #define sigma0_512(x) (S64 (1, (x)) ^ S64 (8, (x)) ^ R (7, (x)))
            #define sigma1_512(x) (S64 (19, (x)) ^ S64 (61, (x)) ^ R (6, (x)))
            #define ROUND512_0_TO_15(a, b, c, d, e, f, g, h) {\
                W512[j] = ByteSwap<HostEndian, BigEndian> (W512[j]);\
                T1 = (h) + Sigma1_512 (e) + Ch ((e), (f), (g)) + K512[j] + W512[j];\
                (d) += T1;\
                (h) = T1 + Sigma0_512 (a) + Maj ((a), (b), (c));\
                ++j;\
            }
            #define ROUND512(a, b, c, d, e, f, g, h) {\
                s0 = W512[(j + 1) & 0x0f];\
                s0 = sigma0_512 (s0);\
                s1 = W512[(j + 14) & 0x0f];\
                s1 = sigma1_512 (s1);\
                T1 = (h) + Sigma1_512 (e) + Ch ((e), (f), (g)) + K512[j] +\
                    (W512[j & 0x0f] += s1 + W512[(j + 9) & 0x0f] + s0);\
                (d) += T1;\
                (h) = T1 + Sigma0_512 (a) + Maj ((a), (b), (c));\
                ++j;\
             }

            inline void ADDINC128 (ui64 w[2], ui64 n) {
                w[0] += n;
                if (w[0] < n) {
                    w[1]++;
                }
            }

            // Initial hash value H for SHA-384
            const ui64 initialHashValue384[8] = {
                THEKOGANS_UTIL_UI64_LITERAL (0xcbbb9d5dc1059ed8),
                THEKOGANS_UTIL_UI64_LITERAL (0x629a292a367cd507),
                THEKOGANS_UTIL_UI64_LITERAL (0x9159015a3070dd17),
                THEKOGANS_UTIL_UI64_LITERAL (0x152fecd8f70e5939),
                THEKOGANS_UTIL_UI64_LITERAL (0x67332667ffc00b31),
                THEKOGANS_UTIL_UI64_LITERAL (0x8eb44a8768581511),
                THEKOGANS_UTIL_UI64_LITERAL (0xdb0c2e0d64f98fa7),
                THEKOGANS_UTIL_UI64_LITERAL (0x47b5481dbefa4fa4)
            };

            // Initial hash value H for SHA-512
            const ui64 initialHashValue512[8] = {
                THEKOGANS_UTIL_UI64_LITERAL (0x6a09e667f3bcc908),
                THEKOGANS_UTIL_UI64_LITERAL (0xbb67ae8584caa73b),
                THEKOGANS_UTIL_UI64_LITERAL (0x3c6ef372fe94f82b),
                THEKOGANS_UTIL_UI64_LITERAL (0xa54ff53a5f1d36f1),
                THEKOGANS_UTIL_UI64_LITERAL (0x510e527fade682d1),
                THEKOGANS_UTIL_UI64_LITERAL (0x9b05688c2b3e6c1f),
                THEKOGANS_UTIL_UI64_LITERAL (0x1f83d9abfb41bd6b),
                THEKOGANS_UTIL_UI64_LITERAL (0x5be0cd19137e2179)
            };
        }

        void SHA2_384_512::Init (std::size_t digestSize_) {
            digestSize = digestSize_;
            memcpy (state,
                digestSize == 48 ? initialHashValue384 : initialHashValue512 ,
                sizeof (state));
            bitCount[0] = bitCount[1] = 0;
            memset (buffer, 0, sizeof (buffer));
            bufferIndex = 0;
        }

        void SHA2_384_512::Update (
                const void *buffer_,
                std::size_t size) {
            if (buffer_ != 0 && size != 0) {
                ADDINC128 (bitCount, size << 3);
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

        void SHA2_384_512::Final (Hash::Digest &digest) {
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
                // Store the message size as the last 16 octets
                union {
                    ui8 *ui8Ptr;
                    ui64 *ui64Ptr;
                } unionBuffer;
                unionBuffer.ui8Ptr = &buffer[SHORT_BLOCK_SIZE];
                *unionBuffer.ui64Ptr++ = ByteSwap<HostEndian, BigEndian> (bitCount[1]);
                *unionBuffer.ui64Ptr = ByteSwap<HostEndian, BigEndian> (bitCount[0]);
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
            memcpy (&digest[0], state, digestSize);
            Reset ();
        }

        void SHA2_384_512::Reset () {
            digestSize = 0;
            memset (state, 0, sizeof (state));
            bitCount[0] = bitCount[1] = 0;
            memset (buffer, 0, sizeof (buffer));
            bufferIndex = 0;
        }

        namespace {
            const ui64 K512[80] = {
                THEKOGANS_UTIL_UI64_LITERAL (0x428a2f98d728ae22),
                THEKOGANS_UTIL_UI64_LITERAL (0x7137449123ef65cd),
                THEKOGANS_UTIL_UI64_LITERAL (0xb5c0fbcfec4d3b2f),
                THEKOGANS_UTIL_UI64_LITERAL (0xe9b5dba58189dbbc),
                THEKOGANS_UTIL_UI64_LITERAL (0x3956c25bf348b538),
                THEKOGANS_UTIL_UI64_LITERAL (0x59f111f1b605d019),
                THEKOGANS_UTIL_UI64_LITERAL (0x923f82a4af194f9b),
                THEKOGANS_UTIL_UI64_LITERAL (0xab1c5ed5da6d8118),
                THEKOGANS_UTIL_UI64_LITERAL (0xd807aa98a3030242),
                THEKOGANS_UTIL_UI64_LITERAL (0x12835b0145706fbe),
                THEKOGANS_UTIL_UI64_LITERAL (0x243185be4ee4b28c),
                THEKOGANS_UTIL_UI64_LITERAL (0x550c7dc3d5ffb4e2),
                THEKOGANS_UTIL_UI64_LITERAL (0x72be5d74f27b896f),
                THEKOGANS_UTIL_UI64_LITERAL (0x80deb1fe3b1696b1),
                THEKOGANS_UTIL_UI64_LITERAL (0x9bdc06a725c71235),
                THEKOGANS_UTIL_UI64_LITERAL (0xc19bf174cf692694),
                THEKOGANS_UTIL_UI64_LITERAL (0xe49b69c19ef14ad2),
                THEKOGANS_UTIL_UI64_LITERAL (0xefbe4786384f25e3),
                THEKOGANS_UTIL_UI64_LITERAL (0x0fc19dc68b8cd5b5),
                THEKOGANS_UTIL_UI64_LITERAL (0x240ca1cc77ac9c65),
                THEKOGANS_UTIL_UI64_LITERAL (0x2de92c6f592b0275),
                THEKOGANS_UTIL_UI64_LITERAL (0x4a7484aa6ea6e483),
                THEKOGANS_UTIL_UI64_LITERAL (0x5cb0a9dcbd41fbd4),
                THEKOGANS_UTIL_UI64_LITERAL (0x76f988da831153b5),
                THEKOGANS_UTIL_UI64_LITERAL (0x983e5152ee66dfab),
                THEKOGANS_UTIL_UI64_LITERAL (0xa831c66d2db43210),
                THEKOGANS_UTIL_UI64_LITERAL (0xb00327c898fb213f),
                THEKOGANS_UTIL_UI64_LITERAL (0xbf597fc7beef0ee4),
                THEKOGANS_UTIL_UI64_LITERAL (0xc6e00bf33da88fc2),
                THEKOGANS_UTIL_UI64_LITERAL (0xd5a79147930aa725),
                THEKOGANS_UTIL_UI64_LITERAL (0x06ca6351e003826f),
                THEKOGANS_UTIL_UI64_LITERAL (0x142929670a0e6e70),
                THEKOGANS_UTIL_UI64_LITERAL (0x27b70a8546d22ffc),
                THEKOGANS_UTIL_UI64_LITERAL (0x2e1b21385c26c926),
                THEKOGANS_UTIL_UI64_LITERAL (0x4d2c6dfc5ac42aed),
                THEKOGANS_UTIL_UI64_LITERAL (0x53380d139d95b3df),
                THEKOGANS_UTIL_UI64_LITERAL (0x650a73548baf63de),
                THEKOGANS_UTIL_UI64_LITERAL (0x766a0abb3c77b2a8),
                THEKOGANS_UTIL_UI64_LITERAL (0x81c2c92e47edaee6),
                THEKOGANS_UTIL_UI64_LITERAL (0x92722c851482353b),
                THEKOGANS_UTIL_UI64_LITERAL (0xa2bfe8a14cf10364),
                THEKOGANS_UTIL_UI64_LITERAL (0xa81a664bbc423001),
                THEKOGANS_UTIL_UI64_LITERAL (0xc24b8b70d0f89791),
                THEKOGANS_UTIL_UI64_LITERAL (0xc76c51a30654be30),
                THEKOGANS_UTIL_UI64_LITERAL (0xd192e819d6ef5218),
                THEKOGANS_UTIL_UI64_LITERAL (0xd69906245565a910),
                THEKOGANS_UTIL_UI64_LITERAL (0xf40e35855771202a),
                THEKOGANS_UTIL_UI64_LITERAL (0x106aa07032bbd1b8),
                THEKOGANS_UTIL_UI64_LITERAL (0x19a4c116b8d2d0c8),
                THEKOGANS_UTIL_UI64_LITERAL (0x1e376c085141ab53),
                THEKOGANS_UTIL_UI64_LITERAL (0x2748774cdf8eeb99),
                THEKOGANS_UTIL_UI64_LITERAL (0x34b0bcb5e19b48a8),
                THEKOGANS_UTIL_UI64_LITERAL (0x391c0cb3c5c95a63),
                THEKOGANS_UTIL_UI64_LITERAL (0x4ed8aa4ae3418acb),
                THEKOGANS_UTIL_UI64_LITERAL (0x5b9cca4f7763e373),
                THEKOGANS_UTIL_UI64_LITERAL (0x682e6ff3d6b2b8a3),
                THEKOGANS_UTIL_UI64_LITERAL (0x748f82ee5defb2fc),
                THEKOGANS_UTIL_UI64_LITERAL (0x78a5636f43172f60),
                THEKOGANS_UTIL_UI64_LITERAL (0x84c87814a1f0ab72),
                THEKOGANS_UTIL_UI64_LITERAL (0x8cc702081a6439ec),
                THEKOGANS_UTIL_UI64_LITERAL (0x90befffa23631e28),
                THEKOGANS_UTIL_UI64_LITERAL (0xa4506cebde82bde9),
                THEKOGANS_UTIL_UI64_LITERAL (0xbef9a3f7b2c67915),
                THEKOGANS_UTIL_UI64_LITERAL (0xc67178f2e372532b),
                THEKOGANS_UTIL_UI64_LITERAL (0xca273eceea26619c),
                THEKOGANS_UTIL_UI64_LITERAL (0xd186b8c721c0c207),
                THEKOGANS_UTIL_UI64_LITERAL (0xeada7dd6cde0eb1e),
                THEKOGANS_UTIL_UI64_LITERAL (0xf57d4f7fee6ed178),
                THEKOGANS_UTIL_UI64_LITERAL (0x06f067aa72176fba),
                THEKOGANS_UTIL_UI64_LITERAL (0x0a637dc5a2c898a6),
                THEKOGANS_UTIL_UI64_LITERAL (0x113f9804bef90dae),
                THEKOGANS_UTIL_UI64_LITERAL (0x1b710b35131c471b),
                THEKOGANS_UTIL_UI64_LITERAL (0x28db77f523047d84),
                THEKOGANS_UTIL_UI64_LITERAL (0x32caab7b40c72493),
                THEKOGANS_UTIL_UI64_LITERAL (0x3c9ebe0a15c9bebc),
                THEKOGANS_UTIL_UI64_LITERAL (0x431d67c49c100d4c),
                THEKOGANS_UTIL_UI64_LITERAL (0x4cc5d4becb3e42b6),
                THEKOGANS_UTIL_UI64_LITERAL (0x597f299cfc657e2a),
                THEKOGANS_UTIL_UI64_LITERAL (0x5fcb6fab3ad6faec),
                THEKOGANS_UTIL_UI64_LITERAL (0x6c44198c4a475817)
            };
        }

    #define THEKOGANS_UTIL_SHA2_UNROLL_TRANSFORM
    #if defined (THEKOGANS_UTIL_SHA2_UNROLL_TRANSFORM)
        void SHA2_384_512::Transform () {
            ui64 *W512 = (ui64 *)buffer;
            // Initialize registers with the prev. intermediate value
            ui64 a = state[0];
            ui64 b = state[1];
            ui64 c = state[2];
            ui64 d = state[3];
            ui64 e = state[4];
            ui64 f = state[5];
            ui64 g = state[6];
            ui64 h = state[7];
            ui64 T1 = 0;
            ui64 s0 = 0;
            ui64 s1 = 0;
            ui32 j = 0;
            do {
                ROUND512_0_TO_15 (a, b, c, d, e, f, g, h);
                ROUND512_0_TO_15 (h, a, b, c, d, e, f, g);
                ROUND512_0_TO_15 (g, h, a, b, c, d, e, f);
                ROUND512_0_TO_15 (f, g, h, a, b, c, d, e);
                ROUND512_0_TO_15 (e, f, g, h, a, b, c, d);
                ROUND512_0_TO_15 (d, e, f, g, h, a, b, c);
                ROUND512_0_TO_15 (c, d, e, f, g, h, a, b);
                ROUND512_0_TO_15 (b, c, d, e, f, g, h, a);
            } while (j < 16);
            // Now for the remaining rounds up to 79
            do {
                ROUND512 (a, b, c, d, e, f, g, h);
                ROUND512 (h, a, b, c, d, e, f, g);
                ROUND512 (g, h, a, b, c, d, e, f);
                ROUND512 (f, g, h, a, b, c, d, e);
                ROUND512 (e, f, g, h, a, b, c, d);
                ROUND512 (d, e, f, g, h, a, b, c);
                ROUND512 (c, d, e, f, g, h, a, b);
                ROUND512 (b, c, d, e, f, g, h, a);
            } while (j < 80);
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
        void SHA2_384_512::Transform () {
            ui64 *W512 = (ui64 *)buffer;
            // Initialize registers with the prev. intermediate value
            ui64 a = state[0];
            ui64 b = state[1];
            ui64 c = state[2];
            ui64 d = state[3];
            ui64 e = state[4];
            ui64 f = state[5];
            ui64 g = state[6];
            ui64 h = state[7];
            ui64 T1 = 0;
            ui64 T2 = 0;
            ui64 s0 = 0;
            ui64 s1 = 0;
            ui32 j = 0;
            do {
                W512[j] = ByteSwap<HostEndian, BigEndian> (W512[j]);
                // Apply the SHA-512 compression function to update a..h
                T1 = h + Sigma1_512 (e) + Ch (e, f, g) + K512[j] + W512[j];
                T2 = Sigma0_512 (a) + Maj (a, b, c);
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
                s0 = W512[(j + 1) & 0x0f];
                s0 = sigma0_512 (s0);
                s1 = W512[(j + 14) & 0x0f];
                s1 =  sigma1_512 (s1);
                // Apply the SHA-512 compression function to update a..h
                T1 = h + Sigma1_512 (e) + Ch (e, f, g) + K512[j] +
                    (W512[j & 0x0f] += s1 + W512[(j + 9) & 0x0f] + s0);
                T2 = Sigma0_512 (a) + Maj (a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + T1;
                d = c;
                c = b;
                b = a;
                a = T1 + T2;
                ++j;
            } while (j < 80);
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
