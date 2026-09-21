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

#include "thekogans/util/Heap.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/HRTimer.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/JobQueue.h"

namespace thekogans {
    namespace util {

        void JobQueue::State::Worker::Run () noexcept {
            RunLoop::WorkerInitializer workerInitializer (state->workerCallback);
            while (true) {
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
                LockGuard<Mutex> guard (state->workersMutex);
                if (state->done) {
                    // If state->done is still true under the lock,
                    // we were NOT rescued by Start. This guarantees
                    // we are orphans sitting inside drainingWorkers.
                    state->drainingWorkers.erase (this);
                    break; // Break the outer loop to terminate the physical thread safely
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
                state (RunLoop::state) {
            if (workerCount > 0) {
                Start ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void JobQueue::Start () {
            // If we were paused and not stoped, continue.
            Continue ();
            LockGuard<Mutex> guard (state->workersMutex);
            state->done = false;
            // Rescue any active threads from the draining list instantly!
            state->workers += state->drainingWorkers;
            state->jobsNotEmpty.SignalAll (); // Wake them up to see state->done is false
            // Create as many workers as needed to == state->workerCount.
            for (std::size_t i = state->workers.size (); i < state->workerCount; ++i) {
                std::string workerName;
                if (!state->name.empty ()) {
                    if (state->workerCount > 1) {
                        workerName = FormatString (
                            "%s-" THEKOGANS_UTIL_SIZE_T_FORMAT,
                            state->name.c_str (),
                            state->workerCounter++);
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
            // Code below assumes the queue is not paused.
            Continue ();
            LockGuard<Mutex> guard (state->workersMutex);
            // Move the current active workers to the drainingWorkers list.
            // There they will spin down and, if not rescued by Start will
            // end their own lives.
            state->drainingWorkers += state->workers;
            // Preclude workers from dequeuing any more pending jobs.
            state->done = true;
            // Wake up sleeping workers to allow them to exit.
            state->jobsNotEmpty.SignalAll ();
            //  Cancel all running jobs.
            if (cancelRunningJobs) {
                CancelRunningJobs ();
            }
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
                RunLoop (state_),
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
