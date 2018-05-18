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
            if (SetDone (false)) {
                struct WorkerInitializer {
                    DefaultRunLoop &runLoop;
                    explicit WorkerInitializer (DefaultRunLoop &runLoop_) :
                            runLoop (runLoop_) {
                        if (runLoop.workerCallback != 0) {
                            runLoop.workerCallback->InitializeWorker ();
                        }
                    }
                    ~WorkerInitializer () {
                        if (runLoop.workerCallback != 0) {
                            runLoop.workerCallback->UninitializeWorker ();
                        }
                    }
                } workerInitializer (*this);
                while (!done) {
                    Job *job = DeqJob ();
                    if (job != 0) {
                        ui64 start = 0;
                        ui64 end = 0;
                        // Short circuit cancelled pending jobs.
                        if (job->GetDisposition () == Job::Unknown) {
                            start = HRTimer::Click ();
                            job->SetStatus (Job::Running);
                            job->Prologue (done);
                            job->Execute (done);
                            job->Epilogue (done);
                            job->Finish ();
                            end = HRTimer::Click ();
                        }
                        FinishedJob (job, start, end);
                    }
                }
            }
        }

        void DefaultRunLoop::Stop (bool cancelPendingJobs) {
            if (cancelPendingJobs) {
                CancelAllJobs ();
                WaitForIdle ();
            }
            if (SetDone (true)) {
                jobsNotEmpty.Signal ();
            }
        }

    } // namespace util
} // namespace thekogans
