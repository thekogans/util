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
#include "thekogans/util/Heap.h"
#include "thekogans/util/Timer.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/JobQueue.h"
#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

        /// \struct JobQueueScheduler JobQueueScheduler.h thekogans/util/JobQueueScheduler.h
        ///
        /// \brief
        /// A JobQueueScheduler is an adoptor class used to schedule \see{JobQueue::Job}s.

        struct _LIB_THEKOGANS_UTIL_DECL JobQueueScheduler : public Timer::Callback {
            /// \brief
            /// Convenient typedef for std::unique_ptr<JobQueueScheduler>.
            typedef std::unique_ptr<JobQueueScheduler> UniquePtr;

        private:
            /// \brief
            /// \see{Timer} used to schedule future jobs.
            Timer timer;
            /// \struct JobQueueScheduler::JobInfo JobQueueScheduler.h thekogans/util/JobQueueScheduler.h
            ///
            /// \brief
            /// Holds information about future jobs.
            struct JobInfo {
                /// \brief
                /// Convenient typedef for std::shared_ptr<JobInfo>.
                typedef std::shared_ptr<JobInfo> SharedPtr;

                /// \brief
                /// JobInfo has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (JobInfo, SpinLock)

                /// \brief
                /// \see{JobQueue} the job will be scheduled on.
                JobQueue &jobQueue;
                /// \brief
                /// \see{JobQueue::Job} that will be scheduled.
                JobQueue::Job::UniquePtr job;
                /// \brief
                /// Absolute time when the job will be scheduled.
                TimeSpec deadline;

                /// \brief
                /// ctor.
                /// \param[in] jobQueue_ \see{JobQueue} the job will be scheduled on.
                /// \param[in] job_ \see{JobQueue::Job} that will be scheduled.
                /// \param[in] deadline_ Absolute time when the job will be scheduled.
                JobInfo (
                    JobQueue &jobQueue_,
                    JobQueue::Job::UniquePtr job_,
                    const TimeSpec &deadline_) :
                    jobQueue (jobQueue_),
                    job (std::move (job_)),
                    deadline (deadline_) {}

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
                            JobInfo::SharedPtr jobInfo1,
                            JobInfo::SharedPtr jobInfo2) {
                        return jobInfo1->deadline > jobInfo2->deadline;
                    }
                } static compare;

                /// \brief
                /// JobInfo is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (JobInfo)
            };
            /// \brief
            /// Priority queue used for job scheduling.
            std::priority_queue<JobInfo::SharedPtr, std::vector<JobInfo::SharedPtr>, JobInfo::Compare> queue;
            /// \brief
            /// Synchronization spin lock.
            SpinLock spinLock;

        public:
            /// \brief
            /// ctor.
            JobQueueScheduler () :
                timer (*this),
                queue (JobInfo::compare) {}

            /// \brief
            /// Schedule a job to be performed in the future.
            /// \param[in] jobQueue \see{JobQueue} that will execute the job.
            /// \param[in] job \see{JobQueue::Job} to execute.
            /// \param[in] timeSpec When in the future to execute the given job.
            /// IMPORTANT: timeSpec is a relative value.
            void Schedule (
                JobQueue &jobQueue,
                JobQueue::Job::UniquePtr job,
                const TimeSpec &timeSpec);

        private:
            // Timer::Callback
            /// \brief
            /// Called every time the timer fires.
            /// \param[in] timer Timer that fired.
            virtual void Alarm (Timer & /*timer*/) throw ();

            /// \brief
            /// JobQueueScheduler is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (JobQueueScheduler)
        };

        /// \brief
        /// A global job queue scheduler instance.
        typedef Singleton<JobQueueScheduler, SpinLock> GlobalJobQueueScheduler;

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_JobQueueScheduler_h)
