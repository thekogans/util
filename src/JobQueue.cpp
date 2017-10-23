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

    #if defined (TOOLCHAIN_OS_Windows)
        void JobQueue::COMInitializer::InitializeWorker () throw () {
            THEKOGANS_UTIL_TRY {
                HRESULT result = CoInitializeEx (0, dwCoInit);
                if (result != S_OK) {
                    THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (result);
                }
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        void JobQueue::COMInitializer::UninitializeWorker () throw () {
            CoUninitialize ();
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

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
                Job::Ptr job = queue.Deq ();
                if (job.Get () != 0) {
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

        void JobQueue::Stop () {
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
                CancelAll ();
            }
        }

        void JobQueue::Enq (
                Job &job,
                bool wait) {
            LockGuard<Mutex> guard (jobsMutex);
            if (!done) {
                if (stats.jobCount < maxPendingJobs) {
                    job.finished = false;
                    if (type == TYPE_FIFO) {
                        jobs.push_back (&job);
                    }
                    else {
                        jobs.push_front (&job);
                    }
                    job.AddRef ();
                    ++stats.jobCount;
                    jobsNotEmpty.Signal ();
                    state = Busy;
                    if (wait) {
                        while (!job.ShouldStop (done) && !job.finished) {
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
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "JobQueue (%s) is not running.",
                    !name.empty () ? name.c_str () : "no name");
            }
        }

        bool JobQueue::Cancel (const Job::Id &jobId) {
            LockGuard<Mutex> guard (jobsMutex);
            for (Job *job = jobs.front (); job != 0; job = jobs.next (job)) {
                if (job->GetId () == jobId) {
                    jobs.erase (job);
                    job->Cancel ();
                    job->Release ();
                    --stats.jobCount;
                    jobFinished.SignalAll ();
                    if (busyWorkers == 0 && jobs.empty ()) {
                        state = Idle;
                        idle.SignalAll ();
                    }
                    return true;
                }
            }
            return false;
        }

        void JobQueue::CancelAll () {
            LockGuard<Mutex> guard (jobsMutex);
            if (!jobs.empty ()) {
                assert (state == Busy);
                struct Callback : public JobList::Callback {
                    typedef JobList::Callback::result_type result_type;
                    typedef JobList::Callback::argument_type argument_type;
                    virtual result_type operator () (argument_type job) {
                        job->Cancel ();
                        job->Release ();
                        return true;
                    }
                } callback;
                jobs.clear (callback);
                stats.jobCount = 0;
                jobFinished.SignalAll ();
                if (busyWorkers == 0) {
                    state = Idle;
                    idle.SignalAll ();
                }
            }
        }

        void JobQueue::WaitForIdle () {
            LockGuard<Mutex> guard (jobsMutex);
            while (state == Busy) {
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

        JobQueue::Job::Ptr JobQueue::Deq () {
            LockGuard<Mutex> guard (jobsMutex);
            while (!done && jobs.empty ()) {
                jobsNotEmpty.Wait ();
            }
            Job::Ptr job;
            if (!jobs.empty ()) {
                job.Reset (jobs.pop_front ());
                if (job.Get () != 0) {
                    job->Release ();
                    --stats.jobCount;
                    ++busyWorkers;
                }
            }
            return job;
        }

        void JobQueue::FinishedJob (
                Job &job,
                ui64 start,
                ui64 end) {
            LockGuard<Mutex> guard (jobsMutex);
            assert (state == Busy);
            stats.Update (job.id, start, end);
            job.finished = true;
            jobFinished.SignalAll ();
            if (--busyWorkers == 0 && jobs.empty ()) {
                state = Idle;
                idle.SignalAll ();
            }
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
        JobQueue::Type GlobalJobQueueCreateInstance::type = JobQueue::TYPE_FIFO;
        ui32 GlobalJobQueueCreateInstance::workerCount = 1;
        i32 GlobalJobQueueCreateInstance::workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY;
        ui32 GlobalJobQueueCreateInstance::workerAffinity = UI32_MAX;
        ui32 GlobalJobQueueCreateInstance::maxPendingJobs = UI32_MAX;
        JobQueue::WorkerCallback *GlobalJobQueueCreateInstance::workerCallback = 0;

        void GlobalJobQueueCreateInstance::Parameterize (
                const std::string &name_,
                JobQueue::Type type_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                ui32 maxPendingJobs_,
                JobQueue::WorkerCallback *workerCallback_) {
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
