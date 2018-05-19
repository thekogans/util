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

#if !defined (__thekogans_util_RunLoopScheduler_h)
#define __thekogans_util_RunLoopScheduler_h

#include <queue>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/Timer.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/RunLoop.h"
#include "thekogans/util/MainRunLoop.h"
#include "thekogans/util/Pipeline.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/GUID.h"

namespace thekogans {
    namespace util {

        /// \struct RunLoopScheduler RunLoopScheduler.h thekogans/util/RunLoopScheduler.h
        ///
        /// \brief
        /// A RunLoopScheduler allows you to schedule \see{RunLoop::Job}s and \see{Pipeline::Job}s
        /// to be executed in the future.

        struct _LIB_THEKOGANS_UTIL_DECL RunLoopScheduler :
                public ThreadSafeRefCounted,
                public Timer::Callback {
            /// \brief
            /// Convenient typedef for ThreadSafeRefCounted::Ptr<RunLoopScheduler>.
            typedef ThreadSafeRefCounted::Ptr<RunLoopScheduler> Ptr;

        private:
            /// \brief
            /// \see{Timer} used to schedule future jobs.
            Timer timer;
            /// \struct RunLoopScheduler::JobInfo RunLoopScheduler.h thekogans/util/RunLoopScheduler.h
            ///
            /// \brief
            /// Base class for information about a future job to be scheduled on
            /// the given \see{RunLoop} or \see{Pipeline}.
            struct JobInfo : public ThreadSafeRefCounted {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<JobInfo>.
                typedef ThreadSafeRefCounted::Ptr<JobInfo> Ptr;

                /// \brief
                /// \see{RunLoop::Job} or \see{Pipeline::Job} that will be scheduled.
                RunLoop::Job::Ptr job;
                /// \brief
                /// Absolute time when the job will be scheduled.
                TimeSpec deadline;

                /// \brief
                /// ctor.
                /// \param[in] job_ \see{RunLoop::Job} or \see{Pipeline::Job} that will be scheduled.
                /// \param[in] deadline_ Absolute time when the job will be scheduled.
                JobInfo (
                    RunLoop::Job::Ptr job_,
                    const TimeSpec &deadline_) :
                    job (job_),
                    deadline (deadline_) {}
                virtual ~JobInfo () {}

                /// \struct RunLoopScheduler::JobInfo::Compare RunLoopScheduler.h thekogans/util/RunLoopScheduler.h
                ///
                /// \brief
                /// Used by the std::priority_queue to sort the jobs.
                struct Compare {
                    /// \brief
                    /// JobInfo comparison operator.
                    /// \param[in] jobInfo1 First JobInfo to compare.
                    /// \param[in] jobInfo2 Second JobInfo to compare.
                    /// \return true if jobInfo1 should come before jobInfo2.
                    inline bool operator () (
                            const JobInfo::Ptr &jobInfo1,
                            const JobInfo::Ptr &jobInfo2) {
                        return jobInfo1->deadline > jobInfo2->deadline;
                    }
                } static compare;

                /// \brief
                /// Return the id associated with this \see{RunLoop} or \see{Pipeline}.
                /// \return Id associated with this \see{RunLoop} or \see{Pipeline}.
                virtual const RunLoop::Id &GetRunLoopId () const = 0;

                /// \brief
                /// Enqueue the job on the specified run loop.
                virtual void EnqJob () = 0;
            };
            /// \struct RunLoopScheduler::RunLoopJobInfo RunLoopScheduler.h thekogans/util/RunLoopScheduler.h
            ///
            /// \brief
            /// Holds information about a future job to be scheduled on the given \see{RunLoop}.
            struct RunLoopJobInfo : public JobInfo {
                /// \brief
                /// RunLoopJobInfo has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (RunLoopJobInfo, SpinLock)

                /// \brief
                /// \see{RunLoop} the job will be scheduled on.
                RunLoop &runLoop;

                /// \brief
                /// ctor.
                /// \param[in] job \see{RunLoop::Job} that will be scheduled.
                /// \param[in] deadline Absolute time when the job will be scheduled.
                /// \param[in] runLoop_ \see{RunLoop} the job will be scheduled on.
                RunLoopJobInfo (
                    RunLoop::Job::Ptr job,
                    const TimeSpec &deadline,
                    RunLoop &runLoop_) :
                    JobInfo (job, deadline),
                    runLoop (runLoop_) {}

                /// \brief
                /// Return the id associated with this \see{RunLoop}.
                /// \return Id associated with this \see{RunLoop}.
                virtual const RunLoop::Id &GetRunLoopId () const {
                    return runLoop.GetId ();
                }

                /// \brief
                /// Enqueue the job on the specified run loop.
                virtual void EnqJob () {
                    runLoop.EnqJob (job);
                }

                /// \brief
                /// RunLoopJobInfo is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (RunLoopJobInfo)
            };
            /// \struct RunLoopScheduler::PipelineJobInfo RunLoopScheduler.h thekogans/util/RunLoopScheduler.h
            ///
            /// \brief
            /// Holds information about a future job to be scheduled on the given \see{Pipeline}.
            struct PipelineJobInfo : public JobInfo {
                /// \brief
                /// PipelineJobInfo has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (PipelineJobInfo, SpinLock)

                /// \brief
                /// \see{Pipeline} the job will be scheduled on.
                Pipeline &pipeline;

                /// \brief
                /// ctor.
                /// \param[in] job \see{Pipeline::Job} that will be scheduled.
                /// \param[in] deadline Absolute time when the job will be scheduled.
                /// \param[in] pipeline_ \see{Pipeline} the job will be scheduled on.
                PipelineJobInfo (
                    Pipeline::Job::Ptr job,
                    const TimeSpec &deadline,
                    Pipeline &pipeline_) :
                    JobInfo (RunLoop::Job::Ptr (job.Get ()), deadline),
                    pipeline (pipeline_) {}

                /// \brief
                /// Return the id associated with this \see{Pipeline}.
                /// \return Id associated with this \see{Pipeline}.
                virtual const RunLoop::Id &GetRunLoopId () const {
                    return pipeline.GetId ();
                }

                /// \brief
                /// Enqueue the job on the specified pipeline.
                virtual void EnqJob () {
                    pipeline.EnqJob (dynamic_refcounted_pointer_cast<Pipeline::Job> (job));
                }

                /// \brief
                /// PipelineJobInfo is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (PipelineJobInfo)
            };
            /// \brief
            /// Convenient typedef for
            /// std::priority_queue<JobInfo::Ptr, std::vector<JobInfo::Ptr>, JobInfo::Compare>.
            typedef std::priority_queue<
                JobInfo::Ptr,
                std::vector<JobInfo::Ptr>,
                JobInfo::Compare> QueueType;
            /// \struct RunLoopScheduler::Queue RunLoopScheduler.h thekogans/util/RunLoopScheduler.h
            ///
            /// \brief
            /// Priority queue used for job scheduling.
            struct Queue : public QueueType {
                /// \brief
                /// ctor.
                Queue () :
                    QueueType (JobInfo::compare) {}

                /// \brief
                /// Cancel the job associated with the given job id.
                /// \param[in] id JobInfo id to cancel.
                void CancelJob (const RunLoop::Job::Id &id);
                /// \brief
                /// Cancel all pending jobs associated with the given \see{RunLoop}.
                /// \param[in] runLoop \see{RunLoop} whose jobs to cancel.
                void CancelJobs (const RunLoop::Id &runLoopId);
            } queue;
            /// \brief
            /// Synchronization spin lock.
            SpinLock spinLock;

        public:
            /// \brief
            /// ctor.
            RunLoopScheduler () :
                timer (*this) {}
            /// \brief
            /// dtor.
            ~RunLoopScheduler () {
                CancelAllJobs ();
            }

            /// \brief
            /// Schedule a job to be performed in the future.
            /// \param[in] job \see{RunLoop::Job} to execute.
            /// \param[in] timeSpec When in the future to execute the given job.
            /// IMPORTANT: timeSpec is a relative value.
            /// \param[in] runLoop \see{RunLoop} that will execute the job.
            /// \return RunLoop::Job::Id which can be used in a call to CancelJob.
            inline RunLoop::Job::Id ScheduleRunLoopJob (
                    RunLoop::Job::Ptr job,
                    const TimeSpec &timeSpec,
                    RunLoop &runLoop = MainRunLoop::Instance ()) {
                return ScheduleJobInfo (
                    JobInfo::Ptr (
                        new RunLoopJobInfo (
                            job,
                            GetCurrentTime () + timeSpec,
                            runLoop)),
                    timeSpec);
            }
            /// \brief
            /// Schedule a job to be performed in the future.
            /// \param[in] job \see{Pipeline::Job} to execute.
            /// \param[in] timeSpec When in the future to execute the given job.
            /// IMPORTANT: timeSpec is a relative value.
            /// \param[in] pipeline \see{Pipeline} that will execute the job.
            /// \return RunLoop::Job::Id which can be used in a call to CancelJob.
            inline Pipeline::Job::Id SchedulePipelineJob (
                    Pipeline::Job::Ptr job,
                    const TimeSpec &timeSpec,
                    Pipeline &pipeline) {
                return ScheduleJobInfo (
                    JobInfo::Ptr (
                        new PipelineJobInfo (
                            job,
                            GetCurrentTime () + timeSpec,
                            pipeline)),
                    timeSpec);
            }

            /// \brief
            /// Cancel the job associated with the given job id.
            /// \param[in] id Job id to cancel.
            void CancelJob (const RunLoop::Job::Id &id);
            /// \brief
            /// Cancel all pending jobs associated with the given \see{RunLoop}.
            /// IMPORTANT: RunLoopJobInfo holds on to the \see{RunLoop} reference.
            /// Use this member to cancel all \see{RunLoop} jobs before that
            /// \see{RunLoop} goes out of scope.
            /// \param[in] runLoop \see{RunLoop} whose jobs to cancel.
            void CancelJobs (const RunLoop::Id &runLoopId);
            /// \brief
            /// Remove all pendig jobs.
            void CancelAllJobs ();

        private:
            // Timer::Callback
            /// \brief
            /// Called every time the timer fires.
            /// \param[in] timer Timer that fired.
            virtual void Alarm (Timer & /*timer*/) throw ();

            /// \brief
            /// Schedule helper.
            /// \param[in] jobInfo JobInfo containing \see{RunLoop::Job} or \see{Pipeline} particulars.
            /// \param[in] timeSpec When in the future to execute the given job.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return JobInfo::Id which can be used in a call to Cancel.
            RunLoop::Job::Id ScheduleJobInfo (
                JobInfo::Ptr jobInfo,
                const TimeSpec &timeSpec);

            /// \brief
            /// RunLoopScheduler is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (RunLoopScheduler)
        };

        /// \struct GlobalRunLoopScheduler RunLoopScheduler.h thekogans/util/RunLoopScheduler.h
        ///
        /// \brief
        /// A global job queue scheduler instance.
        struct _LIB_THEKOGANS_UTIL_DECL GlobalRunLoopScheduler :
            public Singleton<RunLoopScheduler, SpinLock> {};

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RunLoopScheduler_h)
