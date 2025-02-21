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

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
    #include <winsock2.h>
#elif defined (TOOLCHAIN_OS_OSX)
    #include <mach/clock_types.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <ctime>
#include "pugixml/pugixml.hpp"
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Serializable.h"
#include "thekogans/util/Exception.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)

namespace thekogans {
    namespace util {

        /// \struct TimeSpec TimeSpec.h thekogans/util/TimeSpec.h
        ///
        /// \brief
        /// TimeSpec encapsulates the interval used by Windows/POSIX
        /// apis. It makes it easier to specify various threading, and
        /// synchronization apis (Condition.h, Timer.h...).
        ///
        /// NOTE: Windows timeout interval is a relative millisecond
        /// value and is usually provided using a DWORD. Because we
        /// need to handle both relative and absolute (like calculating
        /// a deadline using GetCurrentTime () + interval) intervals,
        /// it's important that we can handle future time. DWORD does
        /// not have enough bits to handle absolute time.
        ///
        /// IMPORTANT: TimeSpec (0, 0) = TimeSpec::Zero = midnight 1/1/1970.
        /// IMPORTANT: TimeSpec's domain is:
        /// [TimeSpec::Zero, TimeSpec::Infinity].
        /// To maintain that, the operators + and - (below),
        /// do range checking on their arguments, and clamp
        /// the result accordingly.
        ///
        /// They also observe the following infinity convention:
        ///
        /// operator + | infinity | < infinity
        /// -----------+----------+-----------
        /// infinity   | infinity | infinity
        /// < infinity | infinity | < infinity
        ///
        /// operator - | infinity | < infinity
        /// -----------+----------+-----------
        /// infinity   |    0     | infinity
        /// < infinity |    0     | < infinity

        struct _LIB_THEKOGANS_UTIL_DECL TimeSpec : public Serializable {
            /// \brief
            /// TimeSpec is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (TimeSpec)

            /// \brief
            /// Seconds value.
            i64 seconds;
            /// \brief
            /// Nanoseconds value.
            i32 nanoseconds;

            /// \brief
            /// ctor. Init to Infinite.
            /// NOTE: This ctor is explicit. You provide either no values or both of them.
            /// \param[in] seconds_ Seconds value to initialize to.
            /// \param[in] nanoseconds_ Nanoseconds value to initialize to.
            /// NOTE: The nanoseconds_ value need not be normalized (<1000000000).
            /// The ctor will normalize it if necessary.
            explicit TimeSpec (
                i64 seconds_ = -1,
                i64 nanoseconds_ = -1);
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// ctor.
            /// \param[in] fileTime Windows FILETIME to initialize to.
            explicit TimeSpec (const FILETIME &fileTime) :
                seconds (os::windows::FILETIMEToi64 (fileTime)),
                nanoseconds (0) {}
            /// \brief
            /// ctor.
            /// \param[in] systemTime Windows SYSTEMTIME to initialize to.
            explicit TimeSpec (const SYSTEMTIME &systemTime);
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// ctor.
            /// \param[in] timeSpec POSIX timespec to initialize to.
            explicit TimeSpec (const timespec &timeSpec);
        #if defined (TOOLCHAIN_OS_OSX)
            /// \brief
            /// ctor.
            /// \param[in] timeSpec OS X Mach timespec to initialize to.
            explicit TimeSpec (const mach_timespec_t &timeSpec);
        #endif // defined (TOOLCHAIN_OS_OSX)
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// ctor.
            /// \param[in] timeVal POSIX timeval to initialize to.
            explicit TimeSpec (const timeval &timeVal);
            /// \brief
            /// ctor.
            /// \param[in] timeSpec TimeSpec to copy..
            TimeSpec (const TimeSpec &timeSpec) :
                seconds (timeSpec.seconds),
                nanoseconds (timeSpec.nanoseconds) {}

            /// \brief
            /// Zero
            /// NOTE: TimeSpec::Zero is our Big Bang. To ask what time
            /// it is before TimeSpec::Zero is equivalent to asking what
            /// time it was before the Big Bang. Neither question makes
            /// much sense.
            static const TimeSpec Zero;
            /// \brief
            /// Infinite
            static const TimeSpec Infinite;

            /// \brief
            /// Create a TimeSpec from hours.
            /// \param[in] hours Value to use to initialize the TimeSpec.
            /// \return TimeSpec initialized to given value.
            static TimeSpec FromHours (i64 hours) {
                if (hours < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
                return TimeSpec (hours * 3600, 0);
            }
            /// \brief
            /// Create a TimeSpec from minutes.
            /// \param[in] minutes Value to use to initialize the TimeSpec.
            /// \return TimeSpec initialized to given value.
            static TimeSpec FromMinutes (i64 minutes) {
                if (minutes < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
                return TimeSpec (minutes * 60, 0);
            }
            /// \brief
            /// Create a TimeSpec from seconds.
            /// \param[in] seconds Value to use to initialize the TimeSpec.
            /// \return TimeSpec initialized to given value.
            static TimeSpec FromSeconds (i64 seconds) {
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
            static TimeSpec FromMilliseconds (i64 milliseconds) {
                if (milliseconds < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
                return TimeSpec (milliseconds / 1000, (milliseconds % 1000) * 1000000);
            }
            /// \brief
            /// Create a TimeSpec from microseconds.
            /// \param[in] microseconds Value to use to initialize the TimeSpec.
            /// \return TimeSpec initialized to given value.
            static TimeSpec FromMicroseconds (i64 microseconds) {
                if (microseconds < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
                return TimeSpec (microseconds / 1000000, (microseconds % 1000000) * 1000);
            }
            /// \brief
            /// Create a TimeSpec from nanoseconds.
            /// \param[in] nanoseconds Value to use to initialize the TimeSpec.
            /// \return TimeSpec initialized to given value.
            static TimeSpec FromNanoseconds (i64 nanoseconds) {
                if (nanoseconds < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
                return TimeSpec (nanoseconds / 1000000000, nanoseconds % 1000000000);
            }

            /// \brief
            /// Assignment operator.
            /// \param[in] timeSpec TimeSpec to assign.
            /// \return *this
            TimeSpec &operator = (const TimeSpec &timeSpec);

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
            inline i64 ToMilliseconds () const {
                return seconds == -1 && nanoseconds == -1 ?
                    I64_MAX : seconds * 1000 + nanoseconds / 1000000;
            }
            /// \brief
            /// Convert this TimeSpec to microseconds.
            /// \return TimeSpec as microseconds.
            inline i64 ToMicroseconds () const {
                return seconds == -1 && nanoseconds == -1 ?
                    I64_MAX : seconds * 1000000 + nanoseconds / 1000;
            }
            /// \brief
            /// Convert this TimeSpec to nanoseconds.
            /// \return TimeSpec as nanoseconds.
            inline i64 ToNanoseconds () const {
                return seconds == -1 && nanoseconds == -1 ?
                    I64_MAX : seconds * 1000000000 + nanoseconds;
            }

        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Convert this TimeSpec to Windows FILETIME.
            /// IMPORTANT: This function is lossy (doesn't use nanoseconds).
            /// \return TimeSpec as FILETIME.
            inline FILETIME ToFILETIME () const {
                return os::windows::i64ToFILETIME (seconds);
            }
            /// \brief
            /// Convert this TimeSpec to Windows SYSTEMTIME.
            /// \return TimeSpec as SYSTEMTIME.
            inline SYSTEMTIME ToSYSTEMTIME () const {
                FILETIME fileTime = os::windows::i64ToFILETIME (seconds);
                SYSTEMTIME systemTime;
                if (!FileTimeToSystemTime (&fileTime, &systemTime)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                systemTime.wMilliseconds = nanoseconds / 1000000;
                return systemTime;
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Convert this TimeSpec to timespec.
            /// \return TimeSpec as timespec.
            inline timespec Totimespec () const {
                timespec timeSpec;
                timeSpec.tv_sec = (time_t)seconds;
                timeSpec.tv_nsec = (long)nanoseconds;
                return timeSpec;
            }
        #endif // defined (TOOLCHAIN_OS_Windows)

            /// \brief
            /// Convert this TimeSpec to timeval.
            /// \return TimeSpec as timeval.
            inline timeval Totimeval () const {
                timeval timeVal;
            #if defined (TOOLCHAIN_OS_Windows)
                timeVal.tv_sec = (long)seconds;
            #else // defined (TOOLCHAIN_OS_Windows)
                timeVal.tv_sec = seconds;
            #endif // defined (TOOLCHAIN_OS_Windows)
                timeVal.tv_usec = nanoseconds == -1 ? nanoseconds : nanoseconds / 1000;
                return timeVal;
            }

            /// \brief
            /// The following convenience functions allow you to create
            /// absolute time deadlines. Here is a canonical use case:
            ///
            /// \code{.cpp}
            /// thekogans::util::TimeSpec deadline =
            ///     thekogans::util::GetCurrentTime ().AddSeconds (5);
            /// \endcode
            ///
            /// You can also chain multiple calls:
            ///
            /// \code{.cpp}
            /// thekogans::util::TimeSpec deadline =
            ///     thekogans::util::GetCurrentTime ().AddMinutes (1).AddSeconds (30);
            /// \endcode

            /// \brief
            /// Create a TimeSpec from adding hours to the current value.
            /// \param[in] hours Value to add to the current value.
            /// \return *this + FromHours (hours).
            TimeSpec AddHours (i64 hours) const;
            /// \brief
            /// Create a TimeSpec from adding minutes to the current value.
            /// \param[in] minutes Value to add to the current value.
            /// \return *this + FromMinutes (minutes).
            TimeSpec AddMinutes (i64 minutes) const;
            /// \brief
            /// Create a TimeSpec from adding seconds to the current value.
            /// \param[in] seconds Value to add to the current value.
            /// \return *this + FromSeconds (seconds).
            TimeSpec AddSeconds (i64 seconds) const;
            /// \brief
            /// Create a TimeSpec from adding milliseconds to the current value.
            /// \param[in] milliseconds Value to add to the current value.
            /// \return *this + FromMilliseconds (milliseconds).
            TimeSpec AddMilliseconds (i64 milliseconds) const;
            /// \brief
            /// Create a TimeSpec from adding microseconds to the current value.
            /// \param[in] microseconds Value to add to the current value.
            /// \return *this + FromMicroseconds (microseconds).
            TimeSpec AddMicroseconds (i64 microseconds) const;
            /// \brief
            /// Create a TimeSpec from adding nanoseconds to the current value.
            /// \param[in] nanoseconds Value to add to the current value.
            /// \return *this + FromNanoseconds (nanoseconds).
            TimeSpec AddNanoseconds (i64 nanoseconds) const;

            // Serializable
            /// \brief
            /// Return the serialized key size.
            /// \return Serialized key size.
            virtual std::size_t Size () const noexcept override;

            /// \brief
            /// Read the key from the given serializer.
            /// \param[in] header \see{Serializable::BinHeader}.
            /// \param[in] serializer \see{Serializer} to read the key from.
            virtual void Read (
                const BinHeader & /*header*/,
                Serializer &serializer) override;
            /// \brief
            /// Write the key to the given serializer.
            /// \param[out] serializer \see{Serializer} to write the key to.
            virtual void Write (Serializer &serializer) const override;

            /// \brief
            /// Read the Serializable from an XML DOM.
            /// \param[in] header \see{Serializable::TextHeader}.
            /// \param[in] node XML DOM representation of a Serializable.
            virtual void Read (
                const TextHeader & /*header*/,
                const pugi::xml_node &node) override;
            /// \brief
            /// Write the Serializable to the XML DOM.
            /// \param[out] node Parent node.
            virtual void Write (pugi::xml_node &node) const override;

            /// \brief
            /// Read a Serializable from an JSON DOM.
            /// \param[in] node JSON DOM representation of a Serializable.
            virtual void Read (
                const TextHeader & /*header*/,
                const JSON::Object &object) override;
            /// \brief
            /// Write a Serializable to the JSON DOM.
            /// \param[out] node Parent node.
            virtual void Write (JSON::Object &object) const override;
        };

        /// \brief
        /// Serialized TimeSpec size.
        const std::size_t TIME_SPEC_SIZE = I64_SIZE + I32_SIZE;

        /// \brief
        /// Compare two TimeSpecs for equality.
        /// \param[in] timeSpec1 First TimeSpec to compare.
        /// \param[in] timeSpec2 Second TimeSpec to compare.
        /// \return true = equal, false = different
        inline bool _LIB_THEKOGANS_UTIL_API operator == (
                const TimeSpec &timeSpec1,
                const TimeSpec &timeSpec2) {
            return
                timeSpec1.seconds == timeSpec2.seconds &&
                timeSpec1.nanoseconds == timeSpec2.nanoseconds;
        }

        /// \brief
        /// Compare two TimeSpecs for inequality.
        /// \param[in] timeSpec1 First TimeSpec to compare.
        /// \param[in] timeSpec2 Second TimeSpec to compare.
        /// \return true = different, false = equal
        inline bool _LIB_THEKOGANS_UTIL_API operator != (
                const TimeSpec &timeSpec1,
                const TimeSpec &timeSpec2) {
            return
                timeSpec1.seconds != timeSpec2.seconds ||
                timeSpec1.nanoseconds != timeSpec2.nanoseconds;
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
        inline bool _LIB_THEKOGANS_UTIL_API operator <= (
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
        inline bool _LIB_THEKOGANS_UTIL_API operator >= (
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
        /// IMPORTANT: timeSpec is a relative value.
        _LIB_THEKOGANS_UTIL_DECL void _LIB_THEKOGANS_UTIL_API Sleep (const TimeSpec &timeSpec);

        /// \brief
        /// Implement TimeSpec extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EXTRACTION_OPERATORS (TimeSpec)

        /// \brief
        /// Implement TimeSpec value parser.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_VALUE_PARSER (TimeSpec)

        /// \brief
        /// Convert TimeSpec to string representation.
        /// NOTE: FormatTimeSpec uses localtime and strftime
        /// representation for the current locale.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API FormatTimeSpec (
            const TimeSpec &timeSpec,
            const char *format = "%c");

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_TimeSpec_h)
