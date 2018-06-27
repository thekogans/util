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

#if !defined (__thekogans_util_Scheduler_h)
#define __thekogans_util_Scheduler_h

#include <memory>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/RunLoop.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/JobQueuePool.h"
#include "thekogans/util/SystemInfo.h"

namespace thekogans {
    namespace util {

        /// \struct Scheduler Scheduler.h thekogans/util/Scheduler.h
        ///
        /// \brief
        /// Scheduler models multiple independent priority job queues.
        /// The queues are independent in that they can be scheduled
        /// in parallel (prioritized round-robin), but need to make
        /// sequential progress. Scheduler is designed to execute in
        /// O(1) time no mater the number of active queues.

        struct _LIB_THEKOGANS_UTIL_DECL Scheduler {
            /// \brief
            /// Forward declaration of JobQueue.
            struct JobQueue;

        private:
            enum {
                /// \brief
                /// JobQueueList ID.
                JOB_QUEUE_LIST_ID
            };
            /// \brief
            /// Convenient typedef for IntrusiveList<JobQueue, JOB_QUEUE_LIST_ID>.
            typedef IntrusiveList<JobQueue, JOB_QUEUE_LIST_ID> JobQueueList;

        public:
        #if defined (_MSC_VER)
            #pragma warning (push)
            #pragma warning (disable : 4275)
        #endif // defined (_MSC_VER)
            /// \struct Scheduler::JobQueue Scheduler.h thekogans/util/Scheduler.h
            ///
            /// \brief
            /// JobQueue is meant to be instantiated by all objects that need to
            /// schedule tasks and have them be executed sequentially in parallel.
            /// Once instantiated, put one or more jobs on the queue and they will
            /// be executed in prioritized, round-robin order. The scheduler runs
            /// in O(1). As there are no job states, if a job is in the queue, it
            /// will be scheduled to execute using one of the workers from a limited
            /// pool. Keep that in mind when designing your jobs as there is a real
            /// possibility of exhausting the job queue pool and effectively killing
            /// the scheduler. Specifically, synchronous io is frowned upon. The
            /// motto is; keep em nimble, keep em moving!
            struct _LIB_THEKOGANS_UTIL_DECL JobQueue :
                    public RunLoop,
                    public JobQueueList::Node {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<JobQueue>.
                typedef ThreadSafeRefCounted::Ptr<JobQueue> Ptr;

                /// \brief
                /// JobQueue has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (JobQueue, SpinLock)

                /// \brief
                /// Queue priority.
                enum Priority {
                    /// \brief
                    /// Lowest priority queue. Will be starved by PRIORITY_NORMAL
                    /// and PRIORITY_HIGH queues.
                    PRIORITY_LOW,
                    /// \brief
                    /// Normal priority queue. Will be starved by PRIORITY_HIGH queues.
                    PRIORITY_NORMAL,
                    /// \brief
                    /// Highest priority queue.
                    PRIORITY_HIGH
                };

            private:
                /// \brief
                /// Scheduler this JobQueue belongs to.
                Scheduler &scheduler;
                /// \brief
                /// Queue priority.
                Priority priority;
                /// \brief
                /// Set true by a worker if it's processing a job from this queue.
                THEKOGANS_UTIL_ATOMIC<bool> inFlight;

            public:
                /// \brief
                /// ctor.
                /// \param[in] scheduler_ Scheduler this JobQueue belongs to.
                /// \param[in] priority_ JobQueue priority.
                JobQueue (
                    Scheduler &scheduler_,
                    Priority priority_ = PRIORITY_NORMAL,
                    const std::string &name = std::string (),
                    Type type = TYPE_FIFO,
                    ui32 maxPendingJobs = UI32_MAX) :
                    RunLoop (name, type, maxPendingJobs),
                    scheduler (scheduler_),
                    priority (priority_),
                    inFlight (false) {}

                /// \brief
                /// Scheduler job queue starts when jobs are enqueued.
                virtual void Start () {}
                /// \brief
                /// Scheduler job queue stops when there are no more jobs to execute.
                /// \param[in] cancelPendingJobs true = Cancel all pending jobs.
                virtual void Stop (bool cancelPendingJobs = true);

                /// \brief
                /// Enqueue a job to be executed by the job queue.
                /// \param[in] job Job to enqueue.
                /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
                /// \param[in] timeSpec How long to wait for the job to complete.
                /// IMPORTANT: timeSpec is a relative value.
                /// \return true == !wait || WaitForJob (...)
                virtual bool EnqJob (
                    Job::Ptr job,
                    bool wait = false,
                    const TimeSpec &timeSpec = TimeSpec::Infinite);
                /// \brief
                /// This is a very useful feature meant to aid in job
                /// design and chunking. The idea is to be able to have
                /// a currently executing job en-queue another job to
                /// follow it. In effect, creating a pipeline. The hope
                /// being that since the scheduler puts the queue back at
                /// the end of it's priority chain that all waiting queues
                /// will be given a chance to make progress.
                /// Enqueue a job to be executed by thejob queue.
                /// \param[in] job Job to enqueue.
                /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
                /// \param[in] timeSpec How long to wait for the job to complete.
                /// IMPORTANT: timeSpec is a relative value.
                /// \return true == !wait || WaitForJob (...)
                virtual bool EnqJobFront (
                    Job::Ptr job,
                    bool wait = false,
                    const TimeSpec &timeSpec = TimeSpec::Infinite);

                /// \brief
                /// Scheduler needs access to protected members.
                friend struct Scheduler;

                /// \brief
                /// JobQueue is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (JobQueue)
            };
        #if defined (_MSC_VER)
            #pragma warning (pop)
        #endif // defined (_MSC_VER)

            /// \brief
            /// ctor.
            /// Create as many active workers as there are cpu cores.
            /// Have as many in reserve for heavy loads.
            /// \param[in] minWorkers Minimum worker count to keep in the pool.
            /// \param[in] maxWorkers Maximum worker count to allow the pool to grow to.
            /// \param[in] name Worker thread name.
            /// \param[in] type Worker queue type.
            /// \param[in] maxPendingJobs Max pending queue jobs.
            /// \param[in] workerCount Number of worker threads service the queue.
            /// \param[in] workerPriority Worker thread priority.
            /// \param[in] workerAffinity Worker thread processor affinity.
            /// \param[in] workerCallback Called to initialize/uninitialize the worker thread.
            Scheduler (
                ui32 minWorkers = SystemInfo::Instance ().GetCPUCount (),
                ui32 maxWorkers = SystemInfo::Instance ().GetCPUCount () * 2,
                const std::string name = std::string (),
                RunLoop::Type type = RunLoop::TYPE_FIFO,
                ui32 maxPendingJobs = UI32_MAX,
                ui32 workerCount = 1,
                i32 workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                RunLoop::WorkerCallback *workerCallback = 0) :
                jobQueuePool (
                    minWorkers,
                    maxWorkers,
                    name,
                    type,
                    maxPendingJobs,
                    workerCount,
                    workerPriority,
                    workerAffinity,
                    workerCallback) {}

        private:
            /// \brief
            /// Low priority JobQueue list.
            JobQueueList low;
            /// \brief
            /// Normal priority JobQueue list.
            JobQueueList normal;
            /// \brief
            /// High priority JobQueue list.
            JobQueueList high;
            /// \brief
            /// Synchronization SpinLock for above lists.
            SpinLock spinLock;
            /// \brief
            /// JobQueuePool executing the jobs.
            JobQueuePool jobQueuePool;

            /// \brief
            /// Add a JobQueue to the appropriate list (governed by it's priority)
            /// and spin up a Worker to process it's head job.
            /// \param[in] jobQueue JobQueue to add.
            /// \param[in] scheduleWorker true = Schedule a worker to process this queue.
            void AddJobQueue (
                JobQueue *jobQueue,
                bool scheduleWorker);
            /// \brief
            /// Used by the worker to get the next appropriate
            /// JobQueue (based on priority).
            /// \return Highest priority JobQueue.
            JobQueue *GetNextJobQueue ();
        };

        /// \struct GlobalSchedulerCreateInstance Scheduler.h thekogans/util/Scheduler.h
        ///
        /// \brief
        /// Call GlobalSchedulerCreateInstance::Parameterize before the first use of
        /// GlobalScheduler::Instance to supply custom arguments to GlobalScheduler ctor.

        struct _LIB_THEKOGANS_UTIL_DECL GlobalSchedulerCreateInstance {
        private:
            /// \brief
            /// Minimum worker count to keep in the pool.
            static ui32 minWorkers;
            /// \brief
            /// Maximum worker count to allow the pool to grow to.
            static ui32 maxWorkers;
            /// \brief
            /// Worker thread name.
            static std::string name;
            /// \brief
            /// Worker queue type.
            static RunLoop::Type type;
            /// \brief
            /// Worker queue max pending jobs.
            static ui32 maxPendingJobs;
            /// \brief
            /// Number of worker threads service the queue.
            static ui32 workerCount;
            /// \brief
            /// Worker thread priority.
            static i32 workerPriority;
            /// \brief
            /// Worker thread processor affinity.
            static ui32 workerAffinity;
            /// \brief
            /// Called to initialize/uninitialize the worker thread.
            static RunLoop::WorkerCallback *workerCallback;

        public:
            /// \brief
            /// Call before the first use of GlobalScheduler::Instance.
            /// \param[in] minWorkers_ Minimum worker count to keep in the pool.
            /// \param[in] maxWorkers_ Maximum worker count to allow the pool to grow to.
            /// \param[in] name_ Worker thread name.
            /// \param[in] type_ Worker queue type.
            /// \param[in] maxPendingJobs_ Max pending queue jobs.
            /// \param[in] workerCount_ Number of worker threads service the queue.
            /// \param[in] workerPriority_ Worker thread priority.
            /// \param[in] workerAffinity_ Worker thread processor affinity.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the worker thread.
            static void Parameterize (
                ui32 minWorkers_ = SystemInfo::Instance ().GetCPUCount (),
                ui32 maxWorkers_ = SystemInfo::Instance ().GetCPUCount () * 2,
                const std::string &name_ = std::string (),
                RunLoop::Type type_ = RunLoop::TYPE_FIFO,
                ui32 maxPendingJobs_ = UI32_MAX,
                ui32 workerCount_ = 1,
                i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity_ = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                RunLoop::WorkerCallback *workerCallback_ = 0);

            /// \brief
            /// Create a global scheduler with custom ctor arguments.
            /// \return A global scheduler with custom ctor arguments.
            Scheduler *operator () ();
        };

        /// \struct GlobalScheduler Scheduler.h thekogans/util/Scheduler.h
        ///
        /// \brief
        /// A global scheduler instance. The Scheduler is designed to be
        /// as flexible as possible. To be useful in different situations
        /// the scheduler's job queue pool needs to be parametrized as we
        /// might need to have different schedulers running workers at
        /// different thread priorities. That said, the most basic (and
        /// the most useful) use case will have a single scheduler using
        /// the defaults. This struct exists to aid in that. If all you
        /// need is a global scheduler then GlobalScheduler::Instance ()
        /// will do the trick.
        struct _LIB_THEKOGANS_UTIL_DECL GlobalScheduler :
            public Singleton<Scheduler, SpinLock, GlobalSchedulerCreateInstance> {};

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Scheduler_h)
