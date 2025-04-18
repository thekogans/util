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
#include <cctype>
#include "thekogans/util/Hash.h"
#include "thekogans/util/MD5.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/GUID.h"

namespace thekogans {
    namespace util {

        namespace {
            inline ui8 GetNumber (char ch) {
                return isdigit (ch) ? (ch - '0') : (10 + tolower (ch) - 'a');
            }

            bool VerifyGUID (const std::string &guid) {
                bool windows = guid.find_first_of ("-", 0) != std::string::npos;
                if (windows) {
                    if (guid.size () == GUID::SIZE * 2 + 4) { // +4 to account for 4 '-'.
                        std::size_t i = 0;
                        for (; i < 8; ++i) {
                            if (!isxdigit (guid[i])) {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                            }
                        }
                        if (guid[i++] != '-') {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                        }
                        for (; i < 13; ++i) {
                            if (!isxdigit (guid[i])) {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                            }
                        }
                        if (guid[i++] != '-') {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                        }
                        for (; i < 18; ++i) {
                            if (!isxdigit (guid[i])) {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                            }
                        }
                        if (guid[i++] != '-') {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                        }
                        for (; i < 23; ++i) {
                            if (!isxdigit (guid[i])) {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                            }
                        }
                        if (guid[i++] != '-') {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                        }
                        for (; i < 36; ++i) {
                            if (!isxdigit (guid[i])) {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                            }
                        }
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }
                else {
                    if (guid.size () == GUID::SIZE * 2) {
                        for (std::size_t i = 0; i < GUID::SIZE * 2; ++i) {
                            if (!isxdigit (guid[i])) {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                            }
                        }
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }
                return windows;
            }
        }

        GUID::GUID (const ui8 data_[SIZE]) {
            if (data_ != nullptr) {
                memcpy (data, data_, SIZE);
            }
            else {
                memset (data, 0, SIZE);
            }
        }

        std::string GUID::ToHexString (
                bool windows,
                bool upperCase) const {
            if (windows) {
                const char * const upperFormat =
                    "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X";
                const char * const lowerFormat =
                    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x";
                return FormatString (upperCase ? upperFormat : lowerFormat,
                    data[0], data[1], data[2], data[3],
                    data[4], data[5],
                    data[6], data[7],
                    data[8], data[9],
                    data[10], data[11], data[12], data[13], data[14], data[15]);
            }
            else {
                const char * const upperFormat =
                    "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X";
                const char * const lowerFormat =
                    "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x";
                return FormatString (upperCase ? upperFormat : lowerFormat,
                    data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
                    data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
            }
        }

        GUID GUID::FromHexString (const std::string &guid) {
            ui8 data[SIZE];
            if (!guid.empty ()) {
                bool windows = VerifyGUID (guid);
                if (windows) {
                    const char *guidPtr = guid.c_str ();
                    ui8 *dataPtr = data;
                    for (std::size_t i = 0; i < 4; ++i) {
                        *dataPtr = GetNumber (*guidPtr++) * 16;
                        *dataPtr++ += GetNumber (*guidPtr++);
                    }
                    ++guidPtr;
                    for (std::size_t i = 0; i < 2; ++i) {
                        *dataPtr = GetNumber (*guidPtr++) * 16;
                        *dataPtr++ += GetNumber (*guidPtr++);
                    }
                    ++guidPtr;
                    for (std::size_t i = 0; i < 2; ++i) {
                        *dataPtr = GetNumber (*guidPtr++) * 16;
                        *dataPtr++ += GetNumber (*guidPtr++);
                    }
                    ++guidPtr;
                    for (std::size_t i = 0; i < 2; ++i) {
                        *dataPtr = GetNumber (*guidPtr++) * 16;
                        *dataPtr++ += GetNumber (*guidPtr++);
                    }
                    ++guidPtr;
                    for (std::size_t i = 0; i < 6; ++i) {
                        *dataPtr = GetNumber (*guidPtr++) * 16;
                        *dataPtr++ += GetNumber (*guidPtr++);
                    }
                }
                else {
                    for (std::size_t i = 0; i < SIZE; ++i) {
                        data[i] = GetNumber (guid[i * 2]) * 16 + GetNumber (guid[i * 2 + 1]);
                    }
                }
            }
            else {
                memset (data, 0, SIZE);
            }
            return GUID (data);
        }

        GUID GUID::FromFile (const std::string &path) {
            Hash::Digest digest;
            {
                MD5 md5;
                md5.FromFile (path, MD5::DIGEST_SIZE_128, digest);
            }
            return GUID (digest.data ());
        }

        GUID GUID::FromBuffer (
                const void *buffer,
                std::size_t length) {
            Hash::Digest digest;
            {
                MD5 md5;
                md5.FromBuffer (buffer, length, MD5::DIGEST_SIZE_128, digest);
            }
            return GUID (digest.data ());
        }

        GUID GUID::FromRandom (std::size_t length) {
            Hash::Digest digest;
            {
                MD5 md5;
                md5.FromRandom (length, MD5::DIGEST_SIZE_128, digest);
            }
            return GUID (digest.data ());
        }

    } // namespace util
} // namespace thekogans
