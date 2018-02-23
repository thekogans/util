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

#include "thekogans/util/Config.h"
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
        ///     util::SpinLock spinLock;
        ///
        /// public:
        ///     MyThread (
        ///             const std::string &name = std::string (),
        ///             util::i32 priority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
        ///             util::ui32 affinity = UI32_MAX) :
        ///             Thread (name) {
        ///         Create (priority, affinity);
        ///         if (!util::RunLoop::WaitForStart (runLoop)) {
        ///             THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
        ///                 "%s", "Timed out waiting for RunLoop to start.");
        ///         }
        ///     }
        ///
        ///     void Stop () {
        ///         util::LockGuard<util::SpinLock> guard (spinLock);
        ///         if (runLoop.get () != 0) {
        ///             runLoop->Stop ();
        ///             Wait ();
        ///         }
        ///     }
        ///
        ///     void Enq (
        ///             Job &job,
        ///             bool wait = false) {
        ///         util::LockGuard<util::SpinLock> guard (spinLock);
        ///         if (runLoop.get () != 0) {
        ///             runLoop->Enq (job, wait);
        ///         }
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
        ///         runLoop.reset ();
        ///     }
        /// }
        /// \endcode

        struct _LIB_THEKOGANS_UTIL_DECL DefaultRunLoop : public RunLoop {
        private:
            /// \brief
            /// RunLoop name.
            const std::string name;
            /// \brief
            /// RunLoop type (TIPE_FIFO or TYPE_LIFO)
            const Type type;
            /// \brief
            /// Max pending jobs.
            const ui32 maxPendingJobs;
            /// \brief
            /// Called to initialize/uninitialize the worker thread.
            WorkerCallback *workerCallback;
            /// \brief
            /// Flag to signal the worker thread.
            volatile bool done;
            /// \brief
            /// Job queue.
            JobList jobs;
            /// \brief
            /// RunLoop stats.
            Stats stats;
            /// \brief
            /// Synchronization lock for jobs.
            Mutex jobsMutex;
            /// \brief
            /// Synchronization condition variable.
            Condition jobsNotEmpty;
            /// \brief
            /// Synchronization condition variable.
            Condition jobFinished;
            /// \brief
            /// Synchronization condition variable.
            Condition idle;
            /// \brief
            /// Worker state.
            enum State {
                /// \brief
                /// Worker(s) is/are idle.
                Idle,
                /// \Worker(s) is/are busy.
                Busy
            } volatile state;
            /// \brief
            /// Count of busy workers.
            ui32 busyWorkers;

        public:
            /// \brief
            /// ctor.
            /// \param[in] name_ RunLoop name.
            /// \param[in] type_ RunLoop queue type.
            /// \param[in] maxPendingJobs_ Max pending run loop jobs.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the worker thread.
            DefaultRunLoop (
                const std::string &name_ = std::string (),
                Type type_ = TYPE_FIFO,
                ui32 maxPendingJobs_ = UI32_MAX,
                WorkerCallback *workerCallback_ = 0) :
                name (name_),
                type (type_),
                maxPendingJobs (maxPendingJobs_),
                workerCallback (workerCallback_),
                done (true),
                jobsNotEmpty (jobsMutex),
                jobFinished (jobsMutex),
                idle (jobsMutex),
                state (Idle),
                busyWorkers (0) {}
            /// \brief
            /// dtor.
            virtual ~DefaultRunLoop () {
                CancelAll ();
            }

            /// \brief
            /// Start the run loop. This is a blocking call and will
            /// only complete when Stop is called.
            virtual void Start ();

            /// \brief
            /// Stop the run loop. Calling this function will cause the Start call
            /// to return.
            /// \param[in] cancelPendingJobs true = Cancel all pending jobs.
            virtual void Stop (bool cancelPendingJobs = true);

            /// \brief
            /// Enqueue a job to be performed on the run loop's thread.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish.
            /// Used for synchronous job execution.
            virtual void Enq (
                Job &job,
                bool wait = false);

            /// \brief
            /// Cancel a queued job with a given id. If the job is not
            /// in the queue (in flight), it is not canceled.
            /// \param[in] jobId Id of job to cancel.
            /// \return true if the job was cancelled. false if in flight.
            virtual bool Cancel (const Job::Id &jobId);
            /// \brief
            /// Cancel all queued jobs. Jobs in flight are unaffected.
            virtual void CancelAll ();

            /// \brief
            /// Blocks until all jobs are complete and the queue is empty.
            virtual void WaitForIdle ();

            /// \brief
            /// Return a snapshot of the queue stats.
            /// \return A snapshot of the queue stats.
            virtual Stats GetStats ();

            /// \brief
            /// Return true if Start was called.
            /// \return true if Start was called.
            virtual bool IsRunning ();
            /// \brief
            /// Return true if there are no pending jobs.
            /// \return true = no pending jobs, false = jobs pending.
            virtual bool IsEmpty ();
            /// \brief
            /// Return true if there are no pending jobs and all the
            /// workers are idle.
            /// \return true = idle, false = busy.
            virtual bool IsIdle ();

        private:
            /// \brief
            /// Used internally to get the next job.
            /// \return The next job to execute.
            Job *Deq ();
            /// \brief
            /// Called by worker(s) after each job is done.
            /// Used to update state and \see{Stats}.
            /// \param[in] job Completed job.
            /// \param[in] start Completed job start time.
            /// \param[in] end Completed job end time.
            void FinishedJob (
                Job &job,
                ui64 start,
                ui64 end);

            /// \brief
            /// Atomically set done to the given value.
            /// \param[in] value value to set done to.
            /// \return true == done was set to the given value.
            /// false == done was already set to the given value.
            bool SetDone (bool value);

            /// \brief
            /// DefaultRunLoop is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (DefaultRunLoop)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_DefaultRunLoop_h)
