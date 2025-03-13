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

#if defined (TOOLCHAIN_OS_OSX)

#include <CoreFoundation/CoreFoundation.h>
#include "thekogans/util/os/osx/NSLogLogger.h"

#if __cplusplus
extern "C" {
#endif // __cplusplus

    void NSLog (CFStringRef format, ...);

#if __cplusplus
}
#endif // __cplusplus

namespace thekogans {
    namespace util {
        namespace os {
            namespace osx {

                THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (
                    thekogans::util::os::osx::NSLogLogger,
                    Logger::TYPE)

                void NSLogLogger::Log (
                        const std::string & /*subsystem*/,
                        ui32 level,
                        const std::string &header,
                        const std::string &message) noexcept {
                    if (level <= this->level && (!header.empty () || !message.empty ())) {
                        NSLog (CFSTR ("%s%s"), header.c_str (), message.c_str ());
                    }
                }

            } // namespace osx
        } // namespace os
    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_OSX)
