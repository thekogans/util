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
            SetState (Pending);
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

        void RunLoop::Job::Succeed (const std::atomic<bool> &done) {
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
                if (FAILED (result)) {
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
                if (FAILED (result)) {
                    THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (result);
                }
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        void RunLoop::OLEInitializer::UninitializeWorker () throw () {
            OleUninitialize ();
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        RunLoop::JobExecutionPolicy::JobExecutionPolicy (std::size_t maxJobs_) :
                maxJobs (maxJobs_) {
            if (maxJobs == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void RunLoop::FIFOJobExecutionPolicy::EnqJob (
                State &state,
                Job *job) {
            if (state.pendingJobs.size () < maxJobs) {
                state.pendingJobs.push_back (job);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "RunLoop (%s) max jobs (%u) reached.",
                    !state.name.empty () ? state.name.c_str () : "no name",
                    maxJobs);
            }
        }

        void RunLoop::FIFOJobExecutionPolicy::EnqJobFront (
                State &state,
                Job *job) {
            if (state.pendingJobs.size () < maxJobs) {
                state.pendingJobs.push_front (job);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "RunLoop (%s) max jobs (%u) reached.",
                    !state.name.empty () ? state.name.c_str () : "no name",
                    maxJobs);
            }
        }

        RunLoop::Job *RunLoop::FIFOJobExecutionPolicy::DeqJob (State &state) {
            return !state.pendingJobs.empty () ? state.pendingJobs.pop_front () : 0;
        }

        void RunLoop::LIFOJobExecutionPolicy::EnqJob (
                State &state,
                Job *job) {
            if (state.pendingJobs.size () < maxJobs) {
                state.pendingJobs.push_front (job);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "RunLoop (%s) max jobs (%u) reached.",
                    !state.name.empty () ? state.name.c_str () : "no name",
                    maxJobs);
            }
        }

        void RunLoop::LIFOJobExecutionPolicy::EnqJobFront (
                State &state,
                Job *job) {
            EnqJob (state, job);
        }

        RunLoop::Job *RunLoop::LIFOJobExecutionPolicy::DeqJob (State &state) {
            return !state.pendingJobs.empty () ? state.pendingJobs.pop_front () : 0;
        }

        #if !defined (THEKOGANS_UTIL_MIN_RUN_LOOP_STATS_JOBS_IN_PAGE)
            #define THEKOGANS_UTIL_MIN_RUN_LOOP_STATS_JOBS_IN_PAGE 64
        #endif // !defined (THEKOGANS_UTIL_MIN_RUN_LOOP_STATS_JOBS_IN_PAGE)

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (
            RunLoop::Stats::Job,
            1,
            SpinLock,
            THEKOGANS_UTIL_MIN_RUN_LOOP_STATS_JOBS_IN_PAGE,
            DefaultAllocator::Instance ())

        RunLoop::Stats::Job &RunLoop::Stats::Job::operator = (const Job &job) {
            if (&job != this) {
                id  = job.id;
                startTime = job.startTime;
                endTime = job.endTime;
                totalTime = job.totalTime;
            }
            return *this;
        }

        void RunLoop::Stats::Job::Reset () {
            id.clear ();
            startTime = 0;
            endTime = 0;
            totalTime = 0;
        }

        std::size_t RunLoop::Stats::Job::Size () const {
            return Serializer::Size (id) +
                Serializer::Size (startTime) +
                Serializer::Size (endTime) +
                Serializer::Size (totalTime);
        }

        void RunLoop::Stats::Job::Read (
                const BinHeader & /*header*/,
                Serializer &serializer) {
            serializer >> id >> startTime >> endTime >> totalTime;
        }

        void RunLoop::Stats::Job::Write (Serializer &serializer) const {
            serializer << id << startTime << endTime << totalTime;
        }

        const char * const RunLoop::Stats::Job::TAG_JOB = "Job";
        const char * const RunLoop::Stats::Job::ATTR_ID = "Id";
        const char * const RunLoop::Stats::Job::ATTR_START_TIME = "StartTime";
        const char * const RunLoop::Stats::Job::ATTR_END_TIME = "EndTime";
        const char * const RunLoop::Stats::Job::ATTR_TOTAL_TIME = "TotalTime";

        void RunLoop::Stats::Job::Read (
                const TextHeader & /*header*/,
                const pugi::xml_node &node) {
            id = node.attribute (ATTR_ID).value ();
            startTime = stringToui64 (node.attribute (ATTR_START_TIME).value ());
            endTime = stringToui64 (node.attribute (ATTR_END_TIME).value ());
            totalTime = stringToui64 (node.attribute (ATTR_TOTAL_TIME).value ());
        }

        void RunLoop::Stats::Job::Write (pugi::xml_node &node) const {
            node.append_attribute (ATTR_ID).set_value (id.c_str ());
            node.append_attribute (ATTR_START_TIME).set_value (ui64Tostring (startTime).c_str ());
            node.append_attribute (ATTR_END_TIME).set_value (ui64Tostring (endTime).c_str ());
            node.append_attribute (ATTR_TOTAL_TIME).set_value (ui64Tostring (totalTime).c_str ());
        }

        void RunLoop::Stats::Job::Read (
                const TextHeader & /*header*/,
                const JSON::Object &object) {
            id = object.Get<JSON::String> (ATTR_ID)->value;
            startTime = object.Get<JSON::Number> (ATTR_START_TIME)->To<ui64> ();
            endTime = object.Get<JSON::Number> (ATTR_END_TIME)->To<ui64> ();
            totalTime = object.Get<JSON::Number> (ATTR_TOTAL_TIME)->To<ui64> ();
        }

        void RunLoop::Stats::Job::Write (JSON::Object &object) const {
            object.Add<const std::string &> (ATTR_ID, id);
            object.Add (ATTR_START_TIME, startTime);
            object.Add (ATTR_END_TIME, endTime);
            object.Add (ATTR_TOTAL_TIME, totalTime);
        }

        #if !defined (THEKOGANS_UTIL_MIN_RUN_LOOP_STATS_IN_PAGE)
            #define THEKOGANS_UTIL_MIN_RUN_LOOP_STATS_IN_PAGE 16
        #endif // !defined (THEKOGANS_UTIL_MIN_RUN_LOOP_STATS_IN_PAGE)

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (
            RunLoop::Stats,
            1,
            SpinLock,
            THEKOGANS_UTIL_MIN_RUN_LOOP_STATS_IN_PAGE,
            DefaultAllocator::Instance ())

        RunLoop::Stats &RunLoop::Stats::operator = (const Stats &stats) {
            if (&stats != this) {
                id = stats.id;
                name = stats.name;
                totalJobs = stats.totalJobs;
                totalJobTime = stats.totalJobTime;
                lastJob = stats.lastJob;
                minJob = stats.minJob;
                maxJob = stats.maxJob;
            }
            return *this;
        }

        void RunLoop::Stats::Reset () {
            totalJobs = 0;
            totalJobTime = 0;
            lastJob.Reset ();
            minJob.Reset ();
            maxJob.Reset ();
        }

        std::size_t RunLoop::Stats::Size () const {
            return Serializer::Size (id) +
                Serializer::Size (name) +
                Serializer::Size (totalJobs) +
                Serializer::Size (totalJobTime) +
                Serializable::Size (lastJob) +
                Serializable::Size (minJob) +
                Serializable::Size (maxJob);
        }

        void RunLoop::Stats::Read (
                const BinHeader & /*header*/,
                Serializer &serializer) {
            serializer >> id >> name >> totalJobs >> totalJobTime >> lastJob >> minJob >> maxJob;
        }

        void RunLoop::Stats::Write (Serializer &serializer) const {
            serializer << id << name << totalJobs << totalJobTime << lastJob << minJob << maxJob;
        }

        const char * const RunLoop::Stats::TAG_RUN_LOOP = "RunLoop";
        const char * const RunLoop::Stats::ATTR_ID = "Id";
        const char * const RunLoop::Stats::ATTR_NAME = "Name";
        const char * const RunLoop::Stats::ATTR_TOTAL_JOBS = "TotalJobs";
        const char * const RunLoop::Stats::ATTR_TOTAL_JOB_TIME = "TotalJobTime";
        const char * const RunLoop::Stats::TAG_LAST_JOB = "LastJob";
        const char * const RunLoop::Stats::TAG_MIN_JOB = "MinJob";
        const char * const RunLoop::Stats::TAG_MAX_JOB = "MaxJob";

        void RunLoop::Stats::Read (
                const TextHeader & /*header*/,
                const pugi::xml_node &node) {
            id = node.attribute (ATTR_ID).value ();
            name = Decodestring (node.attribute (ATTR_NAME).value ());
            totalJobs = stringTosize_t (node.attribute (ATTR_TOTAL_JOBS).value ());
            totalJobTime = stringToui64 (node.attribute (ATTR_TOTAL_JOB_TIME).value ());
            for (pugi::xml_node child = node.first_child ();
                    !child.empty (); child = child.next_sibling ()) {
                if (child.type () == pugi::node_element) {
                    std::string childName = child.name ();
                    if (childName == TAG_LAST_JOB) {
                        child >> lastJob;
                    }
                    else if (childName == TAG_MIN_JOB) {
                        child >> minJob;
                    }
                    else if (childName == TAG_MAX_JOB) {
                        child >> maxJob;
                    }
                }
            }
        }

        void RunLoop::Stats::Write (pugi::xml_node &node) const {
            node.append_attribute (ATTR_ID).set_value (id.c_str ());
            node.append_attribute (ATTR_NAME).set_value (Encodestring (name).c_str ());
            node.append_attribute (ATTR_TOTAL_JOBS).set_value (size_tTostring (totalJobs).c_str ());
            node.append_attribute (ATTR_TOTAL_JOB_TIME).set_value (ui64Tostring (totalJobTime).c_str ());
            {
                pugi::xml_node child = node.append_child (TAG_LAST_JOB);
                child << lastJob;
            }
            {
                pugi::xml_node child = node.append_child (TAG_MIN_JOB);
                child << minJob;
            }
            {
                pugi::xml_node child = node.append_child (TAG_MAX_JOB);
                child << maxJob;
            }
        }

        void RunLoop::Stats::Read (
                const TextHeader & /*header*/,
                const JSON::Object &object) {
            id = object.Get<JSON::String> (ATTR_ID)->value;
            name = object.Get<JSON::String> (ATTR_NAME)->value;
            totalJobs = object.Get<JSON::Number> (ATTR_TOTAL_JOBS)->To<SizeT> ();
            totalJobTime = object.Get<JSON::Number> (ATTR_TOTAL_JOB_TIME)->To<ui64> ();
        }

        void RunLoop::Stats::Write (JSON::Object &object) const {
            object.Add<const std::string &> (ATTR_ID, id);
            object.Add<const std::string &> (ATTR_NAME, name);
            object.Add<const SizeT &> (ATTR_TOTAL_JOBS, totalJobs);
            object.Add (ATTR_TOTAL_JOB_TIME, totalJobTime);
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

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (RunLoop::State, SpinLock)

        RunLoop::State::State (
                const std::string &name_,
                JobExecutionPolicy::SharedPtr jobExecutionPolicy_) :
                id (GUID::FromRandom ().ToString ()),
                name (name_),
                jobExecutionPolicy (jobExecutionPolicy_),
                done (false),
                stats (id, name),
                jobsNotEmpty (jobsMutex),
                idle (jobsMutex),
                paused (false),
                notPaused (jobsMutex) {
            if (jobExecutionPolicy.Get () == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        RunLoop::State::~State () {
            // RunLoop derivatives should have disposed of running jobs.
            // The best we can do here is alert the engineer to a leak.
            assert (runningJobs.empty ());
            // Cancel remaining pending jobs to unblock waiters.
            Job *job;
            while ((job = jobExecutionPolicy->DeqJob (*this)) != 0) {
                job->Cancel ();
                runningJobs.push_back (job);
                FinishedJob (job, 0, 0);
            }
        }

        RunLoop::Job *RunLoop::State::DeqJob (bool wait) {
            LockGuard<Mutex> guard (jobsMutex);
            while (!done && paused && wait) {
                notPaused.Wait ();
            }
            while (!done && pendingJobs.empty () && wait) {
                jobsNotEmpty.Wait ();
            }
            Job *job = 0;
            if (!done && !paused && !pendingJobs.empty ()) {
                job = jobExecutionPolicy->DeqJob (*this);
                runningJobs.push_back (job);
            }
            return job;
        }

        void RunLoop::State::FinishedJob (
                Job *job,
                ui64 start,
                ui64 end) {
            assert (job != 0);
            {
                // Acquire the lock to perform housekeeping chores.
                LockGuard<Mutex> guard (jobsMutex);
                stats.Update (job, start, end);
                runningJobs.erase (job);
                if (pendingJobs.empty () && runningJobs.empty ()) {
                    idle.SignalAll ();
                }
            }
            // Release the lock here in case the job needs to call
            // back in to the RunLoop to prevent deadlocks.
            job->SetState (RunLoop::Job::Completed);
            job->Release ();
        }

        std::size_t RunLoop::GetPendingJobCount () {
            LockGuard<Mutex> guard (state->jobsMutex);
            return state->pendingJobs.size ();
        }

        std::size_t RunLoop::GetRunningJobCount () {
            LockGuard<Mutex> guard (state->jobsMutex);
            return state->runningJobs.size ();
        }

        bool RunLoop::Pause (
                bool cancelRunningJobs,
                const TimeSpec &timeSpec) {
            struct GetRunningJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                bool cancelRunningJobs;
                UserJobList runningJobs;
                explicit GetRunningJobsCallback (bool cancelRunningJobs_) :
                    cancelRunningJobs (cancelRunningJobs_) {}
                virtual result_type operator () (argument_type job) {
                    if (cancelRunningJobs) {
                        job->Cancel ();
                    }
                    runningJobs.push_back (Job::SharedPtr (job));
                    return true;
                }
            } getRunningJobsCallback (cancelRunningJobs);
            {
                LockGuard<Mutex> guard (state->jobsMutex);
                if (!state->paused) {
                    state->paused = true;
                    state->runningJobs.for_each (getRunningJobsCallback);
                    state->jobsNotEmpty.SignalAll ();
                }
            }
            return WaitForJobs (getRunningJobsCallback.runningJobs, timeSpec);
        }

        void RunLoop::Continue () {
            LockGuard<Mutex> guard (state->jobsMutex);
            if (state->paused) {
                state->paused = false;
                state->notPaused.SignalAll ();
            }
        }

        bool RunLoop::IsPaused () {
            LockGuard<Mutex> guard (state->jobsMutex);
            return state->paused;
        }

        bool RunLoop::EnqJob (
                Job::SharedPtr job,
                bool wait,
                const TimeSpec &timeSpec) {
            if (job.Get () != 0 && job->IsCompleted ()) {
                {
                    LockGuard<Mutex> guard (state->jobsMutex);
                    state->jobExecutionPolicy->EnqJob (*state, job.Get ());
                    job->Reset (state->id);
                    job->AddRef ();
                    state->jobsNotEmpty.Signal ();
                }
                return !wait || WaitForJob (job, timeSpec);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        std::pair<RunLoop::Job::SharedPtr, bool> RunLoop::EnqJob (
                const LambdaJob::Function &function,
                bool wait,
                const TimeSpec &timeSpec) {
            std::pair<Job::SharedPtr, bool> result;
            result.first.Reset (new LambdaJob (function));
            result.second = EnqJob (result.first, wait, timeSpec);
            return result;
        }

        bool RunLoop::EnqJobFront (
                Job::SharedPtr job,
                bool wait,
                const TimeSpec &timeSpec) {
            if (job.Get () != 0 && job->IsCompleted ()) {
                {
                    LockGuard<Mutex> guard (state->jobsMutex);
                    state->jobExecutionPolicy->EnqJobFront (*state, job.Get ());
                    job->Reset (state->id);
                    job->AddRef ();
                    state->jobsNotEmpty.Signal ();
                }
                return !wait || WaitForJob (job, timeSpec);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        std::pair<RunLoop::Job::SharedPtr, bool> RunLoop::EnqJobFront (
                const LambdaJob::Function &function,
                bool wait,
                const TimeSpec &timeSpec) {
            std::pair<Job::SharedPtr, bool> result;
            result.first.Reset (new LambdaJob (function));
            result.second = EnqJobFront (result.first, wait, timeSpec);
            return result;
        }

        RunLoop::Job::SharedPtr RunLoop::GetJob (const Job::Id &jobId) {
            LockGuard<Mutex> guard (state->jobsMutex);
            struct GetJobCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                const Job::Id &jobId;
                Job::SharedPtr job;
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
            if (state->runningJobs.for_each (getJobWithIdCallback)) {
                state->pendingJobs.for_each (getJobWithIdCallback);
            }
            return getJobWithIdCallback.job;
        }

        void RunLoop::GetJobs (
                const EqualityTest &equalityTest,
                UserJobList &jobs) {
            LockGuard<Mutex> guard (state->jobsMutex);
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
            state->runningJobs.for_each (getJobsCallback);
            state->pendingJobs.for_each (getJobsCallback);
        }

        void RunLoop::GetPendingJobs (UserJobList &pendingJobs) {
            LockGuard<Mutex> guard (state->jobsMutex);
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
            state->pendingJobs.for_each (getPendingJobsCallback);
        }

        void RunLoop::GetRunningJobs (UserJobList &runningJobs) {
            LockGuard<Mutex> guard (state->jobsMutex);
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
            state->runningJobs.for_each (getRunningJobsCallback);
        }

        void RunLoop::GetAllJobs (
                UserJobList &pendingJobs,
                UserJobList &runningJobs) {
            LockGuard<Mutex> guard (state->jobsMutex);
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
            state->pendingJobs.for_each (getAllJobsCallback);
            getAllJobsCallback.pending = false;
            state->runningJobs.for_each (getAllJobsCallback);
        }

        bool RunLoop::WaitForJob (
                Job::SharedPtr job,
                const TimeSpec &timeSpec) {
            if (job.Get () != 0 && job->GetRunLoopId () == state->id) {
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
            Job::SharedPtr job = GetJob (jobId);
            return job.Get () != 0 && WaitForJob (job, timeSpec);
        }

        bool RunLoop::WaitForJobs (
                const UserJobList &jobs,
                const TimeSpec &timeSpec) {
            UserJobList::const_iterator it = jobs.begin ();
            UserJobList::const_iterator end = jobs.end ();
            if (timeSpec == TimeSpec::Infinite) {
                while (it != end) {
                    if ((*it)->IsCompleted ()) {
                        ++it;
                    }
                    else {
                        (*it)->Wait ();
                    }
                }
            }
            else {
                TimeSpec now = GetCurrentTime ();
                TimeSpec deadline = now + timeSpec;
                while (it != end && deadline > now) {
                    if ((*it)->IsCompleted ()) {
                        ++it;
                    }
                    else {
                        (*it)->Wait (deadline - now);
                    }
                    now = GetCurrentTime ();
                }
            }
            return it == end;
        }

        bool RunLoop::WaitForJobs (
                const EqualityTest &equalityTest,
                const TimeSpec &timeSpec) {
            struct WaitForJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                const EqualityTest &equalityTest;
                UserJobList jobs;
                explicit WaitForJobsCallback (const EqualityTest &equalityTest_) :
                    equalityTest (equalityTest_) {}
                virtual result_type operator () (argument_type job) {
                    if (equalityTest (*job)) {
                        jobs.push_back (Job::SharedPtr (job));
                    }
                    return true;
                }
            } waitForJobsCallback (equalityTest);
            {
                LockGuard<Mutex> guard (state->jobsMutex);
                state->pendingJobs.for_each (waitForJobsCallback);
                state->runningJobs.for_each (waitForJobsCallback);
            }
            return WaitForJobs (waitForJobsCallback.jobs, timeSpec);
        }

        bool RunLoop::WaitForIdle (const TimeSpec &timeSpec) {
            LockGuard<Mutex> guard (state->jobsMutex);
            if (timeSpec == TimeSpec::Infinite) {
                while (IsRunning () && (!state->pendingJobs.empty () || !state->runningJobs.empty ())) {
                    state->idle.Wait ();
                }
            }
            else {
                TimeSpec now = GetCurrentTime ();
                TimeSpec deadline = now + timeSpec;
                while (IsRunning () && (!state->pendingJobs.empty () || !state->runningJobs.empty ()) && deadline > now) {
                    if (!state->idle.Wait (deadline - now)) {
                        return false;
                    }
                    now = GetCurrentTime ();
                }
            }
            return state->pendingJobs.empty () && state->runningJobs.empty ();
        }

        bool RunLoop::CancelJob (const Job::Id &jobId) {
            LockGuard<Mutex> guard (state->jobsMutex);
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
                !state->runningJobs.for_each (cancelJobCallback) ||
                !state->pendingJobs.for_each (cancelJobCallback);
        }

        void RunLoop::CancelJobs (const UserJobList &jobs) {
            for (UserJobList::const_iterator it = jobs.begin (), end = jobs.end (); it != end; ++it) {
                (*it)->Cancel ();
            }
        }

        void RunLoop::CancelJobs (const EqualityTest &equalityTest) {
            LockGuard<Mutex> guard (state->jobsMutex);
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
            state->runningJobs.for_each (cancelJobsCallback);
            state->pendingJobs.for_each (cancelJobsCallback);
        }

        void RunLoop::CancelPendingJobs () {
            LockGuard<Mutex> guard (state->jobsMutex);
            struct CancelPendingJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                virtual result_type operator () (argument_type job) {
                    job->Cancel ();
                    return true;
                }
            } cancelPendingJobsCallback;
            state->pendingJobs.for_each (cancelPendingJobsCallback);
        }

        void RunLoop::CancelRunningJobs () {
            LockGuard<Mutex> guard (state->jobsMutex);
            struct CancelRunningJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                virtual result_type operator () (argument_type job) {
                    job->Cancel ();
                    return true;
                }
            } cancelRunningJobsCallback;
            state->runningJobs.for_each (cancelRunningJobsCallback);
        }

        void RunLoop::CancelAllJobs () {
            LockGuard<Mutex> guard (state->jobsMutex);
            struct CancelAllJobsCallback : public JobList::Callback {
                typedef JobList::Callback::result_type result_type;
                typedef JobList::Callback::argument_type argument_type;
                virtual result_type operator () (argument_type job) {
                    job->Cancel ();
                    return true;
                }
            } cancelAllJobsCallback;
            state->runningJobs.for_each (cancelAllJobsCallback);
            state->pendingJobs.for_each (cancelAllJobsCallback);
        }

        RunLoop::Stats RunLoop::GetStats () {
            LockGuard<Mutex> guard (state->jobsMutex);
            return state->stats;
        }

        void RunLoop::ResetStats () {
            LockGuard<Mutex> guard (state->jobsMutex);
            state->stats.Reset ();
        }

        bool RunLoop::IsIdle () {
            LockGuard<Mutex> guard (state->jobsMutex);
            return !IsRunning () || (state->pendingJobs.empty () && state->runningJobs.empty ());
        }

    } // namespace util
} // namespace thekogans
