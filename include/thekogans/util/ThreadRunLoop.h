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

#if !defined (__thekogans_util_ThreadRunLoop_h)
#define __thekogans_util_ThreadRunLoop_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Condition.h"
#include "thekogans/util/RunLoop.h"

namespace thekogans {
    namespace util {

        /// \struct ThreadRunLoop ThreadRunLoop.h thekogans/util/ThreadRunLoop.h
        ///
        /// \brief
        /// ThreadRunLoop implements a very simple thread run loop. To use it as your main thread
        /// run loop, all you need to do is call thekogans::util::MainRunLoop::Instance ().Start ()
        /// from main. If you initialized the \see{Console}, then the ctrl-break handler will call
        /// thekogans::util::MainRunLoop::Instance ().Stop () and your main thread will exit.
        /// Alternatively, you can call thekogans::util::MainRunLoop::Stop () from a secondary
        /// (worker) thread yourself (if ctrl-break is not appropriate). If your main thread needs
        /// to process UI events, use \see{SystemRunLoop} instead. It's designed to integrate
        /// with various system facilities (Windows: HWND, Linux: Display, OS X: CFRunLoop).
        ///
        /// To Use a ThreadRunLoop in the main thread (main, WinMain), use the following template:
        ///
        /// On Windows:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// int CALLBACK WinMain (
        ///         HINSTANCE /*hInstance*/,
        ///         HINSTANCE /*hPrevInstance*/,
        ///         LPSTR /*lpCmdLine*/,
        ///         int /*nCmdShow*/) {
        ///     ...
        ///     util::MainRunLoop::Instance ().Start ();
        ///     ...
        ///     return 0;
        /// }
        /// \endcode
        ///
        /// On Linux and OS X:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// int main (
        ///         int /*argc*/,
        ///         const char * /*argv*/ []) {
        ///     ...
        ///     util::MainRunLoop::Instance ().Start ();
        ///     ...
        ///     return 0;
        /// }
        /// \endcode
        ///
        /// To Use a ThreadRunLoop in secondary (worker) threads, use the following template:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// struct MyThread : public util::Thread (
        /// private:
        ///     util::ThreadRunLoop runLoop;
        ///
        /// public:
        ///     MyThread (
        ///             const std::string &name = std::string (),
        ///             util::i32 priority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
        ///             util::ui32 affinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
        ///             const util::TimeSpec &sleepTimeSpec = util::TimeSpec::FromMilliseconds (50),
        ///             const util::TimeSpec &waitTimeSpec = util::TimeSpec::FromSeconds (3)) :
        ///             Thread (name) {
        ///         Create (priority, affinity);
        ///     }
        ///
        ///     bool Stop (
        ///             bool cancelRunningJobs = true,
        ///             bool cancelPendingJobs = true,
        ///             const TimeSpec &timeSpec = TimeSpec::Infinite) {
        ///         TimeSpec deadline = GetCurrentTime () + timeSpec;
        ///         retunr
        ///             runLoop.Stop (cancelRunningJobs, cancelPendingJobs, timeSpec) &&
        ///             Wait (deadline - GetCurrentTime ());
        ///     }
        ///
        ///     bool EnqJob (
        ///             util::RunLoop::Job::SharedPtr job,
        ///             bool wait = false,
        ///             const TimeSpec &timeSpec = TimeSpec::Infinite) {
        ///         return runLoop.EnqJob (job, wait, timeSpec);
        ///     }
        ///
        /// private:
        ///     // util::Thread
        ///     virtual void Run () throw () {
        ///         THEKOGANS_UTIL_TRY {
        ///             runLoop.Start ();
        ///         }
        ///         THEKOGANS_UTIL_CATCH_AND_LOG
        ///     }
        /// }
        /// \endcode

        struct _LIB_THEKOGANS_UTIL_DECL ThreadRunLoop : public RunLoop {
            /// \brief
            /// Convenient typedef for RefCounted::SharedPtr<ThreadRunLoop>.
            typedef RefCounted::SharedPtr<ThreadRunLoop> SharedPtr;

        public:
            /// \brief
            /// ctor.
            /// \param[in] name RunLoop name.
            /// \param[in] jobExecutionPolicy RunLoop \see{JobExecutionPolicy}.
            ThreadRunLoop (
                const std::string &name = std::string (),
                JobExecutionPolicy::SharedPtr jobExecutionPolicy =
                    JobExecutionPolicy::SharedPtr (new FIFOJobExecutionPolicy)) :
                RunLoop (name, jobExecutionPolicy) {}

            /// \brief
            /// Start the run loop. This is a blocking call and will
            /// only complete when Stop is called.
            virtual void Start ();
            /// \brief
            /// Stop the run loop. Calling this function will cause the Start call
            /// to return.
            /// \param[in] cancelRunningJobs true = Cancel all running jobs.
            /// \param[in] cancelPendingJobs true = Cancel all pending jobs.
            /// \param[in] timeSpec How long to wait for the run loop to stop.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == Run loop stopped. false == timed out.
            virtual bool Stop (
                bool cancelRunningJobs = true,
                bool cancelPendingJobs = true,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Return true is the run loop is running (Start was called).
            /// \return true is the run loop is running (Start was called).
            virtual bool IsRunning ();

            /// \brief
            /// ThreadRunLoop is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (ThreadRunLoop)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_ThreadRunLoop_h)
