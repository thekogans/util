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

        Pipeline::JobExecutionPolicy::JobExecutionPolicy (std::size_t maxJobs_) :
                maxJobs (maxJobs_) {
            if (maxJobs == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Pipeline::FIFOJobExecutionPolicy::EnqJob (
                State &state,
                Job *job) {
            if (state.pendingJobs.size () < maxJobs) {
                state.pendingJobs.push_back (job);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Pipeline (%s) max jobs (%u) reached.",
                    !state.name.empty () ? state.name.c_str () : "no name",
                    maxJobs);
            }
        }

        void Pipeline::FIFOJobExecutionPolicy::EnqJobFront (
                State &state,
                Job *job) {
            if (state.pendingJobs.size () < maxJobs) {
                state.pendingJobs.push_front (job);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Pipeline (%s) max jobs (%u) reached.",
                    !state.name.empty () ? state.name.c_str () : "no name",
                    maxJobs);
            }
        }

        Pipeline::Job *Pipeline::FIFOJobExecutionPolicy::DeqJob (State &state) {
            return !state.pendingJobs.empty () ? state.pendingJobs.pop_front () : 0;
        }

        void Pipeline::LIFOJobExecutionPolicy::EnqJob (
                State &state,
                Job *job) {
            if (state.pendingJobs.size () < maxJobs) {
                state.pendingJobs.push_front (job);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Pipeline (%s) max jobs (%u) reached.",
                    !state.name.empty () ? state.name.c_str () : "no name",
                    maxJobs);
            }
        }

        void Pipeline::LIFOJobExecutionPolicy::EnqJobFront (
                State &state,
                Job *job) {
            EnqJob (state, job);
        }

        Pipeline::Job *Pipeline::LIFOJobExecutionPolicy::DeqJob (State &state) {
            return !state.pendingJobs.empty () ? state.pendingJobs.pop_front () : 0;
        }

        Pipeline::Job::Job (Pipeline::SharedPtr pipeline_) :
            pipeline (pipeline_->state),
            stage (GetFirstStage ()),
            start (0),
            end (0) {}

        const RunLoop::Id &Pipeline::Job::GetPipelineId () const {
            return pipeline->id;
        }

        void Pipeline::Job::Reset (const RunLoop::Id &runLoopId_) {
            RunLoop::Job::Reset (runLoopId_);
            if (runLoopId_ == pipeline->id) {
                state = Completed;
                stage = GetFirstStage ();
                start = 0;
                end = 0;
            }
        }

        void Pipeline::Job::SetState (State state_) {
            state = state_;
            if (IsRunning ()) {
                if (stage == GetFirstStage ()) {
                    start = HRTimer::Click ();
                    Begin (pipeline->done);
                }
            }
            else if (IsCompleted ()) {
                if (stage == pipeline->stages.size ()) {
                    completed.Signal ();
                }
                else {
                    if (!ShouldStop (pipeline->done) &&
                            ((stage = GetNextStage ()) < pipeline->stages.size ())) {
                        THEKOGANS_UTIL_TRY {
                            pipeline->stages[stage]->EnqJob (RunLoop::Job::SharedPtr (this));
                            return;
                        }
                        THEKOGANS_UTIL_CATCH (Exception) {
                            Fail (exception);
                        }
                    }
                    stage = pipeline->stages.size ();
                    End (pipeline->done);
                    end = HRTimer::Click ();
                    pipeline->FinishedJob (this, start, end);
                }
            }
        }

        void Pipeline::State::Worker::Run () throw () {
            RunLoop::WorkerInitializer workerInitializer (state->workerCallback);
            while (!state->done) {
                Job *job = state->DeqJob ();
                if (job != nullptr) {
                    // Short circuit cancelled pending jobs.
                    if (!job->ShouldStop (state->done) &&
                            ((job->stage = job->GetFirstStage ()) < state->stages.size ())) {
                        THEKOGANS_UTIL_TRY {
                            state->stages[job->stage]->EnqJob (RunLoop::Job::SharedPtr (job));
                            continue;
                        }
                        THEKOGANS_UTIL_CATCH (Exception) {
                            job->Fail (exception);
                        }
                    }
                    state->FinishedJob (job, job->start, job->end);
                }
            }
        }

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (Pipeline::State)

        Pipeline::State::State (
                const Stage *begin,
                const Stage *end,
                const std::string &name_,
                JobExecutionPolicy::SharedPtr jobExecutionPolicy_,
                std::size_t workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                RunLoop::WorkerCallback *workerCallback_) :
                id (GUID::FromRandom ().ToString ()),
                name (name_),
                jobExecutionPolicy (jobExecutionPolicy_),
                done (false),
                stats (id, name),
                jobsNotEmpty (jobsMutex),
                idle (jobsMutex),
                paused (false),
                notPaused (jobsMutex),
                workerCount (workerCount_),
                workerPriority (workerPriority_),
                workerAffinity (workerAffinity_),
                workerCallback (workerCallback_) {
            if (begin != nullptr &&
                    end != nullptr &&
                    jobExecutionPolicy != nullptr &&
                    workerCount > 0) {
                for (; begin != end; ++begin) {
                    stages.push_back (
                        JobQueue::SharedPtr (
                            new JobQueue (
                                begin->name,
                                begin->jobExecutionPolicy,
                                begin->workerCount,
                                begin->workerPriority,
                                begin->workerAffinity,
                                begin->workerCallback)));
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        Pipeline::State::~State () {
            // RunLoop derivatives should have disposed of running jobs.
            // The best we can do here is alert the engineer to a leak.
            assert (runningJobs.empty ());
            // Cancel remaining pending jobs to unblock waiters.
            Job *job;
            while ((job = jobExecutionPolicy->DeqJob (*this)) != nullptr) {
                job->Cancel ();
                runningJobs.push_back (job);
                FinishedJob (job, 0, 0);
            }
        }

        Pipeline::Job *Pipeline::State::DeqJob (bool wait) {
            LockGuard<Mutex> guard (jobsMutex);
            while (!done && paused && wait) {
                notPaused.Wait ();
            }
            while (!done && pendingJobs.empty () && wait) {
                jobsNotEmpty.Wait ();
            }
            Job *job = nullptr;
            if (!done && !paused && !pendingJobs.empty ()) {
                job = jobExecutionPolicy->DeqJob (*this);
                runningJobs.push_back (job);
            }
            return job;
        }

        void Pipeline::State::FinishedJob (
                Job *job,
                ui64 start,
                ui64 end) {
            assert (job != nullptr);
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
            // back in to the Pipeline to prevent deadlocks.
            job->SetState (RunLoop::Job::Completed);
            job->Release ();
        }

        bool Pipeline::Pause (
                bool cancelRunningJobs,
                const TimeSpec &timeSpec) {
            RunLoop::UserJobList runningJobs;
            {
                LockGuard<Mutex> guard (state->jobsMutex);
                if (!state->paused) {
                    state->paused = true;
                    state->runningJobs.for_each (
                        [cancelRunningJobs, &runningJobs] (JobList::Callback::argument_type job) ->
                                JobList::Callback::result_type {
                            if (cancelRunningJobs) {
                                job->Cancel ();
                            }
                            runningJobs.push_back (RunLoop::Job::SharedPtr (job));
                            return true;
                        }
                    );
                    state->jobsNotEmpty.SignalAll ();
                }
            }
            return WaitForJobs (runningJobs, timeSpec);
        }

        void Pipeline::Continue () {
            LockGuard<Mutex> guard (state->jobsMutex);
            if (state->paused) {
                state->paused = false;
                state->notPaused.SignalAll ();
            }
        }

        bool Pipeline::IsPaused () {
            LockGuard<Mutex> guard (state->jobsMutex);
            return state->paused;
        }

        RunLoop::Stats Pipeline::GetStageStats (std::size_t stage) {
            if (stage < state->stages.size ()) {
                return state->stages[stage]->GetStats ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Pipeline::GetStagesStats (std::vector<RunLoop::Stats> &stats) {
            for (std::size_t i = 0, count = state->stages.size (); i < count; ++i) {
                stats.push_back (state->stages[i]->GetStats ());
            }
        }

        void Pipeline::Start () {
            LockGuard<Mutex> guard (state->workersMutex);
            state->done = false;
            for (std::size_t i = 0, count = state->stages.size (); i < count; ++i) {
                state->stages[i]->Start ();
            }
            for (std::size_t i = state->workers.size (); i < state->workerCount; ++i) {
                std::string workerName;
                if (!state->name.empty ()) {
                    if (state->workerCount > 1) {
                        workerName = FormatString ("%s-" THEKOGANS_UTIL_SIZE_T_FORMAT, state->name.c_str (), i);
                    }
                    else {
                        workerName = state->name;
                    }
                }
                state->workers.push_back (new State::Worker (state, workerName));
            }
        }

        void Pipeline::Stop (
                bool cancelRunningJobs,
                bool cancelPendingJobs) {
            LockGuard<Mutex> guard (state->workersMutex);
            // Stop the pipeline stages.
            for (std::size_t i = 0, count = state->stages.size (); i < count; ++i) {
                state->stages[i]->Stop (cancelRunningJobs, cancelPendingJobs);
            }
            // Clear worker list in case Start is called again.
            // The worker threads are responsible for their own
            // lifetimes. Also do it before setting state->done = true
            // below in case workers exit before we can clear the
            // list so as not to have a race leading to a crash.
            state->workers.clear ();
            // Preclude workers from dequeuing any more pending jobs.
            state->done = true;
            // Wake up sleeping workers to allow them to exit.
            state->jobsNotEmpty.SignalAll ();
            //  Cancel all running jobs.
            if (cancelRunningJobs) {
                CancelRunningJobs ();
            }
            // CancelPendingJobs does not block.
            if (cancelPendingJobs) {
                // The queue has no worker threads. Simulate what
                // they would do to make sure anyone waiting on
                // pending jobs gets notified.
                Job *job;
                while ((job = state->jobExecutionPolicy->DeqJob (*state)) != nullptr) {
                    job->Cancel ();
                    state->runningJobs.push_back (job);
                    state->FinishedJob (job, 0, 0);
                }
            }
            // Let everyone know the queue is idle.
            state->idle.SignalAll ();
        }

        bool Pipeline::IsRunning () {
            LockGuard<Mutex> guard (state->workersMutex);
            return !state->workers.empty ();
        }

        bool Pipeline::EnqJob (
                Job::SharedPtr job,
                bool wait,
                const TimeSpec &timeSpec) {
            if (job != nullptr && job->IsCompleted () && job->GetPipelineId () == state->id) {
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

        std::pair<Pipeline::Job::SharedPtr, bool> Pipeline::EnqJob (
                const LambdaJob::Function *&begin,
                const LambdaJob::Function *&end,
                bool wait,
                const TimeSpec &timeSpec) {
            std::pair<Job::SharedPtr, bool> result;
            result.first.Reset (new LambdaJob (this, begin, end));
            result.second = EnqJob (result.first, wait, timeSpec);
            return result;
        }

        bool Pipeline::EnqJobFront (
                Job::SharedPtr job,
                bool wait,
                const TimeSpec &timeSpec) {
            if (job != nullptr && job->IsCompleted ()) {
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

        std::pair<Pipeline::Job::SharedPtr, bool> Pipeline::EnqJobFront (
                const LambdaJob::Function *&begin,
                const LambdaJob::Function *&end,
                bool wait,
                const TimeSpec &timeSpec) {
            std::pair<Job::SharedPtr, bool> result;
            result.first.Reset (new LambdaJob (this, begin, end));
            result.second = EnqJobFront (result.first, wait, timeSpec);
            return result;
        }

        Pipeline::Job::SharedPtr Pipeline::GetJob (const Job::Id &jobId) {
            LockGuard<Mutex> guard (state->jobsMutex);
            Job::SharedPtr job;
            auto callback =
                [&jobId, &job] (JobList::Callback::argument_type job_) -> JobList::Callback::result_type {
                    if (job_->GetId () == jobId) {
                        job.Reset (job_);
                        return false;
                    }
                    return true;
                };
            if (state->runningJobs.for_each (callback)) {
                state->pendingJobs.for_each (callback);
            }
            return job;
        }

        void Pipeline::GetJobs (
                const RunLoop::EqualityTest &equalityTest,
                RunLoop::UserJobList &jobs) {
            LockGuard<Mutex> guard (state->jobsMutex);
            auto callback =
                [&equalityTest, &jobs] (JobList::Callback::argument_type job) -> JobList::Callback::result_type {
                    if (equalityTest (*job)) {
                        jobs.push_back (RunLoop::Job::SharedPtr (job));
                    }
                    return true;
                };
            state->runningJobs.for_each (callback);
            state->pendingJobs.for_each (callback);
        }

        void Pipeline::GetPendingJobs (RunLoop::UserJobList &pendingJobs) {
            LockGuard<Mutex> guard (state->jobsMutex);
            state->pendingJobs.for_each (
                [&pendingJobs] (JobList::Callback::argument_type job) -> JobList::Callback::result_type {
                    pendingJobs.push_back (RunLoop::Job::SharedPtr (job));
                    return true;
                }
            );
        }

        void Pipeline::GetRunningJobs (RunLoop::UserJobList &runningJobs) {
            LockGuard<Mutex> guard (state->jobsMutex);
            state->runningJobs.for_each (
                [&runningJobs] (JobList::Callback::argument_type job) -> JobList::Callback::result_type {
                    runningJobs.push_back (RunLoop::Job::SharedPtr (job));
                    return true;
                }
            );
        }

        void Pipeline::GetAllJobs (
                RunLoop::UserJobList &pendingJobs,
                RunLoop::UserJobList &runningJobs) {
            LockGuard<Mutex> guard (state->jobsMutex);
            state->pendingJobs.for_each (
                [&pendingJobs] (JobList::Callback::argument_type job) -> JobList::Callback::result_type {
                    pendingJobs.push_back (RunLoop::Job::SharedPtr (job));
                    return true;
                }
            );
            state->runningJobs.for_each (
                [&runningJobs] (JobList::Callback::argument_type job) -> JobList::Callback::result_type {
                    runningJobs.push_back (RunLoop::Job::SharedPtr (job));
                    return true;
                }
            );
        }

        bool Pipeline::WaitForJob (
                Job::SharedPtr job,
                const TimeSpec &timeSpec) {
            if (job != nullptr && job->GetPipelineId () == state->id) {
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

        bool Pipeline::WaitForJob (
                const Job::Id &jobId,
                const TimeSpec &timeSpec) {
            Job::SharedPtr job = GetJob (jobId);
            return job != nullptr && WaitForJob (job, timeSpec);
        }

        bool Pipeline::WaitForJobs (
                const RunLoop::UserJobList &jobs,
                const TimeSpec &timeSpec) {
            RunLoop::UserJobList::const_iterator it = jobs.begin ();
            RunLoop::UserJobList::const_iterator end = jobs.end ();
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

        bool Pipeline::WaitForJobs (
                const RunLoop::EqualityTest &equalityTest,
                const TimeSpec &timeSpec) {
            RunLoop::UserJobList jobs;
            auto callback =
                [&equalityTest, &jobs] (JobList::Callback::argument_type job) -> JobList::Callback::result_type {
                    if (equalityTest (*job)) {
                        jobs.push_back (RunLoop::Job::SharedPtr (job));
                    }
                    return true;
                };
            {
                LockGuard<Mutex> guard (state->jobsMutex);
                state->pendingJobs.for_each (callback);
                state->runningJobs.for_each (callback);
            }
            return WaitForJobs (jobs, timeSpec);
        }

        bool Pipeline::WaitForIdle (const TimeSpec &timeSpec) {
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
                    state->idle.Wait (deadline - now);
                    now = GetCurrentTime ();
                }
            }
            return state->pendingJobs.empty () && state->runningJobs.empty ();
        }

        bool Pipeline::CancelJob (const Job::Id &jobId) {
            LockGuard<Mutex> guard (state->jobsMutex);
            auto callback =
                [&jobId] (JobList::Callback::argument_type job) -> JobList::Callback::result_type {
                    if (job->GetId () == jobId) {
                        job->Cancel ();
                        return false;
                    }
                    return true;
                };
            return
                !state->runningJobs.for_each (callback) ||
                !state->pendingJobs.for_each (callback);
        }

        void Pipeline::CancelJobs (const RunLoop::UserJobList &jobs) {
            for (RunLoop::UserJobList::const_iterator it = jobs.begin (), end = jobs.end (); it != end; ++it) {
                (*it)->Cancel ();
            }
        }

        void Pipeline::CancelJobs (const RunLoop::EqualityTest &equalityTest) {
            LockGuard<Mutex> guard (state->jobsMutex);
            auto callback =
                [&equalityTest] (JobList::Callback::argument_type job) -> JobList::Callback::result_type {
                    if (equalityTest (*job)) {
                        job->Cancel ();
                        return false;
                    }
                    return true;
                };
            state->runningJobs.for_each (callback);
            state->pendingJobs.for_each (callback);
        }

        void Pipeline::CancelPendingJobs () {
            LockGuard<Mutex> guard (state->jobsMutex);
            state->pendingJobs.for_each (
                [] (JobList::Callback::argument_type job) -> JobList::Callback::result_type {
                    job->Cancel ();
                    return true;
                }
            );
        }

        void Pipeline::CancelRunningJobs () {
            LockGuard<Mutex> guard (state->jobsMutex);
            state->runningJobs.for_each (
                [] (JobList::Callback::argument_type job) -> JobList::Callback::result_type {
                    job->Cancel ();
                    return true;
                }
            );
        }

        void Pipeline::CancelAllJobs () {
            LockGuard<Mutex> guard (state->jobsMutex);
            auto callback =
                [] (JobList::Callback::argument_type job) -> JobList::Callback::result_type {
                    job->Cancel ();
                    return true;
                };
            state->runningJobs.for_each (callback);
            state->pendingJobs.for_each (callback);
        }

        RunLoop::Stats Pipeline::GetStats () {
            LockGuard<Mutex> guard (state->jobsMutex);
            return state->stats;
        }

        void Pipeline::ResetStats () {
            {
                LockGuard<Mutex> guard (state->jobsMutex);
                state->stats.Reset ();
            }
            for (std::size_t i = 0, count = state->stages.size (); i < count; ++i) {
                state->stages[i]->ResetStats ();
            }
        }

        bool Pipeline::IsIdle () {
            LockGuard<Mutex> guard (state->jobsMutex);
            return !IsRunning () || (state->pendingJobs.empty () && state->runningJobs.empty ());
        }

        Pipeline::Pipeline (State::SharedPtr state_) :
                state (state_) {
            if (state != nullptr) {
                Start ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    } // namespace util
} // namespace thekogans
