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
#include "thekogans/util/HRTimer.h"
#include "thekogans/util/SystemInfo.h"
#include "thekogans/util/Singleton.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Forward declaration of JobQueuePool.
        struct JobQueuePool;
        enum {
            /// \brief
            /// JobQueuePool list id.
            JOB_QUEUE_POOL_LIST_ID
        };
        /// \brief
        /// Convenient typedef for IntrusiveList<JobQueuePool, JOB_QUEUE_POOL_LIST_ID>.
        typedef IntrusiveList<JobQueuePool, JOB_QUEUE_POOL_LIST_ID> JobQueuePoolList;

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
        ///         util::JobQueue::Ptr jobQueue;
        ///         ...
        ///         Job (
        ///             util::JobQueue::Ptr jobQueue_,
        ///             ...) :
        ///             jobQueue (jobQueue_),
        ///             ... {}
        ///
        ///         // util::RunLoop::Job
        ///         virtual void Execute (const THEKOGANS_UTIL_ATOMIC<bool> &) throw () {
        ///             ...
        ///         }
        ///     };
        ///     util::JobQueue::Ptr jobQueue = jobQueuePool.GetJobQueue ();
        ///     if (jobQueue.Get () != 0) {
        ///         jobQueue->EnqJob (RunLoop::Job::Ptr (new Job (jobQueue, ...)));
        ///     }
        /// }
        /// \endcode
        ///
        /// Note how the Job controls the lifetime of the \see{JobQueue}.
        /// By passing util::JobQueue::Ptr in to the Job's ctor we guarantee
        /// that the \see{JobQueue} will be returned back to the pool as soon
        /// as the Job goes out of scope (as Job will be the last reference).

    #if defined (_MSC_VER)
        #pragma warning (push)
        #pragma warning (disable : 4275)
    #endif // defined (_MSC_VER)
        struct _LIB_THEKOGANS_UTIL_DECL JobQueuePool : public JobQueuePoolList::Node {
        private:
            /// \brief
            /// Minimum number of job queues to keep in the pool.
            const ui32 minJobQueues;
            /// \brief
            /// Maximum number of job queues allowed in the pool.
            const ui32 maxJobQueues;
            /// \brief
            /// \see{JobQueue} name.
            const std::string name;
            /// \brief
            /// \see{JobQueue} type.
            const RunLoop::Type type;
            /// \brief
            /// \see{JobQueue} max pending jobs.
            const ui32 maxPendingJobs;
            /// \brief
            /// Number of worker threads service the \see{JobQueue}.
            const ui32 workerCount;
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
        #if defined (_MSC_VER)
            #pragma warning (push)
            #pragma warning (disable : 4275)
        #endif // defined (_MSC_VER)
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
                /// \brief
                /// Used to implement \see{JobQueue} TTL.
                ui64 lastUsedTime;

            public:
                /// \brief
                /// ctor.
                /// \param[in] jobQueuePool_ JobQueuePool to which this jobQueue belongs.
                /// \param[in] name \see{JobQueue} name.
                /// \param[in] type \see{JobQueue} type.
                /// \param[in] maxPendingJobs \see{JobQueue} max pending jobs.
                /// \param[in] workerCount Number of worker threads service the \see{JobQueue}.
                /// \param[in] workerPriority \see{JobQueue} worker thread priority.
                /// \param[in] workerAffinity \see{JobQueue} worker thread processor affinity.
                /// \param[in] workerCallback Called to initialize/uninitialize the
                /// \see{JobQueue} worker thread(s).
                JobQueue (
                    JobQueuePool &jobQueuePool_,
                    const std::string &name = std::string (),
                    Type type = TYPE_FIFO,
                    ui32 maxPendingJobs = UI32_MAX,
                    ui32 workerCount = 1,
                    i32 workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                    ui32 workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                    WorkerCallback *workerCallback = 0) :
                    util::JobQueue (
                        name,
                        type,
                        maxPendingJobs,
                        workerCount,
                        workerPriority,
                        workerAffinity,
                        workerCallback),
                    jobQueuePool (jobQueuePool_),
                    lastUsedTime (0) {}

            protected:
                // RefCounted
                /// \brief
                /// If there are no more references to this job queue,
                /// release it back to the pool.
                virtual void Harakiri () {
                    lastUsedTime = HRTimer::Click ();
                    jobQueuePool.ReleaseJobQueue (this);
                }

                /// \brief
                /// JobQueuePool needs access to lastUsedTime.
                friend struct JobQueuePool;

                /// \brief
                /// JobQueue is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (JobQueue)
            };
        #if defined (_MSC_VER)
            #pragma warning (pop)
        #endif // defined (_MSC_VER)
            /// \brief
            /// List of workers.
            JobQueueList availableJobQueues;
            /// \brief
            /// List of workers.
            JobQueueList borrowedJobQueues;
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
            /// \param[in] type_ \see{JobQueue} type.
            /// \param[in] maxPendingJobs_ Max pending \see{JobQueue} jobs.
            /// \param[in] workerCount_ Number of worker threads service the \see{JobQueue}.
            /// \param[in] workerPriority_ \see{JobQueue} worker thread priority.
            /// \param[in] workerAffinity_ \see{JobQueue} worker thread processor affinity.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the \see{JobQueue}
            /// worker thread.
            JobQueuePool (
                ui32 minJobQueues_ = SystemInfo::Instance ().GetCPUCount (),
                ui32 maxJobQueues_ = SystemInfo::Instance ().GetCPUCount () * 2,
                const std::string &name_ = std::string (),
                RunLoop::Type type_ = RunLoop::TYPE_FIFO,
                ui32 maxPendingJobs_ = UI32_MAX,
                ui32 workerCount_ = 1,
                i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity_ = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                RunLoop::WorkerCallback *workerCallback_ = 0);
            /// \brief
            /// dtor.
            ~JobQueuePool ();

            /// \brief
            /// Acquire a \see{JobQueue} from the pool.
            /// \param[in] retries Number of times to retry if a \see{JobQueue} is not
            /// immediately available.
            /// \param[in] timeSpec How long to wait between retries.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return A \see{JobQueue} from the pool (JobQueue::Ptr () if pool is exhausted).
            util::JobQueue::Ptr GetJobQueue (
                ui32 retries = 1,
                const TimeSpec &timeSpec = TimeSpec::FromMilliseconds (100));

            /// \brief
            /// Blocks until all borrowed \see{JobQueue}s have been returned to the pool.
            /// \param[in] timeSpec How long to wait for \see{JobQueue}s to return.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == JobQueuePool is idle, false == Timed out.
            bool WaitForIdle (
                const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Return true if this pool has no outstanding \see{JobQueue}s.
            /// \return true == this pool has no outstanding \see{JobQueue}s.
            inline bool IsIdle () {
                LockGuard<Mutex> guard (mutex);
                return borrowedJobQueues.empty ();
            }

        private:
            /// \brief
            /// Used by GetJobQueue to acquire a \see{JobQueue} from the pool.
            /// \return \see{JobQueue} pointer.
            JobQueue *GetJobQueueHelper ();
            /// \brief
            /// Used by JobQueue to release itself to the pool.
            /// \param[in] jobQueue JobQueue to release.
            void ReleaseJobQueue (JobQueue *jobQueue);

            /// \brief
            /// Used by DeleteJobQueuePoolJobQueues.
            void DeleteJobQueues ();

            /// \brief
            /// DeleteJobQueuePoolJobQueues needs access to DeleteJobQueues.
            friend struct DeleteJobQueuePoolJobQueues;

            /// \brief
            /// JobQueuePool is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (JobQueuePool)
        };
    #if defined (_MSC_VER)
        #pragma warning (pop)
    #endif // defined (_MSC_VER)

        /// \struct GlobalJobQueuePoolCreateInstance JobQueuePool.h thekogans/util/JobQueuePool.h
        ///
        /// \brief
        /// Call GlobalJobQueuePoolCreateInstance::Parameterize before the first use of
        /// GlobalJobQueuePool::Instance to supply custom arguments to GlobalJobQueuePool ctor.

        struct _LIB_THEKOGANS_UTIL_DECL GlobalJobQueuePoolCreateInstance {
        private:
            /// \brief
            /// Minimum number of \see{JobQueue}s to keep in the pool.
            static ui32 minJobQueues;
            /// \brief
            /// Maximum number of \see{JobQueue}s allowed in the pool.
            static ui32 maxJobQueues;
            /// \brief
            /// \see{JobQueue} name.
            static std::string name;
            /// \brief
            /// \see{JobQueue} type.
            static RunLoop::Type type;
            /// \brief
            /// Max pending \see{JobQueue} jobs.
            static ui32 maxPendingJobs;
            /// \brief
            /// Number of worker threads service the \see{JobQueue}.
            static ui32 workerCount;
            /// \brief
            /// \see{JobQueue} worker thread priority.
            static i32 workerPriority;
            /// \brief
            /// \see{JobQueue} worker thread processor affinity.
            static ui32 workerAffinity;
            /// \brief
            /// Called to initialize/uninitialize the \see{JobQueue} worker thread.
            static RunLoop::WorkerCallback *workerCallback;

        public:
            /// \brief
            /// Call before the first use of GlobalJobQueuePool::Instance.
            /// \param[in] minJobQueues_ Minimum \see{JobQueue}s to keep in the pool.
            /// \param[in] maxJobQueues_ Maximum \see{JobQueue}s to allow the pool to grow to.
            /// \param[in] name_ \see{JobQueue} name.
            /// \param[in] type_ \see{JobQueue} type.
            /// \param[in] maxPendingJobs_ Max pending \see{JobQueue} jobs.
            /// \param[in] workerCount_ Number of worker threads service the \see{JobQueue}.
            /// \param[in] workerPriority_ \see{JobQueue} worker thread priority.
            /// \param[in] workerAffinity_ \see{JobQueue} worker thread processor affinity.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the \see{JobQueue}
            /// thread.
            static void Parameterize (
                ui32 minJobQueues_ = SystemInfo::Instance ().GetCPUCount (),
                ui32 maxJobQueues_ = SystemInfo::Instance ().GetCPUCount () * 2,
                const std::string &name_ = std::string (),
                RunLoop::Type type_ = RunLoop::TYPE_FIFO,
                ui32 maxPendingJobs_ = UI32_MAX,
                ui32 workerCount_ = 1,
                i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity_ = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                RunLoop::WorkerCallback *workerCallback_ = 0);

            /// \brief
            /// Create a global \see{JobQueuePool} with custom ctor arguments.
            /// \return A global \see{JobQueuePool} with custom ctor arguments.
            JobQueuePool *operator () ();
        };

        /// \struct GlobalJobQueuePool JobQueuePool.h thekogans/util/JobQueuePool.h
        ///
        /// \brief
        /// A global \see{JobQueuePool} instance. The \see{JobQueuePool} is
        /// designed to be as flexible as possible. To be useful in different
        /// situations the \see{JobQueuePool}'s min/max worker count needs to
        /// be parametrized as we might need different pools running different
        /// counts at different worker priorities. That said, the most basic
        /// (and the most useful) use case will have a single pool using the
        /// defaults. This struct exists to aid in that. If all you need is a
        /// global \see{JobQueuePool} then GlobalJobQueuePool::Instance () will
        /// do the trick.
        struct _LIB_THEKOGANS_UTIL_DECL GlobalJobQueuePool :
            public Singleton<JobQueuePool, SpinLock, GlobalJobQueuePoolCreateInstance> {};

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_JobQueuePool_h)
