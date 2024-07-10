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

// MD5.cpp - RSA Data Security, Inc., MD5 message-digest algorithm
// Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
// rights reserved.
//
// License to copy and use this software is granted provided that it
// is identified as the "RSA Data Security, Inc. MD5 Message-Digest
// Algorithm" in all material mentioning or referencing this software
// or this function.
//
// License is also granted to make and use derivative works provided
// that such works are identified as "derived from the RSA Data
// Security, Inc. MD5 Message-Digest Algorithm" in all material
// mentioning or referencing the derived work.
//
// RSA Data Security, Inc. makes no representations concerning either
// the merchantability of this software or the suitability of this
// software for any particular purpose. It is provided "as is"
// without express or implied warranty of any kind.
//
// These notices must be retained in any copies of any part of this
// documentation and/or software.

#include <cstring>
#include "thekogans/util/ByteSwap.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/MD5.h"

namespace thekogans {
    namespace util {

        #if !defined (THEKOGANS_UTIL_MIN_HASH_MD5_IN_PAGE)
            #define THEKOGANS_UTIL_MIN_HASH_MD5_IN_PAGE 5
        #endif // !defined (THEKOGANS_UTIL_MIN_HASH_MD5_IN_PAGE)

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (
            MD5,
            SpinLock,
            THEKOGANS_UTIL_MIN_HASH_MD5_IN_PAGE,
            DefaultAllocator::Instance ())

        namespace {
            // Initial hash value H for MD5:
            const ui32 initialHashValue[4] = {
                0x67452301,
                0xefcdab89,
                0x98badcfe,
                0x10325476
            };
        }

        void MD5::Init (std::size_t digestSize) {
            if (digestSize == DIGEST_SIZE_128) {
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

        // MD5 block update operation. Continues an MD5 message-digest
        // operation, processing another message block, and updating the
        // context.
        void MD5::Update (
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

        // MD5 finalization. Ends an MD5 message-digest operation, writing
        // the message digest and zeroizing the context.
        void MD5::Final (Digest &digest) {
            // Check to see if the current message block is too small
            // to hold the initial padding bits and size. If so, we
            // will pad the block, process it, and then continue
            // padding into a second block.
            buffer[bufferIndex++] = 0x80;
            if (bufferIndex >= SHORT_BLOCK_SIZE) {
                memset (&buffer[bufferIndex], 0, BLOCK_SIZE - bufferIndex);
                Transform ();
            }
            memset (&buffer[bufferIndex], 0, SHORT_BLOCK_SIZE - bufferIndex);
            {
                // Store the message size as the last 8 octets
                union {
                    ui64 *ui64Ptr;
                    ui8 *ui8Ptr;
                } unionBuffer;
                unionBuffer.ui8Ptr = &buffer[SHORT_BLOCK_SIZE];
                *unionBuffer.ui64Ptr = ByteSwap<HostEndian, LittleEndian> (bitCount);
            }
            Transform ();
            digest.resize (DIGEST_SIZE_128);
        #if (HostEndian == BigEndian)
            {
                // Convert TO host byte order
                for (std::size_t i = 0; i < 4; ++i) {
                    state[i] = ByteSwap<HostEndian, LittleEndian> (state[i]);
                }
            }
        #endif // (HostEndian == LittleEndian)
            memcpy (digest.data (), state, DIGEST_SIZE_128);
            Reset ();
        }

        void MD5::Reset () {
            memset (state, 0, sizeof (state));
            bitCount = 0;
            memset (buffer, 0, sizeof (buffer));
            bufferIndex = 0;
        }

        namespace {
            // Constants for Transform routine.
            #define S11 7
            #define S12 12
            #define S13 17
            #define S14 22
            #define S21 5
            #define S22 9
            #define S23 14
            #define S24 20
            #define S31 4
            #define S32 11
            #define S33 16
            #define S34 23
            #define S41 6
            #define S42 10
            #define S43 15
            #define S44 21

            // F, G, H and I are basic MD5 functions.
            #define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
            #define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
            #define H(x, y, z) ((x) ^ (y) ^ (z))
            #define I(x, y, z) ((y) ^ ((x) | (~z)))

            // ROTATE_LEFT rotates x left n bits.
            #define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

            // FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
            // Rotation is separate from addition to prevent recomputation.
            #define FF(a, b, c, d, x, s, ac) {\
                (a) += F ((b), (c), (d)) + (x) + (ui32)(ac);\
                (a) = ROTATE_LEFT ((a), (s));\
                (a) += (b);\
            }
            #define GG(a, b, c, d, x, s, ac) {\
                (a) += G ((b), (c), (d)) + (x) + (ui32)(ac);\
                (a) = ROTATE_LEFT ((a), (s));\
                (a) += (b);\
            }
            #define HH(a, b, c, d, x, s, ac) {\
                (a) += H ((b), (c), (d)) + (x) + (ui32)(ac);\
                (a) = ROTATE_LEFT ((a), (s));\
                (a) += (b);\
            }
            #define II(a, b, c, d, x, s, ac) {\
                (a) += I ((b), (c), (d)) + (x) + (ui32)(ac);\
                (a) = ROTATE_LEFT ((a), (s));\
                (a) += (b);\
            }

            // Decodes input (ui8) into output (ui32). Assumes len is
            // a multiple of 4.
            void Decode (ui32 *output, const ui8 *input, std::size_t len) {
                for (std::size_t i = 0, j = 0; j < len; ++i, j += 4) {
                    output[i] = ((ui32)input[j]) | (((ui32)input[j + 1]) << 8) |
                        (((ui32)input[j + 2]) << 16) | (((ui32)input[j + 3]) << 24);
                }
            }
        }

        // MD5 basic transformation. Transforms state based on buffer.
        void MD5::Transform () {
            ui32 x[16];
            Decode (x, buffer, 64);
            ui32 a = state[0];
            ui32 b = state[1];
            ui32 c = state[2];
            ui32 d = state[3];
            // Round 1
            FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
            FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
            FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
            FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
            FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
            FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
            FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
            FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
            FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
            FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
            FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
            FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
            FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
            FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
            FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
            FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */
            // Round 2
            GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
            GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
            GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
            GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
            GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
            GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
            GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
            GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
            GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
            GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
            GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
            GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
            GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
            GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
            GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
            GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */
            // Round 3
            HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
            HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
            HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
            HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
            HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
            HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
            HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
            HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
            HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
            HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
            HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
            HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
            HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
            HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
            HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
            HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */
            // Round 4
            II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
            II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
            II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
            II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
            II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
            II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
            II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
            II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
            II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
            II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
            II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
            II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
            II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
            II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
            II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
            II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */
            state[0] += a;
            state[1] += b;
            state[2] += c;
            state[3] += d;
            bufferIndex = 0;
            // Zeroize sensitive information.
            memset (x, 0, sizeof (x));
        }

    } // namespace util
} // namespace thekogans
