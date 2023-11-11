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

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include <intrin.h>
#elif defined (TOOLCHAIN_OS_OSX)
    #include <CoreFoundation/CoreFoundation.h>
    #include <Security/Security.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Types.h"
#if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
    #include "thekogans/util/DefaultAllocator.h"
    #include "thekogans/util/AlignedAllocator.h"
    #include "thekogans/util/Buffer.h"
#endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
#include "thekogans/util/Exception.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/CPU.h"
#include "thekogans/util/RandomSource.h"

namespace thekogans {
    namespace util {

    #if defined (TOOLCHAIN_OS_Windows)
        RandomSource::RandomSource () :
                cryptProv (0) {
    #elif defined (TOOLCHAIN_OS_Linux)
        RandomSource::RandomSource () :
                // http://www.2uo.de/myths-about-urandom/
                urandom (HostEndian, "/dev/urandom") {
    #elif defined (TOOLCHAIN_OS_OSX)
        RandomSource::RandomSource () {
    #endif // defined (TOOLCHAIN_OS_Windows)
        #if defined (TOOLCHAIN_OS_Windows)
            if (!CryptAcquireContext (&cryptProv, 0, 0,
                    PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        RandomSource::~RandomSource () {
        #if defined (TOOLCHAIN_OS_Windows)
            CryptReleaseContext (cryptProv, 0);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        namespace {
            std::size_t GetHardwareBytes (
                    void *buffer,
                    std::size_t bufferLength) {
            #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
                if (CPU::Instance ().RDRAND ()) {
                    if (((uintptr_t)buffer & (UI32_SIZE - 1)) != 0) {
                        // A misaligned buffer was passed in. The code
                        // below requires the buffer to be aligned on
                        // a ui32 byte boundary.
                        AlignedAllocator allocator (DefaultAllocator::Instance (), UI32_SIZE);
                        Buffer alignedBuffer (HostEndian, bufferLength, 0, 0, &allocator);
                        alignedBuffer.AdvanceWriteOffset (GetHardwareBytes (alignedBuffer.GetWritePtr (), bufferLength));
                        memcpy (buffer, alignedBuffer.GetReadPtr (), alignedBuffer.GetDataAvailableForReading ());
                        alignedBuffer.Clear ();
                        return alignedBuffer.GetDataAvailableForReading ();
                    }
                    // This function was inspired by an answer from:
                    // https://stackoverflow.com/questions/11407103/how-i-can-get-the-random-number-from-intels-processor-with-assembler
                    std::size_t ui32Count = bufferLength >> 2;
                    ui32 *begin = (ui32 *)buffer;
                    ui32 *end = begin + ui32Count;
                    std::size_t retries = ui32Count + 4;
                    ui32 value;
                    while (begin < end) {
                        char rc;
                    #if defined (TOOLCHAIN_OS_Windows)
                        rc = _rdrand32_step (&value);
                    #else // defined (TOOLCHAIN_OS_Windows)
                        __asm__ volatile (
                            "rdrand %0 ; setc %1"
                            : "=r" (value), "=qm" (rc));
                    #endif // defined (TOOLCHAIN_OS_Windows)
                        // 1 = success, 0 = underflow
                        if (rc == 1) {
                            *begin++ = value;
                        }
                        else if (retries-- == 0) {
                            bufferLength = (ui8 *)begin - (ui8 *)buffer;
                            break;
                        }
                    }
                    if (begin == end) {
                        std::size_t remainder = bufferLength & 3;
                        while (remainder > 0) {
                            char rc;
                        #if defined (TOOLCHAIN_OS_Windows)
                            rc = _rdrand32_step (&value);
                        #else // defined (TOOLCHAIN_OS_Windows)
                            __asm__ volatile (
                                "rdrand %0 ; setc %1"
                                : "=r" (value), "=qm" (rc));
                        #endif // defined (TOOLCHAIN_OS_Windows)
                            // 1 = success, 0 = underflow
                            if (rc == 1) {
                                memcpy (begin, &value, remainder);
                                break;
                            }
                            else if (retries-- == 0) {
                                bufferLength = (ui8 *)begin - (ui8 *)buffer;
                                break;
                            }
                        }
                    }
                    // Wipe value on exit.
                    *((volatile ui32 *)&value) = 0;
                    return bufferLength;
                }
            #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
                return 0;
            }
        }

        std::size_t RandomSource::GetBytes (
                void *buffer,
                std::size_t bufferLength) {
            if (buffer != 0 && bufferLength > 0) {
                LockGuard<SpinLock> guard (spinLock);
                // If a hardware random source exists, use it first.
                std::size_t hardwareCount = GetHardwareBytes (buffer, bufferLength);
                if (hardwareCount == bufferLength) {
                    return hardwareCount;
                }
                // If we got here either there's no hardware random source or,
                // it couldn't generate enough bytes. Use other means.
            #if defined (TOOLCHAIN_OS_Windows)
                if (!CryptGenRandom (
                        cryptProv,
                        (DWORD)(bufferLength - hardwareCount),
                        (BYTE *)buffer + hardwareCount)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                return bufferLength;
            #elif defined (TOOLCHAIN_OS_Linux)
                return hardwareCount + urandom.Read (
                    (ui8 *)buffer + hardwareCount,
                    bufferLength - hardwareCount);
            #elif defined (TOOLCHAIN_OS_OSX)
                OSStatus errorCode = SecRandomCopyBytes (kSecRandomDefault, bufferLength, buffer);
                if (errorCode != noErr) {
                    THEKOGANS_UTIL_THROW_SEC_OSSTATUS_ERROR_CODE_EXCEPTION (errorCode);
                }
                return bufferLength;
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        std::size_t RandomSource::GetSeed (
                void *buffer,
                std::size_t bufferLength) {
            if (buffer != 0 && bufferLength > 0) {
            #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
                if (CPU::Instance ().RDSEED ()) {
                    if (((uintptr_t)buffer & (UI32_SIZE - 1)) != 0) {
                        // See above in GetHardwareBytes.
                        AlignedAllocator allocator (DefaultAllocator::Instance (), UI32_SIZE);
                        Buffer alignedBuffer (HostEndian, bufferLength, 0, 0, &allocator);
                        alignedBuffer.AdvanceWriteOffset (GetSeed (alignedBuffer.GetWritePtr (), bufferLength));
                        memcpy (buffer, alignedBuffer.GetReadPtr (), alignedBuffer.GetDataAvailableForReading ());
                        alignedBuffer.Clear ();
                        return alignedBuffer.GetDataAvailableForReading ();
                    }
                    LockGuard<SpinLock> guard (spinLock);
                    // This function was inspired by an answer from:
                    // https://stackoverflow.com/questions/11407103/how-i-can-get-the-random-number-from-intels-processor-with-assembler
                    std::size_t ui32Count = bufferLength >> 2;
                    ui32 *begin = (ui32 *)buffer;
                    ui32 *end = begin + ui32Count;
                    std::size_t retries = ui32Count + 4;
                    ui32 value;
                    while (begin < end) {
                        char rc;
                    #if defined (TOOLCHAIN_OS_Windows)
                        rc = _rdseed32_step (&value);
                    #else // defined (TOOLCHAIN_OS_Windows)
                        __asm__ volatile (
                            "rdseed %0 ; setc %1"
                            : "=r" (value), "=qm" (rc));
                    #endif // defined (TOOLCHAIN_OS_Windows)
                        // 1 = success, 0 = underflow
                        if (rc == 1) {
                            *begin++ = value;
                        }
                        else if (retries-- == 0) {
                            bufferLength = (ui8 *)begin - (ui8 *)buffer;
                            break;
                        }
                    }
                    if (begin == end) {
                        std::size_t remainder = bufferLength & 3;
                        while (remainder > 0) {
                            char rc;
                        #if defined (TOOLCHAIN_OS_Windows)
                            rc = _rdseed32_step (&value);
                        #else // defined (TOOLCHAIN_OS_Windows)
                            __asm__ volatile (
                                "rdseed %0 ; setc %1"
                                : "=r" (value), "=qm" (rc));
                        #endif // defined (TOOLCHAIN_OS_Windows)
                            // 1 = success, 0 = underflow
                            if (rc == 1) {
                                memcpy (begin, &value, remainder);
                                break;
                            }
                            else if (retries-- == 0) {
                                bufferLength = (ui8 *)begin - (ui8 *)buffer;
                                break;
                            }
                        }
                    }
                    // Wipe value on exit.
                    *((volatile ui32 *)&value) = 0;
                    return bufferLength;
                }
            #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
                return 0;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        std::size_t RandomSource::GetSeedOrBytes (
                void *buffer,
                std::size_t bufferLength) {
            std::size_t seedCount = GetSeed (buffer, bufferLength);
            std::size_t byteCount = 0;
            if (seedCount < bufferLength) {
                // Seed is a scarce resource. If we couldn't get enough, backfill with random bytes.
                byteCount = GetBytes ((ui8 *)buffer + seedCount, bufferLength - seedCount);
            }
            return seedCount + byteCount;
        }

        ui32 RandomSource::Getui32 () {
            ui32 value;
            if (GetSeedOrBytes (&value, UI32_SIZE) == UI32_SIZE) {
                return value;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Unable to get " THEKOGANS_UTIL_SIZE_T_FORMAT " random bytes for value.",
                    UI32_SIZE);
            }
        }

        ui64 RandomSource::Getui64 () {
            ui64 value;
            if (GetSeedOrBytes (&value, UI64_SIZE) == UI64_SIZE) {
                return value;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Unable to get " THEKOGANS_UTIL_SIZE_T_FORMAT " random bytes for value.",
                    UI64_SIZE);
            }
        }

        std::size_t RandomSource::Getsize_t () {
            std::size_t value;
            if (GetSeedOrBytes (&value, SIZE_T_SIZE) == SIZE_T_SIZE) {
                return value;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Unable to get " THEKOGANS_UTIL_SIZE_T_FORMAT " random bytes for value.",
                    SIZE_T_SIZE);
            }
        }

    } // namespace util
} // namespace thekogans
