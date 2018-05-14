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
            Mutex jobsMutex;
            Condition jobsNotEmpty;
            Event jobCompleted;

        public:
            ReleaseJobQueue () :
                    Thread ("ReleaseJobQueue"),
                    jobsNotEmpty (jobsMutex) {
                Create ();
            }

            void EnqJob (RunLoop::Job *job) {
                LockGuard<Mutex> guard (jobsMutex);
                jobs.push_back (job);
                jobsNotEmpty.Signal ();
            }

            void Wait (const TimeSpec &timeSpec = TimeSpec::Infinite) {
                jobCompleted.Wait (timeSpec);
            }

        private:
            RunLoop::Job *DeqJob () {
                LockGuard<Mutex> guard (jobsMutex);
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
                        // FIXME: Thundering herd.
                        jobCompleted.SignalAll ();
                    }
                }
            }
        };

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
            attributes.push_back (Attribute ("pendingJobCount", ui32Tostring (pendingJobCount)));
            attributes.push_back (Attribute ("runningJobCount", ui32Tostring (runningJobCount)));
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
                jobsNotEmpty (jobsMutex),
                state (Idle) {
            if ((type != TYPE_FIFO && type != TYPE_LIFO) ||
                    maxPendingJobs == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool RunLoop::WaitForStart (
                Ptr &runLoop,
                const TimeSpec &sleepTimeSpec,
                const TimeSpec &waitTimeSpec) {
            if (sleepTimeSpec != TimeSpec::Infinite) {
                if (waitTimeSpec == TimeSpec::Infinite) {
                    while (runLoop.get () == 0 || !runLoop->IsRunning ()) {
                        Sleep (sleepTimeSpec);
                    }
                }
                else {
                    TimeSpec now = GetCurrentTime ();
                    TimeSpec deadline = now + waitTimeSpec;
                    while ((runLoop.get () == 0 || !runLoop->IsRunning ()) && deadline > now) {
                        Sleep (sleepTimeSpec);
                        now = GetCurrentTime ();
                    }
                }
                return runLoop.get () != 0 && runLoop->IsRunning ();
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
                LockGuard<Mutex> guard (jobsMutex);
                if (stats.pendingJobCount < maxPendingJobs) {
                    job->Reset (id);
                    if (type == TYPE_FIFO) {
                        pendingJobs.push_back (job.Get ());
                    }
                    else {
                        pendingJobs.push_front (job.Get ());
                    }
                    job->AddRef ();
                    ++stats.pendingJobCount;
                    state = Busy;
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
            {
                LockGuard<Mutex> guard (jobsMutex);
                if (runningJobs.for_each (getJobWithIdCallback)) {
                    pendingJobs.for_each (getJobWithIdCallback);
                }
            }
            return getJobWithIdCallback.job;
        }

        bool RunLoop::WaitForJob (
                Job::Ptr job,
                const TimeSpec &timeSpec) {
            if (job.Get () != 0 && job->GetRunLoopId () == id) {
                if (timeSpec == TimeSpec::Infinite) {
                    while (!job->IsCompleted ()) {
                        ReleaseJobQueue::Instance ().Wait ();
                    }
                }
                else {
                    TimeSpec now = GetCurrentTime ();
                    TimeSpec deadline = now + timeSpec;
                    while (!job->IsCompleted () && deadline > now) {
                        ReleaseJobQueue::Instance ().Wait (deadline - now);
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
                LockGuard<Mutex> guard (jobsMutex);
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
                } completedCallback (waitForJobCallback.jobs);
                if (timeSpec == TimeSpec::Infinite) {
                    while (!waitForJobCallback.jobs.for_each (completedCallback)) {
                        ReleaseJobQueue::Instance ().Wait ();
                    }
                }
                else {
                    TimeSpec now = GetCurrentTime ();
                    TimeSpec deadline = now + timeSpec;
                    while (!waitForJobCallback.jobs.for_each (completedCallback) && deadline > now) {
                        ReleaseJobQueue::Instance ().Wait (deadline - now);
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
            if (timeSpec == TimeSpec::Infinite) {
                while (state == Busy) {
                    idle.Wait ();
                }
            }
            else {
                TimeSpec now = GetCurrentTime ();
                TimeSpec deadline = now + timeSpec;
                while (state == Busy && deadline > now) {
                    idle.Wait (deadline - now);
                    now = GetCurrentTime ();
                }
            }
            return state == Idle;
        }

        bool RunLoop::CancelJob (const Job::Id &jobId) {
            LockGuard<Mutex> guard (jobsMutex);
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
                    --stats.pendingJobCount;
                    ReleaseJobQueue::Instance ().EnqJob (job);
                    if (pendingJobs.empty () && runningJobs.empty ()) {
                        assert (state == Busy);
                        state = Idle;
                        idle.SignalAll ();
                    }
                    return true;
                }
            }
            return false;
        }

        void RunLoop::CancelJobs (const EqualityTest &equalityTest) {
            LockGuard<Mutex> guard (jobsMutex);
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
                ui32 count;
                CancelCallback (
                    const EqualityTest &equalityTest_,
                    JobList &pendingJobs_) :
                    equalityTest (equalityTest_),
                    pendingJobs (pendingJobs_),
                    count (0) {}
                virtual result_type operator () (argument_type job) {
                    if (equalityTest (*job)) {
                        job->Cancel ();
                        pendingJobs.erase (job);
                        ReleaseJobQueue::Instance ().EnqJob (job);
                        ++count;
                    }
                    return true;
                }
            } cancelCallback (equalityTest, pendingJobs);
            pendingJobs.for_each (cancelCallback);
            if (cancelCallback.count > 0) {
                stats.pendingJobCount -= cancelCallback.count;
                if (pendingJobs.empty () && runningJobs.empty ()) {
                    assert (state == Busy);
                    state = Idle;
                    idle.SignalAll ();
                }
            }
        }

        void RunLoop::CancelAllJobs () {
            LockGuard<Mutex> guard (jobsMutex);
            for (Job *job = runningJobs.front (); job != 0; job = runningJobs.next (job)) {
                job->Cancel ();
            }
            if (!pendingJobs.empty ()) {
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
                stats.pendingJobCount = 0;
                if (runningJobs.empty ()) {
                    assert (state == Busy);
                    state = Idle;
                    idle.SignalAll ();
                }
            }
        }

        RunLoop::Stats RunLoop::GetStats () {
            LockGuard<Mutex> guard (jobsMutex);
            return stats;
        }

        bool RunLoop::IsRunning () {
            LockGuard<Mutex> guard (jobsMutex);
            return !done;
        }

        bool RunLoop::IsEmpty () {
            LockGuard<Mutex> guard (jobsMutex);
            return pendingJobs.empty ();
        }

        bool RunLoop::IsIdle () {
            return state == Idle;
        }

        RunLoop::Job *RunLoop::DeqJob (bool wait) {
            LockGuard<Mutex> guard (jobsMutex);
            while (!done && pendingJobs.empty () && wait) {
                jobsNotEmpty.Wait ();
            }
            Job *job = 0;
            if (!done && !pendingJobs.empty ()) {
                job = pendingJobs.pop_front ();
                runningJobs.push_back (job);
                --stats.pendingJobCount;
                ++stats.runningJobCount;
            }
            return job;
        }

        void RunLoop::FinishedJob (
                Job *job,
                ui64 start,
                ui64 end) {
            assert (job != 0 && stats.runningJobCount > 0);
            LockGuard<Mutex> guard (jobsMutex);
            stats.Update (job->GetId (), start, end);
            runningJobs.erase (job);
            --stats.runningJobCount;
            ReleaseJobQueue::Instance ().EnqJob (job);
            if (pendingJobs.empty () && runningJobs.empty ()) {
                assert (state == Busy);
                state = Idle;
                idle.SignalAll ();
            }
        }

        bool RunLoop::SetDone (bool value) {
            LockGuard<Mutex> guard (jobsMutex);
            if (done != value) {
                done = value;
                return true;
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
