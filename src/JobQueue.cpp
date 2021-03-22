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

        void JobQueue::Worker::Run () throw () {
            RunLoop::WorkerInitializer workerInitializer (queue.workerCallback);
            while (!queue.done) {
                Job *job = queue.DeqJob ();
                if (job != 0) {
                    ui64 start = 0;
                    ui64 end = 0;
                    // Short circuit cancelled pending jobs.
                    if (!job->ShouldStop (queue.done)) {
                        start = HRTimer::Click ();
                        job->SetState (Job::Running);
                        job->Prologue (queue.done);
                        job->Execute (queue.done);
                        job->Epilogue (queue.done);
                        job->Succeed (queue.done);
                        end = HRTimer::Click ();
                    }
                    queue.FinishedJob (job, start, end);
                }
            }
        }

        JobQueue::JobQueue (
                const std::string &name,
                JobExecutionPolicy::SharedPtr jobExecutionPolicy,
                std::size_t workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                WorkerCallback *workerCallback_) :
                RunLoop (name, jobExecutionPolicy),
                workerCount (workerCount_),
                workerPriority (workerPriority_),
                workerAffinity (workerAffinity_),
                workerCallback (workerCallback_) {
            if (workerCount > 0) {
                Start ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        JobQueue::~JobQueue () {
            // FIXME: Perhaps parameterize the timeout.
            const i64 TIMEOUT_SECONDS = 2;
            if (!Stop (true, true, TimeSpec::FromSeconds (TIMEOUT_SECONDS))) {
                THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (
                    THEKOGANS_UTIL,
                    "Unable to stop the '%s' job queue in alloted time " THEKOGANS_UTIL_I64_FORMAT ".\n",
                    name.c_str (),
                    TIMEOUT_SECONDS);
            }
        }

        void JobQueue::Start () {
            LockGuard<Mutex> guard (workersMutex);
            done = false;
            for (std::size_t i = workers.size (); i < workerCount; ++i) {
                std::string workerName;
                if (!name.empty ()) {
                    if (workerCount > 1) {
                        workerName = FormatString ("%s-" THEKOGANS_UTIL_SIZE_T_FORMAT, name.c_str (), i);
                    }
                    else {
                        workerName = name;
                    }
                }
                workers.push_back (
                    new Worker (
                        *this,
                        workerName,
                        workerPriority,
                        workerAffinity));
            }
        }

        bool JobQueue::Stop (
                bool cancelRunningJobs,
                bool cancelPendingJobs,
                const TimeSpec &timeSpec) {
            LockGuard<Mutex> guard (workersMutex);
            TimeSpec deadline = GetCurrentTime () + timeSpec;
            if (!Pause (cancelRunningJobs, deadline - GetCurrentTime ())) {
                return false;
            }
            // At this point there should be no more running jobs.
            assert (runningJobs.empty ());
            if (cancelPendingJobs) {
                // CancelPendingJobs does not block.
                CancelPendingJobs ();
                // This Continue is necessary to wake up from
                // Pause above so that WaitForIdle can do it's
                // job.
                Continue ();
                if (!workers.empty ()) {
                    // Since there are no more runningJobs, and
                    // pendingJobs have been cancelled, WaitForIdle
                    // should complete quickly.
                    if (!WaitForIdle (deadline - GetCurrentTime ())) {
                        return false;
                    }
                }
                else {
                    // The queue has no worker threads. Simulate what
                    // they would do to make sure anyone waiting on
                    // pending jobs gets notified.
                    Job *job;
                    while ((job = DeqJob (false)) != 0) {
                        FinishedJob (job, 0, 0);
                    }
                }
            }
            done = true;
            // This Continue is necessary in case cancelPendingJobs == false.
            // If cancelPendingJobs == true, it's harmless.
            Continue ();
            jobsNotEmpty.SignalAll ();
            struct Callback : public WorkerList::Callback {
                typedef WorkerList::Callback::result_type result_type;
                typedef WorkerList::Callback::argument_type argument_type;
                const TimeSpec &deadline;
                explicit Callback (const TimeSpec &deadline_) :
                    deadline (deadline_) {}
                virtual result_type operator () (argument_type worker) {
                    // Join the worker thread before deleting it to
                    // let it's thread function finish it's teardown.
                    if (!worker->Wait (deadline - GetCurrentTime ())) {
                        return false;
                    }
                    delete worker;
                    return true;
                }
            } callback (deadline);
            // Since there are no more jobs (running or pending)
            // and done == true, workers.clear should complete
            // quickly.
            if (!workers.clear (callback)) {
                return false;
            }
            idle.SignalAll ();
            return true;
        }

        bool JobQueue::IsRunning () {
            return !workers.empty ();
        }

    } // namespace util
} // namespace thekogans
