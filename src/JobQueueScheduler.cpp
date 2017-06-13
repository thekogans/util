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

#include "thekogans/util/LockGuard.h"
#include "thekogans/util/JobQueueScheduler.h"

namespace thekogans {
    namespace util {

        JobQueueScheduler::JobInfo::Compare JobQueueScheduler::JobInfo::compare;

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (JobQueueScheduler::JobQueueJobInfo, SpinLock)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (JobQueueScheduler::RunLoopJobInfo, SpinLock)

        void JobQueueScheduler::Queue::Cancel (const JobQueue &jobQueue) {
            for (std::size_t i = c.size (); i-- > 0;) {
                JobQueueJobInfo *jobQueueJobInfo =
                    dynamic_cast<JobQueueJobInfo *> (c[i].Get ());
                if (jobQueueJobInfo != 0 && &jobQueueJobInfo->jobQueue == &jobQueue) {
                    c.erase (c.begin () + i);
                }
            }
        }

        void JobQueueScheduler::Queue::Cancel (const RunLoop &runLoop) {
            for (std::size_t i = c.size (); i-- > 0;) {
                RunLoopJobInfo *runLoopJobInfo =
                    dynamic_cast<RunLoopJobInfo *> (c[i].Get ());
                if (runLoopJobInfo != 0 && &runLoopJobInfo->runLoop == &runLoop) {
                    c.erase (c.begin () + i);
                }
            }
        }

        void JobQueueScheduler::Queue::Cancel (const JobQueue::Job::Id &id) {
            for (std::size_t i = c.size (); i-- > 0;) {
                if (c[i]->id == id) {
                    c.erase (c.begin () + i);
                    break;
                }
            }
        }

        void JobQueueScheduler::Cancel (const JobQueue &jobQueue) {
            LockGuard<SpinLock> guard (spinLock);
            if (!queue.empty ()) {
                TimeSpec deadline = queue.top ()->deadline;
                queue.Cancel (jobQueue);
                if (!queue.empty () && queue.top ()->deadline != deadline) {
                    timer.Start (queue.top ()->deadline - GetCurrentTime ());
                }
            }
        }

        void JobQueueScheduler::Cancel (const RunLoop &runLoop) {
            LockGuard<SpinLock> guard (spinLock);
            if (!queue.empty ()) {
                TimeSpec deadline = queue.top ()->deadline;
                queue.Cancel (runLoop);
                if (!queue.empty () && queue.top ()->deadline != deadline) {
                    timer.Start (queue.top ()->deadline - GetCurrentTime ());
                }
            }
        }

        void JobQueueScheduler::Cancel (const JobQueue::Job::Id &id) {
            LockGuard<SpinLock> guard (spinLock);
            if (!queue.empty ()) {
                TimeSpec deadline = queue.top ()->deadline;
                queue.Cancel (id);
                if (!queue.empty () && queue.top ()->deadline != deadline) {
                    timer.Start (queue.top ()->deadline - GetCurrentTime ());
                }
            }
        }

        void JobQueueScheduler::Clear () {
            LockGuard<SpinLock> guard (spinLock);
            timer.Stop ();
            Queue empty;
            queue.swap (empty);
        }

        void JobQueueScheduler::Alarm (Timer & /*timer*/) throw () {
            LockGuard<SpinLock> guard (spinLock);
            TimeSpec now = GetCurrentTime ();
            while (!queue.empty () && queue.top ()->deadline >= now) {
                queue.top ()->EnqJob ();
                queue.pop ();
            }
            if (!queue.empty ()) {
                timer.Start (queue.top ()->deadline - now);
            }
        }

        JobQueue::Job::Id JobQueueScheduler::ScheduleJobInfo (
                JobInfo::Ptr jobInfo,
                const TimeSpec &timeSpec) {
            LockGuard<SpinLock> guard (spinLock);
            bool startTimer = false;
            if (queue.empty () || queue.top ()->deadline > jobInfo->deadline) {
                timer.Stop ();
                startTimer = true;
            }
            queue.push (jobInfo);
            if (startTimer) {
                timer.Start (timeSpec);
            }
            return jobInfo->id;
        }

    } // namespace util
} // namespace thekogans
