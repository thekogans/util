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

#include <cassert>
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Array.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/File.h"
#include "thekogans/util/RandomSource.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/AlignedAllocator.h"
#include "thekogans/util/Hash.h"
#if defined (THEKOGANS_UTIL_TYPE_Static)
    #include "thekogans/util/MD5.h"
    #include "thekogans/util/SHA1.h"
    #include "thekogans/util/SHA2.h"
    #include "thekogans/util/SHA3.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (thekogans::util::Hash)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void Hash::StaticInit () {
            MD5::StaticInit ();
            SHA1::StaticInit ();
            SHA2::StaticInit ();
            SHA3::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        std::string Hash::DigestTostring (const Digest &digest) {
            return HexEncodeBuffer (digest.data (), digest.size ());
        }

        Hash::Digest Hash::stringToDigest (const std::string &digest) {
            return HexDecodeBuffer (digest.c_str (), digest.size ());
        }

        void Hash::FromRandom (
                std::size_t length,
                std::size_t digestSize,
                Digest &digest) {
            if (length > 0) {
                Buffer alignedBuffer (HostEndian, length, 0, 0, new AlignedAllocator (UI32_SIZE));
                std::size_t written = alignedBuffer.AdvanceWriteOffset (
                    RandomSource::Instance ()->GetBytes (
                        alignedBuffer.GetWritePtr (),
                        alignedBuffer.GetDataAvailableForWriting ()));
                if (written == length) {
                    FromBuffer (
                        alignedBuffer.GetReadPtr (),
                        alignedBuffer.GetDataAvailableForReading (),
                        digestSize,
                        digest);
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Unable to get " THEKOGANS_UTIL_SIZE_T_FORMAT " random bytes for hash.",
                        length);
                }
            }
            else {
                digest.clear ();
            }
        }

        void Hash::FromBuffer (
                const void *buffer,
                std::size_t length,
                std::size_t digestSize,
                Digest &digest) {
            if (buffer != nullptr && length > 0) {
                Init (digestSize);
                Update (buffer, length);
                Final (digest);
            }
            else {
                digest.clear ();
            }
        }

        void Hash::FromFile (
                const std::string &path,
                std::size_t digestSize,
                Digest &digest) {
            ReadOnlyFile file (HostEndian, path);
            if (file.GetSize () > 0) {
                Init (digestSize);
                static const std::size_t BUFFER_CAPACITY = 4096;
                Array<ui8> buffer (BUFFER_CAPACITY);
                std::size_t size;
                while ((size = file.Read (buffer, BUFFER_CAPACITY)) != 0) {
                    Update (buffer, size);
                }
                Final (digest);
            }
            else {
                digest.clear ();
            }
        }

        /*
          -------------------------------------------------------------------------------
          lookup3.c, by Bob Jenkins, May 2006, Public Domain.

          These are functions for producing 32-bit hashes for hash table lookup.
          HashBuffer32(), HashBufferLittle(), HashBufferBig(), mix(), and final()
          are externally useful functions.  Routines to test the hash are included
          if SELF_TEST is defined.  You can use this free for any purpose.  It's in
          the public domain.  It has no warranty.

          You probably want to use HashBufferLittle().  HashBufferLittle() and HashBufferBig()
          hash byte arrays.  HashBufferLittle() is is faster than HashBufferBig() on
          little-endian machines.  Intel and AMD are little-endian machines.
          On second thought, you probably want HashBufferLittle2(), which is identical to
          HashBufferLittle() except it returns two 32-bit hashes for the price of one.
          You could implement HashBufferBig2() if you wanted but I haven't bothered here.

          If you want to find a hash of, say, exactly 7 integers, do
          a = i1;  b = i2;  c = i3;
          mix(a,b,c);
          a += i4; b += i5; c += i6;
          mix(a,b,c);
          a += i7;
          final(a,b,c);
          then use c as the hash value.  If you have a variable length array of
          4-byte integers to hash, use HashBuffer32().  If you have a byte array (like
          a character string), use HashBufferLittle().  If you have several byte arrays, or
          a mix of things, see the comments above HashBufferLittle().

          Why is this so big?  I read 12 bytes at a time into 3 4-byte integers,
          then mix those integers.  This is fast (you can do a lot more thorough
          mixing with 12*3 instructions on 3 integers than you can with 3 instructions
          on 1 byte), but shoehorning those bytes into integers efficiently is messy.
          -------------------------------------------------------------------------------
        */
        #define hashsize(n) ((ui32)1 << (n))
        #define hashmask(n) (hashsize (n) - 1)
        #define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

        /*
          -------------------------------------------------------------------------------
          mix -- mix 3 32-bit values reversibly.

          This is reversible, so any information in (a,b,c) before mix() is
          still in (a,b,c) after mix().

          If four pairs of (a,b,c) inputs are run through mix(), or through
          mix() in reverse, there are at least 32 bits of the output that
          are sometimes the same for one pair and different for another pair.
          This was tested for:
          * pairs that differed by one bit, by two bits, in any combination
          of top bits of (a,b,c), or in any combination of bottom bits of
          (a,b,c).
          * "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
          the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
          is commonly produced by subtraction) look like a single 1-bit
          difference.
          * the base values were pseudorandom, all zero but one bit set, or
          all zero plus a counter that starts at zero.

          Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
          satisfy this are
          4  6  8 16 19  4
          9 15  3 18 27 15
          14  9  3  7 17  3
          Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
          for "differ" defined as + with a one-bit base and a two-bit delta.  I
          used http://burtleburtle.net/bob/hash/avalanche.html to choose
          the operations, constants, and arrangements of the variables.

          This does not achieve avalanche.  There are input bits of (a,b,c)
          that fail to affect some output bits of (a,b,c), especially of a.  The
          most thoroughly mixed value is c, but it doesn't really even achieve
          avalanche in c.

          This allows some parallelism.  Read-after-writes are good at doubling
          the number of bits affected, so the goal of mixing pulls in the opposite
          direction as the goal of parallelism.  I did what I could.  Rotates
          seem to cost as much as shifts on every machine I could lay my hands
          on, and rotates are much kinder to the top and bottom bits, so I used
          rotates.
          -------------------------------------------------------------------------------
        */
        #define mix(a, b, c) {\
            a -= c;  a ^= rot (c,  4);  c += b;\
            b -= a;  b ^= rot (a,  6);  a += c;\
            c -= b;  c ^= rot (b,  8);  b += a;\
            a -= c;  a ^= rot (c, 16);  c += b;\
            b -= a;  b ^= rot (a, 19);  a += c;\
            c -= b;  c ^= rot (b,  4);  b += a;\
        }

        /*
          -------------------------------------------------------------------------------
          final -- final mixing of 3 32-bit values (a,b,c) into c

          Pairs of (a,b,c) values differing in only a few bits will usually
          produce values of c that look totally different.  This was tested for
          * pairs that differed by one bit, by two bits, in any combination
          of top bits of (a,b,c), or in any combination of bottom bits of
          (a,b,c).
          * "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
          the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
          is commonly produced by subtraction) look like a single 1-bit
          difference.
          * the base values were pseudorandom, all zero but one bit set, or
          all zero plus a counter that starts at zero.

          These constants passed:
          14 11 25 16 4 14 24
          12 14 25 16 4 14 24
          and these came close:
          4  8 15 26 3 22 24
          10  8 15 26 3 22 24
          11  8 15 26 3 22 24
          -------------------------------------------------------------------------------
        */
        #define final(a, b, c) {\
            c ^= b; c -= rot (b, 14);\
            a ^= c; a -= rot (c, 11);\
            b ^= a; b -= rot (a, 25);\
            c ^= b; c -= rot (b, 16);\
            a ^= c; a -= rot (c, 4);\
            b ^= a; b -= rot (a, 14);\
            c ^= b; c -= rot (b, 24);\
        }

        _LIB_THEKOGANS_UTIL_DECL ui32 _LIB_THEKOGANS_UTIL_API HashBuffer32 (
                const ui32 *buffer,
                std::size_t length,
                ui32 seed) {
            if (buffer != nullptr && length > 0) {
                ui32 a = 0xdeadbeef + (((ui32)length) << 2) + seed;
                ui32 b = a;
                ui32 c = a;
                while (length > 3) {
                    a += buffer[0];
                    b += buffer[1];
                    c += buffer[2];
                    mix (a, b, c);
                    length -= 3;
                    buffer += 3;
                }
                switch (length) {
                    case 3:
                        c += buffer[2];
                    case 2:
                        b += buffer[1];
                    case 1:
                        a += buffer[0];
                        final (a, b, c);
                    case 0:
                        break;
                }
                return c;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL ui32 _LIB_THEKOGANS_UTIL_API HashBufferLittle (
                const void *buffer,
                std::size_t length,
                ui32 seed) {
            if (buffer != nullptr && length > 0) {
                ui32 a = 0xdeadbeef + ((ui32)length) + seed;
                ui32 b = a;
                ui32 c = a;
                union {
                    const void *ptr;
                    std::size_t i;
                } u;
                u.ptr = buffer;
                if (((u.i & 0x3) == 0)) {
                    const ui32 *ptr = (const ui32 *)buffer; // read 32-bit chunks
                    // all but last block: aligned reads and affect 32 bits of (a, b, c)
                    while (length > 12) {
                        a += ptr[0];
                        b += ptr[1];
                        c += ptr[2];
                        mix (a, b, c);
                        length -= 12;
                        ptr += 3;
                    }
                    // handle the last (probably partial) block
                    // "k[2] & 0xffffff" actually reads beyond the end of the string, but
                    // then masks off the part it's not allowed to read. Because the
                    // string is aligned, the masked-off tail is in the same word as the
                    // rest of the string. Every machine with memory protection I've seen
                    // does it on word boundaries, so is OK with this. But VALGRIND will
                    // still catch it and complain. The masking trick does make the hash
                    // noticably faster for short strings (like English words).
                    switch (length) {
                        case 12:
                            c += ptr[2];
                            b += ptr[1];
                            a += ptr[0];
                            break;
                        case 11:
                            c += ptr[2] & 0xffffff;
                            b += ptr[1];
                            a += ptr[0];
                            break;
                        case 10:
                            c += ptr[2] & 0xffff;
                            b += ptr[1];
                            a += ptr[0];
                            break;
                        case 9:
                            c += ptr[2] & 0xff;
                            b += ptr[1];
                            a += ptr[0];
                            break;
                        case 8:
                            b += ptr[1];
                            a += ptr[0];
                            break;
                        case 7:
                            b += ptr[1] & 0xffffff;
                            a += ptr[0];
                            break;
                        case 6:
                            b += ptr[1] & 0xffff;
                            a += ptr[0];
                            break;
                        case 5:
                            b += ptr[1] & 0xff;
                            a += ptr[0];
                            break;
                        case 4:
                            a += ptr[0];
                            break;
                        case 3:
                            a += ptr[0] & 0xffffff;
                            break;
                        case 2:
                            a += ptr[0] & 0xffff;
                            break;
                        case 1:
                            a += ptr[0] & 0xff;
                            break;
                        case 0:
                            return c; // zero length strings require no mixing
                    }
                }
                else if (((u.i & 0x1) == 0)) {
                    const uint16_t *ptr = (const uint16_t *)buffer; // read 16-bit chunks
                    const ui8 *ptr8;
                    // all but last block: aligned reads and different mixing
                    while (length > 12) {
                        a += ptr[0] + (((ui32)ptr[1]) << 16);
                        b += ptr[2] + (((ui32)ptr[3]) << 16);
                        c += ptr[4] + (((ui32)ptr[5]) << 16);
                        mix (a, b, c);
                        length -= 12;
                        ptr += 6;
                    }
                    // handle the last (probably partial) block
                    ptr8 = (const ui8 *)ptr;
                    switch (length) {
                        case 12:
                            c += ptr[4] + (((ui32)ptr[5]) << 16);
                            b += ptr[2] + (((ui32)ptr[3]) << 16);
                            a += ptr[0] + (((ui32)ptr[1]) << 16);
                            break;
                        case 11:
                            c += ((ui32)ptr8[10]) << 16; // fall through
                        case 10:
                            c += ptr[4];
                            b += ptr[2] + (((ui32)ptr[3]) << 16);
                            a += ptr[0] + (((ui32)ptr[1]) << 16);
                            break;
                        case 9:
                            c += ptr8[8]; // fall through
                        case 8:
                            b += ptr[2] + (((ui32)ptr[3]) << 16);
                            a += ptr[0] + (((ui32)ptr[1]) << 16);
                            break;
                        case 7:
                            b += ((ui32)ptr8[6]) << 16; // fall through
                        case 6:
                            b += ptr[2];
                            a += ptr[0] + (((ui32)ptr[1]) << 16);
                            break;
                        case 5:
                            b += ptr8[4]; // fall through
                        case 4:
                            a += ptr[0] + (((ui32)ptr[1]) << 16);
                            break;
                        case 3:
                            a += ((ui32)ptr8[2]) << 16; // fall through
                        case 2:
                            a += ptr[0];
                            break;
                        case 1:
                            a += ptr8[0];
                            break;
                        case 0:
                            return c; // zero length requires no mixing
                    }
                }
                else { // need to read the buffer one byte at a time
                    const ui8 *ptr = (const ui8 *)buffer;
                    // all but the last block: affect some 32 bits of (a, b, c)
                    while (length > 12) {
                        a += ptr[0];
                        a += ((ui32)ptr[1]) << 8;
                        a += ((ui32)ptr[2]) << 16;
                        a += ((ui32)ptr[3]) << 24;
                        b += ptr[4];
                        b += ((ui32)ptr[5]) << 8;
                        b += ((ui32)ptr[6]) << 16;
                        b += ((ui32)ptr[7]) << 24;
                        c += ptr[8];
                        c += ((ui32)ptr[9]) << 8;
                        c += ((ui32)ptr[10]) << 16;
                        c += ((ui32)ptr[11]) << 24;
                        mix (a, b, c);
                        length -= 12;
                        ptr += 12;
                    }
                    // last block: affect all 32 bits of (c)
                    switch (length) { // all the case statements fall through
                        case 12:
                            c += ((ui32)ptr[11]) << 24;
                        case 11:
                            c += ((ui32)ptr[10]) << 16;
                        case 10:
                            c += ((ui32)ptr[9]) << 8;
                        case 9:
                            c += ptr[8];
                        case 8:
                            b += ((ui32)ptr[7]) << 24;
                        case 7:
                            b += ((ui32)ptr[6]) << 16;
                        case 6:
                            b += ((ui32)ptr[5]) << 8;
                        case 5:
                            b += ptr[4];
                        case 4:
                            a += ((ui32)ptr[3]) << 24;
                        case 3:
                            a += ((ui32)ptr[2]) << 16;
                        case 2:
                            a += ((ui32)ptr[1]) << 8;
                        case 1:
                            a += ptr[0];
                            break;
                        case 0:
                            return c;
                    }
                }
                final (a, b, c);
                return c;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL ui32 _LIB_THEKOGANS_UTIL_API HashBufferBig (
                const void *buffer,
                std::size_t length,
                ui32 seed) {
            if (buffer != nullptr && length > 0) {
                ui32 a = 0xdeadbeef + ((ui32)length) + seed;
                ui32 b = a;
                ui32 c = a;
                union {
                    const void *ptr;
                    std::size_t i;
                } u;
                u.ptr = buffer;
                if (((u.i & 0x3) == 0)) {
                    const ui32 *ptr = (const ui32 *)buffer;
                    // all but last block: aligned reads and affect 32 bits of (a, b, c)
                    while (length > 12) {
                        a += ptr[0];
                        b += ptr[1];
                        c += ptr[2];
                        mix (a, b, c);
                        length -= 12;
                        ptr += 3;
                    }
                    // handle the last (probably partial) block
                    // "k[2] << 8" actually reads beyond the end of the string, but
                    // then shifts out the part it's not allowed to read.  Because the
                    // string is aligned, the illegal read is in the same word as the
                    // rest of the string.  Every machine with memory protection I've seen
                    // does it on word boundaries, so is OK with this.  But VALGRIND will
                    // still catch it and complain.  The masking trick does make the hash
                    // noticably faster for short strings (like English words).
                    switch (length) {
                        case 12:
                            c += ptr[2];
                            b += ptr[1];
                            a += ptr[0];
                            break;
                        case 11:
                            c += ptr[2] & 0xffffff00;
                            b += ptr[1];
                            a += ptr[0];
                            break;
                        case 10:
                            c += ptr[2] & 0xffff0000;
                            b += ptr[1];
                            a += ptr[0];
                            break;
                        case 9:
                            c += ptr[2] & 0xff000000;
                            b += ptr[1];
                            a += ptr[0];
                            break;
                        case 8:
                            b += ptr[1];
                            a += ptr[0];
                            break;
                        case 7:
                            b += ptr[1] & 0xffffff00;
                            a += ptr[0];
                            break;
                        case 6:
                            b += ptr[1] & 0xffff0000;
                            a += ptr[0];
                            break;
                        case 5:
                            b += ptr[1] & 0xff000000;
                            a += ptr[0];
                            break;
                        case 4:
                            a += ptr[0];
                            break;
                        case 3:
                            a += ptr[0] & 0xffffff00;
                            break;
                        case 2:
                            a += ptr[0] & 0xffff0000;
                            break;
                        case 1:
                            a += ptr[0] & 0xff000000;
                            break;
                        case 0:
                            return c; // zero length strings require no mixing
                    }
                }
                else { // need to read the buffer one byte at a time
                    const ui8 *ptr = (const ui8 *)buffer;
                    // all but the last block: affect some 32 bits of (a, b, c)
                    while (length > 12) {
                        a += ((ui32)ptr[0]) << 24;
                        a += ((ui32)ptr[1]) << 16;
                        a += ((ui32)ptr[2]) << 8;
                        a += ((ui32)ptr[3]);
                        b += ((ui32)ptr[4]) << 24;
                        b += ((ui32)ptr[5]) << 16;
                        b += ((ui32)ptr[6]) << 8;
                        b += ((ui32)ptr[7]);
                        c += ((ui32)ptr[8]) << 24;
                        c += ((ui32)ptr[9]) << 16;
                        c += ((ui32)ptr[10]) << 8;
                        c += ((ui32)ptr[11]);
                        mix (a, b, c);
                        length -= 12;
                        ptr += 12;
                    }
                    // last block: affect all 32 bits of (c)
                    switch (length) { // all the case statements fall through
                        case 12:
                            c += ptr[11];
                        case 11:
                            c += ((ui32)ptr[10]) << 8;
                        case 10:
                            c += ((ui32)ptr[9]) << 16;
                        case 9:
                            c += ((ui32)ptr[8]) << 24;
                        case 8:
                            b += ptr[7];
                        case 7:
                            b += ((ui32)ptr[6]) << 8;
                        case 6:
                            b += ((ui32)ptr[5]) << 16;
                        case 5:
                            b += ((ui32)ptr[4]) << 24;
                        case 4:
                            a += ptr[3];
                        case 3:
                            a += ((ui32)ptr[2]) << 8;
                        case 2:
                            a += ((ui32)ptr[1]) << 16;
                        case 1:
                            a += ((ui32)ptr[0]) << 24;
                            break;
                        case 0:
                            return c;
                    }
                }
                final (a, b, c);
                return c;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    } // namespace util
} // namespace thekogans
