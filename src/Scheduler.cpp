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
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/HRTimer.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Scheduler.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (Scheduler::JobQueue)

        void Scheduler::JobQueue::Start () {
            state->done = false;
            if (GetPendingJobCount () != 0) {
                context->AddJobQueue (this);
            }
        }

        void Scheduler::JobQueue::Stop (
                bool cancelRunningJobs,
                bool cancelPendingJobs) {
            state->done = true;
            if (cancelRunningJobs) {
                CancelRunningJobs ();
            }
            context->DeleteJobQueue (this);
            if (cancelPendingJobs) {
                Job *job;
                while ((job = state->jobExecutionPolicy->DeqJob (*state)) != nullptr) {
                    job->Cancel ();
                    state->runningJobs.push_back (job);
                    state->FinishedJob (job, 0, 0);
                }
            }
            state->idle.SignalAll ();
        }

        bool Scheduler::JobQueue::IsRunning () {
            return !state->done;
        }

        void Scheduler::JobQueue::Continue () {
            RunLoop::Continue ();
            if (GetPendingJobCount () != 0) {
                context->AddJobQueue (this);
            }
        }

        bool Scheduler::JobQueue::EnqJob (
                Job::SharedPtr job,
                bool wait,
                const TimeSpec &timeSpec) {
            bool result = RunLoop::EnqJob (job);
            if (result) {
                context->AddJobQueue (this);
                result = !wait || WaitForJob (job, timeSpec);
            }
            return result;
        }

        bool Scheduler::JobQueue::EnqJobFront (
                Job::SharedPtr job,
                bool wait,
                const TimeSpec &timeSpec) {
            bool result = RunLoop::EnqJobFront (job);
            if (result) {
                context->AddJobQueue (this);
                result = !wait || WaitForJob (job, timeSpec);
            }
            return result;
        }

        Scheduler::Context::~Context () {
            jobQueuePool.WaitForIdle ();
        }

        void Scheduler::Context::AddJobQueue (JobQueue *jobQueue) {
            if (jobQueue != nullptr) {
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
                struct JobQueueJob : public RunLoop::Job {
                    Scheduler::Context::SharedPtr context;

                    JobQueueJob (Scheduler::Context::SharedPtr context_) :
                        context (context_) {}

                    virtual void Execute (const std::atomic<bool> &done) noexcept {
                        JobQueue *jobQueue;
                        while (!ShouldStop (done) && !context->switchingOff &&
                                (jobQueue = context->GetNextJobQueue ()) != nullptr) {
                            RunLoop::Job *job = nullptr;
                            bool cancelled = false;
                            // Skip over cancelled jobs.
                            do {
                                job = jobQueue->state->DeqJob (false);
                                if (job != nullptr) {
                                    ui64 start = 0;
                                    ui64 end = 0;
                                    // Short circuit cancelled pending jobs.
                                    cancelled = job->ShouldStop (jobQueue->state->done);
                                    if (!cancelled) {
                                        start = HRTimer::Click ();
                                        job->SetState (Job::Running);
                                        job->Prologue (jobQueue->state->done);
                                        job->Execute (jobQueue->state->done);
                                        job->Epilogue (jobQueue->state->done);
                                        job->Succeed (jobQueue->state->done);
                                        end = HRTimer::Click ();
                                    }
                                    jobQueue->state->FinishedJob (job, start, end);
                                }
                            } while (job != nullptr && cancelled);
                            {
                                LockGuard<SpinLock> guard (context->spinLock);
                                if (!jobQueue->IsPaused () && jobQueue->GetPendingJobCount () != 0) {
                                    // If new jobs arrived, we push back directly while holding the lock.
                                    // This ensures an external EnqJob cannot race our check!
                                    switch (jobQueue->priority) {
                                        case JobQueue::PRIORITY_LOW:
                                            context->low.push_back (jobQueue);
                                            break;
                                        case JobQueue::PRIORITY_NORMAL:
                                            context->normal.push_back (jobQueue);
                                            break;
                                        case JobQueue::PRIORITY_HIGH:
                                            context->high.push_back (jobQueue);
                                            break;
                                    }
                                    // Keep inFlight = true because it is safely back in circulation!
                                }
                                else {
                                    // Truly empty or paused. Ground the flag.
                                    jobQueue->inFlight = false;
                                }
                            }
                        }
                    }
                };
                util::JobQueue::SharedPtr jobQueue = jobQueuePool.GetJobQueue (0);
                if (jobQueue != nullptr) {
                    jobQueue->EnqJob (new JobQueueJob (this));
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Scheduler::Context::DeleteJobQueue (JobQueue *jobQueue) {
            if (jobQueue != nullptr) {
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

        Scheduler::JobQueue *Scheduler::Context::GetNextJobQueue () {
            JobQueue *jobQueue = nullptr;
            LockGuard<SpinLock> guard (spinLock);
            if (!switchingOff) {
                // FAIR SCHEDULING LAW: If fairMode is on, check if
                // normal/low are being starved.
                if (fairMode) {
                    // If high has hogged the CPU for highPriorityThreshold
                    // turns in a row, force a normal/low check.
                    if (highExecutionCount >= highPriorityThreshold &&
                            (!normal.empty () || !low.empty ())) {
                        highExecutionCount = 0; // Reset high counter
                        if (!normal.empty ()) {
                            jobQueue = normal.pop_front ();
                            ++normalExecutionCount;
                        }
                        else {
                            jobQueue = low.pop_front ();
                            normalExecutionCount = 0;
                        }
                    }
                    // If normal has hogged its share
                    // (e.g., normalPriorityThreshold turns)
                    // and low has work, let low leak forward.
                    else if (normalExecutionCount >= normalPriorityThreshold &&
                            !low.empty ()) {
                        normalExecutionCount = 0;
                        jobQueue = low.pop_front ();
                    }
                }
                // FALLBACK TO STRICT PRIORITY: If fair mode didn't
                // trigger, or if the starved lists were empty.
                if (jobQueue == nullptr) {
                    if (!high.empty ()) {
                        jobQueue = high.pop_front ();
                        if (fairMode) {
                            ++highExecutionCount;
                            normalExecutionCount = 0;
                        }
                    }
                    else if (!normal.empty ()) {
                        jobQueue = normal.pop_front ();
                        if (fairMode) {
                            highExecutionCount = 0;
                            ++normalExecutionCount;
                        }
                    }
                    else if (!low.empty ()) {
                        jobQueue = low.pop_front ();
                        if (fairMode) {
                            highExecutionCount = 0;
                            normalExecutionCount = 0;
                        }
                    }
                }
                if (jobQueue != nullptr) {
                    jobQueue->inFlight = true;
                }
            }
            return jobQueue;
        }

        Scheduler::~Scheduler () {
            LockGuard<SpinLock> guard (context->spinLock);
            context->high.clear ();
            context->normal.clear ();
            context->low.clear ();
            context->switchingOff = true;
        }

    } // namespace util
} // namespace thekogans
