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
#include "thekogans/util/Exception.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/JobQueue.h"

namespace thekogans {
    namespace util {

        void JobQueue::Stats::Update (
                const JobQueue::Job::Id &jobId,
                ui64 start,
                ui64 end) {
            ++totalJobs;
            ui64 ellapsed =
                HRTimer::ComputeEllapsedTime (start, end);
            totalJobTime += ellapsed;
            lastJob = Job (jobId, start, end, ellapsed);
            if (totalJobs == 1) {
                minJob = Job (jobId, start, end, ellapsed);
                maxJob = Job (jobId, start, end, ellapsed);
            }
            else if (minJob.totalTime > ellapsed) {
                minJob = Job (jobId, start, end, ellapsed);
            }
            else if (maxJob.totalTime < ellapsed) {
                maxJob = Job (jobId, start, end, ellapsed);
            }
        }

        void JobQueue::Worker::Run () throw () {
            while (!queue.done) {
                Job::UniquePtr job = queue.Deq ();
                if (job.get () != 0) {
                    ui64 start = HRTimer::Click ();
                    job->Prologue (queue.done);
                    job->Execute (queue.done);
                    job->Epilogue (queue.done);
                    ui64 end = HRTimer::Click ();
                    if (job->finished != 0) {
                        *job->finished = true;
                    }
                    queue.FinishedJob (job->GetId (), start, end);
                }
            }
            if (!exited) {
                queue.workersBarrier.Wait ();
            }
        }

        JobQueue::JobQueue (
                const std::string &name_,
                Type type_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                ui32 maxPendingJobs_) :
                name (name_),
                type (type_),
                workerCount (workerCount_),
                workerPriority (workerPriority_),
                workerAffinity (workerAffinity_),
                maxPendingJobs (maxPendingJobs_),
                done (true),
                jobsNotEmpty (jobsMutex),
                jobFinished (jobsMutex),
                idle (jobsMutex),
                state (Idle),
                busyWorkers (0),
                workersBarrier (workerCount + 1) {
            if ((type == TYPE_FIFO || type == TYPE_LIFO) &&
                    workerCount > 0 && maxPendingJobs > 0) {
                Start ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void JobQueue::Start () {
            LockGuard<Mutex> guard (workersMutex);
            if (done) {
                done = false;
                for (ui32 i = 0; i < workerCount; ++i) {
                    std::string workerName;
                    if (!name.empty ()) {
                        if (workerCount > 1) {
                            workerName = FormatString ("%s-%u", name.c_str (), i);
                        }
                        else {
                            workerName = name;
                        }
                    }
                    workers.push_back (
                        new Worker (*this, workerName, workerPriority, workerAffinity));
                }
            }
        }

        void JobQueue::Stop () {
            LockGuard<Mutex> guard (workersMutex);
            if (!done) {
                done = true;
                CancelAll ();
                jobsNotEmpty.SignalAll ();
                workersBarrier.Wait ();
                struct Callback : public WorkerList::Callback {
                    typedef WorkerList::Callback::result_type result_type;
                    typedef WorkerList::Callback::argument_type argument_type;
                    virtual result_type operator () (argument_type worker) {
                        // All workers have hit the barrier but their
                        // respective threads might not have exited
                        // yet. Join the worker thread before deleting
                        // it to let it's thread function finish it's
                        // tear down.
                        worker->Wait ();
                        delete worker;
                        return true;
                    }
                } callback;
                assert (busyWorkers == 0);
                workers.clear (callback);
                stats.jobCount = 0;
                if (state == Busy) {
                    state = Idle;
                    idle.SignalAll ();
                }
            }
        }

        JobQueue::Job::Id JobQueue::Enq (
                Job::UniquePtr job,
                bool wait) {
            if (job.get () != 0) {
                LockGuard<Mutex> guard (jobsMutex);
                if (stats.jobCount < maxPendingJobs) {
                    volatile bool finished = false;
                    if (wait) {
                        job->finished = &finished;
                    }
                    Job::Id jobId = job->GetId ();
                    if (type == TYPE_FIFO) {
                        jobs.push_back (job.release ());
                    }
                    else {
                        jobs.push_front (job.release ());
                    }
                    ++stats.jobCount;
                    jobsNotEmpty.Signal ();
                    state = Busy;
                    while (wait && !finished) {
                        jobFinished.Wait ();
                    }
                    return jobId;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Max jobs (%u) reached.",
                        maxPendingJobs);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool JobQueue::Cancel (const Job::Id &jobId) {
            if (!jobId.empty ()) {
                LockGuard<Mutex> guard (jobsMutex);
                for (Job *job = jobs.front (); job != 0; job = jobs.next (job)) {
                    if (job->GetId () == jobId) {
                        jobs.erase (job);
                        delete job;
                        --stats.jobCount;
                        if (busyWorkers == 0 && jobs.empty ()) {
                            state = Idle;
                            idle.SignalAll ();
                        }
                        return true;
                    }
                }
            }
            return false;
        }

        void JobQueue::CancelAll () {
            LockGuard<Mutex> guard (jobsMutex);
            struct Callback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                virtual result_type operator () (argument_type job) {
                    delete job;
                    return true;
                }
            } callback;
            jobs.clear (callback);
            stats.jobCount = 0;
            if (busyWorkers == 0) {
                state = Idle;
                idle.SignalAll ();
            }
        }

        void JobQueue::WaitForIdle () {
            LockGuard<Mutex> guard (jobsMutex);
            while (!done && state == Busy) {
                idle.Wait ();
            }
        }

        JobQueue::Stats JobQueue::GetStats () {
            LockGuard<Mutex> guard (jobsMutex);
            return stats;
        }

        bool JobQueue::IsEmpty () {
            LockGuard<Mutex> guard (jobsMutex);
            return jobs.empty ();
        }

        bool JobQueue::IsIdle () {
            LockGuard<Mutex> guard (jobsMutex);
            return state == Idle;
        }

        JobQueue::Job::UniquePtr JobQueue::Deq () {
            LockGuard<Mutex> guard (jobsMutex);
            while (!done && jobs.empty ()) {
                jobsNotEmpty.Wait ();
            }
            Job::UniquePtr job;
            if (!jobs.empty ()) {
                job.reset (jobs.pop_front ());
                if (job.get () != 0) {
                    --stats.jobCount;
                    ++busyWorkers;
                }
            }
            return job;
        }

        void JobQueue::FinishedJob (
                const Job::Id &jobId,
                ui64 start,
                ui64 end) {
            LockGuard<Mutex> guard (jobsMutex);
            if (--busyWorkers == 0 && jobs.empty ()) {
                state = Idle;
                idle.SignalAll ();
            }
            stats.Update (jobId, start, end);
            jobFinished.SignalAll ();
        }

        std::string GlobalJobQueueCreateInstance::name = std::string ();
        JobQueue::Type GlobalJobQueueCreateInstance::type = JobQueue::TYPE_FIFO;
        ui32 GlobalJobQueueCreateInstance::workerCount = 1;
        i32 GlobalJobQueueCreateInstance::workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY;
        ui32 GlobalJobQueueCreateInstance::workerAffinity = UI32_MAX;
        ui32 GlobalJobQueueCreateInstance::maxPendingJobs = UI32_MAX;

        void GlobalJobQueueCreateInstance::Parameterize (
                const std::string &name_,
                JobQueue::Type type_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                ui32 maxPendingJobs_) {
            name = name_;
            type = type_;
            workerCount = workerCount_;
            workerPriority = workerPriority_;
            workerAffinity = workerAffinity_;
            maxPendingJobs = maxPendingJobs_;
        }

        JobQueue *GlobalJobQueueCreateInstance::operator () () {
            return new JobQueue (
                name,
                type,
                workerCount,
                workerPriority,
                workerAffinity,
                maxPendingJobs);
        }

    } // namespace util
} // namespace thekogans
