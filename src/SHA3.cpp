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

#include <cstring>
#include "thekogans/util/Constants.h"
#include "thekogans/util/ByteSwap.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/SHA3.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HASH (SHA3)

        void SHA3::Init (std::size_t digestSize_) {
            if (digestSize_ == DIGEST_SIZE_224 || digestSize_ == DIGEST_SIZE_256 ||
                    digestSize_ == DIGEST_SIZE_384 || digestSize_ == DIGEST_SIZE_512) {
                digestSize = digestSize_;
                blockSize = 200 - 2 * digestSize;
                memset (state, 0, sizeof (state));
                byteCount = 0;
                memset (buffer, 0, sizeof (buffer));
                bufferIndex = 0;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void SHA3::Update (
                const void *buffer_,
                std::size_t size) {
            if (buffer_ != 0 && size != 0) {
                byteCount += size;
                const ui8 *ptr = (const ui8 *)buffer_;
                do {
                    std::size_t bytesWritten = blockSize - bufferIndex;
                    if (bytesWritten > size) {
                        bytesWritten = size;
                    }
                    memcpy (&buffer[bufferIndex], ptr, bytesWritten);
                    bufferIndex += bytesWritten;
                    ptr += bytesWritten;
                    size -= bytesWritten;
                    if (bufferIndex == blockSize) {
                        Transform ();
                    }
                } while (size != 0);
            }
        }

        void SHA3::Final (Digest &digest) {
            buffer[bufferIndex++] = 0x06;
            while (bufferIndex < blockSize - 1) {
                buffer[bufferIndex++] = 0;
            }
            buffer[bufferIndex++] = 0x80;
            Transform ();
            digest.resize (digestSize);
        #if (HostEndian == BigEndian)
            {
                for (std::size_t i = 0; i < MAX_STATE_SIZE; ++i) {
                    state[i] = ByteSwap<HostEndian, LittleEndian> (state[i]);
                }
            }
        #endif // (HostEndian == BigEndian)
            memcpy (&digest[0], state, digestSize);
            Reset ();
        }

        void SHA3::Reset () {
            digestSize = 0;
            blockSize = 0;
            memset (state, 0, sizeof (state));
            byteCount = 0;
            memset (buffer, 0, sizeof (buffer));
            bufferIndex = 0;
        }

        namespace {
            const ui32 rounds = 24;
            const ui64 masks[rounds] = {
                THEKOGANS_UTIL_UI64_LITERAL (0x0000000000000001),
                THEKOGANS_UTIL_UI64_LITERAL (0x0000000000008082),
                THEKOGANS_UTIL_UI64_LITERAL (0x800000000000808a),
                THEKOGANS_UTIL_UI64_LITERAL (0x8000000080008000),
                THEKOGANS_UTIL_UI64_LITERAL (0x000000000000808b),
                THEKOGANS_UTIL_UI64_LITERAL (0x0000000080000001),
                THEKOGANS_UTIL_UI64_LITERAL (0x8000000080008081),
                THEKOGANS_UTIL_UI64_LITERAL (0x8000000000008009),
                THEKOGANS_UTIL_UI64_LITERAL (0x000000000000008a),
                THEKOGANS_UTIL_UI64_LITERAL (0x0000000000000088),
                THEKOGANS_UTIL_UI64_LITERAL (0x0000000080008009),
                THEKOGANS_UTIL_UI64_LITERAL (0x000000008000000a),
                THEKOGANS_UTIL_UI64_LITERAL (0x000000008000808b),
                THEKOGANS_UTIL_UI64_LITERAL (0x800000000000008b),
                THEKOGANS_UTIL_UI64_LITERAL (0x8000000000008089),
                THEKOGANS_UTIL_UI64_LITERAL (0x8000000000008003),
                THEKOGANS_UTIL_UI64_LITERAL (0x8000000000008002),
                THEKOGANS_UTIL_UI64_LITERAL (0x8000000000000080),
                THEKOGANS_UTIL_UI64_LITERAL (0x000000000000800a),
                THEKOGANS_UTIL_UI64_LITERAL (0x800000008000000a),
                THEKOGANS_UTIL_UI64_LITERAL (0x8000000080008081),
                THEKOGANS_UTIL_UI64_LITERAL (0x8000000000008080),
                THEKOGANS_UTIL_UI64_LITERAL (0x0000000080000001),
                THEKOGANS_UTIL_UI64_LITERAL (0x8000000080008008)
            };

            inline ui64 rotateLeft (ui64 x, ui8 numBits) {
                return (x << numBits) | (x >> (64 - numBits));
            }

            inline ui32 mod5 (ui32 x) {
                return x < 5 ? x : x - 5;
            }
        }

        void SHA3::Transform () {
            const ui64 *buffer64 = (const ui64 *)buffer;
            for (std::size_t i = 0, count = blockSize / 8; i < count; ++i) {
                state[i] ^= ByteSwap<HostEndian, LittleEndian> (buffer64[i]);
            }
            for (std::size_t round = 0; round < rounds; ++round) {
                // Theta
                ui64 coefficients[5];
                for (std::size_t i = 0; i < 5; ++i) {
                    coefficients[i] = state[i] ^ state[i + 5] ^ state[i + 10] ^ state[i + 15] ^ state[i + 20];
                }
                for (std::size_t i = 0; i < 5; ++i) {
                    ui64 one = coefficients[mod5 (i + 4)] ^ rotateLeft (coefficients[mod5 (i + 1)], 1);
                    state[i] ^= one;
                    state[i + 5] ^= one;
                    state[i + 10] ^= one;
                    state[i + 15] ^= one;
                    state[i + 20] ^= one;
                }
                // Rho Pi
                ui64 last = state[1];
                ui64 one = state[10];
                state[10] = rotateLeft (last, 1);
                last = one;
                one = state[7];
                state[7] = rotateLeft (last, 3);
                last = one;
                one = state[11];
                state[11] = rotateLeft (last, 6);
                last = one;
                one = state[17];
                state[17] = rotateLeft (last, 10);
                last = one;
                one = state[18];
                state[18] = rotateLeft (last, 15);
                last = one;
                one = state[ 3];
                state[3] = rotateLeft (last, 21);
                last = one;
                one = state[5];
                state[5] = rotateLeft (last, 28);
                last = one;
                one = state[16];
                state[16] = rotateLeft (last, 36);
                last = one;
                one = state[8];
                state[8] = rotateLeft (last, 45);
                last = one;
                one = state[21];
                state[21] = rotateLeft (last, 55);
                last = one;
                one = state[24];
                state[24] = rotateLeft (last,  2);
                last = one;
                one = state[4];
                state[4] = rotateLeft (last, 14);
                last = one;
                one = state[15];
                state[15] = rotateLeft (last, 27);
                last = one;
                one = state[23];
                state[23] = rotateLeft (last, 41);
                last = one;
                one = state[19];
                state[19] = rotateLeft (last, 56);
                last = one;
                one = state[13];
                state[13] = rotateLeft (last,  8);
                last = one;
                one = state[12];
                state[12] = rotateLeft (last, 25);
                last = one;
                one = state[2];
                state[2] = rotateLeft (last, 43);
                last = one;
                one = state[20];
                state[20] = rotateLeft (last, 62);
                last = one;
                one = state[14];
                state[14] = rotateLeft (last, 18);
                last = one;
                one = state[22];
                state[22] = rotateLeft (last, 39);
                last = one;
                one = state[9];
                state[9] = rotateLeft (last, 61);
                last = one;
                one = state[6];
                state[6] = rotateLeft (last, 20);
                last = one;
                state[1] = rotateLeft (last, 44);
                // Chi
                for (std::size_t j = 0; j < 25; j += 5) {
                    ui64 one = state[j];
                    ui64 two = state[j + 1];
                    state[j] ^= state[j + 2] & ~two;
                    state[j + 1] ^= state[j + 3] & ~state[j + 2];
                    state[j + 2] ^= state[j + 4] & ~state[j + 3];
                    state[j + 3] ^= one & ~state[j + 4];
                    state[j + 4] ^= two & ~one;
                }
                // Iota
                state[0] ^= masks[round];
            }
            bufferIndex = 0;
        }

    } // namespace util
} // namespace thekogans
