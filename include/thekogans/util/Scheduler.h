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
#include <atomic>
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
            /// will be scheduled to execute using one of the job queues from a limited
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
                /// JobQueue priority.
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
                /// JobQueue priority.
                const Priority priority;
                /// \brief
                /// true == a job from this JobQueue is being executed.
                std::atomic<bool> inFlight;

            public:
                /// \brief
                /// ctor.
                /// \param[in] scheduler_ Scheduler this JobQueue belongs to.
                /// \param[in] priority_ JobQueue priority.
                /// \param[in] name JobQueue name.
                /// \param[in] jobExecutionPolicy JobQueue \see{JobExecutionPolicy}.
                JobQueue (
                        Scheduler &scheduler_,
                        Priority priority_ = PRIORITY_NORMAL,
                        const std::string &name = std::string (),
                        JobExecutionPolicy::Ptr jobExecutionPolicy =
                            JobExecutionPolicy::Ptr (new FIFOJobExecutionPolicy)) :
                        RunLoop (name, jobExecutionPolicy),
                        scheduler (scheduler_),
                        priority (priority_),
                        inFlight (false) {
                    Start ();
                }
                /// \brief
                /// dtor. Stop the queue.
                virtual ~JobQueue () {
                    Stop ();
                }

                /// \brief
                /// Scheduler job queue starts when jobs are enqueued.
                virtual void Start ();
                /// \brief
                /// Scheduler job queue stops when there are no more jobs to execute.
                /// \param[in] cancelRunningJobs true = Cancel all running jobs.
                /// \param[in] cancelPendingJobs true = Cancel all pending jobs.
                /// \param[in] timeSpec How long to wait for the job queue to stop.
                /// IMPORTANT: timeSpec is a relative value.
                /// \return true == Job queue stopped. false == timed out.
                virtual bool Stop (
                    bool cancelRunningJobs = true,
                    bool cancelPendingJobs = true,
                    const TimeSpec &timeSpec = TimeSpec::Infinite);

                /// \brief
                /// Continue the job queue execution. If the job queue is not paused, noop.
                virtual void Continue ();

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
            /// Create as many active job queues as there are cpu cores.
            /// Have as many in reserve for heavy loads.
            /// \param[in] minJobQueues Minimum worker count to keep in the pool.
            /// \param[in] maxJobQueues Maximum worker count to allow the pool to grow to.
            /// \param[in] name JobQueue thread name.
            /// \param[in] jobExecutionPolicy JobQueue \see{JobExecutionPolicy}.
            /// \param[in] workerCount Number of worker threads servicing the queue.
            /// \param[in] workerPriority JobQueue thread priority.
            /// \param[in] workerAffinity JobQueue thread processor affinity.
            /// \param[in] workerCallback Called to initialize/uninitialize the worker thread.
            Scheduler (
                std::size_t minJobQueues = SystemInfo::Instance ().GetCPUCount (),
                std::size_t maxJobQueues = SystemInfo::Instance ().GetCPUCount () * 2,
                const std::string name = std::string (),
                RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy =
                    RunLoop::JobExecutionPolicy::Ptr (new RunLoop::FIFOJobExecutionPolicy),
                std::size_t workerCount = 1,
                i32 workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                RunLoop::WorkerCallback *workerCallback = 0) :
                jobQueuePool (
                    minJobQueues,
                    maxJobQueues,
                    name,
                    jobExecutionPolicy,
                    workerCount,
                    workerPriority,
                    workerAffinity,
                    workerCallback) {}
            /// \brief
            /// dtor.
            ~Scheduler ();

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
            /// Synchronization \see{SpinLock} for above lists.
            SpinLock spinLock;
            /// \brief
            /// \see{JobQueuePool} executing the jobs.
            JobQueuePool jobQueuePool;

            /// \brief
            /// Add a JobQueue to the appropriate list (governed by it's priority)
            /// and spin up a JobQueue to process it's head job.
            /// \param[in] jobQueue JobQueue to add.
            /// \param[in] scheduleJobQueue true = Schedule a \see{JobQueuePool} \see{JobQueue}
            /// to process this job queue.
            void AddJobQueue (
                JobQueue *jobQueue,
                bool scheduleJobQueue = true);
            /// \brief
            /// Remove the given JobQueue from its priority list.
            /// \param[in] jobQueue JobQueue to remove.
            void DeleteJobQueue (JobQueue *jobQueue);
            /// \brief
            /// Used by the worker to get the next appropriate
            /// JobQueue (based on priority).
            /// \return Highest priority JobQueue with a job ready to execute.
            JobQueue *GetNextJobQueue ();
        };

        /// \struct GlobalSchedulerCreateInstance Scheduler.h thekogans/util/Scheduler.h
        ///
        /// \brief
        /// Call GlobalScheduler::CreateSingleton before the first use of
        /// GlobalScheduler::Instance to supply custom arguments to GlobalScheduler ctor.

        struct _LIB_THEKOGANS_UTIL_DECL GlobalSchedulerCreateInstance {
            /// \brief
            /// Create a global scheduler with custom ctor arguments.
            /// \param[in] minJobQueues Minimum worker count to keep in the pool.
            /// \param[in] maxJobQueues Maximum worker count to allow the pool to grow to.
            /// \param[in] name JobQueue thread name.
            /// \param[in] jobExecutionPolicy JobQueue \see{RunLoop::JobExecutionPolicy}.
            /// \param[in] workerCount Number of worker threads servicing the queue.
            /// \param[in] workerPriority JobQueue thread priority.
            /// \param[in] workerAffinity JobQueue thread processor affinity.
            /// \param[in] workerCallback Called to initialize/uninitialize the worker thread.
            /// \return A global scheduler with custom ctor arguments.
            Scheduler *operator () (
                    std::size_t minJobQueues = SystemInfo::Instance ().GetCPUCount (),
                    std::size_t maxJobQueues = SystemInfo::Instance ().GetCPUCount () * 2,
                    const std::string &name = "GlobalScheduler",
                    RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy =
                        RunLoop::JobExecutionPolicy::Ptr (new RunLoop::FIFOJobExecutionPolicy),
                    std::size_t workerCount = 1,
                    i32 workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                    ui32 workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                    RunLoop::WorkerCallback *workerCallback = 0) {
                return new Scheduler (
                    minJobQueues,
                    maxJobQueues,
                    name,
                    jobExecutionPolicy,
                    workerCount,
                    workerPriority,
                    workerAffinity,
                    workerCallback);
            }
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
