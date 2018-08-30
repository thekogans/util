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

#if !defined (__thekogans_util_DefaultRunLoop_h)
#define __thekogans_util_DefaultRunLoop_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Condition.h"
#include "thekogans/util/RunLoop.h"

namespace thekogans {
    namespace util {

        /// \struct DefaultRunLoop DefaultRunLoop.h thekogans/util/DefaultRunLoop.h
        ///
        /// \brief
        /// DefaultRunLoop implements a very simple thread run loop. To use it as your main thread
        /// run loop, all you need to do is call thekogans::util::MainRunLoop::Instance ().Start ()
        /// from main. If you initialized the \see{Console}, then the ctrl-break handler will call
        /// thekogans::util::MainRunLoop::Instance ().Stop () and your main thread will exit.
        /// Alternatively, you can call thekogans::util::MainRunLoop::Stop () from a secondary
        /// (worker) thread yourself (if ctrl-break is not appropriate). If your main thread needs
        /// to process UI events, use \see{SystemRunLoop} instead. It's designed to integrate
        /// with various system facilities (Windows: HWND, Linux: Display, OS X: CFRunLoop).
        ///
        /// To Use a DefaultRunLoop in the main thread (main, WinMain), use the following template:
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
        /// To Use a DefaultRunLoop in secondary (worker) threads, use the following template:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// struct MyThread : public util::Thread (
        /// private:
        ///     util::DefaultRunLoop runLoop;
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
        ///     void Stop () {
        ///         runLoop.Stop ();
        ///         Wait ();
        ///     }
        ///
        ///     bool EnqJob (
        ///             util::RunLoop::Job::Ptr job,
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

        struct _LIB_THEKOGANS_UTIL_DECL DefaultRunLoop : public RunLoop {
            /// \brief
            /// Convenient typedef for ThreadSafeRefCounted::Ptr<DefaultRunLoop>.
            typedef ThreadSafeRefCounted::Ptr<DefaultRunLoop> Ptr;

        public:
            /// \brief
            /// ctor.
            /// \param[in] name RunLoop name.
            /// \param[in] type RunLoop queue type.
            /// \param[in] maxPendingJobs Max pending run loop jobs.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the worker thread.
            DefaultRunLoop (
                const std::string &name = std::string (),
                Type type = TYPE_FIFO,
                ui32 maxPendingJobs = UI32_MAX) :
                RunLoop (name, type, maxPendingJobs) {}

            /// \brief
            /// Start the run loop. This is a blocking call and will
            /// only complete when Stop is called.
            virtual void Start ();

            /// \brief
            /// Stop the run loop. Calling this function will cause the Start call
            /// to return.
            /// \param[in] cancelRunningJobs true = Cancel all running jobs.
            /// \param[in] cancelPendingJobs true = Cancel all pending jobs.
            virtual void Stop (
                bool cancelRunningJobs = true,
                bool cancelPendingJobs = true);

            /// \brief
            /// DefaultRunLoop is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (DefaultRunLoop)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_DefaultRunLoop_h)
