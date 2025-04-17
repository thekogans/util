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

#if !defined (__thekogans_util_Version_h)
#define __thekogans_util_Version_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/StringUtils.h"

namespace thekogans {
    namespace util {

        /// \struct Version Version.h thekogans/util/Version.h
        ///
        /// \brief
        /// Version provides runtime access to major, minor and patch
        /// versions found in the project's thekognas_make.xml file.
        ///
        /// major_version=digit(s)
        /// minor_version=digit(s)
        /// patch_version=digit(s)
        ///
        /// thekogans.net build system exposes these numbers through
        /// the compiler command line like this:
        ///
        /// -D$(ORGANIZATION)_$(PROJECT)_MAJOR_VERSION=$(major_version)
        /// -D$(ORGANIZATION)_$(PROJECT)_MINOR_VERSION=$(minor_version)
        /// -D$(ORGANIZATION)_$(PROJECT)_PATCH_VERSION=$(patch_version)
        ///
        /// Version gives your code (and others) the ability to
        /// query the version number that the project was built
        /// with. To do that, do the following in a public header:
        ///
        /// \code{.cpp}
        /// // Replace ORGANIZATION and PROJECT with something appropriate.
        /// _LIB_ORGANIZATION_PROJECT_DECL const thekogans::util::Version &
        /// _LIB_ORGANIZATION_PROJECT_API GetVersion ();
        /// \endcode
        ///
        /// In the compilation unit, do this:
        ///
        /// \code{.cpp}
        /// // Replace ORGANIZATION and PROJECT with something appropriate.
        /// _LIB_ORGANIZATION_PROJECT_DECL const thekogans::util::Version &
        /// _LIB_ORGANIZATION_PROJECT_API GetVersion () {
        ///     static const thekogans::util::Version version (
        ///         ORGANIZATION_PROJECT_MAJOR_VERSION,
        ///         ORGANIZATION_PROJECT_MINOR_VERSION,
        ///         ORGANIZATION_PROJECT_PATCH_VERSION);
        ///     return version;
        /// }
        /// \endcode

        struct _LIB_THEKOGANS_UTIL_DECL Version {
            /// \brief
            /// Project major version.
            ui32 majorVersion;
            /// \brief
            /// Project minor version.
            ui32 minorVersion;
            /// \brief
            /// Project patch version.
            ui32 patchVersion;

            /// \brief
            /// ctor.
            /// \param[in] majorVersion_ Project major version.
            /// \param[in] minorVersion_ Project minor version.
            /// \param[in] patchVersion_ Project patch version.
            Version (
                ui32 majorVersion_ = 0,
                ui32 minorVersion_ = 0,
                ui32 patchVersion_ = 0) :
                majorVersion (majorVersion_),
                minorVersion (minorVersion_),
                patchVersion (patchVersion_) {}
            /// \brief
            /// ctor.
            /// \param[in] value String representation of a version (major.minor.patch).
            explicit Version (const std::string &value);

            /// \brief
            /// Return the serialized size of this version.
            /// \return Serialized size of this version.
            inline std::size_t Size () const {
                return
                    Serializer::Size (majorVersion) +
                    Serializer::Size (minorVersion) +
                    Serializer::Size (patchVersion);
            }

            /// \brief
            /// Increment majorVersion and set minorVersion and patchVersion to 0.
            void IncMajorVersion ();
            /// \brief
            /// Increment minorVersion and set patchVersion to 0.
            void IncMinorVersion ();
            /// \brief
            /// Increment patchVersion.
            void IncPatchVersion ();

            /// \brief
            /// Return the canonical version string this project was compiled with.
            /// \return The canonical Version string this project was compiled with.
            inline std::string ToString () const {
                return FormatString ("%u.%u.%u", majorVersion, minorVersion, patchVersion);
            }
        };

        /// \brief
        /// Serialized Version size.
        const std::size_t VERSION_SIZE = UI32_SIZE + UI32_SIZE + UI32_SIZE;

        /// \brief
        /// Compare two versions for equality.
        /// \param[in] version1 First version to compare.
        /// \param[in] version2 Second version to compare.
        /// \return true = equal, false = not equal.
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator == (
            const Version &version1,
            const Version &version2);
        /// \brief
        /// Compare two versions for inequality.
        /// \param[in] version1 First version to compare.
        /// \param[in] version2 Second version to compare.
        /// \return true = not equal, false = equal.
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator != (
            const Version &version1,
            const Version &version2);
        /// \brief
        /// Compare two versions for order.
        /// \param[in] version1 First version to compare.
        /// \param[in] version2 Second version to compare.
        /// \return true = version1 is older than version2,
        /// false = version1 is newer than or equal to version2.
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator < (
            const Version &version1,
            const Version &version2);
        /// \brief
        /// Compare two versions for order.
        /// \param[in] version1 First version to compare.
        /// \param[in] version2 Second version to compare.
        /// \return true = version1 is newer than version2,
        /// false = version1 is older than or equal to version2.
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator > (
            const Version &version1,
            const Version &version2);
        /// \brief
        /// Compare two versions for order.
        /// \param[in] version1 First version to compare.
        /// \param[in] version2 Second version to compare.
        /// \return true = version1 is older than or equal to version2,
        /// false = version1 is newer than version2.
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator <= (
            const Version &version1,
            const Version &version2);
        /// \brief
        /// Compare two versions for order.
        /// \param[in] version1 First version to compare.
        /// \param[in] version2 Second version to compare.
        /// \return true = version1 is newer than or equal to version2,
        /// false = version1 is older than version2.
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator >= (
            const Version &version1,
            const Version &version2);

        /// \brief
        /// Write the given version to the given serializer.
        /// \param[in] serializer Where to write the given version.
        /// \param[in] version Version to write.
        /// \return serializer.
        inline Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const Version &version) {
            return serializer <<
                version.majorVersion <<
                version.minorVersion <<
                version.patchVersion;
        }

        /// \brief
        /// Read a version from the given serializer.
        /// \param[in] serializer Where to read the version from.
        /// \param[out] version Version to read.
        /// \return serializer.
        inline Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Version &version) {
            return serializer >>
                version.majorVersion >>
                version.minorVersion >>
                version.patchVersion;
        }

        /// \brief
        /// Return the compiled util version.
        /// \return The compiled util version.
        _LIB_THEKOGANS_UTIL_DECL const Version & _LIB_THEKOGANS_UTIL_API GetVersion ();

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Version_h)
