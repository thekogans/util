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

#if !defined (__thekogans_util_WorkerPool_h)
#define __thekogans_util_WorkerPool_h

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
#include "thekogans/util/SystemInfo.h"
#include "thekogans/util/Singleton.h"

namespace thekogans {
    namespace util {

        /// \struct WorkerPool WorkerPool.h thekogans/util/WorkerPool.h
        ///
        /// \brief
        /// WorkerPool implements a very convenient pool of
        /// JobQueues. Here is a canonical use case:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// util::WorkerPool workerPool;
        ///
        /// void foo (...) {
        ///     struct Job : public util::RunLoop::Job {
        ///         util::WorkerPool::WorkerPtr::Ptr workerPtr;
        ///         ...
        ///         Job (
        ///             util::WorkerPool::WorkerPtr::Ptr workerPtr_,
        ///             ...) :
        ///             workerPtr (workerPtr_),
        ///             ... {}
        ///
        ///         // util::RunLoop::Job
        ///         virtual void Execute (const THEKOGANS_UTIL_ATOMIC<bool> &) throw () {
        ///             ...
        ///         }
        ///     };
        ///     util::WorkerPool::WorkerPtr::Ptr workerPtr = workerPool.GetWorker ();
        ///     if (workerPtr.Get () != 0) {
        ///         (*workerPtr)->EnqJob (RunLoop::Job::Ptr (new Job (workerPtr, ...)));
        ///     }
        /// }
        /// \endcode
        ///
        /// Note how the Job controls the lifetime of the WorkerPtr.
        /// By passing util::WorkerPool::WorkerPtr::Ptr in to the
        /// Job's ctor we guarantee that the worker will be released as
        /// soon as the Job goes out of scope (as it will be the last
        /// reference on the ThreadSafeRefCounted).

        struct _LIB_THEKOGANS_UTIL_DECL WorkerPool {
        private:
            /// \brief
            /// Minimum number of workers to keep in the pool.
            const ui32 minWorkers;
            /// \brief
            /// Maximum number of workers allowed in the pool.
            const ui32 maxWorkers;
            /// \brief
            /// Pool name.
            const std::string name;
            /// \brief
            /// Worker queue type.
            const RunLoop::Type type;
            /// \brief
            /// Worker \see{JobQueue} max pending jobs.
            const ui32 maxPendingJobs;
            /// \brief
            /// Number of worker threads service the queue.
            const ui32 workerCount;
            /// \brief
            /// Worker \see{JobQueue} priority.
            const i32 workerPriority;
            /// \brief
            /// Worker \see{JobQueue} processor affinity.
            const ui32 workerAffinity;
            /// \brief
            /// Called to initialize/uninitialize the worker thread.
            RunLoop::WorkerCallback *workerCallback;
            /// \brief
            /// Current number of workers in the pool.
            ui32 activeWorkerCount;
            /// \brief
            /// Forward declaration of Worker.
            struct Worker;
            enum {
                /// \brief
                /// WorkerList list id.
                WORKER_LIST_ID
            };
            /// \brief
            /// Convenient typedef for IntrusiveList<Worker, WORKER_LIST_ID>.
            typedef IntrusiveList<Worker, WORKER_LIST_ID> WorkerList;
            /// \struct WorkerPool::Worker WorkerPool.h thekogans/util/WorkerPool.h
            ///
            /// \brief
            /// Each worker is a \see{JobQueue}. This allows you
            /// to acquire a worker, and feed it jobs to process.
            struct Worker :
                    public WorkerList::Node,
                    public JobQueue {
                /// \brief
                /// Worker has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Worker, SpinLock)

                /// \brief
                /// WorkerPool from which the worker came.
                WorkerPool &workerPool;

                /// \brief
                /// ctor.
                /// \param[in] workerPool_ WorkerPool to which this worker belongs.
                /// \param[in] name Worker thread name.
                /// \param[in] type Worker queue type.
                /// \param[in] maxPendingJobs Worker \see{JobQueue} max pending jobs.
                /// \param[in] workerCount Number of worker threads service the queue.
                /// \param[in] workerPriority Worker thread priority.
                /// \param[in] workerAffinity Worker thread processor affinity.
                /// \param[in] workerCallback Called to initialize/uninitialize the
                /// queue worker thread(s).
                Worker (
                    WorkerPool &workerPool_,
                    const std::string &name = std::string (),
                    Type type = TYPE_FIFO,
                    ui32 maxPendingJobs = UI32_MAX,
                    ui32 workerCount = 1,
                    i32 workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                    ui32 workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                    WorkerCallback *workerCallback = 0) :
                    JobQueue (
                        name,
                        type,
                        maxPendingJobs,
                        workerCount,
                        workerPriority,
                        workerAffinity,
                        workerCallback),
                    workerPool (workerPool_) {}

            protected:
                /// \brief
                /// If there are no more references to this worker,
                /// release it back to the worker pool.
                virtual void Harakiri () {
                    workerPool.ReleaseWorker (this);
                }

                /// \brief
                /// Worker is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Worker)
            };
            /// \brief
            /// List of workers.
            WorkerList workers;
            /// \brief
            /// Synchronization spin lock.
            SpinLock spinLock;

        public:
            /// \brief
            /// ctor.
            /// \param[in] minWorkers_ Minimum worker to keep in the pool.
            /// \param[in] maxWorkers_ Maximum worker to allow the pool to grow to.
            /// \param[in] name_ Pool name.
            /// \param[in] type_ Worker queue type.
            /// \param[in] maxPendingJobs_ Max pending queue jobs.
            /// \param[in] workerCount_ Number of worker threads service the queue.
            /// \param[in] workerPriority_ Worker thread priority.
            /// \param[in] workerAffinity_ Worker thread processor affinity.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the worker thread.
            WorkerPool (
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
            /// dtor.
            ~WorkerPool ();

            /// \brief
            /// Convenient typedef for ThreadSafeRefCounted::Ptr<Worker>.
            typedef ThreadSafeRefCounted::Ptr<Worker> WorkerPtr;

            /// \brief
            /// Acquire a worker from the pool.
            /// \param[in] retries Number of times to retry if a worker is not immediately available.
            /// \param[in] timeSpec How long to wait between retries.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return A worker from the pool.
            WorkerPtr GetWorker (
                ui32 retries = 1,
                const TimeSpec &timeSpec = TimeSpec::FromMilliseconds (100));

        private:
            /// \brief
            /// Used by GetWorker to acquire a worker from the pool.
            /// \return Worker pointer.
            Worker *GetWorkerHelper ();
            /// \brief
            /// Used by Worker to release the worker to the pool.
            /// \param[in] worker Worker to release.
            void ReleaseWorker (Worker *worker);

            /// \brief
            /// DeleteWorker needs access to WorkerList.
            friend struct DeleteWorker;

            /// \brief
            /// WorkerPool is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (WorkerPool)
        };

        /// \struct GlobalWorkerPoolCreateInstance WorkerPool.h thekogans/util/WorkerPool.h
        ///
        /// \brief
        /// Call GlobalWorkerPoolCreateInstance::Parameterize before the first use of
        /// GlobalWorkerPool::Instance to supply custom arguments to GlobalWorkerPool ctor.

        struct _LIB_THEKOGANS_UTIL_DECL GlobalWorkerPoolCreateInstance {
        private:
            /// \brief
            /// Minimum number of workers to keep in the pool.
            static ui32 minWorkers;
            /// \brief
            /// Maximum number of workers allowed in the pool.
            static ui32 maxWorkers;
            /// \brief
            /// Pool name.
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
            /// Worker queue priority.
            static i32 workerPriority;
            /// \brief
            /// Worker queue processor affinity.
            static ui32 workerAffinity;
            /// \brief
            /// Called to initialize/uninitialize the worker thread.
            static RunLoop::WorkerCallback *workerCallback;

        public:
            /// \brief
            /// Call before the first use of GlobalWorkerPool::Instance.
            /// \param[in] minWorkers_ Minimum worker to keep in the pool.
            /// \param[in] maxWorkers_ Maximum worker to allow the pool to grow to.
            /// \param[in] name_ Pool name.
            /// \param[in] type_ Worker queue type.
            /// \param[in] maxPendingJobs_ Max pending queue jobs.
            /// \param[in] workerCount_ Number of worker threads service the queue.
            /// \param[in] workerPriority_ Worker queue priority.
            /// \param[in] workerAffinity_ Worker queue processor affinity.
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
            /// Create a global worker pool with custom ctor arguments.
            /// \return A global worker pool with custom ctor arguments.
            WorkerPool *operator () ();
        };

        /// \struct GlobalWorkerPool WorkerPool.h thekogans/util/WorkerPool.h
        ///
        /// \brief
        /// A global worker pool instance. The WorkerPool is designed to be
        /// as flexible as possible. To be useful in different situations
        /// the worker pool's min/max worker count needs to be parametrized
        /// as we might need different pools running different counts at
        /// different queue priorities. That said, the most basic (and
        /// the most useful) use case will have a single worker pool using
        /// the defaults. This struct exists to aid in that. If all you
        /// need is a global worker pool then GlobalWorkerPool::Instance ()
        /// will do the trick.
        struct _LIB_THEKOGANS_UTIL_DECL GlobalWorkerPool :
            public Singleton<WorkerPool, SpinLock, GlobalWorkerPoolCreateInstance> {};

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_WorkerPool_h)
