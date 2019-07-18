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

#if !defined (__thekogans_util_Pipeline_h)
#define __thekogans_util_Pipeline_h

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/Thread.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Condition.h"
#include "thekogans/util/Event.h"
#include "thekogans/util/JobQueue.h"

namespace thekogans {
    namespace util {

        /// \struct Pipeline Pipeline.h thekogans/util/Pipeline.h
        ///
        /// \brief
        /// A Pipeline builds on the \see{RunLoop} to provide staged
        /// execution. Think of an assembly line where each station
        /// (pipeline stage) performs a specific task, and passes the
        /// job on to the next stage (this is how modern processor
        /// architectures perform scalar, and even super-scalar
        /// execution).

        struct _LIB_THEKOGANS_UTIL_DECL Pipeline :
                public virtual ThreadSafeRefCounted {
            /// \brief
            /// Convenient typedef for ThreadSafeRefCounted::Ptr<Pipeline>.
            typedef ThreadSafeRefCounted::Ptr<Pipeline> Ptr;

            /// \struct Pipeline::Stage Pipeline.h thekogans/util/Pipeline.h
            ///
            /// \brief
            /// Used to specify stage (\see{JobQueue}) ctor parameters.
            struct _LIB_THEKOGANS_UTIL_DECL Stage {
                /// \brief
                /// Stage \see{JobQueue} name.
                std::string name;
                /// \brief
                /// Type of stage (TYPE_FIFO or TYPE_LIFO).
                RunLoop::Type type;
                /// \brief
                /// Max pending jobs.
                std::size_t maxPendingJobs;
                /// \brief
                /// Count of workers servicing this stage.
                std::size_t workerCount;
                /// \brief
                /// Worker thread priority.
                i32 workerPriority;
                /// \brief
                /// Worker thread processor affinity.
                ui32 workerAffinity;
                /// \brief
                /// Called to initialize/uninitialize the worker thread.
                RunLoop::WorkerCallback *workerCallback;

                /// \brief
                /// ctor.
                /// \param[in] name_ Stage \see{JobQueue} name.
                /// \param[in] type_ Stage \see{JobQueue} type.
                /// \param[in] maxPendingJobs_ Max pending stage jobs.
                /// \param[in] workerCount_ Number of workers servicing this stage.
                /// \param[in] workerPriority_ Stage worker thread priority.
                /// \param[in] workerAffinity_ Stage worker thread processor affinity.
                /// \param[in] workerCallback_ Called to initialize/uninitialize the
                /// stage worker thread(s).
                Stage (
                    const std::string &name_ = std::string (),
                    RunLoop::Type type_ = RunLoop::TYPE_FIFO,
                    std::size_t maxPendingJobs_ = SIZE_T_MAX,
                    std::size_t workerCount_ = 1,
                    i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                    ui32 workerAffinity_ = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                    RunLoop::WorkerCallback *workerCallback_ = 0) :
                    name (name_),
                    type (type_),
                    maxPendingJobs (maxPendingJobs_),
                    workerCount (workerCount_),
                    workerPriority (workerPriority_),
                    workerAffinity (workerAffinity_),
                    workerCallback (workerCallback_) {}
            };

            /// \brief
            /// Forward declaration of Job.
            struct Job;
            enum {
                /// \brief
                /// JobList ID.
                JOB_LIST_ID = RunLoop::LAST_JOB_LIST_ID,
                /// \brief
                /// Use this sentinel to create your own job lists.
                LAST_JOB_LIST_ID
            };
            /// \brief
            /// Convenient typedef for IntrusiveList<Job, JOB_LIST_ID>.
            typedef IntrusiveList<Job, JOB_LIST_ID> JobList;

        #if defined (_MSC_VER)
            #pragma warning (push)
            #pragma warning (disable : 4275)
        #endif // defined (_MSC_VER)
            /// \struct Pipeline::Job Pipeline.h thekogans/util/Pipeline.h
            ///
            /// \brief
            /// A pipeline job. Since a pipeline is a collection
            /// of \see{JobQueue}s, the Pipeline::Job derives form
            /// RunLoop::Job. RunLoop::Job::SetState is used to
            /// shepherd the job down the pipeline. Pipeline::Job
            /// Begin and End are provided to perform one time
            /// initialization/tear down.
            struct _LIB_THEKOGANS_UTIL_DECL Job :
                    public RunLoop::Job,
                    public JobList::Node {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<Job>.
                typedef ThreadSafeRefCounted::Ptr<Job> Ptr;

            protected:
                /// \brief
                /// Pipeline on which this job is staged.
                Pipeline &pipeline;
                /// \brief
                /// Current job stage.
                std::size_t stage;
                /// \brief
                /// Job execution start time.
                ui64 start;
                /// \brief
                /// Job execution end time.
                ui64 end;

            public:
                /// \brief
                /// ctor.
                /// \param[in] pipeline_ Pipeline that will execute this job.
                explicit Job (Pipeline &pipeline_) :
                    pipeline (pipeline_),
                    stage (0),
                    start (0),
                    end (0) {}

                /// \brief
                /// Return the pipeline on which this job can run.
                /// \return Pipeline on which this job can run.
                inline Pipeline &GetPipeline () const {
                    return pipeline;
                }
                /// \brief
                /// Return the pipeline id on which this job can run.
                /// \return Pipeline id on which this job can run.
                inline const RunLoop::Id &GetPipelineId () const {
                    return pipeline.GetId ();
                }

            protected:
                /// \brief
                /// Used internally by RunLoop to set the RunLoop id and reset
                /// the state, disposition and completed.
                /// \param[in] runLoopId_ RunLoop id to which this job belongs.
                virtual void Reset (const RunLoop::Id &runLoopId_);
                /// \brief
                /// Used internally by RunLoop and it's derivatives to set the
                /// job state.
                /// \param[in] state_ New job state.
                virtual void SetState (State state_);

                /// \brief
                /// Override this method to provide custom staging.
                /// \return First job pipeline stage.
                virtual std::size_t GetFirstStage () const {
                    return 0;
                }
                /// \brief
                /// Override this method to provide custom staging.
                /// \return Next job pipeline stage.
                virtual std::size_t GetNextStage () const {
                    return stage + 1;
                }

                /// \brief
                /// Provides the same functionality as
                /// Job::Prologue, except at pipeline
                /// global level.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void Begin (const THEKOGANS_UTIL_ATOMIC<bool> &done) throw () {}
                /// \brief
                /// Provides the same functionality as
                /// Job::Epilogue, except at pipeline
                /// global level.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void End (const THEKOGANS_UTIL_ATOMIC<bool> &done) throw () {}

                /// \brief
                /// Pipeline uses Reset.
                friend struct Pipeline;

                /// \brief
                /// Job is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Job)
            };
        #if defined (_MSC_VER)
            #pragma warning (pop)
        #endif // defined (_MSC_VER)

        private:
            /// \brief
            /// Pipeline id.
            const RunLoop::Id id;
            /// \brief
            /// Pipeline name.
            const std::string name;
            /// \brief
            /// Pipeline type (TIPE_FIFO or TYPE_LIFO)
            const RunLoop::Type type;
            /// \brief
            /// Max pending jobs.
            const std::size_t maxPendingJobs;
            /// \brief
            /// Flag to signal the stage thread(s).
            THEKOGANS_UTIL_ATOMIC<bool> done;
            /// \brief
            /// Queue of pending jobs.
            JobList pendingJobs;
            /// \brief
            /// List of running jobs.
            JobList runningJobs;
            /// \brief
            /// Pipeline stats.
            RunLoop::Stats stats;
            /// \brief
            /// Synchronization mutex.
            Mutex jobsMutex;
            /// \brief
            /// Synchronization condition variable.
            Condition jobsNotEmpty;
            /// \brief
            /// Synchronization condition variable.
            Condition idle;
            /// \brief
            /// true == pipeline is paused.
            bool paused;
            /// \brief
            /// Signal waiting workers that the pipeline is not paused.
            Condition notPaused;
            /// \brief
            /// Number of workers servicing the pipeline.
            const std::size_t workerCount;
            /// \brief
            /// \Worker thread priority.
            const i32 workerPriority;
            /// \brief
            /// \Worker thread processor affinity.
            const ui32 workerAffinity;
            /// \brief
            /// Called to initialize/uninitialize the worker thread.
            RunLoop::WorkerCallback *workerCallback;
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
            /// \struct Pipeline::Worker Pipeline.h thekogans/util/Pipeline.h
            ///
            /// \brief
            /// Worker takes pending jobs off the pipeline queue and
            /// executes them. It then reports back to the pipeline
            /// so that it can collect statistics.
            struct Worker :
                    public Thread,
                    public WorkerList::Node {
            private:
                /// \brief
                /// Pipeline to which this worker belongs.
                Pipeline &pipeline;

            public:
                /// \brief
                /// ctor.
                /// \param[in] pipeline_ Pipeline to which this worker belongs.
                /// \param[in] name Worker thread name.
                /// \param[in] priority Worker thread priority.
                /// \param[in] affinity Worker thread processor affinity.
                Worker (Pipeline &pipeline_,
                        const std::string &name = std::string (),
                        i32 priority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                        ui32 affinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY) :
                        Thread (name),
                        pipeline (pipeline_) {
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
            /// Pipeline stages.
            std::vector<JobQueue::Ptr> stages;
            /// \brief
            /// Synchronization mutex.
            Mutex workersMutex;

        public:
            /// \brief
            /// ctor.
            /// \param[in] begin Pointer to the beginning of the Stage array.
            /// \param[in] end Pointer to the end of the Stage array.
            /// \param[in] name_ Pipeline name.
            /// \param[in] type_ Pipeline type.
            /// \param[in] maxPendingJobs_ Max pending pipeline jobs.
            /// \param[in] workerCount_ Max workers to service the pipeline.
            /// \param[in] workerPriority_ Worker thread priority.
            /// \param[in] workerAffinity_ Worker thread processor affinity.
            /// \param[in] workerCallback_ Called to initialize/uninitialize
            /// the worker thread.
            Pipeline (
                const Stage *begin,
                const Stage *end,
                const std::string &name_ = std::string (),
                RunLoop::Type type_ = RunLoop::TYPE_FIFO,
                std::size_t maxPendingJobs_ = SIZE_T_MAX,
                std::size_t workerCount_ = 1,
                i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity_ = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                RunLoop::WorkerCallback *workerCallback_ = 0);
            /// \brief
            /// dtor. Stop the pipeline.
            ~Pipeline () {
                Stop ();
            }

            /// \brief
            /// Return Pipeline id.
            /// \return Pipeline id.
            inline const RunLoop::Id &GetId () const {
                return id;
            }
            /// \brief
            /// Return Pipeline name.
            /// \return Pipeline name.
            inline const std::string &GetName () const {
                return name;
            }

            /// \brief
            /// Pause pipeline execution. Currently running jobs are allowed to finish,
            /// but no other pending jobs are executed until Continue is called. If the
            /// pipeline is paused, noop.
            /// VERY IMPORTANT: A paused pipeline does NOT imply idle. If you pause a
            /// pipeline that has pending jobs, \see{IsIdle} (below) will return false.
            /// \param[in] cancelRunningJobs true == Cancel running jobs.
            /// \param[in] timeSpec How long to wait for the pipeline to pause.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == Pipeline paused. false == timed out.
            bool Pause (
                bool cancelRunningJobs = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Continue the pipeline execution. If the pipeline is not paused, noop.
            void Continue ();
            /// \brief
            /// Return true if the pipeline is paused.
            /// \return true == Pipeline is paused.
            bool IsPaused ();

            /// \brief
            /// Return the stats for a given pipeline stage.
            /// \return Stats corresponding to the given pipeline stage.
            RunLoop::Stats GetStageStats (std::size_t stage);
            /// \brief
            /// Return the stats for all pipeline stages.
            /// \param[out] stats Vector where stage stats will be placed.
            void GetStagesStats (std::vector<RunLoop::Stats> &stats);

            /// \brief
            /// Start the pipeline and it's stages, and start waiting for jobs.
            void Start ();
            /// \brief
            /// Stop the pipeline and it's stages.
            /// \param[in] cancelRunningJobs true = Cancel all running jobs.
            /// \param[in] cancelPendingJobs true = Cancel all pending jobs.
            /// \param[in] timeSpec How long to wait for the job queue to stop.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == Job queue stopped. false == timed out waiting for worker to stop.
            bool Stop (
                bool cancelRunningJobs = true,
                bool cancelPendingJobs = true,
                const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Enqueue a job on the pipeline.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == !wait || WaitForJob (...)
            bool EnqJob (
                Job::Ptr job,
                bool wait = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Enqueue a job to be performed next on the pipeline.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == !wait || WaitForJob (...)
            bool EnqJobFront (
                Job::Ptr job,
                bool wait = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Get a running or a pending job with the given id.
            /// \param[in] jobId Id of job to retrieve.
            /// \return Job matching the given id.
            Job::Ptr GetJob (const Job::Id &jobId);
            /// \brief
            /// Get all running and pending jobs matching the given equality test.
            /// \param[in] equalityTest \see{RunLoop::EqualityTest} to query to determine the matching jobs.
            /// \param[out] jobs \see{RunLoop::UserJobList} containing the matching jobs.
            void GetJobs (
                const RunLoop::EqualityTest &equalityTest,
                RunLoop::UserJobList &jobs);
            /// \brief
            /// Get all pending jobs.
            /// \param[out] pendingJobs \see{RunLoop::UserJobList} containing pending jobs.
            void GetPendingJobs (RunLoop::UserJobList &pendingJobs);
            /// \brief
            /// Get all running jobs.
            /// \param[out] runningJobs \see{RunLoop::UserJobList} containing running jobs.
            void GetRunningJobs (RunLoop::UserJobList &runningJobs);
            /// \brief
            /// Get all running and pending jobs. pendingJobs and runningJobs can be the same
            /// UserJobList. In that case first n jobs will be pending and the final m jobs
            /// will be running.
            /// \param[out] pendingJobs \see{RunLoop::UserJobList} containing pending jobs.
            /// \param[out] runningJobs \see{RunLoop::UserJobList} containing running jobs.
            void GetAllJobs (
                RunLoop::UserJobList &pendingJobs,
                RunLoop::UserJobList &runningJobs);

            // NOTE for all Wait* methods below: If threads are waiting on pending jobs
            // indefinitely and another thread calls Stop (..., false) or Pause () then
            // the waiting threads will be blocked until you call Start () or Continue ().
            // This is a feature, not a bug. It allows you to suspend pipeline execution
            // temporarily without affecting waiters.

            /// \brief
            /// Wait for a given running or pending job to complete.
            /// \param[in] job Job to wait on.
            /// NOTE: The Pipeline will check that the given job was in fact
            /// en-queued on this pipeline (Job::GetPipelineId) and will throw
            /// THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL if it doesn't.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == completed, false == timed out.
            bool WaitForJob (
                Job::Ptr job,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Wait for a running or a pending job with a given id to complete.
            /// \param[in] jobId Id of job to wait on.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == completed,
            /// false == job with a given id was not in the pipeline or timed out.
            bool WaitForJob (
                const Job::Id &jobId,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Wait for all given running and pending jobs.
            /// \param[in] jobs \see{RunLoop::UserJobList} containing the jobs to wait on.
            /// \param[in] timeSpec How long to wait for the jobs to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == All jobs satisfying the equalityTest completed,
            /// false == One or more matching jobs timed out.
            /// NOTE: This is a static method and is designed to allow you to
            /// wait on a collection of jobs without regard as to which pipeline
            /// they're running on.
            static bool WaitForJobs (
                const RunLoop::UserJobList &jobs,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Wait for all running and pending jobs matching the given equality test to complete.
            /// \param[in] equalityTest \see{RunLoop::EqualityTest} to query to determine which
            /// jobs to wait on.
            /// \param[in] timeSpec How long to wait for the jobs to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == All jobs satisfying the equalityTest completed,
            /// false == One or more matching jobs timed out.
            bool WaitForJobs (
                const RunLoop::EqualityTest &equalityTest,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Blocks until all jobs are complete and the pipeline is empty.
            /// IMPORTANT: See VERY IMPORTANT comment in \see{Pause} (above).
            /// \param[in] timeSpec How long to wait for the jobs to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == Pipeline is idle, false == Timed out.
            bool WaitForIdle (const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Cancel a running or a pending job with a given id.
            /// \param[in] jobId Id of job to cancel.
            /// \return true if the job was cancelled.
            bool CancelJob (const Job::Id &jobId);
            /// \brief
            /// Cancel the list of given jobs.
            /// \param[in] jobs List of jobs to cancel.
            /// NOTE: This is a static method and is designed to allow you to
            /// cancel a collection of jobs without regard as to which pipeline
            /// they're running on.
            static void CancelJobs (const RunLoop::UserJobList &jobs);
            /// \brief
            /// Cancel all running and pending jobs matching the given equality test.
            /// \param[in] equalityTest \see{RunLoop::EqualityTest} to query to determine
            /// which jobs to cancel.
            void CancelJobs (const RunLoop::EqualityTest &equalityTest);
            /// \brief
            /// Cancel all pending jobs.
            void CancelPendingJobs ();
            /// \brief
            /// Cancel all running jobs.
            void CancelRunningJobs ();
            /// \brief
            /// Cancel all running and pending jobs.
            void CancelAllJobs ();

            /// \brief
            /// Return a snapshot of the pipeline stats.
            /// \return A snapshot of the pipeline stats.
            RunLoop::Stats GetStats ();
            /// \brief
            /// Reset the pipeline stats.
            void ResetStats ();

            /// \brief
            /// Return true if there are no running jobs and the
            /// stages are idle.
            /// IMPORTANT: See VERY IMPORTANT comment in \see{Pause} (above).
            /// \return true = idle, false = busy.
            bool IsIdle ();

        protected:
            /// \brief
            /// Used internally by worker(s) to get the next job.
            /// \param[in] wait true == Wait until a job becomes available.
            /// \return The next job to execute.
            Job *DeqJob (bool wait = true);
            /// \brief
            /// Called by job after it has traversed the pipeline.
            /// Used to update state and \see{Pipeline::Stats}.
            /// \param[in] job Completed job.
            /// \param[in] start Completed job start time.
            /// \param[in] end Completed job end time.
            void FinishedJob (
                Job *job,
                ui64 start,
                ui64 end);

            /// \brief
            /// Pipeline is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Pipeline)
        };

        /// \struct GlobalPipelineCreateInstance Pipeline.h thekogans/util/Pipeline.h
        ///
        /// \brief
        /// Call GlobalPipelineCreateInstance::Parameterize before the first use of
        /// GlobalPipeline::Instance to supply custom arguments to GlobalPipeline ctor.
        /// NOTE: The call to GlobalPipelineCreateInstance::Parameterize is NOT optional.
        /// You must, at the very least, provide the stages that GlobalPipeline will implement.

        struct _LIB_THEKOGANS_UTIL_DECL GlobalPipelineCreateInstance {
        private:
            /// \brief
            /// Pointer to the beginning of the Pipeline::Stage array.
            static std::vector<Pipeline::Stage> stages;
            /// \brief
            /// Pipeline name. If set, \see{Pipeline::Worker} threads will be named name-%d.
            static std::string name;
            /// \brief
            /// Pipeline type (RunLoop::TIPE_FIFO or RunLoop::TYPE_LIFO)
            static RunLoop::Type type;
            /// \brief
            /// Max pending jobs.
            static std::size_t maxPendingJobs;
            /// \brief
            /// Number of workers servicing the pipeline.
            static std::size_t workerCount;
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
            /// Call before the first use of GlobalPipeline::Instance to provide
            /// custom ctor arguments to GlobalPipeline.
            /// \param[in] begin_ Pointer to the beginning of the Pipeline::Stage array.
            /// \param[in] end_ Pointer to the end of the Pipeline::Stage array.
            /// \param[in] name_ Pipeline name. If set, \see{Pipeline::Worker}
            /// threads will be named name-%d.
            /// \param[in] type_ Pipeline type.
            /// \param[in] maxPendingJobs_ Max pending pipeline jobs.
            /// \param[in] workerCount_ Max workers to service the pipeline.
            /// \param[in] workerPriority_ Worker thread priority.
            /// \param[in] workerAffinity_ Worker thread processor affinity.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the worker thread.
            static void Parameterize (
                const Pipeline::Stage *begin_,
                const Pipeline::Stage *end_,
                const std::string &name_ = std::string (),
                RunLoop::Type type_ = RunLoop::TYPE_FIFO,
                std::size_t maxPendingJobs_ = SIZE_T_MAX,
                std::size_t workerCount_ = 1,
                i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity_ = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                RunLoop::WorkerCallback *workerCallback_ = 0);

            /// \brief
            /// Create a global pipeline with custom ctor arguments.
            /// \return A global pipeline with custom ctor arguments.
            Pipeline *operator () ();
        };

        /// \struct GlobalPipeline Pipeline.h thekogans/util/Pipeline.h
        ///
        /// \brief
        /// A global pipeline instance. The Pipeline is designed to be
        /// as flexible as possible. To be useful in different contexts
        /// the pipelines's worker count needs to be parametrized as we
        /// might need different pipelines running different worker counts
        /// at different thread priorities, as well as different count of
        /// stages. That said, the most basic (and the most useful) use
        /// case will have a single pipeline using the defaults. This struct
        /// exists to aid in that. If all you need is a single pipeline where
        /// you can schedule jobs, then GlobalPipeline::Instance () will do
        /// the trick.
        /// IMPORTANT: Don't forget to call GlobalPipelineCreateInstance::Parameterize
        /// before the first call to GlobalPipeline::Instance to provide the global
        /// pipeline stages.
        struct _LIB_THEKOGANS_UTIL_DECL GlobalPipeline :
            public Singleton<Pipeline, SpinLock, GlobalPipelineCreateInstance> {};

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Pipeline_h)
