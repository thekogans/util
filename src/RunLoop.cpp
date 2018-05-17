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
#include "thekogans/util/Event.h"
#include "thekogans/util/HRTimer.h"
#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/RunLoop.h"

namespace thekogans {
    namespace util {

        struct ReleaseJobQueue :
                public Singleton<ReleaseJobQueue, SpinLock>,
                public Thread {
        private:
            RunLoop::JobList jobs;
            Mutex mutex;
            Condition jobsNotEmpty;

        public:
            ReleaseJobQueue () :
                    Thread ("ReleaseJobQueue"),
                    jobsNotEmpty (mutex) {
                Create ();
            }

            void EnqJob (RunLoop::Job *job) {
                LockGuard<Mutex> guard (mutex);
                jobs.push_back (job);
                jobsNotEmpty.Signal ();
            }

        private:
            RunLoop::Job *DeqJob () {
                LockGuard<Mutex> guard (mutex);
                while (jobs.empty ()) {
                    jobsNotEmpty.Wait ();
                }
                return jobs.pop_front ();
            }

            // Thread
            virtual void Run () throw () {
                while (1) {
                    RunLoop::Job *job = DeqJob ();
                    if (job != 0) {
                        job->SetStatus (RunLoop::Job::Completed);
                        job->Release ();
                    }
                }
            }
        };

        void RunLoop::Job::Reset (const RunLoop::Id &runLoopId_) {
            runLoopId = runLoopId_;
            status = Pending;
            disposition = Unknown;
            completed.Reset ();
        }

        void RunLoop::Job::SetStatus (Status status_) {
            status = status_;
            if (status == Completed) {
                completed.Signal ();
            }
        }

        void RunLoop::Job::Finish () {
            if (disposition == Unknown) {
                disposition = Finished;
            }
        }

    #if defined (TOOLCHAIN_OS_Windows)
        void RunLoop::COMInitializer::InitializeWorker () throw () {
            THEKOGANS_UTIL_TRY {
                HRESULT result = CoInitializeEx (0, dwCoInit);
                if (result != S_OK) {
                    THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (result);
                }
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        void RunLoop::COMInitializer::UninitializeWorker () throw () {
            CoUninitialize ();
        }

        void RunLoop::OLEInitializer::InitializeWorker () throw () {
            THEKOGANS_UTIL_TRY {
                HRESULT result = OleInitialize (0);
                if (result != S_OK) {
                    THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (result);
                }
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        void RunLoop::OLEInitializer::UninitializeWorker () throw () {
            OleUninitialize ();
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        void RunLoop::Stats::Update (
                const RunLoop::Job::Id &jobId,
                ui64 start,
                ui64 end) {
            ++totalJobs;
            ui64 ellapsed =
                HRTimer::ComputeElapsedTime (start, end);
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

        std::string RunLoop::Stats::Job::ToString (
                const std::string &name,
                ui32 indentationLevel) const {
            assert (!name.empty ());
            Attributes attributes;
            attributes.push_back (Attribute ("id", id));
            attributes.push_back (Attribute ("startTime", ui64Tostring (startTime)));
            attributes.push_back (Attribute ("endTime", ui64Tostring (endTime)));
            attributes.push_back (Attribute ("totalTime", ui64Tostring (totalTime)));
            return OpenTag (indentationLevel, name.c_str (), attributes, true, true);
        }

        std::string RunLoop::Stats::ToString (
                const std::string &name,
                ui32 indentationLevel) const {
            Attributes attributes;
            attributes.push_back (Attribute ("totalJobs", ui32Tostring (totalJobs)));
            attributes.push_back (Attribute ("totalJobTime", ui64Tostring (totalJobTime)));
            return
                OpenTag (
                    indentationLevel,
                    !name.empty () ? name.c_str () : "RunLoop",
                    attributes,
                    false,
                    true) +
                lastJob.ToString ("last", indentationLevel + 1) +
                minJob.ToString ("min", indentationLevel + 1) +
                maxJob.ToString ("max", indentationLevel + 1) +
                CloseTag (indentationLevel, !name.empty () ? name.c_str () : "RunLoop");
        }

        RunLoop::RunLoop (
                const std::string &name_,
                Type type_,
                ui32 maxPendingJobs_,
                bool done_) :
                id (GUID::FromRandom ().ToString ()),
                name (name_),
                type (type_),
                maxPendingJobs (maxPendingJobs_),
                done (done_),
                jobsNotEmpty (mutex),
                idle (mutex) {
            if ((type != TYPE_FIFO && type != TYPE_LIFO) ||
                    maxPendingJobs == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        std::size_t RunLoop::GetPendingJobCount () {
            LockGuard<Mutex> guard (mutex);
            return pendingJobs.size ();
        }

        std::size_t RunLoop::GetRunningJobCount () {
            LockGuard<Mutex> guard (mutex);
            return runningJobs.size ();
        }

        bool RunLoop::WaitForStart (
                Ptr &runLoop,
                const TimeSpec &sleepTimeSpec,
                const TimeSpec &waitTimeSpec) {
            if (sleepTimeSpec != TimeSpec::Infinite) {
                if (waitTimeSpec == TimeSpec::Infinite) {
                    while (runLoop.Get () == 0 || !runLoop->IsRunning ()) {
                        Sleep (sleepTimeSpec);
                    }
                }
                else {
                    TimeSpec now = GetCurrentTime ();
                    TimeSpec deadline = now + waitTimeSpec;
                    while ((runLoop.Get () == 0 || !runLoop->IsRunning ()) && deadline > now) {
                        Sleep (sleepTimeSpec);
                        now = GetCurrentTime ();
                    }
                }
                return runLoop.Get () != 0 && runLoop->IsRunning ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool RunLoop::EnqJob (
                Job::Ptr job,
                bool wait,
                const TimeSpec &timeSpec) {
            if (job.Get () != 0 && job->IsCompleted ()) {
                LockGuard<Mutex> guard (mutex);
                if (pendingJobs.size () < (std::size_t)maxPendingJobs) {
                    job->Reset (id);
                    if (type == TYPE_FIFO) {
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

        RunLoop::Job::Ptr RunLoop::GetJobWithId (const Job::Id &jobId) {
            LockGuard<Mutex> guard (mutex);
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

        bool RunLoop::WaitForJob (
                Job::Ptr job,
                const TimeSpec &timeSpec) {
            if (job.Get () != 0 && job->GetRunLoopId () == id) {
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

        bool RunLoop::WaitForJob (
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

                RunLoop::Job::Ptr job;

                explicit JobProxy (RunLoop::Job *job_) :
                    job (job_) {}
            };
        #if defined (_MSC_VER)
            #pragma warning (pop)
        #endif // defined (_MSC_VER)

            THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (JobProxy, SpinLock)
        }

        bool RunLoop::WaitForJobs (
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
                LockGuard<Mutex> guard (mutex);
                pendingJobs.for_each (waitForJobCallback);
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

        bool RunLoop::WaitForIdle (const TimeSpec &timeSpec) {
            LockGuard<Mutex> guard (mutex);
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
            return pendingJobs.empty () && runningJobs.empty ();
        }

        bool RunLoop::CancelJob (const Job::Id &jobId) {
            LockGuard<Mutex> guard (mutex);
            for (Job *job = runningJobs.front (); job != 0; job = runningJobs.next (job)) {
                if (job->GetId () == jobId) {
                    // Since the job is already in flight, all we
                    // need to do is cancel it. If it adheres to
                    // the Job best practices, it will check it's
                    // disposition shortly and exit it's Execute
                    // method. The rest will be done by FinishedJob.
                    // Either way, as far as the RunLoop is concerned,
                    // the job is cancelled successfully.
                    job->Cancel ();
                    return true;
                }
            }
            for (Job *job = pendingJobs.front (); job != 0; job = pendingJobs.next (job)) {
                if (job->GetId () == jobId) {
                    job->Cancel ();
                    pendingJobs.erase (job);
                    ReleaseJobQueue::Instance ().EnqJob (job);
                    if (pendingJobs.empty () && runningJobs.empty ()) {
                        idle.SignalAll ();
                    }
                    return true;
                }
            }
            return false;
        }

        void RunLoop::CancelJobs (const EqualityTest &equalityTest) {
            LockGuard<Mutex> guard (mutex);
            for (Job *job = runningJobs.front (); job != 0; job = runningJobs.next (job)) {
                if (equalityTest (*job)) {
                    job->Cancel ();
                }
            }
            struct CancelCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                const EqualityTest &equalityTest;
                JobList &pendingJobs;
                CancelCallback (
                    const EqualityTest &equalityTest_,
                    JobList &pendingJobs_) :
                    equalityTest (equalityTest_),
                    pendingJobs (pendingJobs_) {}
                virtual result_type operator () (argument_type job) {
                    if (equalityTest (*job)) {
                        job->Cancel ();
                        pendingJobs.erase (job);
                        ReleaseJobQueue::Instance ().EnqJob (job);
                    }
                    return true;
                }
            } cancelCallback (equalityTest, pendingJobs);
            pendingJobs.for_each (cancelCallback);
            if (pendingJobs.empty () && runningJobs.empty ()) {
                idle.SignalAll ();
            }
        }

        void RunLoop::CancelAllJobs () {
            LockGuard<Mutex> guard (mutex);
            for (Job *job = runningJobs.front (); job != 0; job = runningJobs.next (job)) {
                job->Cancel ();
            }
            struct CancelCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                virtual result_type operator () (argument_type job) {
                    job->Cancel ();
                    ReleaseJobQueue::Instance ().EnqJob (job);
                    return true;
                }
            } cancelCallback;
            pendingJobs.clear (cancelCallback);
            if (runningJobs.empty ()) {
                idle.SignalAll ();
            }
        }

        RunLoop::Stats RunLoop::GetStats () {
            LockGuard<Mutex> guard (mutex);
            return stats;
        }

        bool RunLoop::IsRunning () {
            LockGuard<Mutex> guard (mutex);
            return !done;
        }

        bool RunLoop::IsIdle () {
            LockGuard<Mutex> guard (mutex);
            return pendingJobs.empty () && runningJobs.empty ();
        }

        RunLoop::Job *RunLoop::DeqJob (bool wait) {
            LockGuard<Mutex> guard (mutex);
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

        void RunLoop::FinishedJob (
                Job *job,
                ui64 start,
                ui64 end) {
            assert (job != 0);
            LockGuard<Mutex> guard (mutex);
            stats.Update (job->GetId (), start, end);
            runningJobs.erase (job);
            ReleaseJobQueue::Instance ().EnqJob (job);
            if (pendingJobs.empty () && runningJobs.empty ()) {
                idle.SignalAll ();
            }
        }

        bool RunLoop::SetDone (bool value) {
            LockGuard<Mutex> guard (mutex);
            if (done != value) {
                done = value;
                return true;
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
