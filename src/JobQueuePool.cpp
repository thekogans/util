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

#include "thekogans/util/Config.h"
#include "thekogans/util/Timer.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/JobQueuePool.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (JobQueuePool::JobQueue, SpinLock)

        JobQueuePool::JobQueuePool (
                std::size_t minJobQueues_,
                std::size_t maxJobQueues_,
                const std::string &name_,
                RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy_,
                std::size_t workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                RunLoop::WorkerCallback *workerCallback_) :
                minJobQueues (minJobQueues_),
                maxJobQueues (maxJobQueues_),
                name (name_),
                jobExecutionPolicy (jobExecutionPolicy_),
                workerCount (workerCount_),
                workerPriority (workerPriority_),
                workerAffinity (workerAffinity_),
                workerCallback (workerCallback_),
                idPool (0),
                idle (mutex) {
            // By requiring at least one JobQueue in reserve coupled
            // with the logic in ReleaseJobQueue below, we guarantee
            // that we avoid the deadlock associated with trying to
            // delete the JobQueue being released.
            if (0 < minJobQueues && minJobQueues <= maxJobQueues) {
                for (std::size_t i = 0; i < minJobQueues; ++i) {
                    std::string jobQueueName;
                    if (!name.empty ()) {
                        jobQueueName = FormatString (
                            "%s-" THEKOGANS_UTIL_SIZE_T_FORMAT,
                            name.c_str (),
                            ++idPool);
                    }
                    availableJobQueues.push_back (
                        new JobQueue (
                            jobQueueName,
                            jobExecutionPolicy,
                            workerCount,
                            workerPriority,
                            workerAffinity,
                            workerCallback,
                            *this));
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        JobQueuePool::~JobQueuePool () {
            LockGuard<Mutex> guard (mutex);
            struct DeleteCallback : public JobQueueList::Callback {
                typedef JobQueueList::Callback::result_type result_type;
                typedef JobQueueList::Callback::argument_type argument_type;
                virtual result_type operator () (argument_type jobQueue) {
                    delete jobQueue;
                    return true;
                }
            } deleteCallback;
            availableJobQueues.clear (deleteCallback);
            borrowedJobQueues.clear (deleteCallback);
        }

        JobQueue::Ptr JobQueuePool::GetJobQueue (
                std::size_t retries,
                const TimeSpec &timeSpec) {
            JobQueue *jobQueue = AcquireJobQueue ();
            while (jobQueue == 0 && retries-- > 0) {
                Sleep (timeSpec);
                jobQueue = AcquireJobQueue ();
            }
            return util::JobQueue::Ptr (jobQueue);
        }

        void JobQueuePool::GetJobs (
                const RunLoop::EqualityTest &equalityTest,
                RunLoop::UserJobList &jobs) {
            LockGuard<Mutex> guard (mutex);
            struct GetJobsCallback : public JobQueueList::Callback {
                typedef JobQueueList::Callback::result_type result_type;
                typedef JobQueueList::Callback::argument_type argument_type;
                const RunLoop::EqualityTest &equalityTest;
                RunLoop::UserJobList &jobs;
                GetJobsCallback (
                    const RunLoop::EqualityTest &equalityTest_,
                    RunLoop::UserJobList &jobs_) :
                    equalityTest (equalityTest_),
                    jobs (jobs_) {}
                virtual result_type operator () (argument_type jobQueue) {
                    jobQueue->GetJobs (equalityTest, jobs);
                    return true;
                }
            } getJobsCallback (equalityTest, jobs);
            borrowedJobQueues.for_each (getJobsCallback);
        }

        bool JobQueuePool::WaitForJobs (
                const RunLoop::EqualityTest &equalityTest,
                const TimeSpec &timeSpec) {
            RunLoop::UserJobList jobs;
            GetJobs (equalityTest, jobs);
            return RunLoop::WaitForJobs (jobs, timeSpec);
        }

        void JobQueuePool::CancelJobs (RunLoop::EqualityTest &equalityTest) {
            LockGuard<Mutex> guard (mutex);
            struct CancelJobsCallback : public JobQueueList::Callback {
                typedef JobQueueList::Callback::result_type result_type;
                typedef JobQueueList::Callback::argument_type argument_type;
                RunLoop::EqualityTest &equalityTest;
                explicit CancelJobsCallback (RunLoop::EqualityTest &equalityTest_) :
                    equalityTest (equalityTest_) {}
                virtual result_type operator () (argument_type jobQueue) {
                    jobQueue->CancelJobs (equalityTest);
                    return true;
                }
            } cancelJobsCallback (equalityTest);
            borrowedJobQueues.for_each (cancelJobsCallback);
        }

        bool JobQueuePool::WaitForIdle (const TimeSpec &timeSpec) {
            LockGuard<Mutex> guard (mutex);
            if (timeSpec == TimeSpec::Infinite) {
                while (!borrowedJobQueues.empty ()) {
                    idle.Wait ();
                }
            }
            else {
                TimeSpec now = GetCurrentTime ();
                TimeSpec deadline = now + timeSpec;
                while (!borrowedJobQueues.empty () && deadline > now) {
                    idle.Wait (deadline - now);
                    now = GetCurrentTime ();
                }
            }
            return borrowedJobQueues.empty ();
        }

        bool JobQueuePool::IsIdle () {
            LockGuard<Mutex> guard (mutex);
            return borrowedJobQueues.empty ();
        }

        JobQueuePool::JobQueue *JobQueuePool::AcquireJobQueue () {
            JobQueue *jobQueue = 0;
            {
                LockGuard<Mutex> guard (mutex);
                if (!availableJobQueues.empty ()) {
                    // Borrow a job queue from the front of the pool.
                    // This combined with ReleaseJobQueue putting
                    // returned job queue at the front should
                    // guarantee the best cache utilization.
                    jobQueue = availableJobQueues.pop_front ();
                }
                else if (availableJobQueues.size () + borrowedJobQueues.size () < maxJobQueues) {
                    std::string jobQueueName;
                    if (!name.empty ()) {
                        jobQueueName = FormatString (
                            "%s-" THEKOGANS_UTIL_SIZE_T_FORMAT,
                            name.c_str (),
                            ++idPool);
                    }
                    jobQueue = new JobQueue (
                        jobQueueName,
                        jobExecutionPolicy,
                        workerCount,
                        workerPriority,
                        workerAffinity,
                        workerCallback,
                        *this);
                }
                if (jobQueue != 0) {
                    borrowedJobQueues.push_back (jobQueue);
                }
            }
            return jobQueue;
        }

        void JobQueuePool::ReleaseJobQueue (JobQueue *jobQueue) {
            if (jobQueue != 0) {
                LockGuard<Mutex> guard (mutex);
                borrowedJobQueues.erase (jobQueue);
                // Put the recently used job queue at the front of
                // the list. With any luck the next time a job queue
                // is borrowed from this pool, it will be the last
                // one used, and it's cache will be nice and warm.
                availableJobQueues.push_front (jobQueue);
                // If the pool is idle, see if we need to remove excess job queues.
                if (borrowedJobQueues.empty ()) {
                    while (availableJobQueues.size () > minJobQueues) {
                        // Delete the least recently used queues. This logic
                        // guarantees that we avoid the deadlock associated
                        // with deleating the passed in jobQueue.
                        delete availableJobQueues.pop_back ();
                    }
                    idle.SignalAll ();
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    } // namespace util
} // namespace thekogans
