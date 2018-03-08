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

#if !defined (__thekogans_util_HRTimer_h)
#define __thekogans_util_HRTimer_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"

namespace thekogans {
    namespace util {

        /// \struct HRTimer HRTimer.h thekogans/util/HRTimer.h
        ///
        /// \brief
        /// HRTimer is a high resolution, platform independent
        /// timer. Granular code profiling is it's intended use
        /// case. See HRTimerMgr for a complete profiling
        /// framework. Here is a canonical use case:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// util::ui64 start = util::HRTimer::Click ();
        /// {
        ///     code to be timed goes here.
        /// }
        /// util::ui64 stop = util::HRTimer::Click ();
        /// std::cout << "Elapsed time (in seconds): " <<
        ///     util::HRTimer::ToSeconds (
        ///         util::HRTimer::ComputeElapsedTime (start, stop));
        /// \endcode

        struct _LIB_THEKOGANS_UTIL_DECL HRTimer {
            /// \brief
            /// Get platform specific timer frequency.
            /// \return platform specific timer frequency.
            static ui64 GetFrequency ();
            /// \brief
            /// Get current timer value.
            /// \return current timer value.
            static ui64 Click ();
            /// \brief
            /// Given a start and stop timer values, compute the difference.
            /// Takes overflow in to consideration.
            /// \param[in] start The lesser value.
            /// \param[in] stop The grater value.
            /// \return Difference between the two.
            static ui64 ComputeElapsedTime (
                    ui64 start,
                    ui64 stop) {
                return stop > start ? stop - start : UI64_MAX - (start - stop);
            }
            /// \brief
            /// Using the timer frequency, convert ellapsed time to seconds.
            /// \param[in] elapsedTime Value returned by ComputeElapsedTime.
            /// \return Elapsed seconds.
            static f64 ToSeconds (ui64 ellapsedTime) {
                return (f64)(i64)ellapsedTime / (f64)GetFrequency ();
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_HRTimer_h)
