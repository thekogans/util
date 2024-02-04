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

#if !defined (__thekogans_util_SystemRunLoop_h)
#define __thekogans_util_SystemRunLoop_h

#include "thekogans/util/Environment.h"

#if !defined (TOOLCHAIN_OS_Linux) || defined (THEKOGANS_UTIL_HAVE_XLIB)

#include "thekogans/util/Config.h"
#include "thekogans/util/RunLoop.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsUtils.h"
#elif defined (TOOLCHAIN_OS_Linux)
    #include "thekogans/util/os/linux/XlibUtils.h"
#elif defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/os/osx/OSXUtils.h"
#endif // defined (TOOLCHAIN_OS_Linux)
#include "thekogans/util/HRTimer.h"

namespace thekogans {
    namespace util {

    #if defined (TOOLCHAIN_OS_Windows)
        /// \brief
        /// Convenient typedef for os::windows::RunLoop.
        typedef os::windows::RunLoop OSThreadRunLoopType;
    #elif defined (TOOLCHAIN_OS_Linux)
        /// \brief
        /// Convenient typedef for os::linux::RunLoop.
        typedef os::linux::XlibRunLoop OSThreadRunLoopType;
    #elif defined (TOOLCHAIN_OS_OSX)
        /// \brief
        /// Convenient typedef for os::osx::CFRunLoop.
        typedef os::osx::CFRunLoop OSThreadRunLoopType;
    #endif // defined (TOOLCHAIN_OS_Windows)

        /// \struct SystemRunLoop SystemRunLoop.h thekogans/util/SystemRunLoop.h
        ///
        /// \brief
        /// SystemRunLoop is a marriage between a \see{util::RunLoop} and an \see{os::RunLoop}.
        /// It alows the user to make any thread using os specific run loop facilities in to
        /// a thread that supports \see{util::RunLoop::Job} scheduling and execution. SystemRunLoop
        /// is used by \see{MainRunLoop} to make sure the main thread is responsible for UI
        /// updates and other system notifications. But you can use SystemRunLoop in any
        /// thread that requires those facilities.

        template<typename OSRunLoopType = OSThreadRunLoopType>
        struct SystemRunLoop :
                public util::RunLoop,
                private OSRunLoopType {
            /// \brief
            /// ctor.
            /// \param[in] name \see{RunLoop} name.
            /// \param[in] jobExecutionPolicy \see{RunLoop::JobExecutionPolicy}.
            SystemRunLoop (
                const std::string &name = std::string (),
                JobExecutionPolicy::SharedPtr jobExecutionPolicy =
                    JobExecutionPolicy::SharedPtr (new FIFOJobExecutionPolicy)) :
                util::RunLoop (name, jobExecutionPolicy) {}
            /// \brief
            /// dtor.
            virtual ~SystemRunLoop () {
                Stop ();
            }

            // RunLoop
            /// \brief
            /// Start the run loop. This is a blocking call and will
            /// only complete when Stop is called.
            virtual void Start () override {
                state->done = false;
                ExecuteJob ();
                OSRunLoopType::Begin ();
            }
            /// \brief
            /// Stop the run loop. Calling this function will cause the Start call
            /// to return.
            /// \param[in] cancelRunningJobs true = Cancel all running jobs.
            /// \param[in] cancelPendingJobs true = Cancel all pending jobs.
            virtual void Stop (
                    bool cancelRunningJobs = true,
                    bool cancelPendingJobs = true) override {
                state->done = true;
                if (cancelRunningJobs) {
                    CancelRunningJobs ();
                }
                OSRunLoopType::End ();
                if (cancelPendingJobs) {
                    Job *job;
                    while ((job = state->jobExecutionPolicy->DeqJob (*state)) != nullptr) {
                        job->Cancel ();
                        state->runningJobs.push_back (job);
                        state->FinishedJob (job, 0, 0);
                    }
                }
                state->idle.SignalAll ();
            }
            /// \brief
            /// Return true is the run loop is running (Start was called).
            /// \return true is the run loop is running (Start was called).
            virtual bool IsRunning () override {
                return !state->done;
            }

            /// \brief
            /// Enqueue a job to be performed on the run loop thread.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// NOTE: Same constraint applies to EnqJob as Stop. Namely, you can't call EnqJob
            /// from the same thread that called Start.
            /// \return true == !wait || WaitForJob (...)
            virtual bool EnqJob (
                    Job::SharedPtr job,
                    bool wait = false,
                    const TimeSpec &timeSpec = TimeSpec::Infinite) override {
                return EnqJob (job, wait, timeSpec, false);
            }
            /// \brief
            /// Enqueue a job to be performed next on the run loop thread.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// NOTE: Same constraint applies to EnqJob as Stop. Namely, you can't call EnqJob
            /// from the same thread that called Start.
            /// \return true == !wait || WaitForJob (...)
            virtual bool EnqJobFront (
                    Job::SharedPtr job,
                    bool wait = false,
                    const TimeSpec &timeSpec = TimeSpec::Infinite) override {
                return EnqJob (job, wait, timeSpec, true);
            }

        private:
            /// \brief
            /// Helper used by EnqJob and EnqJobFront above.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// NOTE: Same constraint applies to EnqJob as Stop. Namely, you can't call EnqJob
            /// from the same thread that called Start.
            /// \param[in] front true == enq to the front of the queue.
            /// \return true == !wait || WaitForJob (...)
            bool EnqJob (
                    Job::SharedPtr job,
                    bool wait,
                    const TimeSpec &timeSpec,
                    bool front) {
                bool result = front ? util::RunLoop::EnqJobFront (job) : util::RunLoop::EnqJob (job);
                if (result) {
                    OSRunLoopType::ScheduleJob ();
                    result = !wait || WaitForJob (job, timeSpec);
                }
                return result;
            }

            // OSRunLoopType
            /// \brief
            /// Used internally to execute the pending jobs.
            virtual void ExecuteJob () override {
                while (!state->done) {
                    Job *job = state->DeqJob (false);
                    if (job == nullptr) {
                        break;
                    }
                    ui64 start = 0;
                    ui64 end = 0;
                    // Short circuit cancelled pending jobs.
                    if (!job->ShouldStop (state->done)) {
                        start = HRTimer::Click ();
                        job->SetState (Job::Running);
                        job->Prologue (state->done);
                        job->Execute (state->done);
                        job->Epilogue (state->done);
                        job->Succeed (state->done);
                        end = HRTimer::Click ();
                    }
                    state->FinishedJob (job, start, end);
                }
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (TOOLCHAIN_OS_Linux) || defined (THEKOGANS_UTIL_HAVE_XLIB)

#endif // !defined (__thekogans_util_SystemRunLoop_h)
