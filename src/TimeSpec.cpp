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
#include "thekogans/util/Environment.h"
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
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/TimeSpec.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (TimeSpec, 1)

        TimeSpec::TimeSpec (
                i64 seconds_,
                i64 nanoseconds_) {
            if (seconds_ == -1 && nanoseconds_ == -1) {
                seconds = seconds_;
                nanoseconds = (i32)nanoseconds_;
            }
            else if (seconds_ >= 0 && nanoseconds_ >= 0) {
                seconds = seconds_ + nanoseconds_ / 1000000000;
                nanoseconds = (i32)(nanoseconds_ % 1000000000);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    #if defined (TOOLCHAIN_OS_Windows)
        TimeSpec::TimeSpec (const SYSTEMTIME &systemTime) {
            FILETIME fileTime;
            SystemTimeToFileTime (&systemTime, &fileTime);
            seconds = os::windows::FILETIMEToi64 (fileTime) + systemTime.wMilliseconds / 1000;
            nanoseconds = (i32)((systemTime.wMilliseconds % 1000) * 1000000);
        }
    #else // defined (TOOLCHAIN_OS_Windows)
        TimeSpec::TimeSpec (const timespec &timeSpec) {
            if (timeSpec.tv_sec == -1 && timeSpec.tv_nsec == -1) {
                seconds = timeSpec.tv_sec;
                nanoseconds = (i32)timeSpec.tv_nsec;
            }
            else if (timeSpec.tv_sec >= 0 && timeSpec.tv_nsec >= 0) {
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
            else if (timeSpec.tv_sec > 0 && timeSpec.tv_nsec >= 0) {
                seconds = timeSpec.tv_sec + timeSpec.tv_nsec / 1000000000;
                nanoseconds = (i32)(timeSpec.tv_nsec % 1000000000);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }
    #endif // defined (TOOLCHAIN_OS_OSX)
    #endif // defined (TOOLCHAIN_OS_Windows)

        TimeSpec::TimeSpec (const timeval &timeVal) {
            if (timeVal.tv_sec == -1 && timeVal.tv_usec == -1) {
                seconds = (i64)timeVal.tv_sec;
                nanoseconds = (i32)timeVal.tv_usec;
            }
            else if (timeVal.tv_sec >= 0 && timeVal.tv_usec >= 0) {
                seconds = (i64)(timeVal.tv_sec + timeVal.tv_usec / 1000000);
                nanoseconds = (i32)((timeVal.tv_usec % 1000000) * 1000);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        const TimeSpec TimeSpec::Zero (0, 0);
        const TimeSpec TimeSpec::Infinite (-1, -1);

        TimeSpec &TimeSpec::operator = (const TimeSpec &timeSpec) {
            if (&timeSpec != this) {
                seconds = timeSpec.seconds;
                nanoseconds = timeSpec.nanoseconds;
            }
            return *this;
        }

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

        std::size_t TimeSpec::Size () const {
            return Serializer::Size (seconds) + Serializer::Size (nanoseconds);
        }

        void TimeSpec::Read (
                const BinHeader & /*header*/,
                Serializer &serializer) {
            serializer >> seconds >> nanoseconds;
        }

        void TimeSpec::Write (Serializer &serializer) const {
            serializer << seconds << nanoseconds;
        }

        const char * const TimeSpec::TAG_TIME_SPEC = "TimeSpec";
        const char * const TimeSpec::ATTR_SECONDS = "Seconds";
        const char * const TimeSpec::ATTR_NANOSECONDS = "Nanoseconds";

        void TimeSpec::Read (
                const TextHeader & /*header*/,
                const pugi::xml_node &node) {
            seconds = stringToui64 (node.attribute (ATTR_SECONDS).value ());
            nanoseconds = stringToi32 (node.attribute (ATTR_NANOSECONDS).value ());
        }

        void TimeSpec::Write (pugi::xml_node &node) const {
            node.append_attribute (ATTR_SECONDS).set_value (i64Tostring (seconds).c_str ());
            node.append_attribute (ATTR_NANOSECONDS).set_value (i32Tostring (nanoseconds).c_str ());
        }

        void TimeSpec::Read (
                const TextHeader & /*header*/,
                const JSON::Object &object) {
            seconds = object.Get<JSON::Number> (ATTR_SECONDS)->To<i64> ();
            nanoseconds = object.Get<JSON::Number> (ATTR_NANOSECONDS)->To<i32> ();
        }

        void TimeSpec::Write (JSON::Object &object) const {
            object.Add (ATTR_SECONDS, seconds);
            object.Add (ATTR_NANOSECONDS, nanoseconds);
        }

    #if defined (THEKOGANS_UTIL_CONFIG_Debug)
        namespace {
            inline bool IsNormalized (const TimeSpec &timeSpec) {
                return timeSpec == TimeSpec::Infinite ||
                    (timeSpec.seconds >= 0 &&
                        timeSpec.nanoseconds >= 0 && timeSpec.nanoseconds < 1000000000);
            }
        }
    #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator < (
                const TimeSpec &timeSpec1,
                const TimeSpec &timeSpec2) {
        #if defined (THEKOGANS_UTIL_CONFIG_Debug)
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
        #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
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
        #if defined (THEKOGANS_UTIL_CONFIG_Debug)
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
        #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
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
        #if defined (THEKOGANS_UTIL_CONFIG_Debug)
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
        #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
            return
                timeSpec1 == TimeSpec::Infinite || timeSpec2 == TimeSpec::Infinite ?
                    TimeSpec::Infinite :
                    AddWithCarry (timeSpec1, timeSpec2);
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
        #if defined (THEKOGANS_UTIL_CONFIG_Debug)
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
        #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
            return
                timeSpec1 < timeSpec2 ?
                    TimeSpec::Zero :
                    timeSpec1 == TimeSpec::Infinite ?
                        timeSpec2 == TimeSpec::Infinite ?
                            TimeSpec::Zero :
                            TimeSpec::Infinite :
                        SubWithBorrow (timeSpec1, timeSpec2);
        }

        _LIB_THEKOGANS_UTIL_DECL TimeSpec _LIB_THEKOGANS_UTIL_API GetCurrentTime () {
        #if defined (TOOLCHAIN_OS_Windows)
            SYSTEMTIME systemTime;
            GetSystemTime (&systemTime);
            return TimeSpec (systemTime);
        #elif defined (TOOLCHAIN_OS_Linux)
            timespec timeSpec;
            if (clock_gettime (CLOCK_REALTIME, &timeSpec) != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            return TimeSpec (timeSpec);
        #elif defined (TOOLCHAIN_OS_OSX)
            struct MachClock {
            private:
                clock_serv_t serv;
            public:
                explicit MachClock (clock_id_t id) {
                    kern_return_t errorCode =
                        host_get_clock_service (mach_host_self (), id, &serv);
                    if (errorCode != KERN_SUCCESS) {
                        THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
                ~MachClock () {
                    mach_port_deallocate (mach_task_self (), serv);
                }
                void GetTime (mach_timespec_t &machTimeSpec) {
                    kern_return_t errorCode = clock_get_time (serv, &machTimeSpec);
                    if (errorCode != KERN_SUCCESS) {
                        THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
            } machClock (CALENDAR_CLOCK);
            mach_timespec_t machTimeSpec;
            machClock.GetTime (machTimeSpec);
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

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API FormatTimeSpec (
                const TimeSpec &timeSpec,
                const char *format) {
            if (format != nullptr) {
                time_t rawTime = (time_t)timeSpec.seconds;
                struct tm tm;
            #if defined (TOOLCHAIN_OS_Windows)
                localtime_s (&tm, &rawTime);
            #else // defined (TOOLCHAIN_OS_Windows)
                localtime_r (&rawTime, &tm);
            #endif // defined (TOOLCHAIN_OS_Windows)
                char dateTime[101] = {0};
                strftime (dateTime, 100, format, &tm);
                return dateTime;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    } // namespace util
} // namespace thekogans
