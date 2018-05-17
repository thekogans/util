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
#include "thekogans/util/Pipeline.h"

namespace thekogans {
    namespace util {

        void Pipeline::Job::Reset () {
            stage = 0;
            start = 0;
            end = 0;
        }

        void Pipeline::Job::SetStatus (Status status_) {
            if (status == Pending && status_ == Running && stage == 0) {
                status = status_;
                start = HRTimer::Click ();
                Begin (pipeline.done);
            }
            else if (status == Running && status_ == Completed) {
                status = status_;
                stage = IsFinished () ? ++stage : pipeline.stages.size ();
                if (stage < pipeline.stages.size ()) {
                    pipeline.stages[stage]->EnqJob (RunLoop::Job::Ptr (this));
                }
                else {
                    End (pipeline.done);
                    end = HRTimer::Click ();
                    pipeline.FinishedJob (this, start, end);
                }
            }
            else if (status == Completed && status_ == Completed &&
                    stage == pipeline.stages.size ()) {
                completed.SignalAll ();
            }
        }

        Pipeline::Pipeline (
                const std::string &name_,
                const Stage *begin,
                const Stage *end) :
                id (GUID::FromRandom ().ToString ()),
                name (name_),
                done (true),
                idle (jobMutex) {
            if (begin != 0 && end != 0) {
                for (; begin != end; ++begin) {
                    AddStage (*begin);
                }
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
                    while ((pipeline.Get () == 0 || !pipeline->IsRunning ()) && deadline > now) {
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
            LockGuard<Mutex> guard (stageMutex);
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
            LockGuard<Mutex> guard (stageMutex);
            if (stage < stages.size ()) {
                return stages[stage]->GetStats ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Pipeline::GetStagesStats (std::vector<RunLoop::Stats> &stats) {
            LockGuard<Mutex> guard (stageMutex);
            stats.resize (stages.size ());
            for (std::size_t i = 0, count = stages.size (); i < count; ++i) {
                stats[i] = stages[i]->GetStats ();
            }
        }

        void Pipeline::Start () {
            LockGuard<Mutex> guard (stageMutex);
            if (SetDone (false)) {
                for (std::size_t i = 0, count = stages.size (); i < count; ++i) {
                    stages[i]->Start ();
                }
            }
        }

        void Pipeline::Stop (bool cancelRunningJobs) {
            {
                LockGuard<Mutex> guard (stageMutex);
                if (SetDone (true)) {
                    for (std::size_t i = 0, count = stages.size (); i < count; ++i) {
                        stages[i]->Stop (cancelRunningJobs);
                    }
                }
            }
            if (cancelRunningJobs) {
                CancelAllJobs ();
            }
        }

        bool Pipeline::EnqJob (
                Job::Ptr job,
                bool wait,
                const TimeSpec &timeSpec) {
            if (job.Get () != 0 && job->IsCompleted () && job->GetPipelineId () == id) {
                job->Reset ();
                {
                    LockGuard<Mutex> guard (stageMutex);
                    stages[job->stage]->EnqJob (RunLoop::Job::Ptr (job.Get ()));
                }
                {
                    LockGuard<Mutex> guard (jobMutex);
                    runningJobs.push_back (job.Get ());
                }
                job->AddRef ();
                return !wait || WaitForJob (job, timeSpec);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Pipeline::Job::Ptr Pipeline::GetJobWithId (const Job::Id &jobId) {
            LockGuard<Mutex> guard (jobMutex);
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
            runningJobs.for_each (getJobWithIdCallback);
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

        namespace {
            struct JobProxy;
            enum {
                JOB_PROXY_LIST_ID
            };
            typedef IntrusiveList<JobProxy, JOB_PROXY_LIST_ID> JobProxyList;

        #if defined (_MSC_VER)
            #pragma warning (push)
            #pragma warning (disable : 4275)
        #endif // defined (_MSC_VER)
            struct JobProxy : public JobProxyList::Node {
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (JobProxy, SpinLock)

                Pipeline::Job::Ptr job;

                explicit JobProxy (Pipeline::Job *job_) :
                    job (job_) {}
            };
        #if defined (_MSC_VER)
            #pragma warning (pop)
        #endif // defined (_MSC_VER)

            THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (JobProxy, SpinLock)
        }

        bool Pipeline::WaitForJobs (
                const EqualityTest &equalityTest,
                const TimeSpec &timeSpec) {
            struct WaitForJobCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                const EqualityTest &equalityTest;
                JobProxyList jobs;
                explicit WaitForJobCallback (const EqualityTest &equalityTest_) :
                    equalityTest (equalityTest_) {}
                ~WaitForJobCallback () {
                    struct ClearCallback : public JobProxyList::Callback {
                        typedef JobProxyList::Callback::result_type result_type;
                        typedef JobProxyList::Callback::argument_type argument_type;
                        virtual result_type operator () (argument_type jobProxy) {
                            delete jobProxy;
                            return true;
                        }
                    } clearCallback;
                    jobs.clear (clearCallback);
                }
                virtual result_type operator () (argument_type job) {
                    if (equalityTest (*job)) {
                        jobs.push_back (new JobProxy (job));
                    }
                    return true;
                }
            } waitForJobCallback (equalityTest);
            {
                LockGuard<Mutex> guard (jobMutex);
                runningJobs.for_each (waitForJobCallback);
            }
            if (!waitForJobCallback.jobs.empty ()) {
                struct CompletedCallback : public JobProxyList::Callback {
                    typedef JobProxyList::Callback::result_type result_type;
                    typedef JobProxyList::Callback::argument_type argument_type;
                    JobProxyList &jobs;
                    explicit CompletedCallback (JobProxyList &jobs_) :
                        jobs (jobs_) {}
                    virtual result_type operator () (argument_type jobProxy) {
                        if (jobProxy->job->IsCompleted ()) {
                            jobs.erase (jobProxy);
                            delete jobProxy;
                            return true;
                        }
                        return false;
                    }
                    bool Wait (const TimeSpec &timeSpec = TimeSpec::Infinite) {
                        return jobs.empty () || jobs.front ()->job->WaitCompleted (timeSpec);
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
            LockGuard<Mutex> guard (jobMutex);
            if (timeSpec == TimeSpec::Infinite) {
                while (!runningJobs.empty ()) {
                    idle.Wait ();
                }
            }
            else {
                TimeSpec now = GetCurrentTime ();
                TimeSpec deadline = now + timeSpec;
                while (!runningJobs.empty () && deadline > now) {
                    idle.Wait (deadline - now);
                    now = GetCurrentTime ();
                }
            }
            return runningJobs.empty ();
        }

        bool Pipeline::CancelJob (const Job::Id &jobId) {
            LockGuard<Mutex> guard (jobMutex);
            for (Job *job = runningJobs.front (); job != 0; job = runningJobs.next (job)) {
                if (job->GetId () == jobId) {
                    // Since the job is already in flight, all we
                    // need to do is cancel it. If it adheres to
                    // the Job best practices, it will check it's
                    // disposition shortly and exit it's Execute
                    // method. The rest will be done by FinishedJob.
                    // Either way, as far as the Pipeline is concerned,
                    // the job is cancelled successfully.
                    job->Cancel ();
                    return true;
                }
            }
            return false;
        }

        void Pipeline::CancelJobs (const EqualityTest &equalityTest) {
            LockGuard<Mutex> guard (jobMutex);
            for (Job *job = runningJobs.front (); job != 0; job = runningJobs.next (job)) {
                if (equalityTest (*job)) {
                    job->Cancel ();
                }
            }
            if (runningJobs.empty ()) {
                idle.SignalAll ();
            }
        }

        void Pipeline::CancelAllJobs () {
            LockGuard<Mutex> guard (jobMutex);
            for (Job *job = runningJobs.front (); job != 0; job = runningJobs.next (job)) {
                job->Cancel ();
            }
            if (runningJobs.empty ()) {
                idle.SignalAll ();
            }
        }

        RunLoop::Stats Pipeline::GetStats () {
            LockGuard<Mutex> guard (jobMutex);
            return stats;
        }

        bool Pipeline::IsRunning () {
            LockGuard<Mutex> guard (jobMutex);
            return !done;
        }

        bool Pipeline::IsIdle () {
            LockGuard<Mutex> guard (jobMutex);
            return runningJobs.empty ();
        }

        void Pipeline::FinishedJob (
                Job *job,
                ui64 start,
                ui64 end) {
            assert (job != 0);
            LockGuard<Mutex> guard (jobMutex);
            stats.Update (job->GetId (), start, end);
            runningJobs.erase (job);
            job->Release ();
            if (runningJobs.empty ()) {
                idle.SignalAll ();
            }
        }

        bool Pipeline::SetDone (bool value) {
            LockGuard<Mutex> guard (jobMutex);
            if (done != value) {
                done = value;
                return true;
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
