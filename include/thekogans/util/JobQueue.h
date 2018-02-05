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

#if defined (TOOLCHAIN_OS_Windows)
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
        #include <windows.h>
    #endif // !defined (_WINDOWS_)
    #include <objbase.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <memory>
#include <string>
#include <map>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Thread.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Condition.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/XMLUtils.h"

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

        struct _LIB_THEKOGANS_UTIL_DECL JobQueue {
            /// \brief
            /// Convenient typedef for std::unique_ptr<JobQueue>.
            typedef std::unique_ptr<JobQueue> UniquePtr;

            /// \enum
            /// The order in which pending jobs are processed;
            /// First In, First Out (TYPE_FIFO), or Last In,
            /// First Out (TYPE_LIFO).
            enum Type {
                /// \brief
                /// First in first out.
                TYPE_FIFO,
                /// \brief
                /// Last in first out.
                TYPE_LIFO,
            };

            /// \struct JobQueue::WorkerCallback JobQueue.h thekogans/util/JobQueue.h
            ///
            /// \brief
            /// Gives the JobQueue owner a chance to initialize/uninitialize the worker threads.
            struct _LIB_THEKOGANS_UTIL_DECL WorkerCallback {
                /// \brief
                /// dtor.
                virtual ~WorkerCallback () {}

                /// \brief
                /// Called by the worker before entering the job execution loop.
                virtual void InitializeWorker () throw () {}
                /// \brief
                /// Called by the worker before exiting the thread.
                virtual void UninitializeWorker () throw () {}
            };

        #if defined (TOOLCHAIN_OS_Windows)
            /// \struct JobQueue::COMInitializer JobQueue.h thekogans/util/JobQueue.h
            ///
            /// \brief
            /// Initialize the Windows COM library.
            struct _LIB_THEKOGANS_UTIL_DECL COMInitializer : public WorkerCallback {
                /// \brief
                /// CoInitializeEx concurrency model.
                DWORD dwCoInit;

                /// \brief
                /// ctor.
                /// \param[in] dwCoInit_ CoInitializeEx concurrency model.
                COMInitializer (DWORD dwCoInit_ = COINIT_MULTITHREADED) :
                    dwCoInit (dwCoInit_) {}

                /// \brief
                /// Called by the worker before entering the job execution loop.
                virtual void InitializeWorker () throw ();
                /// \brief
                /// Called by the worker before exiting the thread.
                virtual void UninitializeWorker () throw ();
            };

            /// \struct JobQueue::OLEInitializer JobQueue.h thekogans/util/JobQueue.h
            ///
            /// \brief
            /// Initialize the Windows OLE library.
            struct _LIB_THEKOGANS_UTIL_DECL OLEInitializer : public WorkerCallback {
                /// \brief
                /// Called by the worker before entering the job execution loop.
                virtual void InitializeWorker () throw ();
                /// \brief
                /// Called by the worker before exiting the thread.
                virtual void UninitializeWorker () throw ();
            };
        #endif // defined (TOOLCHAIN_OS_Windows)

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
        #if defined (_MSC_VER)
            #pragma warning (push)
            #pragma warning (disable : 4275)
        #endif // defined (_MSC_VER)
            /// \struct JobQueue::Job JobQueue.h thekogans/util/JobQueue.h
            ///
            /// \brief
            /// A JobQueue::Job must, at least, implement the Execute method.
            struct _LIB_THEKOGANS_UTIL_DECL Job :
                    public ThreadSafeRefCounted,
                    public JobList::Node {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<Job>.
                typedef ThreadSafeRefCounted::Ptr<Job> Ptr;

                /// \brief
                /// Convenient typedef for std::string.
                typedef std::string Id;

                /// \brief
                /// Job id.
                const Id id;
                /// \brief
                /// Call Cancel () to set this flag. Monitor this
                /// flag in Execute () to respond to cancellation
                /// requests.
                volatile bool cancelled;
                /// \brief
                /// Flag used internally to wait for synchronous jobs.
                volatile bool finished;

                /// \brief
                /// ctor.
                /// \param[in] id_ Job id.
                Job (const Id &id_ = GUID::FromRandom ().ToString ()) :
                    id (id_),
                    cancelled (false),
                    finished (false) {}
                /// \brief
                /// dtor.
                virtual ~Job () {}

                /// \brief
                /// Call this method on a running job to cancel execution.
                /// Monitor this flag in Execute () to respond to cancellation
                /// requests.
                inline void Cancel () {
                    cancelled = true;
                }

                /// \brief
                /// Return a unique identifier for this
                /// job. Used to cancel a pending job.
                /// \return unique id for the job.
                inline Id GetId () const throw () {
                    return id;
                }

                // See VERY IMPORTANT comment in Stop below.
                /// \brief
                /// Called before the job is executed. Allows
                /// the job to perform one time initialization.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void Prologue (volatile const bool & /*done*/) throw () {}
                /// \brief
                /// Called to execute the job. This is the only api
                /// the job MUST implement.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void Execute (volatile const bool & /*done*/) throw () = 0;
                /// \brief
                /// Called after the job is executed. Allows
                /// the job to perform one time cleanup.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void Epilogue (volatile const bool & /*done*/) throw () {}

                /// \brief
                /// Return true if the job should stop what it's doing and exit.
                /// Use this method in your implementation of Execute to keep the
                /// JobQueue responsive.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                /// \return true == Job should stop what it's doing and exit.
                inline bool ShouldStop (volatile const bool &done) const {
                    return done || cancelled;
                }
                /// \brief
                /// Return true if job is still running.
                /// \return true == Job has not been cancelled and is not finished.
                inline bool Running () const {
                    return !cancelled && !finished;
                }
                /// \brief
                /// Return true if job completed successfully.
                /// \return true == Job has not been cancelled and is finished.
                inline bool Completed () const {
                    return !cancelled && finished;
                }
            };
        #if defined (_MSC_VER)
            #pragma warning (pop)
        #endif // defined (_MSC_VER)

            /// \struct JobQueue::Stats JobQueue.h thekogans/util/JobQueue.h
            ///
            /// \brief
            /// Queue statistics.\n
            /// jobCount - Number of pending jobs.\n
            /// totalJobs - Number of retired (executed) jobs.\n
            /// totalJobTime - Amount of time spent executing jobs.\n
            /// last - Last job.\n
            /// min - Fastest job.\n
            /// max - Slowest job.\n
            struct _LIB_THEKOGANS_UTIL_DECL Stats {
                /// \brief
                /// Pending job count.
                ui32 jobCount;
                /// \brief
                /// Total jobs processed.
                ui32 totalJobs;
                /// \brief
                /// Total time taken to process totalJobs.
                ui64 totalJobTime;
                /// \struct JobQueue::Stats::Job JobQueue.h thekogans/util/JobQueue.h
                ///
                /// \brief
                /// Job stats.
                struct _LIB_THEKOGANS_UTIL_DECL Job {
                    /// \brief
                    /// Job id.
                    JobQueue::Job::Id id;
                    /// \brief
                    /// Job start time.
                    ui64 startTime;
                    /// \brief
                    /// Job end time.
                    ui64 endTime;
                    /// \brief
                    /// Job total execution time.
                    ui64 totalTime;

                    /// \brief
                    /// ctor.
                    Job () :
                        startTime (0),
                        endTime (0),
                        totalTime (0) {}
                    /// \brief
                    /// ctor.
                    /// \param[in] id_ Job id.
                    /// \param[in] startTime_ Job start time.
                    /// \param[in] endTime_ Job end time.
                    /// \param[in] totalTime_ Job total execution time.
                    Job (
                        JobQueue::Job::Id id_,
                        ui64 startTime_,
                        ui64 endTime_,
                        ui64 totalTime_) :
                        id (id_),
                        startTime (startTime_),
                        endTime (endTime_),
                        totalTime (totalTime_) {}

                    /// \brief
                    /// Return the XML representation of the Job stats.
                    /// \param[in] name Job stats name (last, min, max).
                    /// \param[in] indentationLevel Pretty print parameter. If
                    /// the resulting tag is to be included in a larger structure
                    /// you might want to provide a value that will embed it in
                    /// the structure.
                    /// \return The XML reprentation of the Job stats.
                    inline std::string ToString (
                            const std::string &name,
                            ui32 indentationLevel) const {
                        assert (!name.empty ());
                        Attributes attributes;
                        attributes.push_back (Attribute ("id", id));
                        attributes.push_back (Attribute ("startTime", ui64Tostring (startTime)));
                        attributes.push_back (Attribute ("endTime", ui64Tostring (endTime)));
                        attributes.push_back (Attribute ("totalTime", ui64Tostring (totalTime)));
                        return OpenTag (indentationLevel, name.c_str (), attributes, true, true);
                    }
                };
                /// \brief
                /// Last job stats.
                Job lastJob;
                /// \brief
                /// Minimum job stats.
                Job minJob;
                /// \brief
                /// Maximum job stats.
                Job maxJob;

                /// \brief
                /// ctor.
                Stats () :
                    jobCount (0),
                    totalJobs (0),
                    totalJobTime (0) {}

                /// \brief
                /// After completion of each job, used to update the stats.
                /// \param[in] jobId Id of completed job.
                /// \param[in] start Job start time.
                /// \param[in] end Job end time.
                void Update (
                    const JobQueue::Job::Id &jobId,
                    ui64 start,
                    ui64 end);

                /// \brief
                /// Return the XML representation of the Stats.
                /// \param[in] name JobQueue name.
                /// \param[in] indentationLevel Pretty print parameter. If
                /// the resulting tag is to be included in a larger structure
                /// you might want to provide a value that will embed it in
                /// the structure.
                /// \return The XML reprentation of the Stats.
                std::string ToString (
                        const std::string &name,
                        ui32 indentationLevel) const {
                    Attributes attributes;
                    attributes.push_back (Attribute ("jobCount", ui32Tostring (jobCount)));
                    attributes.push_back (Attribute ("totalJobs", ui32Tostring (totalJobs)));
                    attributes.push_back (Attribute ("totalJobTime", ui64Tostring (totalJobTime)));
                    return
                        OpenTag (
                            indentationLevel,
                            !name.empty () ? name.c_str () : "JobQueue",
                            attributes,
                            false,
                            true) +
                        lastJob.ToString ("last", indentationLevel + 1) +
                        minJob.ToString ("min", indentationLevel + 1) +
                        maxJob.ToString ("max", indentationLevel + 1) +
                        CloseTag (indentationLevel, !name.empty () ? name.c_str () : "JobQueue");
                }
            };

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
            const i32 workerAffinity;
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
                /// Worker(s) is/are idle.
                Idle,
                /// \Worker(s) is/are busy.
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

            /// \brief
            /// Create the workers, and start waiting for jobs. The
            /// ctor calls this member, but if you ever need to stop
            /// the queue, you need to call Start manually to restart it.
            void Start ();
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
            void Stop ();

            /// \brief
            /// Enqueue a job. The next idle worker will pick it up,
            /// and execute it on it's thread.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish.
            /// Used for synchronous job execution.
            void Enq (
                Job &job,
                bool wait = false);

            /// \brief
            /// Cancel a queued job with a given id. If the job is not
            /// in the queue (in flight), it is not canceled.
            /// \param[in] jobId Id of job to cancel.
            /// \return true if the job was cancelled. false if in flight.
            bool Cancel (const Job::Id &jobId);
            /// \brief
            /// Cancel all queued jobs. Jobs in flight are unaffected.
            void CancelAll ();

            /// \brief
            /// Blocks until all jobs are complete and the queue is empty.
            void WaitForIdle ();

            /// \brief
            /// Return a snapshot of the queue stats.
            /// \return A snapshot of the queue stats.
            Stats GetStats ();

            /// \brief
            /// Return true if there are no pending jobs.
            /// \return true = no pending jobs, false = jobs pending.
            bool IsEmpty ();
            /// \brief
            /// Return true if there are no pending jobs and all the
            /// workers are idle.
            /// \return true = idle, false = busy.
            bool IsIdle ();

        private:
            /// \brief
            /// Used internally by worker(s) to get the next job.
            /// \return The next job to execute.
            Job::Ptr Deq ();
            /// \brief
            /// Called by worker(s) after each job is done.
            /// Used to update state and \see{JobQueue::Stats}.
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
            /// \return true = done was set to the given value.
            /// false = done was already set to the given value.
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
            static JobQueue::Type type;
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
            static JobQueue::WorkerCallback *workerCallback;

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
                JobQueue::Type type_ = JobQueue::TYPE_FIFO,
                ui32 workerCount_ = 1,
                i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity_ = UI32_MAX,
                ui32 maxPendingJobs_ = UI32_MAX,
                JobQueue::WorkerCallback *workerCallback_ = 0);

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
