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
#include <atomic>
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

        struct _LIB_THEKOGANS_UTIL_DECL Pipeline : public virtual RefCounted {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Pipeline)

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

            struct State;

            /// \struct Pipeline::JobExecutionPolicy Pipeline.h thekogans/util/Pipeline.h
            ///
            /// \brief
            /// JobExecutionPolicy allows the user of the pipeline to
            /// specify the order in which jobs will be executed.
            /// **********************************************************
            /// IMPORTANT: JobExecutionPolicy api is designed to take
            /// a Pipeline and not a job list on which to enqueue (or
            /// from which to dequeue) the given job. This is done so
            /// that the various functions can access Pipeline state if
            /// they need it. It's important that they enqueue/dequeue
            /// these jobs to the pendingJobs list of the given Pipeline
            /// as other Pipeline apis rely on this list to contain the
            /// pending jobs. The various policies are welcome to maintain
            /// a map in to this list to speed up location and retrieval
            /// of the next job to dequeue.
            /// **********************************************************
            struct _LIB_THEKOGANS_UTIL_DECL JobExecutionPolicy : public RefCounted {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (JobExecutionPolicy)

                /// \brief
                /// Max pending pipeline jobs.
                const std::size_t maxJobs;

                /// \brief
                /// ctor.
                /// \param[in] maxJobs_ Max pending pipeline jobs.
                JobExecutionPolicy (std::size_t maxJobs_ = SIZE_T_MAX);
                /// \brief
                /// dtor.
                virtual ~JobExecutionPolicy () {}

                /// \brief
                /// Enqueue a job on the given Pipelines pendingJobs to be performed
                /// on the pipeline thread.
                /// \param[in] pipeline Pipeline on which to enqueue the given job.
                /// \param[in] job Job to enqueue.
                virtual void EnqJob (
                    State &state,
                    Job *job) = 0;
                /// \brief
                /// Enqueue a job on the given Pipelines pendingJobs to be performed
                /// next on the pipeline thread.
                /// \param[in] pipeline Pipeline on which to enqueue the given job.
                /// \param[in] job Job to enqueue.
                virtual void EnqJobFront (
                    State &state,
                    Job *job) = 0;
                /// \brief
                /// Dequeue the next job to be executed on the pipeline thread.
                /// \param[in] pipeline Pipeline from which to dequeue the next job.
                /// \return The next job to execute (0 if no more pending jobs).
                virtual Job *DeqJob (State &state) = 0;
            };

            /// \struct Pipeline::FIFOJobExecutionPolicy Pipeline.h thekogans/util/Pipeline.h
            ///
            /// \brief
            /// First In, First Out execution policy.
            struct _LIB_THEKOGANS_UTIL_DECL FIFOJobExecutionPolicy : public JobExecutionPolicy {
                /// \brief
                /// ctor.
                /// \param[in] maxJobs Max pending pipeline jobs.
                FIFOJobExecutionPolicy (std::size_t maxJobs = SIZE_T_MAX) :
                    JobExecutionPolicy (maxJobs) {}

                /// \brief
                /// Enqueue a job on the given Pipelines pendingJobs to be performed
                /// on the pipeline thread.
                /// \param[in] pipeline Pipeline on which to enqueue the given job.
                /// \param[in] job Job to enqueue.
                virtual void EnqJob (
                    State &state,
                    Job *job) override;
                /// \brief
                /// Enqueue a job on the given Pipelines pendingJobs to be performed
                /// next on the pipeline thread.
                /// \param[in] pipeline Pipeline on which to enqueue the given job.
                /// \param[in] job Job to enqueue.
                virtual void EnqJobFront (
                    State &state,
                    Job *job) override;
                /// \brief
                /// Dequeue the next job to be executed on the pipeline thread.
                /// \param[in] pipeline Pipeline from which to dequeue the next job.
                /// \return The next job to execute (0 if no more pending jobs).
                virtual Job *DeqJob (State &state) override;
            };

            /// \struct Pipeline::LIFOJobExecutionPolicy Pipeline.h thekogans/util/Pipeline.h
            ///
            /// \brief
            /// Last In, First Out execution policy.
            struct _LIB_THEKOGANS_UTIL_DECL LIFOJobExecutionPolicy : public JobExecutionPolicy {
                /// \brief
                /// ctor.
                /// \param[in] maxJobs Max pending pipeline jobs.
                LIFOJobExecutionPolicy (std::size_t maxJobs = SIZE_T_MAX) :
                    JobExecutionPolicy (maxJobs) {}

                /// \brief
                /// Enqueue a job on the given Pipelines pendingJobs to be performed
                /// on the pipeline thread.
                /// \param[in] pipeline Pipeline on which to enqueue the given job.
                /// \param[in] job Job to enqueue.
                virtual void EnqJob (
                    State &state,
                    Job *job) override;
                /// \brief
                /// Enqueue a job on the given Pipelines pendingJobs to be performed
                /// next on the pipeline thread.
                /// \param[in] pipeline Pipeline on which to enqueue the given job.
                /// \param[in] job Job to enqueue.
                virtual void EnqJobFront (
                    State &state,
                    Job *job) override;
                /// \brief
                /// Dequeue the next job to be executed on the pipeline thread.
                /// \param[in] pipeline Pipeline from which to dequeue the next job.
                /// \return The next job to execute (0 if no more pending jobs).
                virtual Job *DeqJob (State &state) override;
            };

        #if defined (TOOLCHAIN_COMPILER_cl)
            #pragma warning (push)
            #pragma warning (disable : 4275)
        #endif // defined (TOOLCHAIN_COMPILER_cl)
            /// \struct Pipeline::Job Pipeline.h thekogans/util/Pipeline.h
            ///
            /// \brief
            /// A pipeline job. Since a pipeline is a collection
            /// of \see{JobQueue}s, the Pipeline::Job derives form
            /// \see{RunLoop::Job}. \see{RunLoop::Job::SetState}
            /// is used to shepherd the job down the pipeline.
            /// Pipeline::Job Begin and End are provided to perform
            /// one time initialization/tear down.
            struct _LIB_THEKOGANS_UTIL_DECL Job :
                    public RunLoop::Job,
                    public JobList::Node {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Job)

            protected:
                /// \brief
                /// Pipeline on which this job is staged.
                RefCounted::SharedPtr<Pipeline::State> pipeline;
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
                explicit Job (Pipeline::SharedPtr pipeline_);

                /// \brief
                /// Return the pipeline id on which this job can run.
                /// \return Pipeline id on which this job can run.
                const RunLoop::Id &GetPipelineId () const;

            protected:
                /// \brief
                /// Used internally by Pipeline to set the RunLoop id and reset
                /// the state, disposition and completed.
                /// \param[in] runLoopId_ Pipeline id to which this job belongs.
                virtual void Reset (const RunLoop::Id &runLoopId_) override;
                /// \brief
                /// Used internally by Pipeline and it's derivatives to set the
                /// job state.
                /// \param[in] state_ New job state.
                virtual void SetState (State state_) override;

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
                virtual void Begin (const std::atomic<bool> &done) throw () {}
                /// \brief
                /// Provides the same functionality as
                /// Job::Epilogue, except at pipeline
                /// global level.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void End (const std::atomic<bool> &done) throw () {}

                /// \brief
                /// Pipeline uses Reset.
                friend struct Pipeline;

                /// \brief
                /// Job is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Job)
            };
        #if defined (TOOLCHAIN_COMPILER_cl)
            #pragma warning (pop)
        #endif // defined (TOOLCHAIN_COMPILER_cl)

            /// \struct Pipeline::LambdaJob Pipeline.h thekogans/util/Pipeline.h
            ///
            /// \brief
            /// A helper class used to execute lambda (function) jobs. If you want to
            /// skip a stage, pass \see{RunLoop::LambdaJob::Function} () instead of a
            /// closure for that slot.

            struct _LIB_THEKOGANS_UTIL_DECL LambdaJob : public Job {
                /// \brief
                /// Convenient typedef for std::function<void (Job & /*job*/, const std::atomic<bool> & /*done*/)>.
                /// \param[in] job Job that is executing the lambda.
                /// \param[in] done Call job.ShouldStop (done) to respond to cancel requests and termination events.
                typedef std::function<
                    void (
                        LambdaJob & /*job*/,
                        const std::atomic<bool> & /*done*/)> Function;

            private:
                /// \brief
                /// Lambdas to execute.
                std::vector<Function> functions;

            public:
                /// \brief
                /// ctor.
                /// \param[in] pipeline Pipeline that will execute this job.
                /// \param[in] begin First lambda in the array.
                /// \param[in] end Just past the last lambda in the array.
                LambdaJob (
                    Pipeline::SharedPtr pipeline,
                    const Function *&begin,
                    const Function *&end) :
                    Job (pipeline),
                    functions (begin, end) {}

                /// \brief
                /// If our run loop is still running, execute the lambda function.
                /// \param[in] done true == The run loop is done and nothing can be executed on it.
                virtual void Execute (const std::atomic<bool> &done) throw () override {
                    if (!ShouldStop (done) && functions[stage] != 0) {
                        functions[stage] (*this, done);
                    }
                }
            };

            /// \struct Pipeline::Stage Pipeline.h thekogans/util/Pipeline.h
            ///
            /// \brief
            /// Used to specify stage (\see{JobQueue}) ctor parameters.
            struct _LIB_THEKOGANS_UTIL_DECL Stage {
                /// \brief
                /// Stage \see{JobQueue} name.
                std::string name;
                /// \brief
                /// Stage \see{JobQueue} \see{RunLoop::JobExecutionPolicy}.
                RunLoop::JobExecutionPolicy::SharedPtr jobExecutionPolicy;
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
                /// \param[in] jobExecutionPolicy_ Stage \see{JobQueue} \see{RunLoop::JobExecutionPolicy}.
                /// \param[in] workerCount_ Number of workers servicing this stage.
                /// \param[in] workerPriority_ Stage worker thread priority.
                /// \param[in] workerAffinity_ Stage worker thread processor affinity.
                /// \param[in] workerCallback_ Called to initialize/uninitialize the stage worker thread(s).
                Stage (
                    const std::string &name_ = std::string (),
                    RunLoop::JobExecutionPolicy::SharedPtr jobExecutionPolicy_ =
                        new RunLoop::FIFOJobExecutionPolicy,
                    std::size_t workerCount_ = 1,
                    i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                    ui32 workerAffinity_ = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                    RunLoop::WorkerCallback *workerCallback_ = nullptr) :
                    name (name_),
                    jobExecutionPolicy (jobExecutionPolicy_),
                    workerCount (workerCount_),
                    workerPriority (workerPriority_),
                    workerAffinity (workerAffinity_),
                    workerCallback (workerCallback_) {}
            };

            /// \struct RunLoop::State RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// Shared RunLoop state. RunLoop lifetime is unpredictable. RunLoops can
            /// be embedded in other objects making them difficult to use in multi-threaded
            /// environment (worker threads). A RunLoop state will always be allocated on
            /// the heap making managing it's lifetime a lot easier.
            struct _LIB_THEKOGANS_UTIL_DECL State : public virtual RefCounted {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (State)

                /// \brief
                /// State has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                /// \brief
                /// Pipeline id.
                const RunLoop::Id id;
                /// \brief
                /// Pipeline name.
                const std::string name;
                /// \brief
                /// Pipeline job execution policy.
                JobExecutionPolicy::SharedPtr jobExecutionPolicy;
                /// \brief
                /// Flag to signal the stage thread(s).
                std::atomic<bool> done;
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
                    State::SharedPtr state;

                public:
                    /// \brief
                    /// ctor.
                    /// \param[in] state_ Pipeline to which this worker belongs.
                    /// \param[in] name Worker thread name.
                    Worker (State::SharedPtr state_,
                            const std::string &name = std::string ()) :
                            Thread (name),
                            state (state_) {
                        Create (state->workerPriority, state->workerAffinity);
                    }

                private:
                    // Thread
                    /// \brief
                    /// Worker thread.
                    virtual void Run () throw () override;
                };
                /// \brief
                /// List of workers.
                WorkerList workers;
                /// \brief
                /// Pipeline stages.
                std::vector<JobQueue::SharedPtr> stages;
                /// \brief
                /// Synchronization mutex.
                Mutex workersMutex;

                /// \brief
                /// ctor.
                /// \param[in] begin Pointer to the beginning of the Stage array.
                /// \param[in] end Pointer to the end of the Stage array.
                /// \param[in] name_ Pipeline name.
                /// \param[in] jobExecutionPolicy_ Pipeline \see{JobExecutionPolicy}.
                /// \param[in] workerCount_ Max workers to service the pipeline.
                /// \param[in] workerPriority_ Worker thread priority.
                /// \param[in] workerAffinity_ Worker thread processor affinity.
                /// \param[in] workerCallback_ Called to initialize/uninitialize
                /// the worker thread.
                State (
                    const Stage *begin,
                    const Stage *end,
                    const std::string &name_ = std::string (),
                    JobExecutionPolicy::SharedPtr jobExecutionPolicy_ = new FIFOJobExecutionPolicy,
                    std::size_t workerCount_ = 1,
                    i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                    ui32 workerAffinity_ = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                    RunLoop::WorkerCallback *workerCallback_ = nullptr);
                /// \brief
                /// dtor.
                virtual ~State ();

                /// \brief
                /// Used internally by worker(s) to get the next job.
                /// \param[in] wait true == Wait until a job becomes available.
                /// \return The next job to execute.
                Job *DeqJob (bool wait = true);
                /// \brief
                /// Called by worker(s) after each job is completed.
                /// Used to update state and \see{RunLoop::Stats}.
                /// \param[in] job Completed job.
                /// \param[in] start Completed job start time.
                /// \param[in] end Completed job end time.
                void FinishedJob (
                    Job *job,
                    ui64 start,
                    ui64 end);
            };

        protected:
            /// \brief
            /// Shared Pipeline state.
            State::SharedPtr state;

        public:
            /// \brief
            /// ctor.
            /// \param[in] begin Pointer to the beginning of the Stage array.
            /// \param[in] end Pointer to the end of the Stage array.
            /// \param[in] name Pipeline name.
            /// \param[in] jobExecutionPolicy Pipeline \see{JobExecutionPolicy}.
            /// \param[in] workerCount Max workers to service the pipeline.
            /// \param[in] workerPriority Worker thread priority.
            /// \param[in] workerAffinity Worker thread processor affinity.
            /// \param[in] workerCallback Called to initialize/uninitialize
            /// the worker thread.
            Pipeline (
                    const Stage *begin,
                    const Stage *end,
                    const std::string &name = std::string (),
                    JobExecutionPolicy::SharedPtr jobExecutionPolicy = new FIFOJobExecutionPolicy,
                    std::size_t workerCount = 1,
                    i32 workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                    ui32 workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                    RunLoop::WorkerCallback *workerCallback = nullptr) :
                    state (
                        new State (
                            begin,
                            end,
                            name,
                            jobExecutionPolicy,
                            workerCount,
                            workerPriority,
                            workerAffinity,
                            workerCallback)) {
                Start ();
            }
            /// \brief
            /// dtor. Stop the pipeline.
            virtual ~Pipeline () {
                Stop ();
            }

            /// \brief
            /// Return Pipeline id.
            /// \return Pipeline id.
            inline const RunLoop::Id &GetId () const {
                return state->id;
            }
            /// \brief
            /// Return Pipeline name.
            /// \return Pipeline name.
            inline const std::string &GetName () const {
                return state->name;
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
            void Stop (
                bool cancelRunningJobs = true,
                bool cancelPendingJobs = true);
            /// \brief
            /// Return true if the pipeline is running (Start was called).
            /// \return true if the pipeline is running (Start was called).
            bool IsRunning ();

            /// \brief
            /// Enqueue a job on the pipeline.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == !wait || WaitForJob (...)
            bool EnqJob (
                Job::SharedPtr job,
                bool wait = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Enqueue a lambda (function) to be performed by the pipeline stages.
            /// \param[in] begin First lambda in the array.
            /// \param[in] end Just past the last lambda in the array.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// NOTE: Same constraint applies to EnqJob as Stop. Namely, you can't call EnqJob
            /// from the same thread that called Start.
            /// \return std::pair<Job::SharedPtr, bool> containing the LambdaJob and the EnqJob return.
            std::pair<Job::SharedPtr, bool> EnqJob (
                const LambdaJob::Function *&begin,
                const LambdaJob::Function *&end,
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
                Job::SharedPtr job,
                bool wait = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Enqueue a lambda (function) to be performed next by the pipeline stages.
            /// \param[in] begin First lambda in the array.
            /// \param[in] end Just past the last lambda in the array.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// NOTE: Same constraint applies to EnqJob as Stop. Namely, you can't call EnqJob
            /// from the same thread that called Start.
            /// \return std::pair<Job::SharedPtr, bool> containing the LambdaJob and the EnqJobFront return.
            std::pair<Job::SharedPtr, bool> EnqJobFront (
                const LambdaJob::Function *&begin,
                const LambdaJob::Function *&end,
                bool wait = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Get a running or a pending job with the given id.
            /// \param[in] jobId Id of job to retrieve.
            /// \return Job matching the given id.
            Job::SharedPtr GetJob (const Job::Id &jobId);
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
                Job::SharedPtr job,
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
            /// ctor.
            /// \param[in] state_ Shared Pipeline state.
            explicit Pipeline (State::SharedPtr state_);

            /// \brief
            /// Pipeline is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Pipeline)
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
        /// IMPORTANT: Don't forget to call GlobalPipeline::CreateInstance
        /// before the first call to GlobalPipeline::Instance to provide
        /// the global pipeline stages.

        struct _LIB_THEKOGANS_UTIL_DECL GlobalPipeline :
                public Pipeline,
                public Singleton<
                    GlobalPipeline,
                    SpinLock,
                    RefCountedInstanceCreator<GlobalPipeline>,
                    RefCountedInstanceDestroyer<GlobalPipeline>> {
            /// \brief
            /// Create a global pipeline with custom ctor arguments.
            /// \param[in] begin Pointer to the beginning of the Pipeline::Stage array.
            /// \param[in] end Pointer to the end of the Pipeline::Stage array.
            /// \param[in] name Pipeline name. If set, \see{Pipeline::Worker}
            /// threads will be named name-%d.
            /// \param[in] jobExecutionPolicy Pipeline \see{Pipeline::JobExecutionPolicy}.
            /// \param[in] workerCount Max workers to service the pipeline.
            /// \param[in] workerPriority Worker thread priority.
            /// \param[in] workerAffinity Worker thread processor affinity.
            /// \param[in] workerCallback Called to initialize/uninitialize the worker thread.
            GlobalPipeline (
                const Pipeline::Stage *begin = nullptr,
                const Pipeline::Stage *end = nullptr,
                const std::string &name = "GlobalPipeline",
                Pipeline::JobExecutionPolicy::SharedPtr jobExecutionPolicy =
                    new Pipeline::FIFOJobExecutionPolicy,
                std::size_t workerCount = 1,
                i32 workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                RunLoop::WorkerCallback *workerCallback = nullptr) :
                Pipeline (
                    begin,
                    end,
                    name,
                    jobExecutionPolicy,
                    workerCount,
                    workerPriority,
                    workerAffinity,
                    workerCallback) {}
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Pipeline_h)
