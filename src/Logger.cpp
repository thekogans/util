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
#include "thekogans/util/Logger.h"
#if defined (THEKOGANS_UTIL_TYPE_Static)
    #include "thekogans/util/ConsoleLogger.h"
    //#include "thekogans/util/FileLogger.h"
    #include "thekogans/util/MemoryLogger.h"
    #include "thekogans/util/NullLogger.h"
    #if defined (TOOLCHAIN_OS_Windows)
        #include "thekogans/util/os/windows/OutputDebugStringLogger.h"
    #elif defined (TOOLCHAIN_OS_OSX)
        #include "thekogans/util/os/osx/NSLogLogger.h"
    #endif // defined (TOOLCHAIN_OS_Windows)
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE (Logger)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void Logger::StaticInit () {
            ConsoleLogger::StaticInit ();
            //FileLogger::StaticInit ();
            MemoryLogger::StaticInit ();
            NullLogger::StaticInit ();
        #if defined (TOOLCHAIN_OS_Windows)
            os::windows::OutputDebugStringLogger::StaticInit ();
        #elif defined (TOOLCHAIN_OS_OSX)
            os::osx::NSLogLogger::StaticInit ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

    } // namespace util
} // namespace thekogans
