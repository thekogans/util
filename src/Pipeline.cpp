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

#include <cassert>
#include "thekogans/util/Heap.h"
#include "thekogans/util/HRTimer.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Pipeline.h"

namespace thekogans {
    namespace util {

        void Pipeline::Job::Reset (const RunLoop::Id &runLoopId_) {
            RunLoop::Job::Reset (runLoopId_);
            if (runLoopId_ == pipeline.GetId ()) {
                stage = 0;
                start = 0;
                end = 0;
            }
        }

        void Pipeline::Job::SetStatus (Status status_) {
            status = status_;
            if (IsRunning ()) {
                if (stage == 0) {
                    start = HRTimer::Click ();
                    Begin (pipeline.done);
                }
            }
            else if (IsCompleted ()) {
                LockGuard<Mutex> guard (pipeline.stagesMutex);
                if (stage == pipeline.stages.size ()) {
                    completed.Signal ();
                }
                else {
                    if (stage < pipeline.stages.size () - 1) {
                        THEKOGANS_UTIL_TRY {
                            pipeline.stages[++stage]->EnqJob (RunLoop::Job::Ptr (this));
                            return;
                        }
                        THEKOGANS_UTIL_CATCH (Exception) {
                            Fail ();
                            THEKOGANS_UTIL_LOG_ERROR ("%s\n", exception.Report ().c_str ());
                        }
                    }
                    stage = pipeline.stages.size ();
                    End (pipeline.done);
                    end = HRTimer::Click ();
                    pipeline.FinishedJob (this, start, end);
                }
            }
        }

        void Pipeline::Worker::Run () throw () {
            struct WorkerInitializer {
                Pipeline &pipeline;
                explicit WorkerInitializer (Pipeline &pipeline_) :
                        pipeline (pipeline_) {
                    if (pipeline.workerCallback != 0) {
                        pipeline.workerCallback->InitializeWorker ();
                    }
                }
                ~WorkerInitializer () {
                    if (pipeline.workerCallback != 0) {
                        pipeline.workerCallback->UninitializeWorker ();
                    }
                }
            } workerInitializer (pipeline);
            while (!pipeline.done) {
                Job *job = pipeline.DeqJob ();
                if (job != 0) {
                    // Short circuit cancelled pending jobs.
                    if (!job->IsCancelled ()) {
                        if (job->stage < pipeline.stages.size ()) {
                            LockGuard<Mutex> guard (pipeline.stagesMutex);
                            THEKOGANS_UTIL_TRY {
                                pipeline.stages[job->stage]->EnqJob (RunLoop::Job::Ptr (job));
                                continue;
                            }
                            THEKOGANS_UTIL_CATCH (Exception) {
                                job->Fail ();
                                THEKOGANS_UTIL_LOG_ERROR ("%s\n", exception.Report ().c_str ());
                            }
                        }
                        else {
                            THEKOGANS_UTIL_LOG_ERROR (
                                "Job stage (%u) exceeds pipeline stage count (%u).\n",
                                job->stage,
                                pipeline.stages.size ());
                        }
                    }
                    pipeline.FinishedJob (job, job->start, job->end);
                }
            }
        }

        Pipeline::Pipeline (
                const std::string &name_,
                RunLoop::Type type_,
                ui32 maxPendingJobs_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                RunLoop::WorkerCallback *workerCallback_,
                const Stage *begin,
                const Stage *end) :
                id (GUID::FromRandom ().ToString ()),
                name (name_),
                type (type_),
                maxPendingJobs (maxPendingJobs_),
                done (true),
                jobsNotEmpty (jobsMutex),
                idle (jobsMutex),
                workerCount (workerCount_),
                workerPriority (workerPriority_),
                workerAffinity (workerAffinity_),
                workerCallback (workerCallback_) {
            if ((type == RunLoop::TYPE_FIFO || type == RunLoop::TYPE_LIFO) &&
                    maxPendingJobs == 0 && workerCount > 0) {
                Start ();
                if (begin != 0 && end != 0) {
                    for (; begin != end; ++begin) {
                        AddStage (*begin);
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool Pipeline::WaitForStart (
                Ptr &pipeline,
                const TimeSpec &sleepTimeSpec,
                const TimeSpec &waitTimeSpec) {
            if (sleepTimeSpec != TimeSpec::Infinite) {
                if (waitTimeSpec == TimeSpec::Infinite) {
                    while (pipeline.Get () == 0 || !pipeline->IsRunning ()) {
                        Sleep (sleepTimeSpec);
                    }
                }
                else {
                    TimeSpec now = GetCurrentTime ();
                    TimeSpec deadline = now + waitTimeSpec;
                    while ((pipeline.Get () == 0 || !pipeline->IsRunning ()) &&
                            deadline > now) {
                        Sleep (sleepTimeSpec);
                        now = GetCurrentTime ();
                    }
                }
                return pipeline.Get () != 0 && pipeline->IsRunning ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Pipeline::AddStage (const Stage &stage) {
            LockGuard<Mutex> guard (stagesMutex);
            stages.push_back (
                JobQueue::Ptr (
                    new JobQueue (
                        stage.name,
                        stage.type,
                        stage.maxPendingJobs,
                        stage.workerCount,
                        stage.workerPriority,
                        stage.workerAffinity,
                        stage.workerCallback)));
        }

        RunLoop::Stats Pipeline::GetStageStats (std::size_t stage) {
            LockGuard<Mutex> guard (stagesMutex);
            if (stage < stages.size ()) {
                return stages[stage]->GetStats ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Pipeline::GetStagesStats (std::vector<RunLoop::Stats> &stats) {
            LockGuard<Mutex> guard (stagesMutex);
            stats.resize (stages.size ());
            for (std::size_t i = 0, count = stages.size (); i < count; ++i) {
                stats[i] = stages[i]->GetStats ();
            }
        }

        void Pipeline::Start () {
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
                        new Worker (
                            *this,
                            workerName,
                            workerPriority,
                            workerAffinity));
                }
            }
        }

        void Pipeline::Stop (bool cancelPendingJobs) {
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
                    assert (runningJobs.empty ());
                }
            }
            if (cancelPendingJobs) {
                CancelAllJobs ();
            }
        }

        bool Pipeline::EnqJob (
                Job::Ptr job,
                bool wait,
                const TimeSpec &timeSpec) {
            if (job.Get () != 0 && job->IsCompleted () && job->GetPipelineId () == id) {
                LockGuard<Mutex> guard (jobsMutex);
                if (pendingJobs.size () < (std::size_t)maxPendingJobs) {
                    job->Reset (id);
                    if (type == RunLoop::TYPE_FIFO) {
                        pendingJobs.push_back (job.Get ());
                    }
                    else {
                        pendingJobs.push_front (job.Get ());
                    }
                    job->AddRef ();
                    jobsNotEmpty.Signal ();
                    return !wait || WaitForJob (job, timeSpec);
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "RunLoop (%s) max jobs (%u) reached.",
                        !name.empty () ? name.c_str () : "no name",
                        maxPendingJobs);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Pipeline::Job::Ptr Pipeline::GetJobWithId (const Job::Id &jobId) {
            LockGuard<Mutex> guard (jobsMutex);
            struct GetJobWithIdCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                const Job::Id &jobId;
                Job::Ptr job;
                explicit GetJobWithIdCallback (const Job::Id &jobId_) :
                    jobId (jobId_) {}
                virtual result_type operator () (argument_type job_) {
                    if (job_->GetId () == jobId) {
                        job.Reset (job_);
                        return false;
                    }
                    return true;
                }
            } getJobWithIdCallback (jobId);
            if (runningJobs.for_each (getJobWithIdCallback)) {
                pendingJobs.for_each (getJobWithIdCallback);
            }
            return getJobWithIdCallback.job;
        }

        bool Pipeline::WaitForJob (
                Job::Ptr job,
                const TimeSpec &timeSpec) {
            if (job.Get () != 0 && job->GetPipelineId () == id) {
                if (timeSpec == TimeSpec::Infinite) {
                    while (!job->IsCompleted ()) {
                        job->WaitCompleted ();
                    }
                }
                else {
                    TimeSpec now = GetCurrentTime ();
                    TimeSpec deadline = now + timeSpec;
                    while (!job->IsCompleted () && deadline > now) {
                        job->WaitCompleted (deadline - now);
                        now = GetCurrentTime ();
                    }
                    if (!job->IsCompleted ()) {
                        return false;
                    }
                }
                return true;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool Pipeline::WaitForJob (
                const Job::Id &jobId,
                const TimeSpec &timeSpec) {
            Job::Ptr job = GetJobWithId (jobId);
            return job.Get () != 0 && WaitForJob (job, timeSpec);
        }

        bool Pipeline::WaitForJobs (
                const RunLoop::EqualityTest &equalityTest,
                const TimeSpec &timeSpec) {
            struct WaitForJobCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                const RunLoop::EqualityTest &equalityTest;
                RunLoop::AuxJobList jobs;
                explicit WaitForJobCallback (const RunLoop::EqualityTest &equalityTest_) :
                    equalityTest (equalityTest_) {}
                virtual result_type operator () (argument_type job) {
                    if (equalityTest (*job)) {
                        jobs.push_back (job);
                    }
                    return true;
                }
            } waitForJobCallback (equalityTest);
            {
                LockGuard<Mutex> guard (jobsMutex);
                pendingJobs.for_each (waitForJobCallback);
                runningJobs.for_each (waitForJobCallback);
            }
            if (!waitForJobCallback.jobs.empty ()) {
                struct CompletedCallback : public RunLoop::AuxJobList::Callback {
                    typedef RunLoop::AuxJobList::Callback::result_type result_type;
                    typedef RunLoop::AuxJobList::Callback::argument_type argument_type;
                    RunLoop::AuxJobList &jobs;
                    explicit CompletedCallback (RunLoop::AuxJobList &jobs_) :
                        jobs (jobs_) {}
                    virtual result_type operator () (argument_type job) {
                        if (job->IsCompleted ()) {
                            jobs.erase (job);
                            return true;
                        }
                        return false;
                    }
                    bool Wait (const TimeSpec &timeSpec = TimeSpec::Infinite) {
                        return jobs.empty () || jobs.front ()->WaitCompleted (timeSpec);
                    }
                } completedCallback (waitForJobCallback.jobs);
                if (timeSpec == TimeSpec::Infinite) {
                    while (!waitForJobCallback.jobs.for_each (completedCallback)) {
                        completedCallback.Wait ();
                    }
                }
                else {
                    TimeSpec now = GetCurrentTime ();
                    TimeSpec deadline = now + timeSpec;
                    while (!waitForJobCallback.jobs.for_each (completedCallback) && deadline > now) {
                        completedCallback.Wait (deadline - now);
                        now = GetCurrentTime ();
                    }
                    if (!waitForJobCallback.jobs.empty ()) {
                        return false;
                    }
                }
            }
            return true;
        }

        bool Pipeline::WaitForIdle (const TimeSpec &timeSpec) {
            LockGuard<Mutex> guard (jobsMutex);
            if (timeSpec == TimeSpec::Infinite) {
                while (!pendingJobs.empty () || !runningJobs.empty ()) {
                    idle.Wait ();
                }
            }
            else {
                TimeSpec now = GetCurrentTime ();
                TimeSpec deadline = now + timeSpec;
                while ((!pendingJobs.empty () || !runningJobs.empty ()) && deadline > now) {
                    idle.Wait (deadline - now);
                    now = GetCurrentTime ();
                }
            }
            return runningJobs.empty ();
        }

        bool Pipeline::CancelJob (const Job::Id &jobId) {
            LockGuard<Mutex> guard (jobsMutex);
            struct CancelCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                const Job::Id &jobId;
                explicit CancelCallback (const Job::Id &jobId_) :
                    jobId (jobId_) {}
                virtual result_type operator () (argument_type job) {
                    if (job->GetId () == jobId) {
                        job->Cancel ();
                        return false;
                    }
                    return true;
                }
            } cancelCallback (jobId);
            return
                !runningJobs.for_each (cancelCallback) ||
                !pendingJobs.for_each (cancelCallback);
        }

        void Pipeline::CancelJobs (const RunLoop::EqualityTest &equalityTest) {
            LockGuard<Mutex> guard (jobsMutex);
            struct CancelCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                const RunLoop::EqualityTest &equalityTest;
                explicit CancelCallback (const RunLoop::EqualityTest &equalityTest_) :
                    equalityTest (equalityTest_) {}
                virtual result_type operator () (argument_type job) {
                    if (equalityTest (*job)) {
                        job->Cancel ();
                    }
                    return true;
                }
            } cancelCallback (equalityTest);
            runningJobs.for_each (cancelCallback);
            pendingJobs.for_each (cancelCallback);
        }

        void Pipeline::CancelAllJobs () {
            LockGuard<Mutex> guard (jobsMutex);
            struct CancelCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                virtual result_type operator () (argument_type job) {
                    job->Cancel ();
                    return true;
                }
            } cancelCallback;
            runningJobs.for_each (cancelCallback);
            pendingJobs.for_each (cancelCallback);
        }

        RunLoop::Stats Pipeline::GetStats () {
            LockGuard<Mutex> guard (jobsMutex);
            return stats;
        }

        bool Pipeline::IsRunning () {
            LockGuard<Mutex> guard (jobsMutex);
            return !done;
        }

        bool Pipeline::IsIdle () {
            LockGuard<Mutex> guard (jobsMutex);
            return pendingJobs.empty () && runningJobs.empty ();
        }

        Pipeline::Job *Pipeline::DeqJob (bool wait) {
            LockGuard<Mutex> guard (jobsMutex);
            while (!done && pendingJobs.empty () && wait) {
                jobsNotEmpty.Wait ();
            }
            Job *job = 0;
            if (!done && !pendingJobs.empty ()) {
                job = pendingJobs.pop_front ();
                runningJobs.push_back (job);
            }
            return job;
        }

        void Pipeline::FinishedJob (
                Job *job,
                ui64 start,
                ui64 end) {
            assert (job != 0);
            LockGuard<Mutex> guard (jobsMutex);
            stats.Update (job, start, end);
            runningJobs.erase (job);
            job->SetStatus (RunLoop::Job::Completed);
            job->Release ();
            if (pendingJobs.empty () && runningJobs.empty ()) {
                idle.SignalAll ();
            }
        }

        bool Pipeline::SetDone (bool value) {
            LockGuard<Mutex> guard (jobsMutex);
            if (done != value) {
                done = value;
                return true;
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
