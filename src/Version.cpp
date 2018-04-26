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

#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/WindowsUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Exception.h"
#include "thekogans/util/Version.h"

namespace thekogans {
    namespace util {

        const Version Version::Empty (0, 0, 0);

    #if defined (_MSC_VER)
        #pragma warning (push)
        #pragma warning (disable : 4996)
    #endif // defined (_MSC_VER)

        Version::Version (const std::string &value) :
                majorVersion (0),
                minorVersion (0),
                patchVersion (0) {
            if (!value.empty ()) {
                if (sscanf (value.c_str (), "%u.%u.%u",
                        &majorVersion, &minorVersion, &patchVersion) != 3) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }
        }

    #if defined (_MSC_VER)
        #pragma warning (pop)
    #endif // defined (_MSC_VER)

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
            static const Version version (
                THEKOGANS_UTIL_MAJOR_VERSION,
                THEKOGANS_UTIL_MINOR_VERSION,
                THEKOGANS_UTIL_PATCH_VERSION);
            return version;
        }

    } // namespace util
} // namespace thekogans
