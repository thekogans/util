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
                        ui64 start = HRTimer::Click ();
                        job->Prologue (done);
                        job->Execute (done);
                        job->Epilogue (done);
                        ui64 end = HRTimer::Click ();
                        FinishedJob (job, start, end);
                    }
                }
            }
        }

        void DefaultRunLoop::Stop (bool cancelPendingJobs) {
            if (SetDone (true)) {
                jobsNotEmpty.Signal ();
            }
            if (cancelPendingJobs) {
                CancelAllJobs ();
            }
        }

        void DefaultRunLoop::EnqJob (
                Job::Ptr job,
                bool wait) {
            if (job.Get () != 0) {
                LockGuard<Mutex> guard (jobsMutex);
                if (stats.jobCount < maxPendingJobs) {
                    job->finished = false;
                    if (type == TYPE_FIFO) {
                        jobs.push_back (job.Get ());
                    }
                    else {
                        jobs.push_front (job.Get ());
                    }
                    job->AddRef ();
                    ++stats.jobCount;
                    state = Busy;
                    jobsNotEmpty.Signal ();
                    if (wait) {
                        while (!job->finished) {
                            jobFinished.Wait ();
                        }
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "DefaultRunLoop (%s) max jobs (%u) reached.",
                        !name.empty () ? name.c_str () : "no name",
                        maxPendingJobs);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool DefaultRunLoop::WaitForJob (const Job::Id &jobId) {
            Job::Ptr job_;
            {
                LockGuard<Mutex> guard (jobsMutex);
                for (Job *job = jobs.front (); job != 0; job = jobs.next (job)) {
                    if (job->GetId () == jobId) {
                        job_.Reset (job);
                        break;
                    }
                }
            }
            if (job_.Get () != 0) {
                while (!job_->finished) {
                    jobFinished.Wait ();
                }
                return true;
            }
            return false;
        }

        void DefaultRunLoop::WaitForJobs (const EqualityTest &equalityTest) {
            JobProxyList jobs_;
            {
                LockGuard<Mutex> guard (jobsMutex);
                for (Job *job = jobs.front (); job != 0; job = jobs.next (job)) {
                    if (equalityTest (*job)) {
                        jobs_.push_back (new JobProxy (job));
                    }
                }
            }
            if (!jobs_.empty ()) {
                struct FinishedCallback : public JobProxyList::Callback {
                    typedef JobProxyList::Callback::result_type result_type;
                    typedef JobProxyList::Callback::argument_type argument_type;
                    JobProxyList &jobs;
                    explicit FinishedCallback (JobProxyList &jobs_) :
                        jobs (jobs_) {}
                    virtual result_type operator () (argument_type jobProxy) {
                        if (jobProxy->job->finished) {
                            jobs.erase (jobProxy);
                            delete jobProxy;
                            return true;
                        }
                        return false;
                    }
                } finishedCallback (jobs_);
                while (!jobs_.for_each (finishedCallback)) {
                    jobFinished.Wait ();
                }
            }
        }

        void DefaultRunLoop::WaitForIdle () {
            LockGuard<Mutex> guard (jobsMutex);
            while (state == Busy) {
                idle.Wait ();
            }
        }

        bool DefaultRunLoop::CancelJob (const Job::Id &jobId) {
            Job *job = 0;
            {
                LockGuard<Mutex> guard (jobsMutex);
                for (job = jobs.front (); job != 0; job = jobs.next (job)) {
                    if (job->GetId () == jobId) {
                        jobs.erase (job);
                        --stats.jobCount;
                        if (busyWorkers == 0 && jobs.empty ()) {
                            assert (state == Busy);
                            state = Idle;
                            idle.SignalAll ();
                        }
                        break;
                    }
                }
            }
            if (job != 0) {
                job->Cancel ();
                ReleaseJobQueue::Instance ().EnqJob (job);
                jobFinished.SignalAll ();
                return true;
            }
            return false;
        }

        void DefaultRunLoop::CancelJobs (const EqualityTest &equalityTest) {
            JobList jobs_;
            {
                LockGuard<Mutex> guard (jobsMutex);
                for (Job *job = jobs.front (); job != 0; job = jobs.next (job)) {
                    if (equalityTest (*job)) {
                        jobs.erase (job);
                        jobs_.push_back (job);
                        --stats.jobCount;
                    }
                }
                if (busyWorkers == 0 && jobs.empty () && !jobs_.empty ()) {
                    assert (state == Busy);
                    state = Idle;
                    idle.SignalAll ();
                }
            }
            if (!jobs_.empty ()) {
                struct CancelCallback : public JobList::Callback {
                    typedef JobList::Callback::result_type result_type;
                    typedef JobList::Callback::argument_type argument_type;
                    virtual result_type operator () (argument_type job) {
                        job->Cancel ();
                        ReleaseJobQueue::Instance ().EnqJob (job);
                        return true;
                    }
                } cancelCallback;
                jobs_.clear (cancelCallback);
                jobFinished.SignalAll ();
            }
        }

        void DefaultRunLoop::CancelAllJobs () {
            JobList jobs_;
            {
                LockGuard<Mutex> guard (jobsMutex);
                if (!jobs.empty ()) {
                    jobs.swap (jobs_);
                    stats.jobCount = 0;
                    if (busyWorkers == 0) {
                        assert (state == Busy);
                        state = Idle;
                        idle.SignalAll ();
                    }
                }
            }
            if (!jobs_.empty ()) {
                struct CancelCallback : public JobList::Callback {
                    typedef JobList::Callback::result_type result_type;
                    typedef JobList::Callback::argument_type argument_type;
                    virtual result_type operator () (argument_type job) {
                        job->Cancel ();
                        ReleaseJobQueue::Instance ().EnqJob (job);
                        return true;
                    }
                } cancelCallback;
                jobs_.clear (cancelCallback);
                jobFinished.SignalAll ();
            }
        }

        RunLoop::Stats DefaultRunLoop::GetStats () {
            LockGuard<Mutex> guard (jobsMutex);
            return stats;
        }

        bool DefaultRunLoop::IsRunning () {
            LockGuard<Mutex> guard (jobsMutex);
            return !done;
        }

        bool DefaultRunLoop::IsEmpty () {
            LockGuard<Mutex> guard (jobsMutex);
            return jobs.empty ();
        }

        bool DefaultRunLoop::IsIdle () {
            LockGuard<Mutex> guard (jobsMutex);
            return state == Idle;
        }

        RunLoop::Job *DefaultRunLoop::DeqJob () {
            LockGuard<Mutex> guard (jobsMutex);
            while (!done && jobs.empty ()) {
                jobsNotEmpty.Wait ();
            }
            Job *job = 0;
            if (!done && !jobs.empty ()) {
                job = jobs.pop_front ();
                --stats.jobCount;
                ++busyWorkers;
            }
            return job;
        }

        void DefaultRunLoop::FinishedJob (
                Job *job,
                ui64 start,
                ui64 end) {
            assert (job != 0);
            {
                LockGuard<Mutex> guard (jobsMutex);
                stats.Update (job->id, start, end);
                if (--busyWorkers == 0 && jobs.empty ()) {
                    assert (state == Busy);
                    state = Idle;
                    idle.SignalAll ();
                }
            }
            job->finished = true;
            ReleaseJobQueue::Instance ().EnqJob (job);
            jobFinished.SignalAll ();
        }

        bool DefaultRunLoop::SetDone (bool value) {
            LockGuard<Mutex> guard (jobsMutex);
            if (done != value) {
                done = value;
                return true;
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
