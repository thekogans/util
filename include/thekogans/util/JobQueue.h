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

#if !defined (__thekogans_util_JobQueue_h)
#define __thekogans_util_JobQueue_h

#include <memory>
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/RunLoop.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/Thread.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Condition.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

        /// \struct JobQueue JobQueue.h thekogans/util/JobQueue.h
        ///
        /// \brief
        /// A JobQueue is a queue of jobs, and one or more workers (threads) servicing it.
        ///
        /// As you add jobs to the queue, the next idle worker removes and executes them.
        /// The queue can be either FIFO or LIFO. While very usefull on it's own, JobQueue
        /// also forms the basis for \see{Pipeline} and \see{JobQueuePool}.

        struct _LIB_THEKOGANS_UTIL_DECL JobQueue : public RunLoop {
            /// \brief
            /// Convenient typedef for ThreadSafeRefCounted::Ptr<JobQueue>.
            typedef ThreadSafeRefCounted::Ptr<JobQueue> Ptr;

        protected:
            /// \brief
            /// Number of workers servicing the queue.
            const std::size_t workerCount;
            /// \brief
            /// \Worker thread priority.
            const i32 workerPriority;
            /// \brief
            /// \Worker thread processor affinity.
            const ui32 workerAffinity;
            /// \brief
            /// Called to initialize/uninitialize the worker thread.
            WorkerCallback *workerCallback;
            /// \brief
            /// Forward declaration of Worker.
            struct Worker;
            enum {
                /// \brief
                /// WorkerList ID.
                WORKER_LIST_ID
            };
            /// \brief
            /// Convenient typedef for IntrusiveList<Worker, WORKER_LIST_ID>.
            typedef IntrusiveList<Worker, WORKER_LIST_ID> WorkerList;
            /// \struct JobQueue::Worker JobQueue.h thekogans/util/JobQueue.h
            ///
            /// \brief
            /// Worker takes pending jobs off the queue and
            /// executes them. It then reports back to the
            /// queue so that it can collect statistics.
            struct Worker :
                    public Thread,
                    public WorkerList::Node {
            private:
                /// \brief
                /// JobQueue to which this worker belongs.
                JobQueue &queue;

            public:
                /// \brief
                /// ctor.
                /// \param[in] queue_ JobQueue to which this worker belongs.
                /// \param[in] name Worker thread name.
                /// \param[in] priority Worker thread priority.
                /// \param[in] affinity Worker thread processor affinity.
                Worker (JobQueue &queue_,
                        const std::string &name = std::string (),
                        i32 priority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                        ui32 affinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY) :
                        Thread (name),
                        queue (queue_) {
                    Create (priority, affinity);
                }

            private:
                // Thread
                /// \brief
                /// Worker thread.
                virtual void Run () throw ();
            };
            /// \brief
            /// List of workers.
            WorkerList workers;
            /// \brief
            /// Synchronization mutex.
            Mutex workersMutex;

        public:
            /// \brief
            /// ctor.
            /// \param[in] name JobQueue name. If set, \see{Worker}
            /// threads will be named name-%d.
            /// \param[in] jobExecutionPolicy JobQueue \see{RunLoop::JobExecutionPolicy}.
            /// \param[in] maxPendingJobs Max pending queue jobs.
            /// \param[in] workerCount_ Max workers to service the queue.
            /// \param[in] workerPriority_ Worker thread priority.
            /// \param[in] workerAffinity_ Worker thread processor affinity.
            /// \param[in] workerCallback_ Called to initialize/uninitialize
            /// the worker thread.
            JobQueue (
                const std::string &name = std::string (),
                JobExecutionPolicy::Ptr jobExecutionPolicy =
                    JobExecutionPolicy::Ptr (new FIFOJobExecutionPolicy),
                std::size_t workerCount_ = 1,
                i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity_ = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                WorkerCallback *workerCallback_ = 0);
            /// \brief
            /// dtor. Stop the queue.
            virtual ~JobQueue () {
                Stop ();
            }

            // RunLoop
            /// \brief
            /// Create the worker(s), and start waiting for jobs. The
            /// ctor calls this member, but if you ever need to stop
            /// the queue, you need to call Start manually to restart it.
            virtual void Start ();
            /// \brief
            /// Stops all running, and cancels all pending jobs. The
            /// queue, and the worker pool are flushed. After calling
            /// this method, the queue is dead, and consumes very little
            /// resources. You need to call Start to get it going again.
            ///
            /// VERY IMPORTANT: In order to stop the workers, the queue
            /// sets done = true. This is the same done as is passed as
            /// const std::atomic<bool> & to Job::Prologue/Execute/Epilog.
            /// Therefore, if you want your code to be responsive, and the
            /// queues to stop quickly, your jobs should pay close attention
            /// to the state of done.
            /// \param[in] cancelRunningJobs true = Cancel all running jobs.
            /// \param[in] cancelPendingJobs true = Cancel all pending jobs.
            /// \param[in] timeSpec How long to wait for the job queue to stop.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == Job queue stopped. false == timed out waiting for worker to stop.
            virtual bool Stop (
                bool cancelRunningJobs = true,
                bool cancelPendingJobs = true,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Return true is the run loop is running (Start was called).
            /// \return true is the run loop is running (Start was called).
            virtual bool IsRunning ();

            /// \brief
            /// JobQueue is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (JobQueue)
        };

        /// \struct GlobalJobQueueCreateInstance JobQueue.h thekogans/util/JobQueue.h
        ///
        /// \brief
        /// Call GlobalJobQueue::CreateInstance before the first use of
        /// GlobalJobQueue::Instance to supply custom arguments to GlobalJobQueue ctor.

        struct _LIB_THEKOGANS_UTIL_DECL GlobalJobQueueCreateInstance {
            /// \brief
            /// Create a global job queue with custom ctor arguments.
            /// \param[in] name JobQueue name. If set, \see{JobQueue::Worker}
            /// threads will be named name-%d.
            /// \param[in] jobExecutionPolicy JobQueue \see{RunLoop::JobExecutionPolicy}.
            /// \param[in] workerCount Max workers to service the queue.
            /// \param[in] workerPriority Worker thread priority.
            /// \param[in] workerAffinity Worker thread processor affinity.
            /// \param[in] workerCallback Called to initialize/uninitialize the worker thread.
            /// \return A global job queue with custom ctor arguments.
            JobQueue *operator () (
                    const std::string &name = "GlobalJobQueue",
                    RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy =
                        RunLoop::JobExecutionPolicy::Ptr (new RunLoop::FIFOJobExecutionPolicy),
                    std::size_t workerCount = 1,
                    i32 workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                    ui32 workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                    RunLoop::WorkerCallback *workerCallback = 0) {
                return new JobQueue (
                    name,
                    jobExecutionPolicy,
                    workerCount,
                    workerPriority,
                    workerAffinity,
                    workerCallback);
            }
        };

        /// \struct GlobalJobQueue JobQueue.h thekogans/util/JobQueue.h
        ///
        /// \brief
        /// A global job queue instance. The JobQueue is designed to be
        /// as flexible as possible. To be useful in different contexts
        /// the job queue's worker count needs to be parametrized as we
        /// might need different queues running different worker counts
        /// at different thread priorities. That said, the most basic
        /// (and the most useful) use case will have a single job queue
        /// using the defaults. This struct exists to aid in that. If all
        /// you need is a background thread where you can schedule jobs,
        /// then GlobalJobQueue::Instance () will do the trick.
        struct _LIB_THEKOGANS_UTIL_DECL GlobalJobQueue :
            public Singleton<JobQueue, SpinLock, GlobalJobQueueCreateInstance> {};

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_JobQueue_h)
