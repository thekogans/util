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

        struct JobQueuePoolRegistry :
                public Singleton<JobQueuePoolRegistry, SpinLock>,
                public Timer::Callback {
        private:
            Timer timer;
            JobQueuePoolList jobQueuePools;
            Mutex mutex;

        public:
            JobQueuePoolRegistry () :
                timer (*this) {}

            void RegisterJobQueuePool (JobQueuePool *jobQueuePool) {
                LockGuard<Mutex> guard (mutex);
                jobQueuePools.push_back (jobQueuePool);
                if (jobQueuePools.size () == 1) {
                    // FIXME: Parametrize the interval.
                    timer.Start (TimeSpec::FromSeconds (10), true);
                }
            }

            void UnregisterJobQueuePool (JobQueuePool *jobQueuePool) {
                LockGuard<Mutex> guard (mutex);
                jobQueuePools.erase (jobQueuePool);
                if (jobQueuePools.empty ()) {
                    timer.Stop ();
                }
            }

        private:
            // Timer
            virtual void Alarm (Timer & /*timer*/) throw () {
                LockGuard<Mutex> guard (mutex);
                struct DeleteJobQueuesCallback : public JobQueuePoolList::Callback {
                    typedef JobQueuePoolList::Callback::result_type result_type;
                    typedef JobQueuePoolList::Callback::argument_type argument_type;
                    virtual result_type operator () (argument_type jobQueuePool) {
                        jobQueuePool->DeleteJobQueues ();
                        return true;
                    }
                } deleteJobQueuesCallback;
                jobQueuePools.for_each (deleteJobQueuesCallback);
            }
        };

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (JobQueuePool::JobQueue, SpinLock)

        JobQueuePool::JobQueuePool (
                ui32 minJobQueues_,
                ui32 maxJobQueues_,
                const std::string &name_,
                RunLoop::Type type_,
                ui32 maxPendingJobs_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                RunLoop::WorkerCallback *workerCallback_) :
                minJobQueues (minJobQueues_),
                maxJobQueues (maxJobQueues_),
                name (name_),
                type (type_),
                maxPendingJobs (maxPendingJobs_),
                workerCount (workerCount_),
                workerPriority (workerPriority_),
                workerAffinity (workerAffinity_),
                workerCallback (workerCallback_),
                idle (mutex) {
            if (minJobQueues > 0 && minJobQueues <= maxJobQueues) {
                for (ui32 i = 0; i < minJobQueues; ++i) {
                    std::string jobQueueName;
                    if (!name.empty ()) {
                        jobQueueName = FormatString ("%s-%u", name.c_str (), i);
                    }
                    availableJobQueues.push_back (
                        new JobQueue (
                            *this,
                            jobQueueName,
                            type,
                            maxPendingJobs,
                            workerCount,
                            workerPriority,
                            workerAffinity,
                            workerCallback));
                }
                if (maxJobQueues > minJobQueues) {
                    JobQueuePoolRegistry::Instance ().RegisterJobQueuePool (this);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        JobQueuePool::~JobQueuePool () {
            if (maxJobQueues > minJobQueues) {
                JobQueuePoolRegistry::Instance ().UnregisterJobQueuePool (this);
            }
            LockGuard<Mutex> guard (mutex);
            {
                struct StopCallback : public JobQueueList::Callback {
                    typedef JobQueueList::Callback::result_type result_type;
                    typedef JobQueueList::Callback::argument_type argument_type;
                    virtual result_type operator () (argument_type jobQueue) {
                        jobQueue->Stop ();
                        return true;
                    }
                } stopCallback;
                availableJobQueues.for_each (stopCallback);
                borrowedJobQueues.for_each (stopCallback);
            }
            WaitForIdle ();
            {
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
        }

        JobQueue::Ptr JobQueuePool::GetJobQueue (
                ui32 retries,
                const TimeSpec &timeSpec) {
            JobQueue *jobQueue = GetJobQueueHelper ();
            while (jobQueue == 0 && retries-- > 0) {
                Sleep (timeSpec);
                jobQueue = GetJobQueueHelper ();
            }
            return util::JobQueue::Ptr (jobQueue);
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

        JobQueuePool::JobQueue *JobQueuePool::GetJobQueueHelper () {
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
                            "%s-%u",
                            name.c_str (),
                            availableJobQueues.size () + borrowedJobQueues.size ());
                    }
                    jobQueue = new JobQueue (
                        *this,
                        jobQueueName,
                        type,
                        maxPendingJobs,
                        workerCount,
                        workerPriority,
                        workerAffinity,
                        workerCallback);
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
                if (borrowedJobQueues.empty ()) {
                    idle.SignalAll ();
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void JobQueuePool::DeleteJobQueues () {
            LockGuard<Mutex> guard (mutex);
            if (borrowedJobQueues.empty () && availableJobQueues.size () > minJobQueues) {
                struct DeleteCallback : public JobQueueList::Callback {
                    typedef JobQueueList::Callback::result_type result_type;
                    typedef JobQueueList::Callback::argument_type argument_type;
                    std::size_t deleteCount;
                    explicit DeleteCallback (std::size_t deleteCount_) :
                        deleteCount (deleteCount_) {}
                    virtual result_type operator () (argument_type jobQueue) {
                        delete jobQueue;
                        return --deleteCount > 0;
                    }
                } deleteCallback (availableJobQueues.size () - minJobQueues);
                // Walk the pool in reverse to delete the least recently used queues.
                availableJobQueues.for_each (deleteCallback, true);
            }
        }

        ui32 GlobalJobQueuePoolCreateInstance::minJobQueues = SystemInfo::Instance ().GetCPUCount ();
        ui32 GlobalJobQueuePoolCreateInstance::maxJobQueues = SystemInfo::Instance ().GetCPUCount () * 2;
        std::string GlobalJobQueuePoolCreateInstance::name = std::string ();
        RunLoop::Type GlobalJobQueuePoolCreateInstance::type = RunLoop::TYPE_FIFO;
        ui32 GlobalJobQueuePoolCreateInstance::maxPendingJobs = UI32_MAX;
        ui32 GlobalJobQueuePoolCreateInstance::workerCount = 1;
        i32 GlobalJobQueuePoolCreateInstance::workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY;
        ui32 GlobalJobQueuePoolCreateInstance::workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY;
        RunLoop::WorkerCallback *GlobalJobQueuePoolCreateInstance::workerCallback = 0;

        void GlobalJobQueuePoolCreateInstance::Parameterize (
                ui32 minJobQueues_,
                ui32 maxJobQueues_,
                const std::string &name_,
                RunLoop::Type type_,
                ui32 maxPendingJobs_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                RunLoop::WorkerCallback *workerCallback_) {
            minJobQueues = minJobQueues_;
            maxJobQueues = maxJobQueues_;
            name = name_;
            type = type_;
            maxPendingJobs = maxPendingJobs_;
            workerCount = workerCount_;
            workerPriority = workerPriority_;
            workerAffinity = workerAffinity_;
            workerCallback = workerCallback_;
        }

        JobQueuePool *GlobalJobQueuePoolCreateInstance::operator () () {
            return new JobQueuePool (
                minJobQueues,
                maxJobQueues,
                name,
                type,
                maxPendingJobs,
                workerCount,
                workerPriority,
                workerAffinity,
                workerCallback);
        }

    } // namespace util
} // namespace thekogans
