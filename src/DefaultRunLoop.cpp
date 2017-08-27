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
                    JobQueue::Job::Ptr job = Deq ();
                    if (job.Get () != 0) {
                        job->Prologue (done);
                        job->Execute (done);
                        job->Epilogue (done);
                        job->finished = true;
                        jobFinished.SignalAll ();
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

        bool DefaultRunLoop::IsRunning () {
            return !done;
        }

        void DefaultRunLoop::Enq (
                JobQueue::Job &job,
                bool wait) {
            LockGuard<Mutex> guard (mutex);
            job.finished = false;
            jobs.push_back (JobQueue::Job::Ptr (&job));
            jobsNotEmpty.Signal ();
            if (wait) {
                while (!job.cancelled && !job.finished) {
                    jobFinished.Wait ();
                }
            }
        }

        JobQueue::Job::Ptr DefaultRunLoop::Deq () {
            LockGuard<Mutex> guard (mutex);
            while (!done && jobs.empty ()) {
                jobsNotEmpty.Wait ();
            }
            JobQueue::Job::Ptr job;
            if (!jobs.empty ()) {
                job = jobs.front ();
                jobs.pop_front ();
            }
            return job;
        }

    } // namespace util
} // namespace thekogans
