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

#include "thekogans/util/Heap.h"
#include "thekogans/util/HRTimerMgr.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (
            thekogans::util::HRTimerMgr::TimerInfoBase)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void HRTimerMgr::TimerInfoBase::StaticInit () {
            HRTimerMgr::TimerInfo::StaticInit ();
            HRTimerMgr::ScopeInfo::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        std::size_t HRTimerMgr::TimerInfoBase::Size () const noexcept {
            return Serializer::Size (name) + Serializer::Size (attributes);
        }

        void HRTimerMgr::TimerInfoBase::Read (Serializer &serializer) {
            serializer >> name >> attributes;
        }

        void HRTimerMgr::TimerInfoBase::Write (Serializer &serializer) {
            serializer << name << attributes;
        }

        ////////////////////////////////////////////////////////////////////////////////////////

        #if !defined (THEKOGANS_UTIL_MIN_TIMER_INFOS_IN_PAGE)
            #define THEKOGANS_UTIL_MIN_TIMER_INFOS_IN_PAGE 64
        #endif // !defined (THEKOGANS_UTIL_MIN_TIMER_INFOS_IN_PAGE)

        THEKOGANS_UTIL_IMPLEMENT_HRTIMERMGR_TIMERINFOBASE (
            thekogans::util::HRTimerMgr::TimerInfo,
            1,
            0,
            THEKOGANS_UTIL_MIN_TIMER_INFOS_IN_PAGE)

        void HRTimerMgr::TimerInfo::Start () {
            start = HRTimer::Click ();
        }

        void HRTimerMgr::TimerInfo::Stop () {
            stop = HRTimer::Click ();
        }

        void HRTimerMgr::TimerInfo::GetStats (
                ui32 &count,
                ui64 &min,
                ui64 &max,
                ui64 &average,
                ui64 &total) const {
            count = 1;
            min = max = average = total = HRTimer::ComputeElapsedTime (start, stop);
        }

        std::size_t HRTimerMgr::TimerInfo::Size () const noexcept {
            return TimerInfoBase::Size () +
                Serializer::Size (start) +
                Serializer::Size (stop);
        }

        void HRTimerMgr::TimerInfo::Read (Serializer &serializer) {
            TimerInfoBase::Read (serializer);
            serializer >> start >> stop;
        }

        void HRTimerMgr::TimerInfo::Write (Serializer &serializer) {
            TimerInfoBase::Write (serializer);
            serializer << start << stop;
        }

        ////////////////////////////////////////////////////////////////////////////////////////

        #if !defined (THEKOGANS_UTIL_MIN_SCOPE_INFOS_IN_PAGE)
            #define THEKOGANS_UTIL_MIN_SCOPE_INFOS_IN_PAGE 64
        #endif // !defined (THEKOGANS_UTIL_MIN_SCOPE_INFOS_IN_PAGE)

        THEKOGANS_UTIL_IMPLEMENT_HRTIMERMGR_TIMERINFOBASE (
            thekogans::util::HRTimerMgr::ScopeInfo,
            1,
            0,
            THEKOGANS_UTIL_MIN_SCOPE_INFOS_IN_PAGE)

        HRTimerMgr::ScopeInfo *HRTimerMgr::ScopeInfo::BeginScope (
                const std::string &name) {
            SharedPtr scope (new ScopeInfo (name));
            open.push_back (dynamic_refcounted_sharedptr_cast<TimerInfoBase> (scope));
            return scope.Get ();
        }

        void HRTimerMgr::ScopeInfo::EndScope () {
            if (!open.empty ()) {
                closed.push_back (open.back ());
                open.pop_back ();
            }
        }

        HRTimerMgr::TimerInfo *HRTimerMgr::ScopeInfo::StartTimer (
                const std::string &name) {
            TimerInfo::SharedPtr timer (new TimerInfo (name));
            open.push_back (dynamic_refcounted_sharedptr_cast<TimerInfoBase> (timer));
            return timer.Get ();
        }

        void HRTimerMgr::ScopeInfo::StopTimer () {
            if (!open.empty ()) {
                closed.push_back (open.back ());
                open.pop_back ();
            }
        }

        void HRTimerMgr::ScopeInfo::GetStats (
                ui32 &count,
                ui64 &min,
                ui64 &max,
                ui64 &average,
                ui64 &total) const {
            for (auto timerInfo : closed) {
                ++count;
                ui32 itemCount = 0;
                ui64 itemMin = UI64_MAX;
                ui64 itemMax = 0;
                ui64 itemAverage = 0;
                ui64 itemTotal = 0;
                timerInfo->GetStats (itemCount, itemMin, itemMax, itemAverage, itemTotal);
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

        std::size_t HRTimerMgr::ScopeInfo::Size () const noexcept {
            std::size_t size = TimerInfoBase::Size ();
            {
                size += util::SizeT (open.size ()).Size ();
                for (auto timerInfo : open) {
                    size += timerInfo->Size ();
                }
            }
            {
                size += util::SizeT (closed.size ()).Size ();
                for (auto timerInfo : closed) {
                    size += timerInfo->Size ();
                }
            }
            return size;
        }

        void HRTimerMgr::ScopeInfo::Read (Serializer &serializer) {
            TimerInfoBase::Read (serializer);
            {
                util::SizeT openCount;
                serializer >> openCount;
                open.clear ();
                while (openCount-- > 0) {
                    TimerInfoBase::SharedPtr openInfo;
                    serializer >> openInfo;
                    open.push_back (openInfo);
                }
            }
            {
                util::SizeT closedCount;
                serializer >> closedCount;
                closed.clear ();
                while (closedCount-- > 0) {
                    TimerInfoBase::SharedPtr closedInfo;
                    serializer >> closedInfo;
                    closed.push_back (closedInfo);
                }
            }
        }

        void HRTimerMgr::ScopeInfo::Write (Serializer &serializer) {
            TimerInfoBase::Write (serializer);
            {
                serializer << util::SizeT (open.size ());
                for (auto timerInfo : open) {
                    serializer << *timerInfo;
                }
            }
            {
                serializer << util::SizeT (closed.size ());
                for (auto timerInfo : closed) {
                    serializer << *timerInfo;
                }
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (thekogans::util::HRTimerMgr, 1, 0)

        std::size_t HRTimerMgr::Size () const noexcept {
            return root.Size ();
        }

        void HRTimerMgr::Read (Serializer &serializer) {
            serializer >> root;
        }

        void HRTimerMgr::Write (Serializer &serializer) {
            serializer << root;
        }

    } // namespace util
} // namespace thekogans
