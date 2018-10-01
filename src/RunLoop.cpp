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

        void RunLoop::Job::Cancel () {
            if (disposition == Unknown) {
                disposition = Cancelled;
                sleeping.Signal ();
            }
        }

        void RunLoop::Job::Reset (const RunLoop::Id &runLoopId_) {
            runLoopId = runLoopId_;
            state = Pending;
            disposition = Unknown;
            completed.Reset ();
        }

        void RunLoop::Job::SetState (State state_) {
            state = state_;
            if (state == Completed) {
                completed.Signal ();
            }
        }

        void RunLoop::Job::Fail (const Exception &exception_) {
            disposition = Failed;
            exception = exception_;
        }

        void RunLoop::Job::Succeed (const THEKOGANS_UTIL_ATOMIC<bool> &done) {
            if (disposition == Unknown) {
                disposition = !done ? Succeeded : Cancelled;
            }
        }

        RunLoop::WorkerInitializer::WorkerInitializer (WorkerCallback *workerCallback_) :
                workerCallback (workerCallback_) {
            if (workerCallback != 0) {
                workerCallback->InitializeWorker ();
            }
        }

        RunLoop::WorkerInitializer::~WorkerInitializer () {
            if (workerCallback != 0) {
                workerCallback->UninitializeWorker ();
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

        const char * const RunLoop::Stats::Job::TAG_JOB = "Job";
        const char * const RunLoop::Stats::Job::ATTR_ID = "Id";
        const char * const RunLoop::Stats::Job::ATTR_START_TIME = "StartTime";
        const char * const RunLoop::Stats::Job::ATTR_END_TIME = "EndTime";
        const char * const RunLoop::Stats::Job::ATTR_TOTAL_TIME = "TotalTime";

        void RunLoop::Stats::Job::Reset () {
            id.clear ();
            startTime = 0;
            endTime = 0;
            totalTime = 0;
        }

    #if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
        void RunLoop::Stats::Job::Parse (const pugi::xml_node &node) {
            id = node.attribute (ATTR_ID).value ();
            startTime = stringToui64 (node.attribute (ATTR_START_TIME).value ());
            endTime = stringToui64 (node.attribute (ATTR_END_TIME).value ());
            totalTime = stringToui64 (node.attribute (ATTR_TOTAL_TIME).value ());
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)

        std::string RunLoop::Stats::Job::ToString (
                std::size_t indentationLevel,
                const char *tagName) const {
            Attributes attributes;
            attributes.push_back (Attribute (ATTR_ID, id));
            attributes.push_back (Attribute (ATTR_START_TIME, ui64Tostring (startTime)));
            attributes.push_back (Attribute (ATTR_END_TIME, ui64Tostring (endTime)));
            attributes.push_back (Attribute (ATTR_TOTAL_TIME, ui64Tostring (totalTime)));
            return OpenTag (indentationLevel, tagName, attributes, true, true);
        }

        const char * const RunLoop::Stats::TAG_RUN_LOOP = "RunLoop";
        const char * const RunLoop::Stats::ATTR_NAME = "Name";
        const char * const RunLoop::Stats::ATTR_TOTAL_JOBS = "TotalJobs";
        const char * const RunLoop::Stats::ATTR_TOTAL_JOB_TIME = "TotalJobTime";
        const char * const RunLoop::Stats::TAG_LAST_JOB = "LastJob";
        const char * const RunLoop::Stats::TAG_MIN_JOB = "MinJob";
        const char * const RunLoop::Stats::TAG_MAX_JOB = "MaxJob";

        void RunLoop::Stats::Reset () {
            totalJobs = 0;
            totalJobTime = 0;
            lastJob.Reset ();
            minJob.Reset ();
            maxJob.Reset ();
        }

    #if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
        void RunLoop::Stats::Parse (const pugi::xml_node &node) {
            name = Decodestring (node.attribute (ATTR_NAME).value ());
            totalJobs = stringToui32 (node.attribute (ATTR_TOTAL_JOBS).value ());
            totalJobTime = stringToui64 (node.attribute (ATTR_TOTAL_JOB_TIME).value ());
            for (pugi::xml_node child = node.first_child ();
                    !child.empty (); child = child.next_sibling ()) {
                if (child.type () == pugi::node_element) {
                    std::string childName = child.name ();
                    if (childName == TAG_LAST_JOB) {
                        lastJob.Parse (child);
                    }
                    else if (childName == TAG_MIN_JOB) {
                        minJob.Parse (child);
                    }
                    else if (childName == TAG_MAX_JOB) {
                        maxJob.Parse (child);
                    }
                }
            }
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)

        std::string RunLoop::Stats::ToString (
                std::size_t indentationLevel,
                const char *tagName) const {
            Attributes attributes;
            attributes.push_back (Attribute (ATTR_NAME, Encodestring (name)));
            attributes.push_back (Attribute (ATTR_TOTAL_JOBS, ui32Tostring (totalJobs)));
            attributes.push_back (Attribute (ATTR_TOTAL_JOB_TIME, ui64Tostring (totalJobTime)));
            return
                OpenTag (indentationLevel, tagName, attributes, false, true) +
                lastJob.ToString (indentationLevel + 1, TAG_LAST_JOB) +
                minJob.ToString (indentationLevel + 1, TAG_MIN_JOB) +
                maxJob.ToString (indentationLevel + 1, TAG_MAX_JOB) +
                CloseTag (indentationLevel, tagName);
        }

        void RunLoop::Stats::Update (
                RunLoop::Job *job,
                ui64 start,
                ui64 end) {
            if (job->IsSucceeded ()) {
                ++totalJobs;
                ui64 ellapsed = HRTimer::ComputeElapsedTime (start, end);
                totalJobTime += ellapsed;
                lastJob = Job (job->GetId (), start, end, ellapsed);
                if (totalJobs == 1) {
                    minJob = Job (job->GetId (), start, end, ellapsed);
                    maxJob = Job (job->GetId (), start, end, ellapsed);
                }
                else if (minJob.totalTime > ellapsed) {
                    minJob = Job (job->GetId (), start, end, ellapsed);
                }
                else if (maxJob.totalTime < ellapsed) {
                    maxJob = Job (job->GetId (), start, end, ellapsed);
                }
            }
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
                stats (name_),
                jobsNotEmpty (jobsMutex),
                idle (jobsMutex),
                paused (false),
                notPaused (jobsMutex) {
            if ((type != TYPE_FIFO && type != TYPE_LIFO) ||
                    maxPendingJobs == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        RunLoop::~RunLoop () {
            // RunLoop derivatives should have disposed of running jobs.
            // The best we can do here is alert the engineer to a leak.
            assert (runningJobs.empty ());
            // Cancel remaining pending jobs to unblock waiters.
            while (!pendingJobs.empty ()) {
                Job *job = pendingJobs.pop_front ();
                if (job != 0) {
                    runningJobs.push_back (job);
                    job->Cancel ();
                    FinishedJob (job, 0, 0);
                }
            }
        }

        std::size_t RunLoop::GetPendingJobCount () {
            LockGuard<Mutex> guard (jobsMutex);
            return pendingJobs.size ();
        }

        std::size_t RunLoop::GetRunningJobCount () {
            LockGuard<Mutex> guard (jobsMutex);
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
                    while ((runLoop.Get () == 0 || !runLoop->IsRunning ()) &&
                            deadline > now) {
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

        namespace {
            bool WaitForJobsHelper (
                    RunLoop::AuxJobList &jobs,
                    const TimeSpec &timeSpec,
                    bool release) {
                if (!jobs.empty ()) {
                    struct CompletedCallback : public RunLoop::AuxJobList::Callback {
                        typedef RunLoop::AuxJobList::Callback::result_type result_type;
                        typedef RunLoop::AuxJobList::Callback::argument_type argument_type;
                        RunLoop::AuxJobList &jobs;
                        bool release;
                        CompletedCallback (
                            RunLoop::AuxJobList &jobs_,
                            bool release_) :
                            jobs (jobs_),
                            release (release_) {}
                        virtual result_type operator () (argument_type job) {
                            if (job->IsCompleted ()) {
                                jobs.erase (job);
                                if (release) {
                                    job->Release ();
                                }
                                return true;
                            }
                            return false;
                        }
                        bool Wait (const TimeSpec &timeSpec = TimeSpec::Infinite) {
                            return jobs.empty () || jobs.front ()->Wait (timeSpec);
                        }
                    } completedCallback (jobs, release);
                    if (timeSpec == TimeSpec::Infinite) {
                        while (!jobs.for_each (completedCallback)) {
                            completedCallback.Wait ();
                        }
                    }
                    else {
                        TimeSpec now = GetCurrentTime ();
                        TimeSpec deadline = now + timeSpec;
                        while (!jobs.for_each (completedCallback) && deadline > now) {
                            completedCallback.Wait (deadline - now);
                            now = GetCurrentTime ();
                        }
                    }
                }
                return jobs.empty ();
            }
        }

        void RunLoop::Pause (bool cancelRunningJobs) {
            struct GetRunningJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                bool cancelRunningJobs;
                AuxJobList runningJobs;
                explicit GetRunningJobsCallback (bool cancelRunningJobs_) :
                    cancelRunningJobs (cancelRunningJobs_) {}
                virtual result_type operator () (argument_type job) {
                    if (cancelRunningJobs) {
                        job->Cancel ();
                    }
                    job->AddRef ();
                    runningJobs.push_back (job);
                    return true;
                }
            } getRunningJobsCallback (cancelRunningJobs);
            {
                LockGuard<Mutex> guard (jobsMutex);
                if (!paused) {
                    paused = true;
                    runningJobs.for_each (getRunningJobsCallback);
                    jobsNotEmpty.SignalAll ();
                }
            }
            WaitForJobsHelper (getRunningJobsCallback.runningJobs, TimeSpec::Infinite, true);
        }

        void RunLoop::Continue () {
            LockGuard<Mutex> guard (jobsMutex);
            if (paused) {
                paused = false;
                notPaused.SignalAll ();
            }
        }

        bool RunLoop::IsPaused () {
            LockGuard<Mutex> guard (jobsMutex);
            return paused;
        }

        bool RunLoop::EnqJob (
                Job::Ptr job,
                bool wait,
                const TimeSpec &timeSpec) {
            if (job.Get () != 0 && job->IsCompleted ()) {
                {
                    LockGuard<Mutex> guard (jobsMutex);
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
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "RunLoop (%s) max jobs (%u) reached.",
                            !name.empty () ? name.c_str () : "no name",
                            maxPendingJobs);
                    }
                }
                return !wait || WaitForJob (job, timeSpec);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool RunLoop::EnqJobFront (
                Job::Ptr job,
                bool wait,
                const TimeSpec &timeSpec) {
            if (job.Get () != 0 && job->IsCompleted ()) {
                {
                    LockGuard<Mutex> guard (jobsMutex);
                    if (pendingJobs.size () < (std::size_t)maxPendingJobs) {
                        job->Reset (id);
                        pendingJobs.push_front (job.Get ());
                        job->AddRef ();
                        jobsNotEmpty.Signal ();
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "RunLoop (%s) max jobs (%u) reached.",
                            !name.empty () ? name.c_str () : "no name",
                            maxPendingJobs);
                    }
                }
                return !wait || WaitForJob (job, timeSpec);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        RunLoop::Job::Ptr RunLoop::GetJob (const Job::Id &jobId) {
            LockGuard<Mutex> guard (jobsMutex);
            struct GetJobCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                const Job::Id &jobId;
                Job::Ptr job;
                explicit GetJobCallback (const Job::Id &jobId_) :
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

        void RunLoop::GetJobs (
                const EqualityTest &equalityTest,
                UserJobList &jobs) {
            LockGuard<Mutex> guard (jobsMutex);
            struct GetJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                const EqualityTest &equalityTest;
                UserJobList &jobs;
                GetJobsCallback (
                    const EqualityTest &equalityTest_,
                    UserJobList &jobs_) :
                    equalityTest (equalityTest_),
                    jobs (jobs_) {}
                virtual result_type operator () (argument_type job) {
                    if (equalityTest (*job)) {
                        job->AddRef ();
                        jobs.push_back (job);
                    }
                    return true;
                }
            } getJobsCallback (equalityTest, jobs);
            runningJobs.for_each (getJobsCallback);
            pendingJobs.for_each (getJobsCallback);
        }

        void RunLoop::GetPendingJobs (UserJobList &pendingJobs) {
            LockGuard<Mutex> guard (jobsMutex);
            struct GetPendingJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                UserJobList &pendingJobs;
                explicit GetPendingJobsCallback (UserJobList &pendingJobs_) :
                    pendingJobs (pendingJobs_) {}
                virtual result_type operator () (argument_type job) {
                    job->AddRef ();
                    pendingJobs.push_back (job);
                    return true;
                }
            } getPendingJobsCallback (pendingJobs);
            this->pendingJobs.for_each (getPendingJobsCallback);
        }

        void RunLoop::GetRunningJobs (UserJobList &runningJobs) {
            LockGuard<Mutex> guard (jobsMutex);
            struct GetRunningJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                UserJobList &runningJobs;
                explicit GetRunningJobsCallback (UserJobList &runningJobs_) :
                    runningJobs (runningJobs_) {}
                virtual result_type operator () (argument_type job) {
                    job->AddRef ();
                    runningJobs.push_back (job);
                    return true;
                }
            } getRunningJobsCallback (runningJobs);
            this->runningJobs.for_each (getRunningJobsCallback);
        }

        void RunLoop::GetAllJobs (
                UserJobList &pendingJobs,
                UserJobList &runningJobs) {
            LockGuard<Mutex> guard (jobsMutex);
            struct GetAllJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                UserJobList &pendingJobs;
                UserJobList &runningJobs;
                bool pending;
                GetAllJobsCallback (
                    UserJobList &pendingJobs_,
                    UserJobList &runningJobs_) :
                    pendingJobs (pendingJobs_),
                    runningJobs (runningJobs_),
                    pending (true) {}
                virtual result_type operator () (argument_type job) {
                    job->AddRef ();
                    if (pending) {
                        pendingJobs.push_back (job);
                    }
                    else {
                        runningJobs.push_back (job);
                    }
                    return true;
                }
            } getAllJobsCallback (pendingJobs, runningJobs);
            this->pendingJobs.for_each (getAllJobsCallback);
            getAllJobsCallback.pending = false;
            this->runningJobs.for_each (getAllJobsCallback);
        }

        bool RunLoop::WaitForJob (
                Job::Ptr job,
                const TimeSpec &timeSpec) {
            if (job.Get () != 0 && job->GetRunLoopId () == id) {
                if (timeSpec == TimeSpec::Infinite) {
                    while (IsRunning () && !job->IsCompleted ()) {
                        job->Wait ();
                    }
                }
                else {
                    TimeSpec now = GetCurrentTime ();
                    TimeSpec deadline = now + timeSpec;
                    while (IsRunning () && !job->IsCompleted () && deadline > now) {
                        job->Wait (deadline - now);
                        now = GetCurrentTime ();
                    }
                }
                return job->IsCompleted ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool RunLoop::WaitForJob (
                const Job::Id &jobId,
                const TimeSpec &timeSpec) {
            Job::Ptr job = GetJob (jobId);
            return job.Get () != 0 && WaitForJob (job, timeSpec);
        }

        bool RunLoop::WaitForJobs (
                const UserJobList &jobs,
                const TimeSpec &timeSpec,
                bool release) {
            struct WaitForJobsCallback : public UserJobList::Callback {
                typedef UserJobList::Callback::result_type result_type;
                typedef UserJobList::Callback::argument_type argument_type;
                AuxJobList jobs;
                virtual result_type operator () (argument_type job) {
                    jobs.push_back (job);
                    return true;
                }
            } waitForJobsCallback;
            jobs.for_each (waitForJobsCallback);
            return WaitForJobsHelper (waitForJobsCallback.jobs, timeSpec, release);
        }

        bool RunLoop::WaitForJobs (
                const EqualityTest &equalityTest,
                const TimeSpec &timeSpec) {
            struct WaitForJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                const EqualityTest &equalityTest;
                AuxJobList jobs;
                explicit WaitForJobsCallback (const EqualityTest &equalityTest_) :
                    equalityTest (equalityTest_) {}
                virtual result_type operator () (argument_type job) {
                    if (equalityTest (*job)) {
                        job->AddRef ();
                        jobs.push_back (job);
                    }
                    return true;
                }
            } waitForJobsCallback (equalityTest);
            {
                LockGuard<Mutex> guard (jobsMutex);
                pendingJobs.for_each (waitForJobsCallback);
                runningJobs.for_each (waitForJobsCallback);
            }
            return WaitForJobsHelper (waitForJobsCallback.jobs, timeSpec, true);
        }

        bool RunLoop::WaitForIdle (const TimeSpec &timeSpec) {
            LockGuard<Mutex> guard (jobsMutex);
            if (timeSpec == TimeSpec::Infinite) {
                while (IsRunning () && (!pendingJobs.empty () || !runningJobs.empty ())) {
                    idle.Wait ();
                }
            }
            else {
                TimeSpec now = GetCurrentTime ();
                TimeSpec deadline = now + timeSpec;
                while (IsRunning () && (!pendingJobs.empty () || !runningJobs.empty ()) &&
                        deadline > now) {
                    idle.Wait (deadline - now);
                    now = GetCurrentTime ();
                }
            }
            return pendingJobs.empty () && runningJobs.empty ();
        }

        bool RunLoop::CancelJob (const Job::Id &jobId) {
            LockGuard<Mutex> guard (jobsMutex);
            struct CancelJobCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                const Job::Id &jobId;
                explicit CancelJobCallback (const Job::Id &jobId_) :
                    jobId (jobId_) {}
                virtual result_type operator () (argument_type job) {
                    if (job->GetId () == jobId) {
                        job->Cancel ();
                        return false;
                    }
                    return true;
                }
            } cancelJobCallback (jobId);
            return
                !runningJobs.for_each (cancelJobCallback) ||
                !pendingJobs.for_each (cancelJobCallback);
        }

        void RunLoop::CancelJobs (
                const UserJobList &jobs,
                bool release) {
            struct CancelJobsCallback : public UserJobList::Callback {
                typedef UserJobList::Callback::result_type result_type;
                typedef UserJobList::Callback::argument_type argument_type;
                bool release;
                explicit CancelJobsCallback (bool release_) :
                    release (release_) {}
                virtual result_type operator () (argument_type job) {
                    job->Cancel ();
                    if (release) {
                        job->Release ();
                    }
                    return true;
                }
            } cancelJobsCallback (release);
            jobs.for_each (cancelJobsCallback);
        }

        void RunLoop::CancelJobs (const EqualityTest &equalityTest) {
            LockGuard<Mutex> guard (jobsMutex);
            struct CancelJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                const EqualityTest &equalityTest;
                explicit CancelJobsCallback (const EqualityTest &equalityTest_) :
                    equalityTest (equalityTest_) {}
                virtual result_type operator () (argument_type job) {
                    if (equalityTest (*job)) {
                        job->Cancel ();
                    }
                    return true;
                }
            } cancelJobsCallback (equalityTest);
            runningJobs.for_each (cancelJobsCallback);
            pendingJobs.for_each (cancelJobsCallback);
        }

        void RunLoop::CancelPendingJobs () {
            LockGuard<Mutex> guard (jobsMutex);
            struct CancelPendingJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                virtual result_type operator () (argument_type job) {
                    job->Cancel ();
                    return true;
                }
            } cancelPendingJobsCallback;
            pendingJobs.for_each (cancelPendingJobsCallback);
        }

        void RunLoop::CancelRunningJobs () {
            LockGuard<Mutex> guard (jobsMutex);
            struct CancelRunningJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                virtual result_type operator () (argument_type job) {
                    job->Cancel ();
                    return true;
                }
            } cancelRunningJobsCallback;
            runningJobs.for_each (cancelRunningJobsCallback);
        }

        void RunLoop::CancelAllJobs () {
            LockGuard<Mutex> guard (jobsMutex);
            struct CancelAllJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                virtual result_type operator () (argument_type job) {
                    job->Cancel ();
                    return true;
                }
            } cancelAllJobsCallback;
            runningJobs.for_each (cancelAllJobsCallback);
            pendingJobs.for_each (cancelAllJobsCallback);
        }

        RunLoop::Stats RunLoop::GetStats () {
            LockGuard<Mutex> guard (jobsMutex);
            return stats;
        }

        void RunLoop::ResetStats () {
            LockGuard<Mutex> guard (jobsMutex);
            stats.Reset ();
        }

        bool RunLoop::IsRunning () {
            return !done;
        }

        bool RunLoop::IsIdle () {
            LockGuard<Mutex> guard (jobsMutex);
            return pendingJobs.empty () && runningJobs.empty ();
        }

        RunLoop::Job *RunLoop::DeqJob (bool wait) {
            LockGuard<Mutex> guard (jobsMutex);
            while (IsRunning () && paused && wait) {
                notPaused.Wait ();
            }
            while (IsRunning () && pendingJobs.empty () && wait) {
                jobsNotEmpty.Wait ();
            }
            Job *job = 0;
            if (IsRunning () && !paused && !pendingJobs.empty ()) {
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
            LockGuard<Mutex> guard (jobsMutex);
            stats.Update (job, start, end);
            runningJobs.erase (job);
            job->SetState (RunLoop::Job::Completed);
            job->Release ();
            if (pendingJobs.empty () && runningJobs.empty ()) {
                idle.SignalAll ();
            }
        }

    } // namespace util
} // namespace thekogans
