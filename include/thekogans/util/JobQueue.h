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
#include <map>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/RunLoop.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/Thread.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Condition.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/StringUtils.h"

namespace thekogans {
    namespace util {

        /// \struct JobQueue JobQueue.h thekogans/util/JobQueue.h
        ///
        /// \brief
        /// A JobQueue is a queue of jobs, and one or more workers (threads) servicing it.
        ///
        /// As you add jobs to the queue, the next idle worker removes and executes them.
        /// The queue can be either FIFO or LIFO. While very usefull on it's own, JobQueue
        /// also forms the basis for \see{Pipeline}, \see{Vectorizer}, \see{WorkerPool} and
        /// \see{Scheduler}.

        struct _LIB_THEKOGANS_UTIL_DECL JobQueue : public RunLoop {
            /// \brief
            /// Convenient typedef for std::unique_ptr<JobQueue>.
            typedef std::unique_ptr<JobQueue> UniquePtr;

        protected:
            /// \brief
            /// JobQueue name. If set, \see{Worker} threads will be named name-%d.
            const std::string name;
            /// \brief
            /// JobQueue type (TIPE_FIFO or TYPE_LIFO)
            const Type type;
            /// \brief
            /// Number of workers servicing the queue.
            const ui32 workerCount;
            /// \brief
            /// \Worker thread priority.
            const i32 workerPriority;
            /// \brief
            /// \Worker thread processor affinity.
            const ui32 workerAffinity;
            /// \brief
            /// Max pending jobs.
            const ui32 maxPendingJobs;
            /// \brief
            /// Called to initialize/uninitialize the worker thread.
            WorkerCallback *workerCallback;
            /// \brief
            /// Flag to signal the worker thread.
            volatile bool done;
            /// \brief
            /// Queue of jobs.
            JobList jobs;
            /// \brief
            /// Queue stats.
            Stats stats;
            /// \brief
            /// Synchronization mutex.
            Mutex jobsMutex;
            /// \brief
            /// Synchronization condition variable.
            Condition jobsNotEmpty;
            /// \brief
            /// Synchronization condition variable.
            Condition jobFinished;
            /// \brief
            /// Synchronization condition variable.
            Condition idle;
            /// \brief
            /// Worker state.
            enum State {
                /// \brief
                /// Worker is idle.
                Idle,
                /// \Worker is busy.
                Busy
            } volatile state;
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
                /// \brief
                /// Convenient typedef for std::unique_ptr<Worker>.
                typedef std::unique_ptr<Worker> UniquePtr;

                /// \brief
                /// JobQueue to which this worker belongs.
                JobQueue &queue;

                /// \brief
                /// ctor.
                /// \param[in] queue_ JobQueue to which this worker belongs.
                /// \param[in] name Worker thread name.
                /// \param[in] priority Worker thread priority.
                /// \param[in] affinity Worker thread processor affinity.
                Worker (JobQueue &queue_,
                        const std::string &name = std::string (),
                        i32 priority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                        ui32 affinity = UI32_MAX) :
                        Thread (name),
                        queue (queue_) {
                    Create (priority, affinity);
                }

                // Thread
                /// \brief
                /// Worker thread.
                virtual void Run () throw ();
            };
            /// \brief
            /// Count of busy workers.
            ui32 busyWorkers;
            /// \brief
            /// List of workers.
            WorkerList workers;
            /// \brief
            /// Synchronization mutex.
            Mutex workersMutex;

        public:
            /// \brief
            /// ctor.
            /// \param[in] name_ JobQueue name. If set, \see{Worker}
            /// threads will be named name-%d.
            /// \param[in] type_ Queue type.
            /// \param[in] workerCount_ Max workers to service the queue.
            /// \param[in] workerPriority_ Worker thread priority.
            /// \param[in] workerAffinity_ Worker thread processor affinity.
            /// \param[in] maxPendingJobs_ Max pending queue jobs.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the worker thread.
            JobQueue (
                const std::string &name_ = std::string (),
                Type type_ = TYPE_FIFO,
                ui32 workerCount_ = 1,
                i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity_ = UI32_MAX,
                ui32 maxPendingJobs_ = UI32_MAX,
                WorkerCallback *workerCallback_ = 0);
            /// \brief
            /// dtor. Stop the queue.
            virtual ~JobQueue () {
                Stop ();
            }

            /// \brief
            /// Return JobQueue name.
            /// \return JobQueue name.
            inline const std::string &GetName () const {
                return name;
            }

            // RunLoop
            /// \brief
            /// Create the workers, and start waiting for jobs. The
            /// ctor calls this member, but if you ever need to stop
            /// the queue, you need to call Start manually to restart it.
            virtual void Start ();
            /// \brief
            /// Stops all in flight, and cancels all pending jobs. The
            /// queue, and the worker pool are flushed. After calling
            /// this method, the queue is dead, and consumes very little
            /// resources. You need to call Start to get it going
            /// again.
            ///
            /// VERY IMPORTANT: In order to stop the workers, the queue
            /// sets done = true. This is the same done as is passed as
            /// volatile const bool & to Job::Prologue/Execute/Epilog.
            /// Therefore, if you want your code to be responsive, and
            /// the queues to stop quickly, your jobs should pay close
            /// attention to the state of done.
            /// \param[in] cancelPendingJobs true = Cancel all pending jobs.
            virtual void Stop (bool cancelPendingJobs = true);

            /// \brief
            /// Enqueue a job. The next idle worker will pick it up,
            /// and execute it on it's thread.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish.
            /// Used for synchronous job execution.
            virtual void Enq (
                Job &job,
                bool wait = false);

            /// \brief
            /// Cancel a queued job with a given id. If the job is not
            /// in the queue (in flight), it is not canceled.
            /// \param[in] jobId Id of job to cancel.
            /// \return true if the job was cancelled. false if in flight.
            virtual bool Cancel (const Job::Id &jobId);
            /// \brief
            /// Cancel all queued jobs. Jobs in flight are unaffected.
            virtual void CancelAll ();

            /// \brief
            /// Return a snapshot of the queue stats.
            /// \return A snapshot of the queue stats.
            virtual Stats GetStats ();

            /// \brief
            /// Blocks until all jobs are complete and the queue is empty.
            virtual void WaitForIdle ();

            /// \brief
            /// Return true if Start was called.
            /// \return true = Start was called, false = Start was not called.
            virtual bool IsRunning ();
            /// \brief
            /// Return true if there are no pending jobs.
            /// \return true = no pending jobs, false = jobs pending.
            virtual bool IsEmpty ();
            /// \brief
            /// Return true if there are no pending jobs and all the
            /// workers are idle.
            /// \return true = idle, false = busy.
            virtual bool IsIdle ();

        private:
            /// \brief
            /// Used internally by worker(s) to get the next job.
            /// \return The next job to execute.
            Job *Deq ();
            /// \brief
            /// Called by worker(s) after each job is done.
            /// Used to update state and \see{RunLoop::Stats}.
            /// \param[in] job Completed job.
            /// \param[in] start Completed job start time.
            /// \param[in] end Completed job end time.
            void FinishedJob (
                Job &job,
                ui64 start,
                ui64 end);

            /// \brief
            /// Atomically set done to the given value.
            /// \param[in] value value to set done to.
            /// \return true == done was set to the given value.
            /// false == done was already set to the given value.
            bool SetDone (bool value);

            /// \brief
            /// JobQueue is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (JobQueue)
        };

        /// \struct GlobalJobQueueCreateInstance JobQueue.h thekogans/util/JobQueue.h
        ///
        /// \brief
        /// Call GlobalJobQueueCreateInstance::Parameterize before the first use of
        /// GlobalJobQueue::Instance to supply custom arguments to GlobalJobQueue ctor.

        struct _LIB_THEKOGANS_UTIL_DECL GlobalJobQueueCreateInstance {
        private:
            /// \brief
            /// JobQueue name. If set, \see{JobQueue::Worker} threads will be named name-%d.
            static std::string name;
            /// \brief
            /// JobQueue type (TIPE_FIFO or TYPE_LIFO)
            static RunLoop::Type type;
            /// \brief
            /// Number of workers servicing the queue.
            static ui32 workerCount;
            /// \brief
            /// Worker thread priority.
            static i32 workerPriority;
            /// \brief
            /// Worker thread processor affinity.
            static ui32 workerAffinity;
            /// \brief
            /// Max pending jobs.
            static ui32 maxPendingJobs;
            /// \brief
            /// Called to initialize/uninitialize the worker thread.
            static RunLoop::WorkerCallback *workerCallback;

        public:
            /// \brief
            /// Call before the first use of GlobalJobQueue::Instance.
            /// \param[in] name_ JobQueue name. If set, \see{JobQueue::Worker}
            /// threads will be named name-%d.
            /// \param[in] type_ Queue type.
            /// \param[in] workerCount_ Max workers to service the queue.
            /// \param[in] workerPriority_ Worker thread priority.
            /// \param[in] workerAffinity_ Worker thread processor affinity.
            /// \param[in] maxPendingJobs_ Max pending queue jobs.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the worker thread.
            static void Parameterize (
                const std::string &name_ = std::string (),
                RunLoop::Type type_ = RunLoop::TYPE_FIFO,
                ui32 workerCount_ = 1,
                i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity_ = UI32_MAX,
                ui32 maxPendingJobs_ = UI32_MAX,
                RunLoop::WorkerCallback *workerCallback_ = 0);

            /// \brief
            /// Create a global job queue with custom ctor arguments.
            /// \return A global job queue with custom ctor arguments.
            JobQueue *operator () ();
        };

        /// \struct GlobalJobQueue JobQueue.h thekogans/util/JobQueue.h
        ///
        /// \brief
        /// A global job queue instance. The JobQueue is designed to be
        /// as flexible as possible. To be useful in different situations
        /// the job queue's worker count needs to be parametrized as we
        /// might need different queues running different worker counts at
        /// different thread priorities. That said, the most basic (and
        /// the most useful) use case will have a single job queue using
        /// the defaults. This typedef exists to aid in that. If all you
        /// need is a background thread where you can schedule jobs, then
        /// GlobalJobQueue::Instance () will do the trick.
        struct _LIB_THEKOGANS_UTIL_DECL GlobalJobQueue :
            public Singleton<JobQueue, SpinLock, GlobalJobQueueCreateInstance> {};

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_JobQueue_h)
