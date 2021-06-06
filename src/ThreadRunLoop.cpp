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

#include "thekogans/util/HRTimer.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/ThreadRunLoop.h"

namespace thekogans {
    namespace util {

        void ThreadRunLoop::Start () {
            state->done = false;
            while (!state->done) {
                Job *job = state->DeqJob ();
                if (job != 0) {
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
        }

        void ThreadRunLoop::Stop (
                bool cancelRunningJobs,
                bool cancelPendingJobs) {
            state->done = true;
            if (cancelRunningJobs) {
                CancelRunningJobs ();
            }
            state->jobsNotEmpty.Signal ();
            if (cancelPendingJobs) {
                Job *job;
                while ((job = state->jobExecutionPolicy->DeqJob (*state)) != 0) {
                    job->Cancel ();
                    state->runningJobs.push_back (job);
                    state->FinishedJob (job, 0, 0);
                }
            }
            state->idle.SignalAll ();
        }

        bool ThreadRunLoop::IsRunning () {
            return !state->done;
        }

    } // namespace util
} // namespace thekogans
