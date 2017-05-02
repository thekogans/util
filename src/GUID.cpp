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
#include "thekogans/util/RandomSource.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/GUID.h"

namespace thekogans {
    namespace util {

        namespace {
            const ui8 _data_[GUID::LENGTH] = {
                UI8_MAX, UI8_MAX, UI8_MAX, UI8_MAX, UI8_MAX, UI8_MAX, UI8_MAX, UI8_MAX,
                UI8_MAX, UI8_MAX, UI8_MAX, UI8_MAX, UI8_MAX, UI8_MAX, UI8_MAX, UI8_MAX
            };

            inline ui8 GetNumber (char ch) {
                return isdigit (ch) ? (ch - '0') : (10 + tolower (ch) - 'a');
            }

            void VerifyGUID (
                    const std::string &guid,
                    bool windowsGUID) {
                if (windowsGUID) {
                    if (guid.size () == GUID::LENGTH * 2 + 4) { // +4 to account for 4 '-'.
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
                    if (guid.size () == GUID::LENGTH * 2) {
                        for (std::size_t i = 0; i < GUID::LENGTH * 2; ++i) {
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
            }
        }

        const GUID GUID::Empty (_data_);

        GUID::GUID (
                const std::string &guid,
                bool windowsGUID) {
            if (!guid.empty ()) {
                VerifyGUID (guid, windowsGUID);
                if (windowsGUID) {
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
                    for (std::size_t i = 0; i < LENGTH; ++i) {
                        data[i] = GetNumber (guid[i * 2]) * 16 + GetNumber (guid[i * 2 + 1]);
                    }
                }
            }
            else {
                memcpy (data, _data_, LENGTH);
            }
        }

        std::string GUID::ToString (bool upperCase) const {
            const char * const upperFormat =
                "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X";
            const char * const lowerFormat =
                "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x";
            return FormatString (upperCase ? upperFormat : lowerFormat,
                data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
                data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
        }

        std::string GUID::ToWindowsGUIDString (bool upperCase) const {
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

        GUID GUID::FromFile (const std::string &path) {
            Hash::Digest digest;
            {
                MD5 md5;
                md5.FromFile (path, MD5::DIGEST_SIZE_128, digest);
            }
            return GUID (&digest[0]);
        }

        GUID GUID::FromBuffer (
                const void *buffer,
                std::size_t length) {
            Hash::Digest digest;
            {
                MD5 md5;
                md5.FromBuffer ((const ui8 *)buffer, length, MD5::DIGEST_SIZE_128, digest);
            }
            return GUID (&digest[0]);
        }

        GUID GUID::FromRandom () {
            GUID guid;
            GlobalRandomSource::Instance ().GetBytes (guid.data, LENGTH);
            return guid;
        }

    } // namespace util
} // namespace thekogans
