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
#include <iostream>
#include "thekogans/util/Config.h"
#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/HRTimerMgr.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (HRTimerMgr::TimerInfo, SpinLock)

        const char * const HRTimerMgr::TimerInfo::TAG_TIMER = "Timer";
        const char * const HRTimerMgr::TimerInfo::ATTR_NAME = "Name";
        const char * const HRTimerMgr::TimerInfo::ATTR_START = "Start";
        const char * const HRTimerMgr::TimerInfo::ATTR_STOP = "Stop";
        const char * const HRTimerMgr::TimerInfo::ATTR_ELLAPSED = "Ellapsed";

        void HRTimerMgr::TimerInfo::GetStats (
                ui32 &count,
                ui64 &min,
                ui64 &max,
                ui64 &average,
                ui64 &total) const {
            count = 1;
            min = max = average = total = HRTimer::ComputeEllapsedTime (start, stop);
        }

        std::string HRTimerMgr::TimerInfo::ToString (ui32 indentationLevel) const {
            Attributes attributes_;
            attributes_.push_back (Attribute (ATTR_NAME, Encodestring (name)));
            attributes_.push_back (Attribute (ATTR_START, ui64Tostring (start)));
            attributes_.push_back (Attribute (ATTR_STOP, ui64Tostring (stop)));
            attributes_.push_back (
                Attribute (ATTR_ELLAPSED,
                    f64Tostring (
                        HRTimer::ToSeconds (
                            HRTimer::ComputeEllapsedTime (start, stop)))));
            for (Attributes::const_iterator it = attributes.begin (),
                    end = attributes.end (); it != end; ++it) {
                attributes_.push_back (Attribute ((*it).first, Encodestring ((*it).second)));
            }
            return OpenTag (indentationLevel, TAG_TIMER, attributes_, true, true);
        }

        void HRTimerMgr::TimerInfo::Start () {
            start = HRTimer::Click ();
        }

        void HRTimerMgr::TimerInfo::Stop () {
            stop = HRTimer::Click ();
        }

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (HRTimerMgr::ScopeInfo, SpinLock)

        const char * const HRTimerMgr::ScopeInfo::TAG_SCOPE = "Scope";
        const char * const HRTimerMgr::ScopeInfo::ATTR_NAME = "Name";
        const char * const HRTimerMgr::ScopeInfo::ATTR_COUNT = "Count";
        const char * const HRTimerMgr::ScopeInfo::ATTR_MIN = "Min";
        const char * const HRTimerMgr::ScopeInfo::ATTR_MAX = "Max";
        const char * const HRTimerMgr::ScopeInfo::ATTR_AVERAGE = "Average";
        const char * const HRTimerMgr::ScopeInfo::ATTR_TOTAL = "Total";

        void HRTimerMgr::ScopeInfo::GetStats (
                ui32 &count,
                ui64 &min,
                ui64 &max,
                ui64 &average,
                ui64 &total) const {
            assert (open.empty ());
            for (AbstractOwnerList<TimerInfoBase>::const_iterator it = closed.begin (),
                    end = closed.end (); it != end; ++it) {
                ++count;
                ui32 itemCount = 0;
                ui64 itemMin = UI64_MAX;
                ui64 itemMax = 0;
                ui64 itemAverage = 0;
                ui64 itemTotal = 0;
                (*it)->GetStats (itemCount, itemMin, itemMax, itemAverage, itemTotal);
                if (min > itemMin) {
                    min = itemMin;
                }
                if (max < itemMax) {
                    max = itemMax;
                }
                average += itemAverage;
                total += itemTotal;
            }
            if (count > 0) {
                average /= count;
            }
        }

        std::string HRTimerMgr::ScopeInfo::ToString (ui32 indentationLevel) const {
            ui32 count = 0;
            ui64 min = UI64_MAX;
            ui64 max = 0;
            ui64 average = 0;
            ui64 total = 0;
            GetStats (count, min, max, average, total);
            Attributes attributes_;
            attributes_.push_back (Attribute (ATTR_NAME, Encodestring (name)));
            attributes_.push_back (Attribute (ATTR_COUNT, ui32Tostring (count)));
            attributes_.push_back (Attribute (ATTR_MIN, f64Tostring (HRTimer::ToSeconds (min))));
            attributes_.push_back (Attribute (ATTR_MAX, f64Tostring (HRTimer::ToSeconds (max))));
            attributes_.push_back (Attribute (ATTR_AVERAGE, f64Tostring (HRTimer::ToSeconds (average))));
            attributes_.push_back (Attribute (ATTR_TOTAL, f64Tostring (HRTimer::ToSeconds (total))));
            for (Attributes::const_iterator it = attributes.begin (),
                    end = attributes.end (); it != end; ++it) {
                attributes_.push_back (Attribute ((*it).first, Encodestring ((*it).second)));
            }
            return OpenTag (indentationLevel, TAG_SCOPE, attributes_, true, true);
        }

        HRTimerMgr::ScopeInfo *HRTimerMgr::ScopeInfo::BeginScope (
                const std::string &name) {
            UniquePtr scope (new ScopeInfo (name));
            assert (scope.get () != 0);
            if (scope.get () != 0) {
                open.push_back (scope.get ());
                return static_cast<ScopeInfo *> (scope.release ());
            }
            return 0;
        }

        void HRTimerMgr::ScopeInfo::EndScope () {
            if (!open.empty ()) {
                closed.push_back (open.back ());
                open.pop_back ();
            }
        }

        HRTimerMgr::TimerInfo *HRTimerMgr::ScopeInfo::StartTimer (
                const std::string &name) {
            UniquePtr timer (new TimerInfo (name));
            assert (timer.get () != 0);
            if (timer.get () != 0) {
                open.push_back (timer.get ());
                return static_cast<TimerInfo *> (timer.release ());
            }
            return 0;
        }

        void HRTimerMgr::ScopeInfo::StopTimer () {
            if (!open.empty ()) {
                closed.push_back (open.back ());
                open.pop_back ();
            }
        }

    } // namespace util
} // namespace thekogans
