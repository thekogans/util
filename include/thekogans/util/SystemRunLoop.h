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

#if !defined (TOOLCHAIN_OS_Linux) || defined (THEKOGANS_UTIL_HAVE_XLIB)

#if defined (TOOLCHAIN_OS_Windows)
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
        #include <windows.h>
    #endif // !defined (_WINDOWS_)
#elif defined (TOOLCHAIN_OS_Linux)
    #include <X11/Xlib.h>
    #include <X11/Xatom.h>
#elif defined (TOOLCHAIN_OS_OSX)
    #include <CoreFoundation/CFRunLoop.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <memory>
#include "thekogans/util/Config.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Condition.h"
#include "thekogans/util/JobQueue.h"
#include "thekogans/util/RunLoop.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/WindowsUtils.h"
#elif defined (TOOLCHAIN_OS_Linux)
    #include "thekogans/util/XlibUtils.h"
#endif // defined (TOOLCHAIN_OS_Linux)

namespace thekogans {
    namespace util {

        /// \struct SystemRunLoop SystemRunLoop.h thekogans/util/SystemRunLoop.h
        ///
        /// \brief
        /// SystemRunLoop is intended to be used in threads that need to interact with the UI.
        /// SystemRunLoop is a thin wrapper around OS X CFRunLoopRef. On Windows and Linux
        /// the functionality is emulated using a window and a dispatch loop. SystemRunLoop
        /// is designed so that it can be used as either the main thread run loop or coexist
        /// with an existing one.
        ///
        /// The call to thekogans::util::MainRunLoop::Instance ().Start () is optional. If
        /// you want thekogans::util::MainRunLoop to be your main (WinMain) run loop, call
        /// thekogans::util::MainRunLoop::Instance ().Start () to enter it. Otherwise just
        /// call thekogans::util::MainRunLoop::Instance ().Enq () from secondary threads,
        /// and as long as someone, somewhere is pumping messages for the main thread, the
        /// MainRunLoop will do it's job.
        ///
        /// To Use a SystemRunLoop in secondary (worker) threads, use the following template:
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
        ///     void EnqJob (
        ///             util::RunLoop::Job::Ptr job,
        ///             bool wait = false) {
        ///         util::LockGuard<util::SpinLock> guard (spinLock);
        ///         if (runLoop.get () != 0) {
        ///             runLoop->EnqJob (job, wait);
        ///         }
        ///     }
        ///
        /// private:
        ///     // util::Thread
        ///     virtual void Run () {
        ///         THEKOGANS_UTIL_TRY {
        ///             runLoop.reset (new util::SystemRunLoop);
        ///             runLoop->Start ();
        ///         }
        ///         THEKOGANS_UTIL_CATCH_AND_LOG
        ///         // This call to reset is very important as it allows the thread that
        ///         // created the SystemRunLoop to destroy it too. This is especially
        ///         // important under X as Xlib is not thread safe.
        ///         runLoop.reset ();
        ///     }
        /// }
        /// \endcode

        struct _LIB_THEKOGANS_UTIL_DECL SystemRunLoop : public RunLoop {
            /// \brief
            /// Convenient typedef for std::unique_ptr<SystemRunLoop>.
            typedef std::unique_ptr<SystemRunLoop> Ptr;

        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Convenient typedef for LRESULT (CALLBACK *) (HWND, UINT, WPARAM, LPARAM, void *).
            /// NOTE: Return 0 if you processed the event, otherwise call DefWindowProc.
            typedef LRESULT (CALLBACK *EventProcessor) (
                HWND /*wnd*/,
                UINT /*message*/,
                WPARAM /*wParam*/,
                LPARAM /*lParam*/,
                void * /*userData*/);
        #elif defined (TOOLCHAIN_OS_Linux)
            /// \brief
            /// Convenient typedef for void (*) (Display *, const XEvent &, void *).
            /// NOTE: Returning false from this callback will cause
            /// the event loop to terminate.
            typedef void (*EventProcessor) (
                Display * /*display*/,
                const XEvent & /*event*/,
                void * /*userData*/);

            /// \struct SystemRunLoop::XlibWindow SystemRunLoop.h thekogans/util/SystemRunLoop.h
            ///
            /// \brief
            /// Encapsulates Xlib Display, Window and Atom used to implement the run loop.
            struct XlibWindow {
                /// \brief
                /// Convenient typedef for std::unique_ptr<XlibWindow>.
                typedef std::unique_ptr<XlibWindow> Ptr;

                /// \struct SystemRunLoop::XlibWindow::_Display SystemRunLoop.h thekogans/util/SystemRunLoop.h
                ///
                /// \brief
                /// Manages the lifetime of Xlib Display.
                struct _Display {
                    /// \brief
                    /// Xlib Display.
                    Display *display;
                    /// \brief
                    /// true = We own the display and need to close it in our dtor.
                    bool owner;

                    /// \brief
                    /// ctor.
                    /// \param[in] display_ Xlib Display.
                    _Display (Display *display_);
                    /// \brief
                    /// dtor.
                    ~_Display ();
                } display;
                /// \struct SystemRunLoop::XlibWindow::_Window SystemRunLoop.h thekogans/util/SystemRunLoop.h
                ///
                /// \brief
                /// Manages the lifetime of Xlib Window.
                struct _Window {
                    /// \brief
                    /// Xlib Display.
                    Display *display;
                    /// \brief
                    /// Xlib Window.
                    Window window;
                    /// \brief
                    /// true = We own the window and need to close it in our dtor.
                    bool owner;

                    /// \brief
                    /// ctor.
                    /// \param[in] display_ Xlib Display.
                    /// \param[in] window_ Xlib Window.
                    _Window (
                        Display *display_,
                        Window window_);
                    /// \brief
                    /// dtor.
                    ~_Window ();

                private:
                    /// \brief
                    /// Create the Xlib window if none were provided in the ctor.
                    /// \return A simple 0 area, invisible window that will receive
                    /// job processing events.
                    Window CreateWindow ();
                } window;
                /// \brief
                /// A custom Xlib message type used to signal our window.
                Atom message_type;

                /// \brief
                /// ctor.
                /// \param[in] display_ Xlib Display.
                /// \param[in] window_ Xlib Window.
                XlibWindow (
                    Display *display_ = 0,
                    Window window_ = 0);

            private:
                enum {
                    /// \brief
                    /// Execute the next waiting job.
                    ID_RUN_LOOP,
                    /// \brief
                    /// Stop the run loop.
                    ID_STOP
                };

                /// \brief
                /// Post an event with the given id.
                /// \param[in] id Event id.
                void PostEvent (long id);

                /// \brief
                /// SystemRunLoop needs access to PostEvent and event ids.
                friend SystemRunLoop;
            };
        #endif // defined (TOOLCHAIN_OS_Windows)

        private:
            /// \brief
            /// RunLoop name.
            const std::string name;
            /// \brief
            /// JobQueue type (TIPE_FIFO or TYPE_LIFO)
            const Type type;
            /// \brief
            /// Max pending jobs.
            const ui32 maxPendingJobs;
            /// \brief
            /// Called to initialize/uninitialize the worker thread.
            WorkerCallback *workerCallback;
            /// \brief
            /// true = exit the run loop.
            volatile bool done;
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Callback to process window events.
            EventProcessor eventProcessor;
            /// \brief
            /// Optional user data passed to eventProcessor.
            void *userData;
            /// \brief
            /// Windows window.
            Window::Ptr window;
        #elif defined (TOOLCHAIN_OS_Linux)
            /// \brief
            /// Callback to process Xlib XEvent events.
            EventProcessor eventProcessor;
            /// \brief
            /// Optional user data passed to eventProcessor.
            void *userData;
            /// \brief
            /// Xlib window.
            XlibWindow::Ptr window;
            /// \brief
            /// A list of displays to listen to.
            std::vector<Display *> displays;
        #elif defined (TOOLCHAIN_OS_OSX)
            /// \brief
            /// OS X run loop object.
            CFRunLoopRef runLoop;
        #endif // defined (TOOLCHAIN_OS_Windows)
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
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// ctor.
            /// \param[in] name_ RunLoop name.
            /// \param[in] type_ RunLoop queue type.
            /// \param[in] maxPendingJobs_ Max pending run loop jobs.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the worker thread.
            /// \param[in] done_ true = must call Start.
            /// \param[in] eventProcessor_ Callback to process window events.
            /// \param[in] userData_ Optional user data passed to eventProcessor.
            /// \param[in] window_ Windows window.
            /// NOTE: SystemRunLoop takes ownership of the passed in wnd_ and
            /// will destroy it in it's dtor.
            SystemRunLoop (
                const std::string &name_ = std::string (),
                Type type_ = TYPE_FIFO,
                ui32 maxPendingJobs_ = UI32_MAX,
                WorkerCallback *workerCallback_ = 0,
                bool done_ = true,
                EventProcessor eventProcessor_ = 0,
                void *userData_ = 0,
                Window::Ptr window_ = CreateThreadWindow ());
            /// \brief
            /// dtor.
            virtual ~SystemRunLoop ();

            /// \brief
            /// Create a run loop window to service job requests.
            /// \return Window that will process job requests.
            static Window::Ptr CreateThreadWindow ();
            /// \brief
            /// Return the Windows window associated with this run loop.
            /// \return Windows window associated with this run loop.
            inline Window &GetWindow () const {
                return *window;
            }
        #elif defined (TOOLCHAIN_OS_Linux)
            /// \brief
            /// ctor.
            /// \param[in] name_ RunLoop name.
            /// \param[in] type_ RunLoop queue type.
            /// \param[in] maxPendingJobs_ Max pending run loop jobs.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the worker thread.
            /// \param[in] done_ true = must call Start before Enq.
            /// \param[in] eventProcessor_ Callback to process Xlib XEvent events.
            /// \param[in] userData_ Optional user data passed to eventProcessor.
            /// \param[in] window_ Xlib window.
            /// \param[in] displays_ A list of displays to listen to.
            SystemRunLoop (
                const std::string &name_ = std::string (),
                Type type_ = TYPE_FIFO,
                ui32 maxPendingJobs_ = UI32_MAX,
                WorkerCallback *workerCallback_ = 0,
                bool done_ = true,
                EventProcessor eventProcessor_ = 0,
                void *userData_ = 0,
                XlibWindow::Ptr window_ = CreateThreadWindow (0),
                const std::vector<Display *> &displays_ = std::vector<Display *> ());
            /// \brief
            /// dtor.
            virtual ~SystemRunLoop ();

            /// \brief
            /// Create a run loop window to service job requests.
            /// \param[in] display Display for which to create the window.
            /// \return Window that will process job requests.
            static XlibWindow::Ptr CreateThreadWindow (Display *display);
            /// \brief
            /// Return the Xlib window associated with this run loop.
            /// \return Xlib window associated with this run loop.
            inline XlibWindow &GetWindow () const {
                return *window;
            }
            /// \brief
            /// Dispatch an event to the given run loop.
            /// \param[in] display Xlib display that received the event.
            /// \param[in] event Event to dispatch.
            /// \return true = continue dispatching events,
            /// false = break out of the run loop.
            bool DispatchEvent (
                Display *display,
                const XEvent &event);
        #elif defined (TOOLCHAIN_OS_OSX)
            /// \brief
            /// ctor.
            /// \param[in] name_ RunLoop name.
            /// \param[in] type_ RunLoop queue type.
            /// \param[in] maxPendingJobs_ Max pending run loop jobs.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the worker thread.
            /// \param[in] done_ true = must call Start before Enq.
            /// \param[in] runLoop_ OS X run loop object.
            /// NOTE: if runLoop_ == 0, SystemRunLoop will use \see{CocoaStart) \see{CocoaStop)
            /// from OSXUtils.[h | mm]. \see{MainRunLoopCreateInstance} takes care of the details
            /// if you call \see{MainRunLoopCreateInstance::Parametrize} with runLoop == 0.
            SystemRunLoop (
                const std::string &name_ = std::string (),
                Type type_ = TYPE_FIFO,
                ui32 maxPendingJobs_ = UI32_MAX,
                WorkerCallback *workerCallback_ = 0,
                bool done_ = true,
                CFRunLoopRef runLoop_ = CFRunLoopGetCurrent ());
            /// \brief
            /// dtor.
            virtual ~SystemRunLoop ();

            /// \brief
            /// Return the OS X CFRunLoopRef associated with this run loop.
            /// \return OS X CFRunLoopRef associated with this run loop.
            inline CFRunLoopRef GetRunLoop () const {
                return runLoop;
            }
        #endif // defined (TOOLCHAIN_OS_Windows)

            // RunLoop
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
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            virtual void EnqJob (
                Job::Ptr job,
                bool wait = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Wait for a queued job with a given id. If the job is not
            /// in the queue (in flight), it is not waited on.
            /// \param[in] jobId Id of job to wait on.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true if the job was waited on. false if in flight.
            virtual bool WaitForJob (
                const Job::Id &jobId,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Wait for all queued jobs matching the given equality test. Jobs in flight
            /// are not waited on.
            /// \param[in] equalityTest EqualityTest to query to determine which jobs to wait on.
            /// \param[in] timeSpec How long to wait for the jobs to complete.
            /// IMPORTANT: timeSpec is a relative value.
            virtual void WaitForJobs (
                const EqualityTest &equalityTest,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Blocks until all jobs are complete and the queue is empty.
            /// \param[in] timeSpec How long to wait for the jobs to complete.
            /// IMPORTANT: timeSpec is a relative value.
            virtual void WaitForIdle (
                const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Cancel a queued job with a given id. If the job is not
            /// in the queue (in flight), it is not canceled.
            /// \param[in] jobId Id of job to cancel.
            /// \return true if the job was cancelled. false if in flight.
            virtual bool CancelJob (const Job::Id &jobId);
            /// \brief
            /// Cancel all queued job matching the given equality test. Jobs in flight
            /// are not canceled.
            /// \param[in] equalityTest EqualityTest to query to determine which jobs to cancel.
            virtual void CancelJobs (const EqualityTest &equalityTest);
            /// \brief
            /// Cancel all queued jobs. Jobs in flight are unaffected.
            virtual void CancelAllJobs ();

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
            /// Return true if there are no pending jobs and the
            /// worker is idle.
            /// \return true = idle, false = busy.
            virtual bool IsIdle ();

        private:
            /// \brief
            /// Used internally to execute the pending jobs.
            void ExecuteJobs ();
            /// \brief
            /// Used internally to get the next job.
            /// \return The next job to execute.
            Job *DeqJob ();
            /// \brief
            /// Called by worker after each job is done.
            /// Used to update state and \see{Stats}.
            /// \param[in] job Completed job.
            /// \param[in] start Completed job start time.
            /// \param[in] end Completed job end time.
            void FinishedJob (
                Job *job,
                ui64 start,
                ui64 end);

            /// \brief
            /// Atomically set done to the given value.
            /// \param[in] value value to set done to.
            /// \return true = done was set to the given value.
            /// false = done was already set to the given value.
            bool SetDone (bool value);

        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows window procedure.
            friend LRESULT CALLBACK WndProc (
                HWND /*wnd*/,
                UINT /*message*/,
                WPARAM /*wParam*/,
                LPARAM /*lParam*/);
        #endif // defined (TOOLCHAIN_OS_Windows)

            /// \brief
            /// SystemRunLoop is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SystemRunLoop)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (TOOLCHAIN_OS_Linux) || defined (THEKOGANS_UTIL_HAVE_XLIB)

#endif // !defined (__thekogans_util_SystemRunLoop_h)
