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
            std::string::size_type start = 0;
            std::string::size_type end = value.find_first_of (".", start);
            if (end != std::string::npos) {
                majorVersion = stringToui32 (value.substr (start, end - start).c_str ());
                start = end + 1;
                end = value.find_first_of (".", start);
                if (end != std::string::npos) {
                    patchVersion = stringToui32 (value.substr (start, end - start).c_str ());
                    start = end + 1;
                    patchVersion = stringToui32 (value.substr (start).c_str ());
                }
                else {
                    minorVersion = stringToui32 (value.substr (start).c_str ());
                    patchVersion = 0;
                }
            }
            else {
                majorVersion = stringToui32 (value.substr (start).c_str ());
                minorVersion = 0;
                patchVersion = 0;
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
