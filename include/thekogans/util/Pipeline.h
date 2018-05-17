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
        /// A Pipeline builds on the JobQueue to provide staged
        /// execution. Think of an assembly line where each station
        /// (pipeline stage) performs a specific task, and passes the
        /// job on to the next stage (this is how modern processor
        /// architectures perform scalar, and even super-scalar
        /// execution).

        struct _LIB_THEKOGANS_UTIL_DECL Pipeline : public ThreadSafeRefCounted {
            /// \brief
            /// Convenient typedef for ThreadSafeRefCounted::Ptr<Pipeline>.
            typedef ThreadSafeRefCounted::Ptr<Pipeline> Ptr;

            /// \struct Pipeline::Stage Pipeline.h thekogans/util/Pipeline.h
            ///
            /// \brief
            /// Used to specify stage parameters.
            struct _LIB_THEKOGANS_UTIL_DECL Stage {
                /// \brief
                /// Stage \see{JobQueue} name.
                std::string name;
                /// \brief
                /// Type of stage (TYPE_FIFO or TYPE_LIFO).
                RunLoop::Type type;
                /// \brief
                /// Max pending jobs.
                ui32 maxPendingJobs;
                /// \brief
                /// Count of workers servicing this stage.
                ui32 workerCount;
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
                /// \param[in] workerCallback_ Called to initialize/uninitialize the worker thread.
                Stage (
                    const std::string &name_ = std::string (),
                    RunLoop::Type type_ = RunLoop::TYPE_FIFO,
                    ui32 maxPendingJobs_ = UI32_MAX,
                    ui32 workerCount_ = 1,
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
                JOB_LIST_ID = RunLoop::LAST_JOB_LIST_ID
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
            /// of JobQueues, the Pipeline::Job derives form
            /// RunLoop::Job. RunLoop::Job::SetStatus is used
            /// to shepherd the job down the pipeline.
            /// Pipeline::Job Begin and End are provided
            /// to perform one time initialization/tear down.
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
                /// the status, disposition and completed.
                /// \param[in] runLoopId_ RunLoop id to which this job belongs.
                virtual void Reset (const RunLoop::Id &runLoopId_);
                /// \brief
                /// Used internally by RunLoop and it's derivatives to set the
                /// job status.
                /// \param[in] status_ New job status.
                virtual void SetStatus (Status status_);

                /// \brief
                /// Provides the same functionality as
                /// Job::Prologue, except at pipeline
                /// global level.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void Begin (volatile const bool &done) throw () {}
                /// \brief
                /// Provides the same functionality as
                /// Job::Epilogue, except at pipeline
                /// global level.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void End (volatile const bool &done) throw () {}

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
            const ui32 maxPendingJobs;
            /// \brief
            /// Flag to signal the stage thread(s).
            volatile bool done;
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
            /// Synchronization event.
            Condition idle;
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
                /// Pipeline to which this worker belongs.
                Pipeline &pipeline;

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
            /// \brief
            /// Pipeline stages.
            std::vector<JobQueue::Ptr> stages;
            /// \brief
            /// Synchronization mutex.
            Mutex stagesMutex;

        public:
            /// \brief
            /// ctor.
            /// \param[in] name_ Pipeline name.
            /// \param[in] type_ Pipeline type.
            /// \param[in] maxPendingJobs_ Max pending pipeline jobs.
            /// \param[in] workerCount_ Max workers to service the pipeline.
            /// \param[in] workerPriority_ Worker thread priority.
            /// \param[in] workerAffinity_ Worker thread processor affinity.
            /// \param[in] workerCallback_ Called to initialize/uninitialize
            /// the worker thread.
            /// \param[in] begin Pointer to the beginning of the Stage array.
            /// \param[in] end Pointer to the end of the Stage array.
            Pipeline (
                const std::string &name_ = std::string (),
                RunLoop::Type type_ = RunLoop::TYPE_FIFO,
                ui32 maxPendingJobs_ = UI32_MAX,
                ui32 workerCount_ = 1,
                i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity_ = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                RunLoop::WorkerCallback *workerCallback_ = 0,
                const Stage *begin = 0,
                const Stage *end = 0);

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
            /// Wait until the given pipeline is created the and it starts running.
            /// \param[in] pipeline Pipeline to wait for.
            /// \param[in] sleepTimeSpec How long to sleep between tries.
            /// \param[in] waitTimeSpec Total time to wait.
            /// \return true == the given pipeline is running.
            /// false == timed out waiting for the pipeline to start.
            static bool WaitForStart (
                Ptr &pipeline,
                const TimeSpec &sleepTimeSpec = TimeSpec::FromMilliseconds (50),
                const TimeSpec &waitTimeSpec = TimeSpec::FromSeconds (3));

            /// \brief
            /// Add a stage to the pipeline.
            /// \param[in] stage Stage to add.
            void AddStage (const Stage &stage);
            /// \brief
            /// Return the stats for a given pipeline stage.
            /// \return Stats corresponding to the given pipeline stage.
            RunLoop::Stats GetStageStats (std::size_t stage);
            /// \brief
            /// Return the stats for all pipeline stages.
            /// \param[out] stats Vector where stage stats will be placed.
            void GetStagesStats (std::vector<RunLoop::Stats> &stats);

            /// \brief
            /// Start the pipeline. Create stages, and start waiting for jobs.
            void Start ();
            /// \brief
            /// Stop the pipeline. All stages are destroyed.
            /// \param[in] cancelRunningJobs true = Cancel all running jobs.
            void Stop (bool cancelRunningJobs = true);

            /// \brief
            /// Enqueue a job on a pipeline stage.
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
            /// Get a running job with the given id.
            /// \param[in] jobId Id of job to retrieve.
            /// \return Job matching the given id.
            Job::Ptr GetJobWithId (const Job::Id &jobId);

            /// \brief
            /// Wait for a given job to complete.
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
            /// Wait for a running job with a given id to complete.
            /// \param[in] jobId Id of job to wait on.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == completed,
            /// false == job with a given id was not in the queue or timed out.
            bool WaitForJob (
                const Job::Id &jobId,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Wait for all running jobs matching the given equality test to complete.
            /// \param[in] equalityTest EqualityTest to query to determine which jobs to wait on.
            /// \param[in] timeSpec How long to wait for the jobs to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == All jobs satisfying the equalityTest completed,
            /// false == One or more matching jobs timed out.
            bool WaitForJobs (
                const RunLoop::EqualityTest &equalityTest,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Blocks until all jobs are complete and the queue is empty.
            /// \param[in] timeSpec How long to wait for the jobs to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == Pipeline is idle, false == Timed out.
            bool WaitForIdle (
                const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Cancel a running job with a given id.
            /// \param[in] jobId Id of job to cancel.
            /// \return true if the job was cancelled.
            bool CancelJob (const Job::Id &jobId);
            /// \brief
            /// Cancel all running jobs matching the given equality test.
            /// \param[in] equalityTest EqualityTest to query to determine which jobs to cancel.
            void CancelJobs (const RunLoop::EqualityTest &equalityTest);
            /// \brief
            /// Cancel all running jobs.
            void CancelAllJobs ();

            /// \brief
            /// Return a snapshot of the run loop stats.
            /// \return A snapshot of the run loop stats.
            RunLoop::Stats GetStats ();

            /// \brief
            /// Return true if Start was called.
            /// \return true if Start was called.
            bool IsRunning ();
            /// \brief
            /// Return true if there are no running jobs and the
            /// stages are idle.
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
            /// Atomically set done to the given value.
            /// \param[in] value Value to set done to.
            /// \return true == done was set to the given value.
            /// false == done was already set to the given value.
            bool SetDone (bool value);

            /// \brief
            /// Pipeline is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Pipeline)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Pipeline_h)
