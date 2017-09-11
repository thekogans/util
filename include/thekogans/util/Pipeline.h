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

#include <memory>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/AbstractOwnerVector.h"
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

        struct _LIB_THEKOGANS_UTIL_DECL Pipeline {
            /// \struct Pipeline::Job Pipeline.h thekogans/util/Pipeline.h
            ///
            /// \brief
            /// A pipeline job. Since a pipeline is a collection
            /// of JobQueues, the Pipeline::Job derives form
            /// JobQueue::Job. JobQueue::Job Prologue and Epilogue
            /// are used to shepherd the job down the pipeline.
            /// Pipeline::Job Begin and End are used instead
            /// to perform one time initialization/tear down.
            struct _LIB_THEKOGANS_UTIL_DECL Job : public JobQueue::Job {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<Job>.
                typedef ThreadSafeRefCounted::Ptr<Job> Ptr;

                /// \brief
                /// Pipeline on which this job is staged.
                Pipeline &pipeline;
                /// \brief
                /// Current job stage.
                std::size_t stage;

                /// \brief
                /// ctor.
                /// \param[in] pipeline_ Pipeline that will execute this job.
                /// \param[in] stage_ Pipeline stage that will execute this job.
                Job (
                    Pipeline &pipeline_,
                    std::size_t stage_) :
                    pipeline (pipeline_),
                    stage (stage_) {}
                // JobQueue::Job
                /// \brief
                /// JobQueue will call this before Execute.
                /// If Stage == 0 will call Begin.
                /// \param[in] done Check this to see if processing should be aborted.
                /// NOTE: Be careful overriding this method as it is
                /// part of the pipeline protocol. If you do, make
                /// sure to call Pipeline::Job::Protocol somewhere
                /// along the line.
                virtual void Prologue (volatile const bool &done) throw ();
                /// \brief
                /// JobQueue will call this after Execute.
                /// If at the end of the pipeline, call End.
                /// \param[in] done Check this to see if processing should be aborted.
                /// NOTE: Be careful overriding this method as it is
                /// part of the pipeline protocol. If you do, make
                /// sure to call Pipeline::Job::Epilogue somewhere
                /// along the line.
                virtual void Epilogue (volatile const bool &done) throw ();
                /// \brief
                /// Called at the end of each stage to provide
                /// a job for the next stage.
                /// \return Job for next stage (Job::UniquePtr () to end)
                virtual Job::Ptr Clone () throw () = 0;
                /// \brief
                /// These provide the same functionality as
                /// Prologue/Epilogue, except at pipeline
                /// global level.
                /// Called by the pipeline stage before calling Execute.
                virtual void Begin () throw () {}
                /// \brief
                /// These provide the same functionality as
                /// Prologue/Epilogue, except at pipeline
                /// global level.
                /// Called by the pipeline stage after calling Execute.
                virtual void End () throw () {}

                /// \brief
                /// Job is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Job)
            };

        private:
            /// \brief
            /// Pipeline stages.
            AbstractOwnerVector<JobQueue> stages;

        public:
            /// \struct Pipeline::StageInfo Pipeline.h thekogans/util/Pipeline.h
            ///
            /// \brief
            /// Used to specify each stage parameters individually.
            struct _LIB_THEKOGANS_UTIL_DECL StageInfo {
                /// \brief
                /// Stage \see{JobQueue} name.
                std::string name;
                /// \brief
                /// Type of stage (TYPE_FIFO or TYPE_LIFO).
                JobQueue::Type type;
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
                /// ctor.
                /// \param[in] name_ Stage \see{JobQueue} name.
                /// \param[in] type_ Stage \see{JobQueue} type.
                /// \param[in] workerCount_ Number of workers servicing this stage.
                /// \param[in] workerPriority_ Stage worker thread priority.
                /// \param[in] workerAffinity_ Stage worker thread processor affinity.
                StageInfo (
                    const std::string &name_ = std::string (),
                    JobQueue::Type type_ = JobQueue::TYPE_FIFO,
                    ui32 workerCount_ = 1,
                    i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                    ui32 workerAffinity_ = UI32_MAX) :
                    name (name_),
                    type (type_),
                    workerCount (workerCount_),
                    workerPriority (workerPriority_),
                    workerAffinity (workerAffinity_) {}
            };

            /// \brief
            /// ctor.
            /// \param[in] begin Pointer to the beginning of the StageInfo array.
            /// \param[in] end Pointer to the end of the StageInfo array.
            Pipeline (
                const StageInfo *begin,
                const StageInfo *end);
            /// \enum
            /// NOTE: Windows thread priorities are not integral
            /// values (they are tokens). And as such, cannot be
            /// arithmetically adjusted. On Windows, the last two
            /// arguments will be ignored.
            enum StagePriorityAdjustment {
                /// \brief
                /// No priority adjustment.
                NoAdjustment,
                /// \brief
                /// Each successive stage will get an elevated priority.
                Inc,
                /// \brief
                /// Each successive stage priority will be elevated by delta.
                Add,
                /// \brief
                /// Each successive stage will get an de-elevated priority.
                Dec,
                /// \brief
                /// Each successive stage priority will be de-elevated by delta.
                Sub
            };
            /// \brief
            /// ctor.
            /// \param[in] stageCount Number of stages in the pipeline.
            /// \param[in] stageName Stage name.
            /// \param[in] stageType Stage type (fifo | lifo).
            /// \param[in] stageWorkerCount Number of workers servicing each stage.
            /// \param[in] stageWorkerPriority Stage worker thread priority.
            /// \param[in] stagePriorityAdjustment How to adjust priorities for each successive stage.
            /// \param[in] stagePriorityAdjustmentDelta Add/Sub adjustment delta.
            /// \param[in] stageWorkerAffinity Stage worker thread processor affinity.
            Pipeline (
                ui32 stageCount,
                const std::string &stageName = std::string (),
                JobQueue::Type stageType = JobQueue::TYPE_FIFO,
                ui32 stageWorkerCount = 1,
                i32 stageWorkerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                StagePriorityAdjustment stagePriorityAdjustment = NoAdjustment,
                i32 stagePriorityAdjustmentDelta = 0,
                ui32 stageWorkerAffinity = UI32_MAX);

            /// \brief
            /// Start the pipeline. Create stages, and start waiting for jobs.
            void Start ();
            /// \brief
            /// Stop the pipeline. All stages are destroyed.
            void Stop ();

            /// \brief
            /// Enqueue a job on a pipeline stage.
            /// \param[in] job Job to enqueue.
            /// \param[in] stage Stage to enqueue the job on.
            void Enq (
                Job &job,
                std::size_t stage = 0);

            /// \brief
            /// Cancel all waiting jobs on all pipeline stages.
            /// Jobs in flight are not canceled.
            void Flush ();

            /// \brief
            /// Return the stats for each stage of the pipeline.
            /// \param[out] stats Vector where stage stats will be placed.\see{JobQueue}
            void GetStats (std::vector<JobQueue::Stats> &stats);

            /// \brief
            /// Blocks until all jobs are complete and the pipeline is empty.
            void WaitForIdle ();

            /// \brief
            /// Pipeline is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Pipeline)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Pipeline_h)
