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

#include <string>
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Version.h"

namespace thekogans {
    namespace util {

        Version::Version (const std::string &value) :
                majorVersion (0),
                minorVersion (0),
                patchVersion (0) {
            if (!value.empty ()) {
                std::size_t i = 0;
                auto ExtractVersion = [&] (ui32 &version) {
                    // Any integer bigger than 10 digits would overflow a ui32.
                    const std::size_t MAX_VERSION_SIZE = 10;
                    // +1 for '\0' terminator.
                    char buffer[MAX_VERSION_SIZE + 1];
                    std::size_t index = 0;
                    while (i < value.size () && index < MAX_VERSION_SIZE) {
                        if (value[i] == '.') {
                            ++i;
                            break;
                        }
                        else if (isdigit (value[i])) {
                            buffer[index++] = value[i++];
                        }
                        else {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "Unrecognized version: %s, should be [%u[.%u[.%u]]]", value.c_str ());
                        }
                    }
                    buffer[index] = '\0';
                    version = stringToui32 (buffer);
                };
                ExtractVersion (majorVersion);
                ExtractVersion (minorVersion);
                ExtractVersion (patchVersion);
            }
        }

        void Version::IncMajorVersion () {
            ++majorVersion;
            minorVersion = 0;
            patchVersion = 0;
        }

        void Version::IncMinorVersion () {
            ++minorVersion;
            patchVersion = 0;
        }

        void Version::IncPatchVersion () {
            ++patchVersion;
        }

        int Version::Compare (const Version &version) const {
            return
                majorVersion < version.majorVersion ? -1 :
                majorVersion > version.majorVersion ? 1 :
                minorVersion < version.minorVersion ? -1 :
                minorVersion > version.minorVersion ? 1 :
                patchVersion < version.patchVersion ? -1 :
                patchVersion > version.patchVersion ? 1 : 0;
        }

        bool Version::SatisfiesConstraint (
                OP op,
                const Version &version) const {
            if (op == NOP || IsEmpty () || version.IsEmpty ()) {
                return true;
            }
            int result = Compare (version);
            return op == EQ ? result == 0 :
                op == NEQ ? result != 0 :
                op == GEQ ? result >= 0 :
                op == LEQ ? result <= 0 :
                op == GT ? result > 0 :
                op == LT ? result < 0 : false;
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator == (
                const Version &version1,
                const Version &version2) {
            return
                version1.majorVersion == version2.majorVersion &&
                version1.minorVersion == version2.minorVersion &&
                version1.patchVersion == version2.patchVersion;
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator != (
                const Version &version1,
                const Version &version2) {
            return
                version1.majorVersion != version2.majorVersion ||
                version1.minorVersion != version2.minorVersion ||
                version1.patchVersion != version2.patchVersion;
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator < (
                const Version &version1,
                const Version &version2) {
            return
                version1.majorVersion < version2.majorVersion ||
                (version1.majorVersion == version2.majorVersion &&
                    version1.minorVersion < version2.minorVersion) ||
                (version1.majorVersion == version2.majorVersion &&
                    version1.minorVersion == version2.minorVersion &&
                    version1.patchVersion < version2.patchVersion);
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator > (
                const Version &version1,
                const Version &version2) {
            return
                version1.majorVersion > version2.majorVersion ||
                (version1.majorVersion == version2.majorVersion &&
                    version1.minorVersion > version2.minorVersion) ||
                (version1.majorVersion == version2.majorVersion &&
                    version1.minorVersion == version2.minorVersion &&
                    version1.patchVersion > version2.patchVersion);
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator <= (
                const Version &version1,
                const Version &version2) {
            return
                version1.majorVersion <= version2.majorVersion ||
                (version1.majorVersion == version2.majorVersion &&
                    version1.minorVersion <= version2.minorVersion) ||
                (version1.majorVersion == version2.majorVersion &&
                    version1.minorVersion == version2.minorVersion &&
                    version1.patchVersion <= version2.patchVersion);
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator >= (
                const Version &version1,
                const Version &version2) {
            return
                version1.majorVersion >= version2.majorVersion ||
                (version1.majorVersion == version2.majorVersion &&
                    version1.minorVersion >= version2.minorVersion) ||
                (version1.majorVersion == version2.majorVersion &&
                    version1.minorVersion == version2.minorVersion &&
                    version1.patchVersion >= version2.patchVersion);
        }

        _LIB_THEKOGANS_UTIL_DECL const Version & _LIB_THEKOGANS_UTIL_API GetVersion () {
            static const Version *version = new Version (
                THEKOGANS_UTIL_MAJOR_VERSION,
                THEKOGANS_UTIL_MINOR_VERSION,
                THEKOGANS_UTIL_PATCH_VERSION);
            return *version;
        }

    } // namespace util
} // namespace thekogans
