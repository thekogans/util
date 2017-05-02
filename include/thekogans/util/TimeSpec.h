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

#if !defined (__thekogans_util_TimeSpec_h)
#define __thekogans_util_TimeSpec_h

#if defined (TOOLCHAIN_OS_Windows)
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
    #endif // !defined (_WINDOWS_)
    #include <winsock2.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <ctime>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        /// \struct TimeSpec TimeSpec.h thekogans/util/TimeSpec.h
        ///
        /// \brief
        /// TimeSpec encapsulates the interval used by Windows/POSIX
        /// apis. It makes it easier to specify various threading, and
        /// synchronization apis (Condition.h, Timer.h).
        ///
        /// NOTE: Windows timeout interval is a relative millisecond
        /// value and is usually provided using a DWORD. Because we
        /// need to handle both relative and absolute (like calculating
        /// a deadline using GetCurrentTime () + interval) intervals,
        /// it's important that we can handle future time. DWORD does
        /// not have enough bits to handle absolute time.
        ///
        /// IMPORTANT: TimeSpec's domain is:
        /// TimeSpec::Zero <= timeSpec <= TimeSpec::Infinity.
        /// To maintain that, the operators + and - (below),
        /// do range checking on their arguments, and clamp
        /// the result accordingly.

    #if defined (TOOLCHAIN_OS_Windows)
        /// \struct timespec TimeSpec.h thekogans/util/TimeSpec.h
        ///
        /// \brief
        /// timespec is missing on Windows.
        struct timespec {
            /// \brief
            /// seconds.
            time_t tv_sec;
            /// \brief
            /// nanoseconds.
            long tv_nsec;
        };
    #endif // defined (TOOLCHAIN_OS_Windows)

        struct _LIB_THEKOGANS_UTIL_DECL TimeSpec : public timespec {
            /// \brief
            /// ctor. Init to Infinite.
            /// \param[in] sec Seconds value to initialize to.
            /// \param[in] nsec Nanoseconds value to initialize to.
            explicit TimeSpec (
                time_t sec = -1,
                long nsec = -1);
            /// \brief
            /// ctor.
            /// \param[in] timeSpec POSIX timespec to initialize to.
            TimeSpec (const timespec &timeSpec);

            /// \brief
            /// Zero
            /// NOTE: TimeSpec::Zero is our Big Bang. To ask what time
            /// it is before TimeSpec::Zero is equivalent to asking what
            /// time it was before the Big Bang. Neither question makes
            /// much sense.
            static TimeSpec Zero;
            /// \brief
            /// Infinite
            static TimeSpec Infinite;

            /// \brief
            /// Create a TimeSpec from hours.
            /// \param[in] hours Value to use to initialize the TimeSpec.
            /// \return TimeSpec initialized to given value.
            static TimeSpec FromHours (time_t hours) {
                if (hours < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
                return TimeSpec (hours * 3600, 0);
            }
            /// \brief
            /// Create a TimeSpec from seconds.
            /// \param[in] seconds Value to use to initialize the TimeSpec.
            /// \return TimeSpec initialized to given value.
            static TimeSpec FromSeconds (time_t seconds) {
                if (seconds < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
                return TimeSpec (seconds, 0);
            }
            /// \brief
            /// Create a TimeSpec from milliseconds.
            /// \param[in] milliseconds Value to use to initialize the TimeSpec.
            /// \return TimeSpec initialized to given value.
            static TimeSpec FromMilliseconds (time_t milliseconds) {
                if (milliseconds < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
                return TimeSpec (milliseconds / 1000, (milliseconds % 1000) * 1000000);
            }
            /// \brief
            /// Create a TimeSpec from timeval.
            /// \param[in] timeval Value to use to initialize the TimeSpec.
            /// \return TimeSpec initialized to given value.
            static TimeSpec Fromtimeval (const timeval &timeVal) {
                if (timeVal.tv_sec < 0 || timeVal.tv_usec < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
                return TimeSpec (timeVal.tv_sec, timeVal.tv_usec * 1000);
            }

            /// \brief
            /// Add and assign operator.
            /// \param[in] timeSpec Value to add.
            /// \return *this
            TimeSpec &operator += (const TimeSpec &timeSpec);
            /// \brief
            /// Subtract and assign operator.
            /// \param[in] timeSpec Value to subtract.
            /// \return *this
            TimeSpec &operator -= (const TimeSpec &timeSpec);

            /// \brief
            /// Convert this TimeSpec to milliseconds.
            /// \return TimeSpec as milliseconds.
            inline ui64 ToMilliseconds () const {
                return tv_sec == -1 && tv_nsec == -1 ?
                    UI64_MAX : tv_sec * 1000 + tv_nsec / 1000000;
            }
            /// \brief
            /// Convert this TimeSpec to microseconds.
            /// \return TimeSpec as microseconds.
            inline ui64 ToMicroseconds () const {
                return tv_sec == -1 && tv_nsec == -1 ?
                    UI64_MAX : tv_sec * 1000000 + tv_nsec / 1000;
            }
            /// \brief
            /// Convert this TimeSpec to nanoseconds.
            /// \return TimeSpec as nanoseconds.
            inline ui64 ToNanoseconds () const {
                return tv_sec == -1 && tv_nsec == -1 ?
                    UI64_MAX : tv_sec * 1000000000 + tv_nsec;
            }

            /// \brief
            /// Convert this TimeSpec to timeval.
            /// \return TimeSpec as timeval.
            inline timeval Totimeval () const {
                timeval timeVal;
            #if defined (TOOLCHAIN_OS_Windows)
                timeVal.tv_sec = (long)tv_sec;
            #else // defined (TOOLCHAIN_OS_Windows)
                timeVal.tv_sec = tv_sec;
            #endif // defined (TOOLCHAIN_OS_Windows)
                timeVal.tv_usec = tv_nsec == -1 ? tv_nsec : tv_nsec / 1000;
                return timeVal;
            }
        };

        /// \brief
        /// TimeSpec size.
        const std::size_t TIME_SPEC_SIZE = sizeof (TimeSpec);

        /// \brief
        /// Compare two TimeSpecs for equality.
        /// \param[in] timeSpec1 First TimeSpec to compare.
        /// \param[in] timeSpec2 Second TimeSpec to compare.
        /// \return true = equal, false = different
        inline bool operator == (
                const TimeSpec &timeSpec1,
                const TimeSpec &timeSpec2) {
            return
                timeSpec1.tv_sec == timeSpec2.tv_sec &&
                timeSpec1.tv_nsec == timeSpec2.tv_nsec;
        }

        /// \brief
        /// Compare two TimeSpecs for inequality.
        /// \param[in] timeSpec1 First TimeSpec to compare.
        /// \param[in] timeSpec2 Second TimeSpec to compare.
        /// \return true = different, false = equal
        inline bool operator != (
                const TimeSpec &timeSpec1,
                const TimeSpec &timeSpec2) {
            return
                timeSpec1.tv_sec != timeSpec2.tv_sec ||
                timeSpec1.tv_nsec != timeSpec2.tv_nsec;
        }

        /// \brief
        /// Test if timeSpec1 is less than timeSpec2.
        /// \param[in] timeSpec1 First TimeSpec to compare.
        /// \param[in] timeSpec2 Second TimeSpec to compare.
        /// \return true = timeSpec1 < timeSpec2, false = timeSpec1 >= timeSpec2
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator < (
            const TimeSpec &timeSpec1,
            const TimeSpec &timeSpec2);
        /// \brief
        /// Test if timeSpec1 is less than or equal to timeSpec2.
        /// \param[in] timeSpec1 First TimeSpec to compare.
        /// \param[in] timeSpec2 Second TimeSpec to compare.
        /// \return true = timeSpec1 <= timeSpec2, false = timeSpec1 > timeSpec2
        inline bool operator <= (
                const TimeSpec &timeSpec1,
                const TimeSpec &timeSpec2) {
            return timeSpec1 < timeSpec2 || timeSpec1 == timeSpec2;
        }

        /// \brief
        /// Test if timeSpec1 is grater than timeSpec2.
        /// \param[in] timeSpec1 First TimeSpec to compare.
        /// \param[in] timeSpec2 Second TimeSpec to compare.
        /// \return true = timeSpec1 > timeSpec2, false = timeSpec1 <= timeSpec2
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API operator > (
            const TimeSpec &timeSpec1,
            const TimeSpec &timeSpec2);
        /// \brief
        /// Test if timeSpec1 is grater than or equal to timeSpec2.
        /// \param[in] timeSpec1 First TimeSpec to compare.
        /// \param[in] timeSpec2 Second TimeSpec to compare.
        /// \return true = timeSpec1 >= timeSpec2, false = timeSpec1 < timeSpec2
        inline bool operator >= (
                const TimeSpec &timeSpec1,
                const TimeSpec &timeSpec2) {
            return timeSpec1 > timeSpec2 || timeSpec1 == timeSpec2;
        }

        /// \brief
        /// Add two TimeSpecs.
        /// \param[in] timeSpec1 First TimeSpec to add.
        /// \param[in] timeSpec2 Second TimeSpec to add.
        /// \return timeSpec1 + timeSpec2
        _LIB_THEKOGANS_UTIL_DECL TimeSpec _LIB_THEKOGANS_UTIL_API operator + (
            const TimeSpec &timeSpec1,
            const TimeSpec &timeSpec2);
        /// \brief
        /// Subtract timeSpec2 from timeSpec1.
        /// \param[in] timeSpec1 First TimeSpec to subtract.
        /// \param[in] timeSpec2 Second TimeSpec to subtract.
        /// \return timeSpec1 - timeSpec2
        _LIB_THEKOGANS_UTIL_DECL TimeSpec _LIB_THEKOGANS_UTIL_API operator - (
            const TimeSpec &timeSpec1,
            const TimeSpec &timeSpec2);

        /// \brief
        /// Get current system time.
        /// \return Current system time.
        _LIB_THEKOGANS_UTIL_DECL TimeSpec _LIB_THEKOGANS_UTIL_API GetCurrentTime ();
        /// \brief
        /// Put the calling thread to sleep.
        /// \param[in] timeSpec How long to sleep for.
        _LIB_THEKOGANS_UTIL_DECL void _LIB_THEKOGANS_UTIL_API Sleep (const TimeSpec &timeSpec);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_TimeSpec_h)
