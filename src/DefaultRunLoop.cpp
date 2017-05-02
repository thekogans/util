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
#include "thekogans/util/DefaultRunLoop.h"

namespace thekogans {
    namespace util {

        void DefaultRunLoop::Start () {
            if (done) {
                done = false;
                while (!done) {
                    JobQueue::Job::UniquePtr job = Deq ();
                    if (job.get () != 0) {
                        job->Prologue (done);
                        job->Execute (done);
                        job->Epilogue (done);
                        if (job->finished != 0) {
                            *job->finished = true;
                            jobFinished.SignalAll ();
                        }
                    }
                }
            }
        }

        void DefaultRunLoop::Stop () {
            if (!done) {
                done = true;
                LockGuard<Mutex> guard (mutex);
                jobs.clear ();
                jobsNotEmpty.Signal ();
            }
        }

        void DefaultRunLoop::Enq (
                JobQueue::Job::UniquePtr job,
                bool wait) {
            if (job.get () != 0) {
                LockGuard<Mutex> guard (mutex);
                volatile bool finished = false;
                if (wait) {
                    job->finished = &finished;
                }
                jobs.push_back (std::move (job));
                jobsNotEmpty.Signal ();
                while (wait && !finished) {
                    jobFinished.Wait ();
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        JobQueue::Job::UniquePtr DefaultRunLoop::Deq () {
            LockGuard<Mutex> guard (mutex);
            while (!done && jobs.empty ()) {
                jobsNotEmpty.Wait ();
            }
            JobQueue::Job::UniquePtr job;
            if (!jobs.empty ()) {
                job = std::move (jobs.front ());
                jobs.pop_front ();
            }
            return job;
        }

    } // namespace util
} // namespace thekogans
