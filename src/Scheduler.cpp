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

        void Scheduler::JobQueue::Stop (
                bool cancelRunningJobs,
                bool cancelPendingJobs) {
            struct SavePendingJobs {
                JobList &pendingJobs;
                Mutex &jobsMutex;
                JobList savedPendingJobs;
                SavePendingJobs (
                    JobList &pendingJobs_,
                    Mutex &jobsMutex_) :
                    pendingJobs (pendingJobs_),
                    jobsMutex (jobsMutex_) {}
                ~SavePendingJobs () {
                    LockGuard<Mutex> guard (jobsMutex);
                    savedPendingJobs.swap (pendingJobs);
                }
                void Save () {
                    LockGuard<Mutex> guard (jobsMutex);
                    savedPendingJobs.swap (pendingJobs);
                }
            } savePendingJobs (pendingJobs, jobsMutex);
            if (cancelRunningJobs && cancelPendingJobs) {
                CancelAllJobs ();
            }
            else if (!cancelRunningJobs && cancelPendingJobs) {
                CancelPendingJobs ();
            }
            else if (cancelRunningJobs && !cancelPendingJobs) {
                savePendingJobs.Save ();
                CancelRunningJobs ();
            }
            else if (!cancelRunningJobs && !cancelPendingJobs) {
                savePendingJobs.Save ();
            }
            WaitForIdle ();
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
            jobQueuePool.WaitForIdle ();
            struct ReleaseCallback : public JobQueueList::Callback {
                typedef JobQueueList::Callback::result_type result_type;
                typedef JobQueueList::Callback::argument_type argument_type;
                virtual result_type operator () (argument_type jobQueue) {
                    jobQueue->Release ();
                    return true;
                }
            } releaseCallback;
            high.clear (releaseCallback);
            normal.clear (releaseCallback);
            low.clear (releaseCallback);
        }

        void Scheduler::AddJobQueue (
                JobQueue *jobQueue,
                bool scheduleJobQueue) {
            if (jobQueue != 0) {
                {
                    // NOTE: It's okay for push_back to fail. It simply means
                    // the queue is already in it's proper list and will be
                    // returned by GetNextJobQueue when it's time to execute
                    // one of it's jobs.
                    LockGuard<SpinLock> guard (spinLock);
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
                    jobQueue->inFlight = false;
                    jobQueue->AddRef ();
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

                        virtual void Execute (const THEKOGANS_UTIL_ATOMIC<bool> &done) throw () {
                            JobQueue::Ptr jobQueue;
                            while (!ShouldStop (done) && (jobQueue = scheduler.GetNextJobQueue ()).Get () != 0) {
                                RunLoop::Job *job;
                                bool cancelled;
                                // Skip over cancelled jobs.
                                do {
                                    job = jobQueue->DeqJob (false);
                                    if (job != 0) {
                                        ui64 start = 0;
                                        ui64 end = 0;
                                        // Short circuit cancelled pending jobs.
                                        cancelled = job->ShouldStop (done);
                                        if (!cancelled) {
                                            start = HRTimer::Click ();
                                            job->SetState (Job::Running);
                                            job->Prologue (done);
                                            job->Execute (done);
                                            job->Epilogue (done);
                                            job->Succeed (done);
                                            end = HRTimer::Click ();
                                        }
                                        jobQueue->FinishedJob (job, start, end);
                                    }
                                } while (job != 0 && cancelled);
                                if (jobQueue->GetPendingJobCount () != 0) {
                                    scheduler.AddJobQueue (jobQueue.Get (), false);
                                }
                            }
                        }
                    };
                    util::JobQueue::Ptr jobQueue = jobQueuePool.GetJobQueue (0);
                    if (jobQueue.Get () != 0) {
                        jobQueue->EnqJob (
                            RunLoop::Job::Ptr (
                                new JobQueueJob (jobQueue, *this)));
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Scheduler::JobQueue::Ptr Scheduler::GetNextJobQueue () {
            JobQueue::Ptr jobQueue;
            {
                // Priority based, round-robin, O(1) scheduler!
                LockGuard<SpinLock> guard (spinLock);
                if (!high.empty () && !high.front ()->inFlight) {
                    jobQueue.Reset (high.pop_front ());
                }
                else if (!normal.empty () && !normal.front ()->inFlight) {
                    jobQueue.Reset (normal.pop_front ());
                }
                else if (!low.empty () && !low.front ()->inFlight) {
                    jobQueue.Reset (low.pop_front ());
                }
                if (jobQueue.Get () != 0) {
                    jobQueue->inFlight = true;
                    jobQueue->Release ();
                }
            }
            return jobQueue;
        }

        ui32 GlobalSchedulerCreateInstance::minJobQueues = SystemInfo::Instance ().GetCPUCount ();
        ui32 GlobalSchedulerCreateInstance::maxJobQueues = SystemInfo::Instance ().GetCPUCount () * 2;
        std::string GlobalSchedulerCreateInstance::name = std::string ();
        RunLoop::Type GlobalSchedulerCreateInstance::type = RunLoop::TYPE_FIFO;
        ui32 GlobalSchedulerCreateInstance::maxPendingJobs = UI32_MAX;
        ui32 GlobalSchedulerCreateInstance::workerCount = 1;
        i32 GlobalSchedulerCreateInstance::workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY;
        ui32 GlobalSchedulerCreateInstance::workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY;
        RunLoop::WorkerCallback *GlobalSchedulerCreateInstance::workerCallback = 0;

        void GlobalSchedulerCreateInstance::Parameterize (
                ui32 minJobQueues_,
                ui32 maxJobQueues_,
                const std::string &name_,
                RunLoop::Type type_,
                ui32 maxPendingJobs_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                RunLoop::WorkerCallback *workerCallback_) {
            minJobQueues = minJobQueues_;
            maxJobQueues = maxJobQueues_;
            name = name_;
            type = type_;
            maxPendingJobs = maxPendingJobs_;
            workerCount = workerCount_;
            workerPriority = workerPriority_;
            workerAffinity = workerAffinity_;
            workerCallback = workerCallback_;
        }

        Scheduler *GlobalSchedulerCreateInstance::operator () () {
            return new Scheduler (
                minJobQueues,
                maxJobQueues,
                name,
                type,
                maxPendingJobs,
                workerCount,
                workerPriority,
                workerAffinity,
                workerCallback);
        }

    } // namespace util
} // namespace thekogans
