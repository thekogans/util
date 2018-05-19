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

        void Scheduler::JobQueue::Stop (bool cancelPendingJobs) {
            if (cancelPendingJobs) {
                CancelAllJobs ();
                WaitForIdle ();
            }
        }

        bool Scheduler::JobQueue::EnqJob (
                Job::Ptr job,
                bool wait,
                const TimeSpec &timeSpec) {
            bool result = RunLoop::EnqJob (job);
            if (result) {
                if (!inList && !inFlight) {
                    scheduler.AddJobQueue (this, true);
                }
                result = !wait || WaitForJob (job, timeSpec);
            }
            return result;
        }

        void Scheduler::AddJobQueue (
                JobQueue *jobQueue,
                bool scheduleWorker) {
            if (jobQueue != 0) {
                {
                    LockGuard<SpinLock> guard (spinLock);
                    switch (jobQueue->priority) {
                        case JobQueue::PRIORITY_LOW:
                            low.push_back (jobQueue);
                            break;
                        case JobQueue::PRIORITY_NORMAL:
                            normal.push_back (jobQueue);
                            break;
                        case JobQueue::PRIORITY_HIGH:
                            high.push_back (jobQueue);
                            break;
                    }
                }
                jobQueue->AddRef ();
                if (scheduleWorker) {
                    struct WorkerJob : public util::RunLoop::Job {
                        WorkerPool::WorkerPtr::Ptr workerPtr;
                        Scheduler &scheduler;

                        WorkerJob (
                            WorkerPool::WorkerPtr::Ptr workerPtr_,
                            Scheduler &scheduler_) :
                            workerPtr (workerPtr_),
                            scheduler (scheduler_) {}

                        virtual void Execute (const THEKOGANS_UTIL_ATOMIC<bool> &done) throw () {
                            // Use a warm worker to minimize cache thrashing.
                            while (!done) {
                                Scheduler::JobQueue *jobQueue = scheduler.GetNextJobQueue ();
                                if (jobQueue != 0) {
                                    RunLoop::Job *job = jobQueue->DeqJob ();
                                    if (job != 0) {
                                        ui64 start = 0;
                                        ui64 end = 0;
                                        // Short circuit cancelled pending jobs.
                                        if (!job->ShouldStop (done)) {
                                            start = HRTimer::Click ();
                                            job->SetStatus (Job::Running);
                                            job->Prologue (done);
                                            job->Execute (done);
                                            job->Epilogue (done);
                                            job->Succeed ();
                                            end = HRTimer::Click ();
                                        }
                                        jobQueue->FinishedJob (job, start, end);
                                    }
                                    {
                                        // Put the queue in the back of it's priority list
                                        // so that we respect the scheduling policy we advertise
                                        // (see GetNextJobQueue).
                                        jobQueue->inFlight = false;
                                        if (jobQueue->GetPendingJobCount () != 0) {
                                            scheduler.AddJobQueue (jobQueue, false);
                                        }
                                    }
                                }
                            }
                        }
                    };
                    WorkerPool::WorkerPtr::Ptr workerPtr = workerPool.GetWorkerPtr (0);
                    if (workerPtr.Get () != 0) {
                        (*workerPtr)->EnqJob (
                            util::RunLoop::Job::Ptr (
                                new WorkerJob (workerPtr, *this)));
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Scheduler::JobQueue *Scheduler::GetNextJobQueue () {
            // Priority based, round-robin, O(1) scheduler!
            JobQueue *jobQueue = 0;
            {
                LockGuard<SpinLock> guard (spinLock);
                if (!high.empty ()) {
                    jobQueue = high.pop_front ();
                }
                else if (!normal.empty ()) {
                    jobQueue = normal.pop_front ();
                }
                else if (!low.empty ()) {
                    jobQueue = low.pop_front ();
                }
            }
            if (jobQueue != 0) {
                jobQueue->inFlight = true;
                jobQueue->Release ();
            }
            return jobQueue;
        }

        ui32 GlobalSchedulerCreateInstance::minWorkers = SystemInfo::Instance ().GetCPUCount ();
        ui32 GlobalSchedulerCreateInstance::maxWorkers = SystemInfo::Instance ().GetCPUCount () * 2;
        std::string GlobalSchedulerCreateInstance::name = std::string ();
        RunLoop::Type GlobalSchedulerCreateInstance::type = RunLoop::TYPE_FIFO;
        ui32 GlobalSchedulerCreateInstance::maxPendingJobs = UI32_MAX;
        ui32 GlobalSchedulerCreateInstance::workerCount = 1;
        i32 GlobalSchedulerCreateInstance::workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY;
        ui32 GlobalSchedulerCreateInstance::workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY;
        RunLoop::WorkerCallback *GlobalSchedulerCreateInstance::workerCallback = 0;

        void GlobalSchedulerCreateInstance::Parameterize (
                ui32 minWorkers_,
                ui32 maxWorkers_,
                const std::string &name_,
                RunLoop::Type type_,
                ui32 maxPendingJobs_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                RunLoop::WorkerCallback *workerCallback_) {
            minWorkers = minWorkers_;
            maxWorkers = maxWorkers_;
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
                minWorkers,
                maxWorkers,
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
