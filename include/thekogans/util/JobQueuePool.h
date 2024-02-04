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

#if !defined (__thekogans_util_JobQueuePool_h)
#define __thekogans_util_JobQueuePool_h

#include <memory>
#include <string>
#include <atomic>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/Thread.h"
#include "thekogans/util/JobQueue.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Condition.h"
#include "thekogans/util/SystemInfo.h"
#include "thekogans/util/Singleton.h"

namespace thekogans {
    namespace util {

        /// \struct JobQueuePool JobQueuePool.h thekogans/util/JobQueuePool.h
        ///
        /// \brief
        /// JobQueuePool implements a very convenient pool of
        /// \see{JobQueue}s. Here is a canonical use case:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// util::JobQueuePool jobQueuePool;
        ///
        /// void foo (...) {
        ///     struct Job : public util::RunLoop::Job {
        ///         util::JobQueue::SharedPtr jobQueue;
        ///         ...
        ///         Job (
        ///             util::JobQueue::SharedPtr jobQueue_,
        ///             ...) :
        ///             jobQueue (jobQueue_),
        ///             ... {}
        ///
        ///         // util::RunLoop::Job
        ///         virtual void Execute (const std::atomic<bool> &) throw () {
        ///             ...
        ///         }
        ///     };
        ///     util::JobQueue::SharedPtr jobQueue = jobQueuePool.GetJobQueue ();
        ///     if (jobQueue.Get () != 0) {
        ///         jobQueue->EnqJob (RunLoop::Job::SharedPtr (new Job (jobQueue, ...)));
        ///     }
        /// }
        /// \endcode
        ///
        /// Note how the Job controls the lifetime of the \see{JobQueue}.
        /// By passing util::JobQueue::SharedPtr in to the Job's ctor we guarantee
        /// that the \see{JobQueue} will be returned back to the pool as soon
        /// as the Job goes out of scope (as Job will be the last reference).

        struct _LIB_THEKOGANS_UTIL_DECL JobQueuePool {
        private:
            /// \brief
            /// Minimum number of job queues to keep in the pool.
            const std::size_t minJobQueues;
            /// \brief
            /// Maximum number of job queues allowed in the pool.
            const std::size_t maxJobQueues;
            /// \brief
            /// \see{JobQueue} name.
            const std::string name;
            /// \brief
            /// JobQueue \see{RunLoop::JobExecutionPolicy}.
            RunLoop::JobExecutionPolicy::SharedPtr jobExecutionPolicy;
            /// \brief
            /// Number of worker threads servicing the \see{JobQueue}.
            const std::size_t workerCount;
            /// \brief
            /// \see{JobQueue} worker thread priority.
            const i32 workerPriority;
            /// \brief
            /// \see{JobQueue} worker thread processor affinity.
            const ui32 workerAffinity;
            /// \brief
            /// Called to initialize/uninitialize the \see{JobQueue} worker thread.
            RunLoop::WorkerCallback *workerCallback;
            /// \brief
            /// Forward declaration of JobQueue.
            struct JobQueue;
            enum {
                /// \brief
                /// JobQueueList list id.
                JOB_QUEUE_LIST_ID
            };
            /// \brief
            /// Convenient typedef for IntrusiveList<JobQueue, JOB_QUEUE_LIST_ID>.
            typedef IntrusiveList<JobQueue, JOB_QUEUE_LIST_ID> JobQueueList;
        #if defined (TOOLCHAIN_COMPILER_cl)
            #pragma warning (push)
            #pragma warning (disable : 4275)
        #endif // defined (TOOLCHAIN_COMPILER_cl)
            /// \struct JobQueuePool::JobQueue JobQueuePool.h thekogans/util/JobQueuePool.h
            ///
            /// \brief
            /// Extends \see{JobQueue} to enable returning self to the pool after use.
            struct JobQueue :
                    public JobQueueList::Node,
                    public util::JobQueue {
                /// \brief
                /// JobQueue has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (JobQueue, SpinLock)

            private:
                /// \brief
                /// JobQueuePool from which this \see{JobQueue} came.
                JobQueuePool &jobQueuePool;

            public:
                /// \brief
                /// ctor.
                /// \param[in] name \see{JobQueue} name.
                /// \param[in] jobExecutionPolicy \see{JobQueue} \see{RunLoop::JobExecutionPolicy}.
                /// \param[in] workerCount Number of worker threads servicing the \see{JobQueue}.
                /// \param[in] workerPriority \see{JobQueue} worker thread priority.
                /// \param[in] workerAffinity \see{JobQueue} worker thread processor affinity.
                /// \param[in] workerCallback Called to initialize/uninitialize the
                /// \see{JobQueue} worker thread(s).
                /// \param[in] jobQueuePool_ JobQueuePool to which this jobQueue belongs.
                JobQueue (
                    const std::string &name,
                    RunLoop::JobExecutionPolicy::SharedPtr jobExecutionPolicy,
                    std::size_t workerCount,
                    i32 workerPriority,
                    ui32 workerAffinity,
                    WorkerCallback *workerCallback,
                    JobQueuePool &jobQueuePool_) :
                    util::JobQueue (
                        name,
                        jobExecutionPolicy,
                        workerCount,
                        workerPriority,
                        workerAffinity,
                        workerCallback),
                    jobQueuePool (jobQueuePool_) {}

            protected:
                // RefCounted
                /// \brief
                /// If there are no more references to this job queue,
                /// release it back to the pool.
                virtual void Harakiri () {
                    jobQueuePool.ReleaseJobQueue (this);
                }

                /// \brief
                /// JobQueue is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (JobQueue)
            };
        #if defined (TOOLCHAIN_COMPILER_cl)
            #pragma warning (pop)
        #endif // defined (TOOLCHAIN_COMPILER_cl)
            /// \brief
            /// List of available \see{JobQueue}s.
            JobQueueList availableJobQueues;
            /// \brief
            /// List of borrowed \see{JobQueue}s.
            JobQueueList borrowedJobQueues;
            /// \brief
            /// \see{JobQueue} id pool. If !name.empty (),
            /// each \see{JobQueue} created by this pool
            /// will have the following name:
            /// FormatString ("%s-" THEKOGANS_UTIL_SIZE_T_FORMAT, name.c_str (), ++idPool);
            std::atomic<std::size_t> idPool;
            /// \brief
            /// Synchronization mutex.
            Mutex mutex;
            /// \brief
            /// Synchronization condition variable.
            Condition idle;

        public:
            /// \brief
            /// ctor.
            /// \param[in] minJobQueues_ Minimum \see{JobQueue}s to keep in the pool.
            /// \param[in] maxJobQueues_ Maximum \see{JobQueue}s to allow the pool to grow to.
            /// \param[in] name_ \see{JobQueue} name.
            /// \param[in] jobExecutionPolicy_ \see{JobQueue} \see{RunLoop::JobExecutionPolicy}.
            /// \param[in] workerCount_ Number of worker threads servicing the \see{JobQueue}.
            /// \param[in] workerPriority_ \see{JobQueue} worker thread priority.
            /// \param[in] workerAffinity_ \see{JobQueue} worker thread processor affinity.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the \see{JobQueue}
            /// worker thread.
            JobQueuePool (
                std::size_t minJobQueues_,
                std::size_t maxJobQueues_,
                const std::string &name_ = std::string (),
                RunLoop::JobExecutionPolicy::SharedPtr jobExecutionPolicy_ =
                    RunLoop::JobExecutionPolicy::SharedPtr (new RunLoop::FIFOJobExecutionPolicy),
                std::size_t workerCount_ = 1,
                i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity_ = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                RunLoop::WorkerCallback *workerCallback_ = nullptr);
            /// \brief
            /// dtor.
            virtual ~JobQueuePool ();

            /// \brief
            /// Acquire a \see{JobQueue} from the pool.
            /// \param[in] retries Number of times to retry if a \see{JobQueue} is not
            /// immediately available.
            /// \param[in] timeSpec How long to wait between retries.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return A \see{JobQueue} from the pool (JobQueue::SharedPtr () if pool is exhausted).
            util::JobQueue::SharedPtr GetJobQueue (
                std::size_t retries = 1,
                const TimeSpec &timeSpec = TimeSpec::FromMilliseconds (100));

            /// \brief
            /// Return all borrowedJobQueues jobs matching the given equality test.
            /// \param[in] equalityTest \see{RunLoop::EqualityTest} to query to determine the matching jobs.
            /// \param[out] jobs \see{RunLoop::UserJobList} containing the matching jobs.
            /// NOTE: This method will take a reference on all matching jobs.
            void GetJobs (
                const RunLoop::EqualityTest &equalityTest,
                RunLoop::UserJobList &jobs);
            /// \brief
            /// Return all borrowedJobQueues jobs matching the given equality test.
            /// \param[in] function \see{RunLoop::LambdaEqualityTest::Function} to query to determine the matching jobs.
            /// \param[out] jobs \see{RunLoop::UserJobList} containing the matching jobs.
            /// NOTE: This method will take a reference on all matching jobs.
            inline void GetJobs (
                    const RunLoop::LambdaEqualityTest::Function &function,
                    RunLoop::UserJobList &jobs) {
                GetJobs (RunLoop::LambdaEqualityTest (function), jobs);
            }
            /// \brief
            /// Wait for all borrowedJobQueues jobs matching the given equality test to complete.
            /// \param[in] equalityTest \see{RunLoop::EqualityTest} to query to determine which jobs to wait on.
            /// \param[in] timeSpec How long to wait for the jobs to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == All jobs satisfying the equalityTest completed,
            /// false == One or more matching jobs timed out.
            bool WaitForJobs (
                const RunLoop::EqualityTest &equalityTest,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Wait for all borrowedJobQueues jobs matching the given equality test to complete.
            /// \param[in] function \see{RunLoop::LambdaEqualityTest::Function} to query to
            /// determine which jobs to wait on.
            /// \param[in] timeSpec How long to wait for the jobs to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == All jobs satisfying the equalityTest completed,
            /// false == One or more matching jobs timed out.
            inline bool WaitForJobs (
                    const RunLoop::LambdaEqualityTest::Function &function,
                    const TimeSpec &timeSpec = TimeSpec::Infinite) {
                return WaitForJobs (RunLoop::LambdaEqualityTest (function), timeSpec);
            }
            /// \brief
            /// Cancel all borrowedJobQueues jobs matching the given equality test.
            /// \param[in] equalityTest EqualityTest to query to determine the matching jobs.
            void CancelJobs (const RunLoop::EqualityTest &equalityTest);
            /// \brief
            /// Cancel all borrowedJobQueues jobs matching the given equality test.
            /// \param[in] function \see{RunLoop::LambdaEqualityTest::Function} to query to determine the matching jobs.
            inline void CancelJobs (const RunLoop::LambdaEqualityTest::Function &function) {
                CancelJobs (RunLoop::LambdaEqualityTest (function));
            }

            /// \brief
            /// Blocks until all borrowed \see{JobQueue}s have been returned to the pool.
            /// \param[in] timeSpec How long to wait for \see{JobQueue}s to return.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == JobQueuePool is idle, false == Timed out.
            bool WaitForIdle (const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Return true if this pool has no outstanding \see{JobQueue}s.
            /// \return true == this pool has no outstanding \see{JobQueue}s.
            bool IsIdle ();

        private:
            /// \brief
            /// Used by \see{GetJobQueue} to acquire a \see{JobQueue} from the pool.
            /// \return \see{JobQueue} pointer.
            JobQueue *AcquireJobQueue ();
            /// \brief
            /// Used by \see{JobQueue} to release itself to the pool.
            /// \param[in] jobQueue \see{JobQueue} to release.
            void ReleaseJobQueue (JobQueue *jobQueue);

            /// \brief
            /// JobQueuePool is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (JobQueuePool)
        };

        /// \struct GlobalJobQueuePool JobQueuePool.h thekogans/util/JobQueuePool.h
        ///
        /// \brief
        /// A global \see{JobQueuePool} instance. The \see{JobQueuePool} is
        /// designed to be as flexible as possible. To be useful in different
        /// situations the \see{JobQueuePool}'s min/max queue count needs to
        /// be parametrized as we might need different pools running different
        /// counts at different queue priorities. That said, the most basic
        /// (and the most useful) use case will have the global pool using the
        /// defaults. This struct exists to aid in that. If all you need is a
        /// global \see{JobQueuePool} then GlobalJobQueuePool::Instance () will
        /// do the trick.
        /// IMPORTANT: Unlike some other global objects, you cannot use this one
        /// without first calling GlobalJobQueuePool::CreateInstance first. This
        /// is because you need to provide the min and max \see{JobQueue}s that
        /// this pool will manage.

        struct _LIB_THEKOGANS_UTIL_DECL GlobalJobQueuePool :
                public JobQueuePool,
                public Singleton<GlobalJobQueuePool, SpinLock> {
            /// \brief
            /// Create a global \see{JobQueuePool} with custom ctor arguments.
            /// \param[in] minJobQueues Minimum \see{JobQueue}s to keep in the pool.
            /// \param[in] maxJobQueues Maximum \see{JobQueue}s to allow the pool to grow to.
            /// \param[in] name \see{JobQueue} name.
            /// \param[in] jobExecutionPolicy \see{JobQueue} \see{RunLoop::JobExecutionPolicy}.
            /// \param[in] workerCount Number of worker threads servicing the \see{JobQueue}.
            /// \param[in] workerPriority \see{JobQueue} worker thread priority.
            /// \param[in] workerAffinity \see{JobQueue} worker thread processor affinity.
            /// \param[in] workerCallback Called to initialize/uninitialize the \see{JobQueue}
            /// thread.
            GlobalJobQueuePool (
                std::size_t minJobQueues = 0,
                std::size_t maxJobQueues = 0,
                const std::string &name = "GlobalJobQueuePool",
                RunLoop::JobExecutionPolicy::SharedPtr jobExecutionPolicy =
                    RunLoop::JobExecutionPolicy::SharedPtr (new RunLoop::FIFOJobExecutionPolicy),
                std::size_t workerCount = 1,
                i32 workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                RunLoop::WorkerCallback *workerCallback = nullptr) :
                JobQueuePool (
                    minJobQueues,
                    maxJobQueues,
                    name,
                    jobExecutionPolicy,
                    workerCount,
                    workerPriority,
                    workerAffinity,
                    workerCallback) {}
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_JobQueuePool_h)
