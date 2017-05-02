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

#include "thekogans/util/ByteSwap.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/RandomSource.h"

namespace thekogans {
    namespace util {

        RandomSource::RandomSource () :
            #if defined (TOOLCHAIN_OS_Windows)
                cryptProv (0) {
            #else // defined (TOOLCHAIN_OS_Windows)
                // http://www.2uo.de/myths-about-urandom/
                urandom (HostEndian, "/dev/urandom") {
            #endif // defined (TOOLCHAIN_OS_Windows)
        #if defined (TOOLCHAIN_OS_Windows)
            if (!CryptAcquireContext (&cryptProv, 0, 0, PROV_RSA_FULL, 0)) {
                THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                if (errorCode != (THEKOGANS_UTIL_ERROR_CODE)NTE_BAD_KEYSET) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
                if (!CryptAcquireContext (&cryptProv, 0, 0, PROV_RSA_FULL, CRYPT_NEWKEYSET)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        RandomSource::~RandomSource () {
        #if defined (TOOLCHAIN_OS_Windows)
            CryptReleaseContext (cryptProv, 0);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        ui32 RandomSource::GetBytes (
                void *buffer,
                std::size_t count) {
            LockGuard<SpinLock> guatd (spinLock);
        #if defined (TOOLCHAIN_OS_Windows)
            if (!CryptGenRandom (cryptProv, (DWORD)count, (BYTE *)buffer)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            return (ui32)count;
        #else // defined (TOOLCHAIN_OS_Windows)
            return urandom.Read (buffer, (ui32)count);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        ui32 RandomSource::Getui32 () {
            ui32 value;
            GetBytes (&value, UI32_SIZE);
            return value;
        }

    } // namespace util
} // namespace thekogans
