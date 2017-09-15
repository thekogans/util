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
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/WorkerPool.h"
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
            enum {
                /// \brief
                /// JobQueueList ID.
                JOB_QUEUE_LIST_ID
            };
            /// \brief
            /// Convenient typedef for IntrusiveList<JobQueue, JOB_QUEUE_LIST_ID>.
            typedef IntrusiveList<JobQueue, JOB_QUEUE_LIST_ID> JobQueueList;
        #if defined (_MSC_VER)
            #pragma warning (push)
            #pragma warning (disable : 4275)
        #endif // defined (_MSC_VER)
            /// \struct Scheduler::JobQueue Scheduler.h thekogans/util/Scheduler.h
            ///
            /// \brief
            /// A very lightweight (~50 bytes on a 64 bit architecture, less on
            /// 32 bit ones) queue meant to be instantiated by all objects that
            /// need to schedule tasks and have them be executed sequentially in
            /// parallel. Once instantiated, put one or more jobs on the queue and
            /// they will be executed in prioritized, round-robin order. The scheduler
            /// runs in O(1). As there are no job states, if a job is in the queue,
            /// it will be scheduled to execute using one of the workers from a
            /// limited pool. Keep that in mind when designing your jobs as there
            /// is a real possibility of exhausting the worker pool and effectively
            /// killing the scheduler. Specifically, synchronous io is frowned upon.
            /// The motto is; keep em nimble, keep em moving!
            struct _LIB_THEKOGANS_UTIL_DECL JobQueue :
                    public ThreadSafeRefCounted,
                    public JobQueueList::Node {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<JobQueue>.
                typedef ThreadSafeRefCounted::Ptr<JobQueue> Ptr;

                /// \brief
                /// JobQueue has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (JobQueue, SpinLock)

                /// \brief
                /// Scheduler this JobQueue belongs to.
                Scheduler &scheduler;
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
                } priority;
                /// \brief
                /// Set true by a worker if it's processing a job from this queue.
                volatile bool inFlight;
                /// \brief
                /// Forward declaration of Job.
                struct Job;
                enum {
                    /// \brief
                    /// JobList ID.
                    JOB_LIST_ID
                };
                /// \brief
                /// Convenient typedef for IntrusiveList<Job, JOB_LIST_ID>.
                typedef IntrusiveList<Job, JOB_LIST_ID> JobList;
                /// \struct Scheduler::JobQueue::Job Scheduler.h thekogans/util/Scheduler.h
                ///
                /// \brief
                /// In keeping with the theme of keeping it lightweight, a
                /// job has but one responsibility, and that's to execute
                /// a task (as quickly as possible).
                struct _LIB_THEKOGANS_UTIL_DECL Job : public JobList::Node {
                    /// \brief
                    /// Convenient typedef for std::unique_ptr<Job>.
                    typedef std::unique_ptr<Job> UniquePtr;

                    /// \brief
                    /// dtor.
                    virtual ~Job () {}

                    /// \brief
                    /// Called by a worker thread to execute the job.
                    /// See notes on keeping it brief above.
                    /// \param[in] done A job should monitor this flag
                    /// with enough frequency to be responsive. Once
                    /// done == true, a job should stop what it's doing
                    /// as soon as possible and exit.
                    virtual void Execute (volatile const bool & /*done*/) throw () = 0;
                };
                /// \brief
                /// List of queue jobs.
                JobList jobs;
                /// \brief
                /// Synchronization lock.
                SpinLock spinLock;

                /// \brief
                /// ctor.
                /// \param[in] scheduler_ Scheduler this JobQueue belongs to.
                /// \param[in] priority_ JobQueue priority.
                JobQueue (
                    Scheduler &scheduler_,
                    Priority priority_ = PRIORITY_NORMAL) :
                    scheduler (scheduler_),
                    priority (priority_),
                    inFlight (false) {}
                /// \brief
                /// dtor.
                ~JobQueue () {
                    Flush ();
                }

                /// \brief
                /// En-queue a job. The next idle worker will pick it up
                /// and execute it on it's thread.
                /// \param[in] job Job to en-queue.
                void Enq (Job::UniquePtr job);
                /// \brief
                /// This is a very useful feature meant to aid in job
                /// design and chunking. The idea is to be able to have
                /// a currently executing job en-queue another job to
                /// follow it. In effect, creating a pipeline. The hope
                /// being that since the scheduler puts the queue back at
                /// the end of it's priority chain that all waiting queues
                /// will be given a chance to make progress.
                /// \param[in] job Job to enqueue.
                void EnqFront (Job::UniquePtr job);
                /// \brief
                /// Used internally by worker(s) to get the next job.
                /// \return The next job to execute.
                Job::UniquePtr Deq ();

                /// \brief
                /// Flush all pending jobs. If a job is in flight, it's not affected.
                void Flush ();
            };
        #if defined (_MSC_VER)
            #pragma warning (pop)
        #endif // defined (_MSC_VER)

            /// \brief
            /// ctor.
            /// Create as many active workers as there are cpu cores.
            /// Have as many in reserve for heavy loads.
            /// \param[in] workerName Worker thread name.
            /// \param[in] minWorkers Minimum worker count to keep in the pool.
            /// \param[in] maxWorkers Maximum worker count to allow the pool to grow to.
            /// \param[in] workerPriority Worker thread priority.
            /// \param[in] workerAffinity Worker thread processor affinity.
            Scheduler (
                const std::string workerName = std::string (),
                ui32 minWorkers = SystemInfo::Instance ().GetCPUCount (),
                ui32 maxWorkers = SystemInfo::Instance ().GetCPUCount () * 2,
                i32 workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity = UI32_MAX) :
                workerPool (workerName, minWorkers, maxWorkers, workerPriority, workerAffinity) {}

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
            /// WorkerPool executing the jobs.
            WorkerPool workerPool;

            /// \brief
            /// Add a JobQueue to the appropriate list (governed by it's priority)
            /// and spin up a Worker to process it's head job.
            /// \param[in] jobQueue JobQueue to add.
            /// \param[in] scheduleWorker true = Schedule a worker to process this queue.
            void AddJobQueue (
                JobQueue *jobQueue,
                bool scheduleWorker);
            /// \brief
            /// Remove the JobQueue from it's corresponding list.
            /// \param[in] jobQueue JobQueue to remove.
            void RemoveJobQueue (JobQueue *jobQueue);
            /// \brief
            /// Used by the worker to get the next appropriate
            /// JobQueue (based on priority).
            /// \return Highest priority JobQueue.
            JobQueue::Ptr GetNextJobQueue ();
        };

        /// \struct GlobalSchedulerCreateInstance Scheduler.h thekogans/util/Scheduler.h
        ///
        /// \brief
        /// Call GlobalSchedulerCreateInstance::Parameterize before the first use of
        /// GlobalScheduler::Instance to supply custom arguments to GlobalScheduler ctor.

        struct _LIB_THEKOGANS_UTIL_DECL GlobalSchedulerCreateInstance {
        private:
            /// \brief
            /// Worker thread name.
            static std::string workerName;
            /// \brief
            /// Minimum worker count to keep in the pool.
            static ui32 minWorkers;
            /// \brief
            /// Maximum worker count to allow the pool to grow to.
            static ui32 maxWorkers;
            /// \brief
            /// Worker thread priority.
            static i32 workerPriority;
            /// \brief
            /// Worker thread processor affinity.
            static ui32 workerAffinity;

        public:
            /// \brief
            /// Call before the first use of GlobalScheduler::Instance.
            /// \param[in] workerName_ Worker thread name.
            /// \param[in] minWorkers_ Minimum worker count to keep in the pool.
            /// \param[in] maxWorkers_ Maximum worker count to allow the pool to grow to.
            /// \param[in] workerPriority_ Worker thread priority.
            /// \param[in] workerAffinity_ Worker thread processor affinity.
            static void Parameterize (
                const std::string &workerName_ = std::string (),
                ui32 minWorkers_ = SystemInfo::Instance ().GetCPUCount (),
                ui32 maxWorkers_ = SystemInfo::Instance ().GetCPUCount () * 2,
                i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity_ = UI32_MAX);

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
        /// the scheduler's worker pool needs to be parametrized as we
        /// might need to have different schedulers running workers at
        /// different thread priorities. That said, the most basic (and
        /// the most useful) use case will have a single scheduler using
        /// the defaults. This typedef exists to aid in that. If all you
        /// need is a global scheduler then GlobalScheduler::Instance ()
        /// will do the trick.
        struct _LIB_THEKOGANS_UTIL_DECL GlobalScheduler :
            public Singleton<Scheduler, SpinLock, GlobalSchedulerCreateInstance> {};

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Scheduler_h)
