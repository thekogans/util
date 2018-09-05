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
#include "thekogans/util/DefaultRunLoop.h"

namespace thekogans {
    namespace util {

        void DefaultRunLoop::Start () {
            bool expected = true;
            if (done.compare_exchange_strong (expected, false)) {
                while (!done) {
                    Job *job = DeqJob ();
                    if (job != 0) {
                        ui64 start = 0;
                        ui64 end = 0;
                        // Short circuit cancelled pending jobs.
                        if (!job->ShouldStop (done)) {
                            start = HRTimer::Click ();
                            job->SetState (Job::Running);
                            job->Prologue (done);
                            job->Execute (done);
                            job->Epilogue (done);
                            job->Succeed (done);
                            end = HRTimer::Click ();
                        }
                        FinishedJob (job, start, end);
                    }
                }
            }
        }

        void DefaultRunLoop::Stop (
                bool cancelRunningJobs,
                bool cancelPendingJobs) {
            struct SavePendingJobs {
                JobList &pendingJobs;
                Mutex &jobsMutex;
                JobList savedPendingJobs;
                SavePendingJobs (
                    JobList &pendingJobs_,
                    Mutex &jobsMutex_) :
                    pendingJobs (pendingJobs_),
                    jobsMutex (jobsMutex_) {}
                ~SavePendingJobs () {
                    LockGuard<Mutex> guard (jobsMutex);
                    savedPendingJobs.swap (pendingJobs);
                }
                void Save () {
                    LockGuard<Mutex> guard (jobsMutex);
                    savedPendingJobs.swap (pendingJobs);
                }
            } savePendingJobs (pendingJobs, jobsMutex);
            if (cancelRunningJobs && cancelPendingJobs) {
                CancelAllJobs ();
            }
            else if (!cancelRunningJobs && cancelPendingJobs) {
                CancelPendingJobs ();
            }
            else if (cancelRunningJobs && !cancelPendingJobs) {
                savePendingJobs.Save ();
                CancelRunningJobs ();
            }
            else if (!cancelRunningJobs && !cancelPendingJobs) {
                savePendingJobs.Save ();
            }
            WaitForIdle ();
            bool expected = false;
            if (done.compare_exchange_strong (expected, true)) {
                jobsNotEmpty.Signal ();
            }
        }

    } // namespace util
} // namespace thekogans
