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

#include <memory>
#include <queue>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/Timer.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/RunLoop.h"
#include "thekogans/util/MainRunLoop.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/GUID.h"

namespace thekogans {
    namespace util {

        /// \struct RunLoopScheduler RunLoopScheduler.h thekogans/util/RunLoopScheduler.h
        ///
        /// \brief
        /// A RunLoopScheduler allows you to schedule \see{RunLoop::Job}s to be executed
        /// in the future.

        struct _LIB_THEKOGANS_UTIL_DECL RunLoopScheduler : public Timer::Callback {
            /// \brief
            /// Convenient typedef for std::shared_ptr<RunLoopScheduler>.
            typedef std::shared_ptr<RunLoopScheduler> SharedPtr;

        private:
            /// \brief
            /// \see{Timer} used to schedule future jobs.
            Timer timer;
            /// \struct RunLoopScheduler::JobInfo RunLoopScheduler.h thekogans/util/RunLoopScheduler.h
            ///
            /// \brief
            /// Holds information about a future job to be scheduled on the given \see{RunLoop}.
            struct JobInfo : public ThreadSafeRefCounted {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<JobInfo>.
                typedef ThreadSafeRefCounted::Ptr<JobInfo> Ptr;

                /// \brief
                /// RunLoopJobInfo has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (JobInfo, SpinLock)

                /// \brief
                /// \see{RunLoop::Job} that will be scheduled.
                RunLoop::Job::Ptr job;
                /// \brief
                /// Absolute time when the job will be scheduled.
                TimeSpec deadline;
                /// \brief
                /// \see{RunLoop} the job will be scheduled on.
                RunLoop &runLoop;

                /// \brief
                /// ctor.
                /// \param[in] job_ \see{RunLoop::Job} that will be scheduled.
                /// \param[in] deadline_ Absolute time when the job will be scheduled.
                /// \param[in] runLoop_ \see{RunLoop} the job will be scheduled on.
                JobInfo (
                    RunLoop::Job &job_,
                    const TimeSpec &deadline_,
                    RunLoop &runLoop_) :
                    job (&job_),
                    deadline (deadline_),
                    runLoop (runLoop_) {}

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
                            JobInfo::Ptr jobInfo1,
                            JobInfo::Ptr jobInfo2) {
                        return jobInfo1->deadline > jobInfo2->deadline;
                    }
                } static compare;

                /// \brief
                /// Enqueue the job on the specified run loop.
                inline void EnqJob () {
                    runLoop.Enq (*job);
                }

                /// \brief
                /// RunLoopJobInfo is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (JobInfo)
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
                /// Cancel all pending jobs associated with the given \see{RunLoop}.
                /// \param[in] runLoop \see{RunLoop} whose jobs to cancel.
                void Cancel (const RunLoop &runLoop);
                /// \brief
                /// Cancel the job associated with the given job id.
                /// \param[in] id JobInfo id to cancel.
                void Cancel (const RunLoop::Job::Id &id);
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
                Clear ();
            }

            /// \brief
            /// Schedule a job to be performed in the future.
            /// \param[in] job \see{RunLoop::Job} to execute.
            /// \param[in] timeSpec When in the future to execute the given job.
            /// \param[in] runLoop \see{RunLoop} that will execute the job.
            /// \return JobInfo::Id which can be used in a call to Cancel.
            /// IMPORTANT: timeSpec is a relative value.
            inline RunLoop::Job::Id Schedule (
                    RunLoop::Job &job,
                    const TimeSpec &timeSpec,
                    RunLoop &runLoop = MainRunLoop::Instance ()) {
                return ScheduleJobInfo (
                    JobInfo::Ptr (
                        new JobInfo (
                            job,
                            GetCurrentTime () + timeSpec,
                            runLoop)),
                    timeSpec);
            }

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
            void Cancel (const RunLoop::Job::Id &id);

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
            /// \param[in] jobInfo JobInfo containing \see{RunLoop::Job} particulars.
            /// \param[in] timeSpec When in the future to execute the given job.
            /// \return JobInfo::Id which can be used in a call to Cancel.
            /// IMPORTANT: timeSpec is a relative value.
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
