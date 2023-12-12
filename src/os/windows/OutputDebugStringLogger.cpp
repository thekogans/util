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

#include "thekogans/util/os/windows/WindowsHeader.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/os/windows/OutputDebugStringLogger.h"

namespace thekogans {
    namespace util {
        namespace os {
            namespace windows {

                void OutputDebugStringLogger::Log (
                        const std::string & /*subsystem*/,
                        ui32 level,
                        const std::string &header,
                        const std::string &message) throw () {
                    if (level <= this->level && (!header.empty () || !message.empty ())) {
                        std::string logEntry = FormatString ("%s%s", header.c_str (), message.c_str ());
                        // These are log entries. There's no need to for high fidelity conversion.
                        OutputDebugStringW (UTF8ToUTF16 (logEntry, 0).c_str ());
                    }
                }

            } // namespace windows
        } // namespace os
    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Windows)
