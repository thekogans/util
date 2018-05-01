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
#if defined (TOOLCHAIN_OS_OSX)
    #include <mach/mach.h>
    #include <mach/clock.h>
    #include <mach/mach_time.h>
    #include <mach/kern_return.h>
    #include "thekogans/util/Singleton.h"
    #include "thekogans/util/SpinLock.h"
#elif defined (TOOLCHAIN_OS_Linux)
    #include <ctime>
#endif // defined (TOOLCHAIN_OS_OSX)
#include "thekogans/util/Constants.h"
#include "thekogans/util/TimeSpec.h"

namespace thekogans {
    namespace util {

        TimeSpec::TimeSpec (
                i64 seconds_,
                i64 nanoseconds_) {
            if (seconds_ == -1 && nanoseconds_ == -1) {
                seconds = seconds_;
                nanoseconds = (i32)nanoseconds_;
            }
            else if (seconds_ != -1 && nanoseconds_ != -1) {
                seconds = seconds_ + nanoseconds_ / 1000000000;
                nanoseconds = (i32)(nanoseconds_ % 1000000000);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    #if !defined (TOOLCHAIN_OS_Windows)
        TimeSpec::TimeSpec (const timespec &timeSpec) {
            if (timeSpec.tv_sec == -1 && timeSpec.tv_nsec == -1) {
                seconds = timeSpec.tv_sec;
                nanoseconds = (i32)timeSpec.tv_nsec;
            }
            else if (timeSpec.tv_sec != -1 && timeSpec.tv_nsec != -1) {
                seconds = timeSpec.tv_sec + timeSpec.tv_nsec / 1000000000;
                nanoseconds = (i32)(timeSpec.tv_nsec % 1000000000);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }
    #if defined (TOOLCHAIN_OS_OSX)
        TimeSpec::TimeSpec (const mach_timespec_t &timeSpec) {
            if (timeSpec.tv_sec == -1 && timeSpec.tv_nsec == -1) {
                seconds = timeSpec.tv_sec;
                nanoseconds = (i32)timeSpec.tv_nsec;
            }
            else if (timeSpec.tv_sec != -1 && timeSpec.tv_nsec != -1) {
                seconds = timeSpec.tv_sec + timeSpec.tv_nsec / 1000000000;
                nanoseconds = (i32)(timeSpec.tv_nsec % 1000000000);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }
    #endif // defined (TOOLCHAIN_OS_OSX)
    #endif // !defined (TOOLCHAIN_OS_Windows)

        TimeSpec::TimeSpec (const timeval &timeVal) {
            if (timeVal.tv_sec == -1 && timeVal.tv_usec == -1) {
                seconds = (i64)timeVal.tv_sec;
                nanoseconds = (i32)timeVal.tv_usec;
            }
            else if (timeVal.tv_sec != -1 && timeVal.tv_usec != -1) {
                seconds = (i64)(timeVal.tv_sec + timeVal.tv_usec / 1000000);
                nanoseconds = (i32)((timeVal.tv_usec % 1000000) * 1000);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        TimeSpec TimeSpec::Zero (0, 0);
        TimeSpec TimeSpec::Infinite (-1, -1);

        TimeSpec &TimeSpec::operator += (const TimeSpec &timeSpec) {
            *this = *this + timeSpec;
            return *this;
        }

        TimeSpec &TimeSpec::operator -= (const TimeSpec &timeSpec) {
            *this = *this - timeSpec;
            return *this;
        }

        TimeSpec TimeSpec::AddHours (i64 hours) const {
            return *this + FromHours (hours);
        }

        TimeSpec TimeSpec::AddMinutes (i64 minutes) const {
            return *this + FromMinutes (minutes);
        }

        TimeSpec TimeSpec::AddSeconds (i64 seconds) const {
            return *this + FromSeconds (seconds);
        }

        TimeSpec TimeSpec::AddMilliseconds (i64 milliseconds) const {
            return *this + FromMilliseconds (milliseconds);
        }

        TimeSpec TimeSpec::AddMicroseconds (i64 microseconds) const {
            return *this + FromMicroseconds (microseconds);
        }

        TimeSpec TimeSpec::AddNanoseconds (i64 nanoseconds) const {
            return *this + FromNanoseconds (nanoseconds);
        }

    #if defined (TOOLCHAIN_CONFIG_Debug)
        namespace {
            inline bool IsNormalized (const TimeSpec &timeSpec) {
                return timeSpec == TimeSpec::Infinite ||
                    (timeSpec.seconds >= 0 &&
                        timeSpec.nanoseconds >= 0 && timeSpec.nanoseconds < 1000000000);
            }
        }
    #endif // defined (TOOLCHAIN_CONFIG_Debug)

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator < (
                const TimeSpec &timeSpec1,
                const TimeSpec &timeSpec2) {
        #if defined (TOOLCHAIN_CONFIG_Debug)
            THEKOGANS_UTIL_ASSERT (IsNormalized (timeSpec1),
                FormatString (
                    THEKOGANS_UTIL_I64_FORMAT ":" THEKOGANS_UTIL_I32_FORMAT,
                    timeSpec1.seconds,
                    timeSpec1.nanoseconds));
            THEKOGANS_UTIL_ASSERT (IsNormalized (timeSpec2),
                FormatString (
                    THEKOGANS_UTIL_I64_FORMAT ":" THEKOGANS_UTIL_I32_FORMAT,
                    timeSpec2.seconds,
                    timeSpec2.nanoseconds));
        #endif // defined (TOOLCHAIN_CONFIG_Debug)
            return
                timeSpec1 == TimeSpec::Infinite ? false :
                timeSpec2 == TimeSpec::Infinite ? true :
                timeSpec1.seconds < timeSpec2.seconds ||
                (timeSpec1.seconds == timeSpec2.seconds &&
                    timeSpec1.nanoseconds < timeSpec2.nanoseconds);
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator > (
                const TimeSpec &timeSpec1,
                const TimeSpec &timeSpec2) {
        #if defined (TOOLCHAIN_CONFIG_Debug)
            THEKOGANS_UTIL_ASSERT (IsNormalized (timeSpec1),
                FormatString (
                    THEKOGANS_UTIL_I64_FORMAT ":" THEKOGANS_UTIL_I32_FORMAT,
                    timeSpec1.seconds,
                    timeSpec1.nanoseconds));
            THEKOGANS_UTIL_ASSERT (IsNormalized (timeSpec2),
                FormatString (
                    THEKOGANS_UTIL_I64_FORMAT ":" THEKOGANS_UTIL_I32_FORMAT,
                    timeSpec2.seconds,
                    timeSpec2.nanoseconds));
        #endif // defined (TOOLCHAIN_CONFIG_Debug)
            return
                timeSpec2 == TimeSpec::Infinite ? false :
                timeSpec1 == TimeSpec::Infinite ? true :
                timeSpec1.seconds > timeSpec2.seconds ||
                (timeSpec1.seconds == timeSpec2.seconds &&
                    timeSpec1.nanoseconds > timeSpec2.nanoseconds);
        }

        namespace {
            inline TimeSpec AddWithCarry (
                    const TimeSpec &timeSpec1,
                    const TimeSpec &timeSpec2) {
                i64 seconds = timeSpec1.seconds + timeSpec2.seconds;
                i32 nanoseconds = timeSpec1.nanoseconds + timeSpec2.nanoseconds;
                if (nanoseconds > 999999999) {
                    seconds += nanoseconds / 1000000000;
                    nanoseconds %= 1000000000;
                }
                return TimeSpec (seconds, nanoseconds);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL TimeSpec _LIB_THEKOGANS_UTIL_API operator + (
                const TimeSpec &timeSpec1,
                const TimeSpec &timeSpec2) {
        #if defined (TOOLCHAIN_CONFIG_Debug)
            THEKOGANS_UTIL_ASSERT (IsNormalized (timeSpec1),
                FormatString (
                    THEKOGANS_UTIL_I64_FORMAT ":" THEKOGANS_UTIL_I32_FORMAT,
                    timeSpec1.seconds,
                    timeSpec1.nanoseconds));
            THEKOGANS_UTIL_ASSERT (IsNormalized (timeSpec2),
                FormatString (
                    THEKOGANS_UTIL_I64_FORMAT ":" THEKOGANS_UTIL_I32_FORMAT,
                    timeSpec2.seconds,
                    timeSpec2.nanoseconds));
        #endif // defined (TOOLCHAIN_CONFIG_Debug)
            return
                timeSpec1 == TimeSpec::Infinite || timeSpec2 == TimeSpec::Infinite ?
                    TimeSpec::Infinite : AddWithCarry (timeSpec1, timeSpec2);
        }

        namespace {
            inline TimeSpec SubWithBorrow (
                    const TimeSpec &timeSpec1,
                    const TimeSpec &timeSpec2) {
                i64 seconds = timeSpec1.seconds - timeSpec2.seconds;
                i32 nanoseconds = timeSpec1.nanoseconds - timeSpec2.nanoseconds;
                if (nanoseconds < 0) {
                    --seconds;
                    assert (seconds >= 0);
                    nanoseconds += 1000000000;
                }
                return TimeSpec (seconds, nanoseconds);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL TimeSpec _LIB_THEKOGANS_UTIL_API operator - (
                const TimeSpec &timeSpec1,
                const TimeSpec &timeSpec2) {
        #if defined (TOOLCHAIN_CONFIG_Debug)
            THEKOGANS_UTIL_ASSERT (IsNormalized (timeSpec1),
                FormatString (
                    THEKOGANS_UTIL_I64_FORMAT ":" THEKOGANS_UTIL_I32_FORMAT,
                    timeSpec1.seconds,
                    timeSpec1.nanoseconds));
            THEKOGANS_UTIL_ASSERT (IsNormalized (timeSpec2),
                FormatString (
                    THEKOGANS_UTIL_I64_FORMAT ":" THEKOGANS_UTIL_I32_FORMAT,
                    timeSpec2.seconds,
                    timeSpec2.nanoseconds));
        #endif // defined (TOOLCHAIN_CONFIG_Debug)
            return
                timeSpec1 < timeSpec2 ?
                    TimeSpec::Zero :
                    timeSpec1 == TimeSpec::Infinite && timeSpec2 == TimeSpec::Infinite ?
                        TimeSpec::Infinite :
                        SubWithBorrow (timeSpec1, timeSpec2);
        }

        _LIB_THEKOGANS_UTIL_DECL TimeSpec _LIB_THEKOGANS_UTIL_API GetCurrentTime () {
        #if defined (TOOLCHAIN_OS_Windows)
            SYSTEMTIME systemTime;
            union {
                ui64 ul;
                FILETIME fileTime;
            } now;
            GetSystemTime (&systemTime);
            SystemTimeToFileTime (&systemTime, &now.fileTime);
            // re-bias to 1/1/1970
            now.ul -= THEKOGANS_UTIL_UI64_LITERAL (116444736000000000);
            return TimeSpec (
                (i64)(now.ul / THEKOGANS_UTIL_UI64_LITERAL (10000000)),
                (i32)(systemTime.wMilliseconds * 1000));
        #elif defined (TOOLCHAIN_OS_Linux)
            timespec timeSpec;
            if (clock_gettime (CLOCK_REALTIME, &timeSpec) != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            return TimeSpec (timeSpec);
        #elif defined (TOOLCHAIN_OS_OSX)
            struct ClockServer : public Singleton<ClockServer, SpinLock> {
                clock_serv_t calendarClockService;
                ClockServer () {
                    kern_return_t errorCode =
                        host_get_clock_service (
                            mach_host_self (),
                            CALENDAR_CLOCK,
                            &calendarClockService);
                    if (errorCode != KERN_SUCCESS) {
                        THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
                ~ClockServer () {
                    // For completeness only. Since ClockServer is a
                    // private singleton, it's dtor should never be
                    // called.
                    assert (0);
                    mach_port_deallocate (mach_task_self (), calendarClockService);
                }
                void GetTime (mach_timespec_t &machTimeSpec) {
                    kern_return_t errorCode =
                        clock_get_time (calendarClockService, &machTimeSpec);
                    if (errorCode != KERN_SUCCESS) {
                        THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
            };
            mach_timespec_t machTimeSpec;
            ClockServer::Instance ().GetTime (machTimeSpec);
            return TimeSpec (machTimeSpec.tv_sec, machTimeSpec.tv_nsec);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        _LIB_THEKOGANS_UTIL_DECL void _LIB_THEKOGANS_UTIL_API Sleep (const TimeSpec &timeSpec) {
            if (timeSpec != TimeSpec::Infinite) {
            #if defined (TOOLCHAIN_OS_Windows)
                ::Sleep ((DWORD)timeSpec.ToMilliseconds ());
            #else // defined (TOOLCHAIN_OS_Windows)
                timespec required = {timeSpec.seconds, timeSpec.nanoseconds};
                timespec remainder;
                while (nanosleep (&required, &remainder) != 0) {
                    THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                    if (errorCode != EINTR) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                    required = remainder;
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    } // namespace util
} // namespace thekogans
