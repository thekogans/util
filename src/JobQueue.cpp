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
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/JobQueue.h"

namespace thekogans {
    namespace util {

        void JobQueue::State::Worker::Run () throw () {
            RunLoop::WorkerInitializer workerInitializer (state->workerCallback);
            while (!state->done) {
                Job *job = state->DeqJob ();
                if (job != nullptr) {
                    ui64 start = 0;
                    ui64 end = 0;
                    // Short circuit cancelled pending jobs.
                    if (!job->ShouldStop (state->done)) {
                        start = HRTimer::Click ();
                        job->SetState (Job::Running);
                        job->Prologue (state->done);
                        job->Execute (state->done);
                        job->Epilogue (state->done);
                        job->Succeed (state->done);
                        end = HRTimer::Click ();
                    }
                    state->FinishedJob (job, start, end);
                }
            }
            ThreadReaper::Instance ()->ReapThread (this);
        }

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (JobQueue::State)

        JobQueue::JobQueue (
                const std::string &name,
                JobExecutionPolicy::SharedPtr jobExecutionPolicy,
                std::size_t workerCount,
                i32 workerPriority,
                ui32 workerAffinity,
                WorkerCallback *workerCallback) :
                RunLoop (
                    RunLoop::State::SharedPtr (
                        new State (
                            name,
                            jobExecutionPolicy,
                            workerCount,
                            workerPriority,
                            workerAffinity,
                            workerCallback))),
                state (dynamic_refcounted_sharedptr_cast<State> (RunLoop::state)) {
            if (workerCount > 0) {
                Start ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void JobQueue::Start () {
            LockGuard<Mutex> guard (state->workersMutex);
            state->done = false;
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

        void JobQueue::Stop (
                bool cancelRunningJobs,
                bool cancelPendingJobs) {
            LockGuard<Mutex> guard (state->workersMutex);
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

        bool JobQueue::IsRunning () {
            LockGuard<Mutex> guard (state->workersMutex);
            return !state->workers.empty ();
        }

        JobQueue::JobQueue (State::SharedPtr state_) :
                RunLoop (dynamic_refcounted_sharedptr_cast<RunLoop::State> (state_)),
                state (state_) {
            if (state.Get () != nullptr) {
                Start ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    } // namespace util
} // namespace thekogans
