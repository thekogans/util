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

#if !defined (__thekogans_util_RunLoop_h)
#define __thekogans_util_RunLoop_h

#include "thekogans/util/Config.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/JobQueue.h"

namespace thekogans {
    namespace util {

        /// \struct RunLoop RunLoop.h thekogans/util/RunLoop.h
        ///
        /// \brief
        /// RunLoop is an abstract base for \see{DefaultRunLoop} and \see{SystemRunLoop}.
        /// RunLoop allows you to schedule \see{JobQueue::Job} to be executed on the thread
        /// that's running the run loop.

        struct _LIB_THEKOGANS_UTIL_DECL RunLoop {
            /// \brief
            /// Convenient typedef for std::unique_ptr<RunLoop>.
            typedef std::unique_ptr<RunLoop> Ptr;

            /// \brief
            /// dtor.
            virtual ~RunLoop () {}

            /// \brief
            /// Start waiting for jobs.
            virtual void Start () = 0;
            /// \brief
            /// Stop the run loop and in all likelihood exit the thread hosting it.
            /// Obviously, this function needs to be called from a different thread
            /// than the one that called Start.
            /// \param[in] cancelPendingJobs true = Cancel all pending jobs.
            virtual void Stop (bool cancelPendingJobs = true) = 0;

            /// \brief
            /// Enqueue a job to be performed on the run loop thread.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// NOTE: Same constraint applies to Enq as Stop. Namely, you can't call Enq
            /// from the same thread that called Start.
            virtual void Enq (
                JobQueue::Job &job,
                bool wait = false) = 0;

            /// \brief
            /// Cancel a queued job with a given id. If the job is not
            /// in the queue (in flight), it is not canceled.
            /// \param[in] jobId Id of job to cancel.
            /// \return true = the job was cancelled. false = in flight or nonexistent.
            virtual bool Cancel (const JobQueue::Job::Id &jobId) = 0;
            /// \brief
            /// Cancel all queued jobs. Job in flight is unaffected.
            virtual void CancelAll () = 0;

            /// \brief
            /// Return a snapshot of the run loop stats.
            /// \return A snapshot of the run loop stats.
            virtual JobQueue::Stats GetStats () = 0;

            /// \brief
            /// Blocks until all jobs are complete and the queue is empty.
            virtual void WaitForIdle () = 0;

            /// \brief
            /// Return true if Start was called.
            /// \return true if Start was called.
            virtual bool IsRunning () = 0;
            /// \brief
            /// Return true if there are no pending jobs.
            /// \return true = no pending jobs, false = jobs pending.
            virtual bool IsEmpty () = 0;
            /// \brief
            /// Return true if there are no pending jobs and the
            /// worker is idle.
            /// \return true = idle, false = busy.
            virtual bool IsIdle () = 0;

            /// \brief
            /// Wait until the given run loop is created the and it starts running.
            /// \param[in] runLoop RunLoop to wait for.
            /// \param[in] sleepTimeSpec How long to sleep between tries.
            /// \param[in] waitTimeSpec Total time to wait.
            /// \return true == the given run loop is running.
            /// false == timed out waiting for the run loop to start.
            static bool WaitForStart (
                Ptr &runLoop,
                const TimeSpec &sleepTimeSpec = TimeSpec::FromMilliseconds (50),
                const TimeSpec &waitTimeSpec = TimeSpec::FromSeconds (3));
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RunLoop_h)
