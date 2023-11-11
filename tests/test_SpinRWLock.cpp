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

#include <cstdlib>
#include <string>
#include <CppUnitXLite/CppUnitXLite.cpp>
#include "thekogans/util/Types.h"
#include "thekogans/util/Version.h"

using namespace thekogans;

TEST (thekogans, Version) {
    const util::Version &version = util::GetVersion ();
    // The build system exposes the versions numbers through macros
    // on the compiler command line:
    // -D$(ORGANIZATION)_$(PROJECT)_MAJOR_VERSION=$(major_version)
    // -D$(ORGANIZATION)_$(PROJECT)_MINOR_VERSION=$(minor_version)
    // -D$(ORGANIZATION)_$(PROJECT)_PATCH_VERSION=$(patch_version)
    //
    // For the test to work, the test Version file must be a copy
    // of the util Version file
    //
    // This may seem silly but it actually verifies
    // the library is loadable and the build environment
    // is correctly configured
    CHECK_EQUAL (static_cast<util::ui32> (THEKOGANS_UTIL_MAJOR_VERSION), version.majorVersion);
    CHECK_EQUAL (static_cast<util::ui32> (THEKOGANS_UTIL_MINOR_VERSION), version.minorVersion);
    CHECK_EQUAL (static_cast<util::ui32> (THEKOGANS_UTIL_PATCH_VERSION), version.patchVersion);
}

// Following the Clean Code recommendation of one
// concept per test block.
// Also checks that invoking the functions more
// than once doesn't cause side-effects
TEST (thekogans, VersionToString) {
    #define expand(s) #s
    #define stringize(s) expand (s)
    #define VERSIONSTRING\
        stringize (THEKOGANS_UTIL_MAJOR_VERSION) "."\
        stringize (THEKOGANS_UTIL_MINOR_VERSION) "."\
        stringize (THEKOGANS_UTIL_PATCH_VERSION)
    CHECK_EQUAL (VERSIONSTRING, util::GetVersion ().ToString ().c_str ());
}

TESTMAIN
