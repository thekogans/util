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

#include "thekogans/util/RunLoop.h"

namespace thekogans {
    namespace util {

        bool RunLoop::WaitForStart (
                Ptr &runLoop,
                const TimeSpec &sleepTimeSpec,
                const TimeSpec &waitTimeSpec) {
            TimeSpec now = GetCurrentTime ();
            TimeSpec deadline = now + waitTimeSpec;
            while ((runLoop.get () == 0 || !runLoop->IsRunning ()) && deadline > now) {
                util::Sleep (sleepTimeSpec);
                now = GetCurrentTime ();
            }
            return runLoop.get () != 0 && runLoop->IsRunning ();
        }

    } // namespace util
} // namespace thekogans
