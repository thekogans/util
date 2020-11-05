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
#include "thekogans/util/HRTimer.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Scheduler.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (Scheduler::JobQueue, SpinLock)

        void Scheduler::JobQueue::Start () {
            done = false;
            if (GetPendingJobCount () != 0) {
                scheduler.AddJobQueue (this);
            }
        }

        bool Scheduler::JobQueue::Stop (
                bool cancelRunningJobs,
                bool cancelPendingJobs,
                const TimeSpec &timeSpec) {
            TimeSpec deadline = GetCurrentTime () + timeSpec;
            if (!Pause (cancelRunningJobs, deadline - GetCurrentTime ())) {
                return false;
            }
            if (cancelPendingJobs) {
                CancelPendingJobs ();
                Continue ();
                if (!WaitForIdle (deadline - GetCurrentTime ())) {
                    return false;
                }
            }
            done = true;
            Continue ();
            scheduler.DeleteJobQueue (this);
            idle.SignalAll ();
            return true;
        }

        void Scheduler::JobQueue::Continue () {
            RunLoop::Continue ();
            if (GetPendingJobCount () != 0) {
                scheduler.AddJobQueue (this);
            }
        }

        bool Scheduler::JobQueue::EnqJob (
                Job::Ptr job,
                bool wait,
                const TimeSpec &timeSpec) {
            bool result = RunLoop::EnqJob (job);
            if (result) {
                scheduler.AddJobQueue (this);
                result = !wait || WaitForJob (job, timeSpec);
            }
            return result;
        }

        bool Scheduler::JobQueue::EnqJobFront (
                Job::Ptr job,
                bool wait,
                const TimeSpec &timeSpec) {
            bool result = RunLoop::EnqJobFront (job);
            if (result) {
                scheduler.AddJobQueue (this);
                result = !wait || WaitForJob (job, timeSpec);
            }
            return result;
        }

        Scheduler::~Scheduler () {
            {
                LockGuard<SpinLock> guard (spinLock);
                JobQueueList::DefaultCallback callback;
                high.clear (callback);
                normal.clear (callback);
                low.clear (callback);
            }
            jobQueuePool.WaitForIdle ();
        }

        void Scheduler::AddJobQueue (
                JobQueue *jobQueue,
                bool scheduleJobQueue) {
            if (jobQueue != 0) {
                {
                    LockGuard<SpinLock> guard (spinLock);
                    // In flight job queues are the ones executing
                    // currently executing jobs. They add themselves
                    // to the back of the priority queue after the job
                    // is done.
                    if (jobQueue->inFlight) {
                        return;
                    }
                    // NOTE: It's okay for push_back to fail. It simply means
                    // the queue is already in it's proper list and will be
                    // returned by GetNextJobQueue when it's time to execute
                    // one of it's jobs.
                    switch (jobQueue->priority) {
                        case JobQueue::PRIORITY_LOW:
                            if (!low.push_back (jobQueue)) {
                                return;
                            }
                            break;
                        case JobQueue::PRIORITY_NORMAL:
                            if (!normal.push_back (jobQueue)) {
                                return;
                            }
                            break;
                        case JobQueue::PRIORITY_HIGH:
                            if (!high.push_back (jobQueue)) {
                                return;
                            }
                            break;
                    }
                }
                if (scheduleJobQueue) {
                    struct JobQueueJob : public RunLoop::Job {
                        util::JobQueue::Ptr jobQueue;
                        Scheduler &scheduler;

                        JobQueueJob (
                            util::JobQueue::Ptr jobQueue_,
                            Scheduler &scheduler_) :
                            jobQueue (jobQueue_),
                            scheduler (scheduler_) {}

                        virtual void Execute (const std::atomic<bool> &done) throw () {
                            JobQueue *jobQueue;
                            while (!ShouldStop (done) && (jobQueue = scheduler.GetNextJobQueue ()) != 0) {
                                RunLoop::Job *job;
                                bool cancelled;
                                // Skip over cancelled jobs.
                                do {
                                    job = jobQueue->DeqJob (false);
                                    if (job != 0) {
                                        ui64 start = 0;
                                        ui64 end = 0;
                                        // Short circuit cancelled pending jobs.
                                        cancelled = job->ShouldStop (jobQueue->done);
                                        if (!cancelled) {
                                            start = HRTimer::Click ();
                                            job->SetState (Job::Running);
                                            job->Prologue (jobQueue->done);
                                            job->Execute (jobQueue->done);
                                            job->Epilogue (jobQueue->done);
                                            job->Succeed (jobQueue->done);
                                            end = HRTimer::Click ();
                                        }
                                        jobQueue->FinishedJob (job, start, end);
                                    }
                                } while (job != 0 && cancelled);
                                jobQueue->inFlight = false;
                                if (!jobQueue->IsPaused () && jobQueue->GetPendingJobCount () != 0) {
                                    scheduler.AddJobQueue (jobQueue, false);
                                }
                            }
                        }
                    };
                    util::JobQueue::Ptr jobQueue = jobQueuePool.GetJobQueue (0);
                    if (jobQueue.Get () != 0) {
                        jobQueue->EnqJob (
                            RunLoop::Job::Ptr (new JobQueueJob (jobQueue, *this)));
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Scheduler::DeleteJobQueue (JobQueue *jobQueue) {
            if (jobQueue != 0) {
                LockGuard<SpinLock> guard (spinLock);
                switch (jobQueue->priority) {
                    case JobQueue::PRIORITY_LOW:
                        low.erase (jobQueue);
                        break;
                    case JobQueue::PRIORITY_NORMAL:
                        normal.erase (jobQueue);
                        break;
                    case JobQueue::PRIORITY_HIGH:
                        high.erase (jobQueue);
                        break;
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Scheduler::JobQueue *Scheduler::GetNextJobQueue () {
            JobQueue *jobQueue = 0;
            LockGuard<SpinLock> guard (spinLock);
            // Priority based, round-robin, O(1) scheduler!
            if (!high.empty ()) {
                jobQueue = high.pop_front ();
            }
            else if (!normal.empty ()) {
                jobQueue = normal.pop_front ();
            }
            else if (!low.empty ()) {
                jobQueue = low.pop_front ();
            }
            if (jobQueue != 0) {
                jobQueue->inFlight = true;
            }
            return jobQueue;
        }

    } // namespace util
} // namespace thekogans
