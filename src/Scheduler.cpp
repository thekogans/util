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
#include "thekogans/util/Scheduler.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (Scheduler::JobQueue, SpinLock)

        void Scheduler::JobQueue::Enq (Job::UniquePtr job) {
            if (job.get () != 0) {
                LockGuard<SpinLock> guard (spinLock);
                jobs.push_back (job.release ());
                if (!inList && !inFlight) {
                    scheduler.AddJobQueue (this, true);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Scheduler::JobQueue::EnqFront (Job::UniquePtr job) {
            if (job.get () != 0) {
                LockGuard<SpinLock> guard (spinLock);
                jobs.push_front (job.release ());
                if (!inList && !inFlight) {
                    scheduler.AddJobQueue (this, true);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Scheduler::JobQueue::Job::UniquePtr Scheduler::JobQueue::Deq () {
            LockGuard<SpinLock> guard (spinLock);
            Job::UniquePtr job;
            if (!jobs.empty ()) {
                job.reset (jobs.pop_front ());
            }
            return job;
        }

        void Scheduler::JobQueue::Flush () {
            LockGuard<SpinLock> guard (spinLock);
            if (!jobs.empty ()) {
                struct Callback : public JobList::Callback {
                    typedef JobList::Callback::result_type result_type;
                    typedef JobList::Callback::argument_type argument_type;
                    virtual result_type operator () (argument_type job) {
                        delete job;
                        return true;
                    }
                } callback;
                jobs.clear (callback);
                scheduler.RemoveJobQueue (this);
            }
        }

        void Scheduler::AddJobQueue (
                JobQueue *jobQueue,
                bool scheduleWorker) {
            if (jobQueue != 0) {
                LockGuard<SpinLock> guard (spinLock);
                jobQueue->AddRef ();
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
                if (scheduleWorker) {
                    WorkerPool::WorkerPtr::SharedPtr workerPtr (
                        new WorkerPool::WorkerPtr (workerPool, 0));
                    if (workerPtr.get () != 0 && workerPtr->worker != 0) {
                        struct WorkerJob : util::JobQueue::Job {
                            WorkerPool::WorkerPtr::SharedPtr workerPtr;
                            Scheduler &scheduler;

                            WorkerJob (
                                WorkerPool::WorkerPtr::SharedPtr workerPtr_,
                                Scheduler &scheduler_) :
                                workerPtr (workerPtr_),
                                scheduler (scheduler_) {}

                            virtual void Execute (volatile const bool &done) throw () {
                                // Use a warm worker to minimize cache thrashing.
                                while (!done) {
                                    Scheduler::JobQueue::Ptr jobQueue =
                                        scheduler.GetNextJobQueue ();
                                    if (jobQueue.Get () == 0) {
                                        return;
                                    }
                                    Scheduler::JobQueue::Job::UniquePtr job = jobQueue->Deq ();
                                    if (job.get () != 0) {
                                        job->Execute (done);
                                    }
                                    {
                                        // Put the queue in the back of it's priority list
                                        // so that we respect the scheduling policy we advertise
                                        // (see GetNextJobQueue).
                                        LockGuard<SpinLock> guard (jobQueue->spinLock);
                                        jobQueue->inFlight = false;
                                        if (!jobQueue->jobs.empty ()) {
                                            scheduler.AddJobQueue (jobQueue.Get (), false);
                                        }
                                    }
                                }
                            }
                        };
                        util::JobQueue::Job::UniquePtr job (
                            new WorkerJob (workerPtr, *this));
                        if (job.get () != 0) {
                            (*workerPtr)->Enq (std::move (job));
                        }
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Scheduler::RemoveJobQueue (JobQueue *jobQueue) {
            if (jobQueue != 0) {
                LockGuard<SpinLock> guard (spinLock);
                if (jobQueue->inList) {
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
                    jobQueue->Release ();
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Scheduler::JobQueue::Ptr Scheduler::GetNextJobQueue () {
            // Priority based, round-robin, O(1) scheduler!
            LockGuard<SpinLock> guard (spinLock);
            JobQueue::Ptr jobQueue;
            if (!high.empty ()) {
                LockGuard<SpinLock> guard (high.front ()->spinLock);
                high.front ()->inFlight = true;
                jobQueue.Reset (high.pop_front ());
            }
            else if (!normal.empty ()) {
                LockGuard<SpinLock> guard (normal.front ()->spinLock);
                normal.front ()->inFlight = true;
                jobQueue.Reset (normal.pop_front ());
            }
            else if (!low.empty ()) {
                LockGuard<SpinLock> guard (low.front ()->spinLock);
                low.front ()->inFlight = true;
                jobQueue.Reset (low.pop_front ());
            }
            if (jobQueue.Get () != 0) {
                jobQueue->Release ();
            }
            return jobQueue;
        }

        std::string GlobalSchedulerCreateInstance::workerName = std::string ();
        ui32 GlobalSchedulerCreateInstance::minWorkers = SystemInfo::Instance ().GetCPUCount ();
        ui32 GlobalSchedulerCreateInstance::maxWorkers = SystemInfo::Instance ().GetCPUCount () * 2;
        i32 GlobalSchedulerCreateInstance::workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY;
        ui32 GlobalSchedulerCreateInstance::workerAffinity = UI32_MAX;

        void GlobalSchedulerCreateInstance::Parameterize (
                const std::string &workerName_,
                ui32 minWorkers_,
                ui32 maxWorkers_,
                i32 workerPriority_,
                ui32 workerAffinity_) {
            workerName = workerName_;
            minWorkers = minWorkers_;
            maxWorkers = maxWorkers_;
            workerPriority = workerPriority_;
            workerAffinity = workerAffinity_;
        }

        Scheduler *GlobalSchedulerCreateInstance::operator () () {
            return new Scheduler (
                workerName,
                minWorkers,
                maxWorkers,
                workerPriority,
                workerAffinity);
        }

    } // namespace util
} // namespace thekogans
