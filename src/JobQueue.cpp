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
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/LoggerMgr.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/JobQueue.h"

namespace thekogans {
    namespace util {

        void JobQueue::Worker::Run () throw () {
            struct WorkerInitializer {
                JobQueue &queue;
                explicit WorkerInitializer (JobQueue &queue_) :
                        queue (queue_) {
                    if (queue.workerCallback != 0) {
                        queue.workerCallback->InitializeWorker ();
                    }
                }
                ~WorkerInitializer () {
                    if (queue.workerCallback != 0) {
                        queue.workerCallback->UninitializeWorker ();
                    }
                }
            } workerInitializer (queue);
            while (!queue.done) {
                Job *job = queue.DeqJob ();
                if (job != 0) {
                    ui64 start = HRTimer::Click ();
                    job->Prologue (queue.done);
                    job->Execute (queue.done);
                    job->Epilogue (queue.done);
                    ui64 end = HRTimer::Click ();
                    queue.FinishedJob (*job, start, end);
                }
            }
        }

        JobQueue::JobQueue (
                const std::string &name_,
                Type type_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                ui32 maxPendingJobs_,
                WorkerCallback *workerCallback_) :
                name (name_),
                type (type_),
                workerCount (workerCount_),
                workerPriority (workerPriority_),
                workerAffinity (workerAffinity_),
                maxPendingJobs (maxPendingJobs_),
                workerCallback (workerCallback_),
                done (true),
                jobsNotEmpty (jobsMutex),
                jobFinished (jobsMutex),
                idle (jobsMutex),
                state (Idle),
                busyWorkers (0) {
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
            if (SetDone (false)) {
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

        void JobQueue::Stop (bool cancelPendingJobs) {
            {
                LockGuard<Mutex> guard (workersMutex);
                if (SetDone (true)) {
                    jobsNotEmpty.SignalAll ();
                    struct Callback : public WorkerList::Callback {
                        typedef WorkerList::Callback::result_type result_type;
                        typedef WorkerList::Callback::argument_type argument_type;
                        virtual result_type operator () (argument_type worker) {
                            // Join the worker thread before deleting it to
                            // let it's thread function finish it's tear down.
                            worker->Wait ();
                            delete worker;
                            return true;
                        }
                    } callback;
                    workers.clear (callback);
                    assert (busyWorkers == 0);
                }
            }
            if (cancelPendingJobs) {
                CancelAllJobs ();
            }
        }

        void JobQueue::EnqJob (
                Job::Ptr job,
                bool wait) {
            if (job.Get () != 0) {
                LockGuard<Mutex> guard (jobsMutex);
                if (stats.jobCount < maxPendingJobs) {
                    job->finished = false;
                    if (type == TYPE_FIFO) {
                        jobs.push_back (job.Get ());
                    }
                    else {
                        jobs.push_front (job.Get ());
                    }
                    job->AddRef ();
                    ++stats.jobCount;
                    state = Busy;
                    jobsNotEmpty.Signal ();
                    if (wait) {
                        while (!job->ShouldStop (done) && !job->finished) {
                            jobFinished.Wait ();
                        }
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "JobQueue (%s) max jobs (%u) reached.",
                        !name.empty () ? name.c_str () : "no name",
                        maxPendingJobs);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool JobQueue::CancelJob (const Job::Id &jobId) {
            Job *job = 0;
            {
                LockGuard<Mutex> guard (jobsMutex);
                for (job = jobs.front (); job != 0; job = jobs.next (job)) {
                    if (job->GetId () == jobId) {
                        jobs.erase (job);
                        --stats.jobCount;
                        if (busyWorkers == 0 && jobs.empty ()) {
                            assert (state == Busy);
                            state = Idle;
                            idle.SignalAll ();
                        }
                        break;
                    }
                }
            }
            if (job != 0) {
                job->Cancel ();
                job->Release ();
                jobFinished.SignalAll ();
                return true;
            }
            return false;
        }

        void JobQueue::CancelJobs (const EqualityTest &equalityTest) {
            JobList jobs_;
            {
                LockGuard<Mutex> guard (jobsMutex);
                for (Job *job = jobs.front (); job != 0; job = jobs.next (job)) {
                    if (equalityTest (*job)) {
                        jobs.erase (job);
                        jobs_.push_back (job);
                        --stats.jobCount;
                    }
                }
                if (busyWorkers == 0 && jobs.empty () && !jobs_.empty ()) {
                    assert (state == Busy);
                    state = Idle;
                    idle.SignalAll ();
                }
            }
            if (!jobs_.empty ()) {
                struct Callback : public JobList::Callback {
                    typedef JobList::Callback::result_type result_type;
                    typedef JobList::Callback::argument_type argument_type;
                    virtual result_type operator () (argument_type job) {
                        job->Cancel ();
                        job->Release ();
                        return true;
                    }
                } callback;
                jobs_.clear (callback);
                jobFinished.SignalAll ();
            }
        }

        void JobQueue::CancelAllJobs () {
            JobList jobs_;
            {
                LockGuard<Mutex> guard (jobsMutex);
                if (!jobs.empty ()) {
                    jobs.swap (jobs_);
                    stats.jobCount = 0;
                    if (busyWorkers == 0) {
                        assert (state == Busy);
                        state = Idle;
                        idle.SignalAll ();
                    }
                }
            }
            if (!jobs_.empty ()) {
                struct Callback : public JobList::Callback {
                    typedef JobList::Callback::result_type result_type;
                    typedef JobList::Callback::argument_type argument_type;
                    virtual result_type operator () (argument_type job) {
                        job->Cancel ();
                        job->Release ();
                        return true;
                    }
                } callback;
                jobs_.clear (callback);
                jobFinished.SignalAll ();
            }
        }

        JobQueue::Stats JobQueue::GetStats () {
            LockGuard<Mutex> guard (jobsMutex);
            return stats;
        }

        void JobQueue::WaitForIdle () {
            LockGuard<Mutex> guard (jobsMutex);
            while (state == Busy) {
                idle.Wait ();
            }
        }

        bool JobQueue::IsRunning () {
            LockGuard<Mutex> guard (jobsMutex);
            return !done;
        }

        bool JobQueue::IsEmpty () {
            LockGuard<Mutex> guard (jobsMutex);
            return jobs.empty ();
        }

        bool JobQueue::IsIdle () {
            LockGuard<Mutex> guard (jobsMutex);
            return state == Idle;
        }

        RunLoop::Job *JobQueue::DeqJob () {
            LockGuard<Mutex> guard (jobsMutex);
            while (!done && jobs.empty ()) {
                jobsNotEmpty.Wait ();
            }
            Job *job = 0;
            if (!done && !jobs.empty ()) {
                job = jobs.pop_front ();
                --stats.jobCount;
                ++busyWorkers;
            }
            return job;
        }

        void JobQueue::FinishedJob (
                Job &job,
                ui64 start,
                ui64 end) {
            {
                LockGuard<Mutex> guard (jobsMutex);
                stats.Update (job.id, start, end);
                if (--busyWorkers == 0 && jobs.empty ()) {
                    assert (state == Busy);
                    state = Idle;
                    idle.SignalAll ();
                }
            }
            // NOTE: This work needs to be done without holding a
            // jobsMutex as the job dtor itself can call Stop (as is
            // done by the WorkerPool::WorkerPtr).
            job.finished = true;
            job.Release ();
            jobFinished.SignalAll ();
        }

        bool JobQueue::SetDone (bool value) {
            LockGuard<Mutex> guard (jobsMutex);
            if (done != value) {
                done = value;
                return true;
            }
            return false;
        }

        std::string GlobalJobQueueCreateInstance::name = std::string ();
        RunLoop::Type GlobalJobQueueCreateInstance::type = RunLoop::TYPE_FIFO;
        ui32 GlobalJobQueueCreateInstance::workerCount = 1;
        i32 GlobalJobQueueCreateInstance::workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY;
        ui32 GlobalJobQueueCreateInstance::workerAffinity = UI32_MAX;
        ui32 GlobalJobQueueCreateInstance::maxPendingJobs = UI32_MAX;
        RunLoop::WorkerCallback *GlobalJobQueueCreateInstance::workerCallback = 0;

        void GlobalJobQueueCreateInstance::Parameterize (
                const std::string &name_,
                RunLoop::Type type_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                ui32 maxPendingJobs_,
                RunLoop::WorkerCallback *workerCallback_) {
            name = name_;
            type = type_;
            workerCount = workerCount_;
            workerPriority = workerPriority_;
            workerAffinity = workerAffinity_;
            maxPendingJobs = maxPendingJobs_;
            workerCallback = workerCallback_;
        }

        JobQueue *GlobalJobQueueCreateInstance::operator () () {
            return new JobQueue (
                name,
                type,
                workerCount,
                workerPriority,
                workerAffinity,
                maxPendingJobs,
                workerCallback);
        }

    } // namespace util
} // namespace thekogans
