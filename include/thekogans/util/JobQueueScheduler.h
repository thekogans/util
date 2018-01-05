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

#if !defined (__thekogans_util_JobQueueScheduler_h)
#define __thekogans_util_JobQueueScheduler_h

#include <memory>
#include <queue>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/Timer.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/JobQueue.h"
#include "thekogans/util/RunLoop.h"
#include "thekogans/util/MainRunLoop.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/GUID.h"

namespace thekogans {
    namespace util {

        /// \struct JobQueueScheduler JobQueueScheduler.h thekogans/util/JobQueueScheduler.h
        ///
        /// \brief
        /// A JobQueueScheduler allows you to schedule \see{JobQueue::Job}s to be executed
        /// in the future. Both \see{JobQueue} and \see{RunLoop} are supported.

        struct _LIB_THEKOGANS_UTIL_DECL JobQueueScheduler : public Timer::Callback {
            /// \brief
            /// Convenient typedef for std::shared_ptr<JobQueueScheduler>.
            typedef std::shared_ptr<JobQueueScheduler> SharedPtr;

        private:
            /// \brief
            /// \see{Timer} used to schedule future jobs.
            Timer timer;
            /// \struct JobQueueScheduler::JobInfo JobQueueScheduler.h thekogans/util/JobQueueScheduler.h
            ///
            /// \brief
            /// Abstract base for \see{JobQueue}/\see{RunLoop} jobs.
            struct JobInfo : public ThreadSafeRefCounted {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<JobInfo>.
                typedef ThreadSafeRefCounted::Ptr<JobInfo> Ptr;

                /// \brief
                /// \see{JobQueue::Job} that will be scheduled.
                JobQueue::Job::Ptr job;
                /// \brief
                /// Absolute time when the job will be scheduled.
                TimeSpec deadline;

                /// \brief
                /// ctor.
                /// \param[in] job_ \see{JobQueue::Job} that will be scheduled.
                /// \param[in] deadline_ Absolute time when the job will be scheduled.
                JobInfo (
                    JobQueue::Job &job_,
                    const TimeSpec &deadline_) :
                    job (&job_),
                    deadline (deadline_) {}
                /// \brief
                /// dtor.
                virtual ~JobInfo () {}

                /// \struct JobQueueScheduler::JobInfo::Compare JobQueueScheduler.h thekogans/util/JobQueueScheduler.h
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
                            JobInfo::Ptr jobInfo1,
                            JobInfo::Ptr jobInfo2) {
                        return jobInfo1->deadline > jobInfo2->deadline;
                    }
                } static compare;

                /// \brief
                /// Enqueue the job on the specified job queue.
                virtual void EnqJob () = 0;
            };
            /// \struct JobQueueScheduler::JobQueueJobInfo JobQueueScheduler.h thekogans/util/JobQueueScheduler.h
            ///
            /// \brief
            /// Holds information about a future job to be scheduled on the given \see{JobQueue}.
            struct JobQueueJobInfo : public JobInfo {
                /// \brief
                /// JobQueueJobInfo has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (JobQueueJobInfo, SpinLock)

                /// \brief
                /// \see{JobQueue} the job will be scheduled on.
                JobQueue &jobQueue;

                /// \brief
                /// ctor.
                /// \param[in] job \see{JobQueue::Job} that will be scheduled.
                /// \param[in] deadline Absolute time when the job will be scheduled.
                /// \param[in] jobQueue_ \see{JobQueue} the job will be scheduled on.
                JobQueueJobInfo (
                    JobQueue::Job &job,
                    const TimeSpec &deadline,
                    JobQueue &jobQueue_) :
                    JobInfo (job, deadline),
                    jobQueue (jobQueue_) {}

                /// \brief
                /// Enqueue the job on the specified job queue.
                virtual void EnqJob () {
                    jobQueue.Enq (*job);
                }

                /// \brief
                /// JobQueueJobInfo is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (JobQueueJobInfo)
            };
            /// \struct JobQueueScheduler::RunLoopJobInfo JobQueueScheduler.h thekogans/util/JobQueueScheduler.h
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
                /// \param[in] job \see{JobQueue::Job} that will be scheduled.
                /// \param[in] deadline Absolute time when the job will be scheduled.
                /// \param[in] runLoop_ \see{RunLoop} the job will be scheduled on.
                RunLoopJobInfo (
                    JobQueue::Job &job,
                    const TimeSpec &deadline,
                    RunLoop &runLoop_) :
                    JobInfo (job, deadline),
                    runLoop (runLoop_) {}

                /// \brief
                /// Enqueue the job on the specified run loop.
                virtual void EnqJob () {
                    runLoop.Enq (*job);
                }

                /// \brief
                /// RunLoopJobInfo is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (RunLoopJobInfo)
            };
            /// \brief
            /// Convenient typedef for
            /// std::priority_queue<JobInfo::Ptr, std::vector<JobInfo::Ptr>, JobInfo::Compare>.
            typedef std::priority_queue<
                JobInfo::Ptr,
                std::vector<JobInfo::Ptr>,
                JobInfo::Compare> QueueType;
            /// \struct JobQueueScheduler::Queue JobQueueScheduler.h thekogans/util/JobQueueScheduler.h
            ///
            /// \brief
            /// Priority queue used for job scheduling.
            struct Queue : public QueueType {
                /// \brief
                /// ctor.
                Queue () :
                    QueueType (JobInfo::compare) {}

                /// \brief
                /// Cancel all pending jobs associated with the given \see{JobQueue}.
                /// \param[in] jobQueue \see{JobQueue} whose jobs to cancel.
                void Cancel (const JobQueue &jobQueue);
                /// \brief
                /// Cancel all pending jobs associated with the given \see{RunLoop}.
                /// \param[in] runLoop \see{RunLoop} whose jobs to cancel.
                void Cancel (const RunLoop &runLoop);
                /// \brief
                /// Cancel the job associated with the given job id.
                /// \param[in] id JobInfo id to cancel.
                void Cancel (const JobQueue::Job::Id &id);
            } queue;
            /// \brief
            /// Synchronization spin lock.
            SpinLock spinLock;

        public:
            /// \brief
            /// ctor.
            JobQueueScheduler () :
                timer (*this) {}
            /// \brief
            /// dtor.
            ~JobQueueScheduler () {
                Clear ();
            }

            /// \brief
            /// Schedule a job to be performed in the future.
            /// \param[in] job \see{JobQueue::Job} to execute.
            /// \param[in] timeSpec When in the future to execute the given job.
            /// \param[in] jobQueue \see{JobQueue} that will execute the job.
            /// \return JobInfo::Id which can be used in a call to Cancel.
            /// IMPORTANT: timeSpec is a relative value.
            inline JobQueue::Job::Id Schedule (
                    JobQueue::Job &job,
                    const TimeSpec &timeSpec,
                    JobQueue &jobQueue = GlobalJobQueue::Instance ()) {
                return ScheduleJobInfo (
                    JobInfo::Ptr (
                        new JobQueueJobInfo (
                            job,
                            GetCurrentTime () + timeSpec,
                            jobQueue)),
                    timeSpec);
            }

            /// \brief
            /// Schedule a job to be performed in the future.
            /// \param[in] job \see{JobQueue::Job} to execute.
            /// \param[in] timeSpec When in the future to execute the given job.
            /// \param[in] runLoop \see{RunLoop} that will execute the job.
            /// \return JobInfo::Id which can be used in a call to Cancel.
            /// IMPORTANT: timeSpec is a relative value.
            inline JobQueue::Job::Id Schedule (
                    JobQueue::Job &job,
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
            /// Cancel all pending jobs associated with the given \see{JobQueue}.
            /// IMPORTANT: JobQueueJobInfo holds on to the \see{JobQueue} reference.
            /// Use this member to cancel all \see{JobQueue} jobs before that
            /// \see{JobQueue} goes out of scope.
            /// \param[in] jobQueue \see{JobQueue} whose jobs to cancel.
            void Cancel (const JobQueue &jobQueue);
            /// \brief
            /// Cancel all pending jobs associated with the given \see{RunLoop}.
            /// IMPORTANT: RunLoopJobInfo holds on to the \see{RunLoop} reference.
            /// Use this member to cancel all \see{RunLoop} jobs before that
            /// \see{RunLoop} goes out of scope.
            /// \param[in] runLoop \see{RunLoop} whose jobs to cancel.
            void Cancel (const RunLoop &runLoop);
            /// \brief
            /// Cancel the job associated with the given job id.
            /// \param[in] id JobInfo id to cancel.
            void Cancel (const JobQueue::Job::Id &id);

            /// \brief
            /// Remove all pendig jobs.
            void Clear ();

        private:
            // Timer::Callback
            /// \brief
            /// Called every time the timer fires.
            /// \param[in] timer Timer that fired.
            virtual void Alarm (Timer & /*timer*/) throw ();

            /// \brief
            /// Schedule helper.
            /// \param[in] jobInfo JobInfo containing \see{JobQueue::Job} particulars.
            /// \param[in] timeSpec When in the future to execute the given job.
            /// \return JobInfo::Id which can be used in a call to Cancel.
            /// IMPORTANT: timeSpec is a relative value.
            JobQueue::Job::Id ScheduleJobInfo (
                JobInfo::Ptr jobInfo,
                const TimeSpec &timeSpec);

            /// \brief
            /// JobQueueScheduler is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (JobQueueScheduler)
        };

        /// \struct GlobalJobQueueScheduler JobQueueScheduler.h thekogans/util/JobQueueScheduler.h
        ///
        /// \brief
        /// A global job queue scheduler instance.
        struct _LIB_THEKOGANS_UTIL_DECL GlobalJobQueueScheduler :
            public Singleton<JobQueueScheduler, SpinLock> {};

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_JobQueueScheduler_h)
