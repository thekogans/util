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
    #include "thekogans/util/os/windows/RunLoop.h"
#elif defined (TOOLCHAIN_OS_Linux)
    #include "thekogans/util/os/linux/RunLoop.h"
#elif defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/os/osx/RunLoop.h"
#endif // defined (TOOLCHAIN_OS_Linux)

namespace thekogans {
    namespace util {

        /// \struct SystemRunLoop SystemRunLoop.h thekogans/util/SystemRunLoop.h
        ///
        /// \brief
        /// SystemRunLoop is a marriage between an \see{os::RunLoop} and \see{RunLoop}. It
        /// alows the user to make any thread using os specific run loop facilities in to a
        /// thread that supports \see{RunLoop::Job} scheduling and execution. SystemRunLoop
        /// is used by \see{MainRunLoop} to make sure the main thread is responsible for
        /// UI updates and other system notifications. But you can use SystemRunLoop in any
        /// thread that requires those facilities.

        struct _LIB_THEKOGANS_UTIL_DECL SystemRunLoop :
                public RunLoop,
            #if defined (TOOLCHAIN_OS_Windows)
                public os::windows::RunLoop {
            #elif defined (TOOLCHAIN_OS_Linux)
                public os::linux::RunLoop {
            #elif defined (TOOLCHAIN_OS_OSX)
                public os::osx::RunLoop {
            #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// ctor.
            /// \param[in] name \see{RunLoop} name.
            /// \param[in] jobExecutionPolicy \see{RunLoop::JobExecutionPolicy}.
            SystemRunLoop (
                const std::string &name = std::string (),
                JobExecutionPolicy::SharedPtr jobExecutionPolicy =
                    JobExecutionPolicy::SharedPtr (new FIFOJobExecutionPolicy)) :
                RunLoop (name, jobExecutionPolicy) {}
            /// \brief
            /// dtor.
            virtual ~SystemRunLoop () {
                Stop ();
            }

            // RunLoop
            /// \brief
            /// Start the run loop. This is a blocking call and will
            /// only complete when Stop is called.
            virtual void Start () override;
            /// \brief
            /// Stop the run loop. Calling this function will cause the Start call
            /// to return.
            /// \param[in] cancelRunningJobs true = Cancel all running jobs.
            /// \param[in] cancelPendingJobs true = Cancel all pending jobs.
            virtual void Stop (
                bool cancelRunningJobs = true,
                bool cancelPendingJobs = true) override;
            /// \brief
            /// Return true is the run loop is running (Start was called).
            /// \return true is the run loop is running (Start was called).
            virtual bool IsRunning () override;

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
            bool EnqJob (
                Job::SharedPtr job,
                bool wait,
                const TimeSpec &timeSpec,
                bool front);
            // os::RunLoop
            /// \brief
            /// Used internally to execute the pending jobs.
            virtual void ExecuteJob () override;

            /// \brief
            /// SystemRunLoop is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SystemRunLoop)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (TOOLCHAIN_OS_Linux) || defined (THEKOGANS_UTIL_HAVE_XLIB)

#endif // !defined (__thekogans_util_SystemRunLoop_h)
