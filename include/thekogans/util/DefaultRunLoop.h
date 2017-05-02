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

#include <list>
#include "thekogans/util/Config.h"
#include "thekogans/util/JobQueue.h"
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
        ///     util::RunLoop::Ptr runLoop;
        ///
        /// public:
        ///     MyThread (
        ///             util::i32 priority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
        ///             util::ui32 affinity = UI32_MAX) {
        ///         Create (priority, affinity);
        ///     }
        ///
        ///     void Stop () {
        ///         runLoop->Stop ();
        ///     }
        ///
        ///     void Enq (util::JobQueue::Job::UniquePtr job) {
        ///         runLoop->Enq (std::move (job));
        ///     }
        ///
        /// private:
        ///     // util::Thread
        ///     virtual void Run () {
        ///         THEKOGANS_UTIL_TRY {
        ///             runLoop.reset (new util::DefaultRunLoop);
        ///             runLoop->Start ();
        ///         }
        ///         THEKOGANS_UTIL_CATCH_AND_LOG
        ///     }
        /// }
        /// \endcode

        struct _LIB_THEKOGANS_UTIL_DECL DefaultRunLoop : public RunLoop {
        private:
            /// \brief
            /// Flag to signal the worker thread.
            volatile bool done;
            /// \brief
            /// Synchronization lock for jobs.
            Mutex mutex;
            /// \brief
            /// Synchronization condition variable.
            Condition jobsNotEmpty;
            /// \brief
            /// Synchronization condition variable.
            Condition jobFinished;
            /// \brief
            /// Job queue.
            std::list<JobQueue::Job::UniquePtr> jobs;

        public:
            /// \brief
            /// ctor.
            DefaultRunLoop () :
                done (true),
                jobsNotEmpty (mutex),
                jobFinished (mutex) {}

            /// \brief
            /// Start the run loop. This is a blocking call and will
            /// only complete when Stop is called.
            virtual void Start ();

            /// \brief
            /// Stop the run loop. Calling this function will cause the Start call
            /// to return.
            virtual void Stop ();

            /// \brief
            /// Enqueue a job to be performed on the run loop's thread.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish.
            /// Used for synchronous job execution.
            virtual void Enq (
                JobQueue::Job::UniquePtr job,
                bool wait = false);

        private:
            /// \brief
            /// Used internally to get the next job.
            /// \return The next job to execute.
            JobQueue::Job::UniquePtr Deq ();

            /// \brief
            /// DefaultRunLoop is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (DefaultRunLoop)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_DefaultRunLoop_h)
