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
#elif defined (TOOLCHAIN_OS_Linux)
    #include <sys/time.h>
#elif defined (TOOLCHAIN_OS_OSX)
    #include <mach/mach_time.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/HRTimer.h"

namespace thekogans {
    namespace util {

        ui64 HRTimer::GetFrequency () {
        #if defined (TOOLCHAIN_OS_Windows)
            LARGE_INTEGER frequency;
            QueryPerformanceFrequency (&frequency);
            return frequency.QuadPart;
        #elif defined (TOOLCHAIN_OS_Linux)
            return 1000000;
        #elif defined (TOOLCHAIN_OS_OSX)
            mach_timebase_info_data_t timeBaseInfoData;
            mach_timebase_info (&timeBaseInfoData);
            return 1000000000 * (ui64)timeBaseInfoData.denom / (ui64)timeBaseInfoData.numer;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        ui64 HRTimer::Click () {
        #if defined (TOOLCHAIN_OS_Windows)
            LARGE_INTEGER instant;
            QueryPerformanceCounter (&instant);
            return instant.QuadPart;
        #elif defined (TOOLCHAIN_OS_Linux)
            timeval tv;
            gettimeofday (&tv, 0);
            return tv.tv_sec * 1000000 + tv.tv_usec;
        #elif defined (TOOLCHAIN_OS_OSX)
            return mach_absolute_time ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
