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
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/LoggerMgr.h"
#endif // defined (TOOLCHAIN_OS_Windows)
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
                JobExecutionPolicy::Ptr jobExecutionPolicy,
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

        void JobQueue::Start () {
            LockGuard<Mutex> guard (workersMutex);
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
                // Since there are no more runningJobs, and
                // pendingJobs have been cancelled, WaitForIdle
                // should complete quickly.
                if (!WaitForIdle (deadline - GetCurrentTime ())) {
                    return false;
                }
            }
            struct ToggleDone {
                THEKOGANS_UTIL_ATOMIC<bool> &done;
                ToggleDone (THEKOGANS_UTIL_ATOMIC<bool> &done_) :
                        done (done_) {
                    done = true;
                }
                ~ToggleDone () {
                    done = false;
                }
            } toggleDone (done);
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
            return workers.clear (callback);
        }

        const char *GlobalJobQueueCreateInstance::GLOBAL_JOB_QUEUE_NAME = "GlobalJobQueue";
        std::string GlobalJobQueueCreateInstance::name = GlobalJobQueueCreateInstance::GLOBAL_JOB_QUEUE_NAME;
        RunLoop::JobExecutionPolicy::Ptr GlobalJobQueueCreateInstance::jobExecutionPolicy =
            RunLoop::JobExecutionPolicy::Ptr (new RunLoop::FIFOJobExecutionPolicy);
        std::size_t GlobalJobQueueCreateInstance::workerCount = 1;
        i32 GlobalJobQueueCreateInstance::workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY;
        ui32 GlobalJobQueueCreateInstance::workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY;
        RunLoop::WorkerCallback *GlobalJobQueueCreateInstance::workerCallback = 0;

        void GlobalJobQueueCreateInstance::Parameterize (
                const std::string &name_,
                RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy_,
                std::size_t workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                RunLoop::WorkerCallback *workerCallback_) {
            name = name_;
            jobExecutionPolicy = jobExecutionPolicy_;
            workerCount = workerCount_;
            workerPriority = workerPriority_;
            workerAffinity = workerAffinity_;
            workerCallback = workerCallback_;
        }

        JobQueue *GlobalJobQueueCreateInstance::operator () () {
            return new JobQueue (
                name,
                jobExecutionPolicy,
                workerCount,
                workerPriority,
                workerAffinity,
                workerCallback);
        }

    } // namespace util
} // namespace thekogans
