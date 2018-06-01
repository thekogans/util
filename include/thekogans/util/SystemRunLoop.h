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
        ///         // Wait for the SystemRunLoop to be created and started. This will make
        ///         // it unnecessary to check runLoop.Get () != 0 in Stop and EnqJob below.
        ///         if (!util::RunLoop::WaitForStart (runLoop, sleepTimeSpec, waitTimeSpec)) {
        ///             THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
        ///                 "%s", "Timed out waiting for RunLoop to start.");
        ///         }
        ///     }
        ///
        ///     void Stop () {
        ///         runLoop->Stop ();
        ///         Wait ();
        ///     }
        ///
        ///     bool EnqJob (
        ///             util::RunLoop::Job::Ptr job,
        ///             bool wait = false,
        ///             const TimeSpec &timeSpec = TimeSpec::Infinite) {
        ///         return runLoop->EnqJob (job, wait, timeSpec);
        ///     }
        ///
        /// private:
        ///     // util::Thread
        ///     virtual void Run () {
        ///         THEKOGANS_UTIL_TRY {
        ///             // NOTE: Windows will only deliver HWND events to a thread
        ///             // that created the HWND. It's, therefore, important that
        ///             // SystemRunLoop be created on the thread that will call
        ///             // Start (unlike \see{DefaultRunLoop}).
        ///             runLoop.Reset (new util::SystemRunLoop);
        ///             runLoop->Start ();
        ///         }
        ///         THEKOGANS_UTIL_CATCH_AND_LOG
        ///         // This call to reset is very important as it allows the thread that
        ///         // created the SystemRunLoop resources (HWND) to destroy them too.
        ///         runLoop.Reset ();
        ///     }
        /// }
        /// \endcode

        struct _LIB_THEKOGANS_UTIL_DECL SystemRunLoop : public RunLoop {
            /// \brief
            /// Convenient typedef for ThreadSafeRefCounted::Ptr<SystemRunLoop>.
            typedef ThreadSafeRefCounted::Ptr<SystemRunLoop> Ptr;

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
            /// Called to initialize/uninitialize the worker thread.
            WorkerCallback *workerCallback;
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

        public:
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// ctor.
            /// \param[in] name RunLoop name.
            /// \param[in] type RunLoop queue type.
            /// \param[in] maxPendingJobs Max pending run loop jobs.
            /// \param[in] done true = must call Start.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the worker thread.
            /// \param[in] eventProcessor_ Callback to process window events.
            /// \param[in] userData_ Optional user data passed to eventProcessor.
            /// \param[in] window_ Windows window.
            SystemRunLoop (
                const std::string &name = std::string (),
                Type type = TYPE_FIFO,
                ui32 maxPendingJobs = UI32_MAX,
                bool done = true,
                WorkerCallback *workerCallback_ = 0,
                EventProcessor eventProcessor_ = 0,
                void *userData_ = 0,
                Window::Ptr window_ = CreateThreadWindow ());

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
            /// \param[in] name RunLoop name.
            /// \param[in] type RunLoop queue type.
            /// \param[in] maxPendingJobs Max pending run loop jobs.
            /// \param[in] done true = must call Start before Enq.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the worker thread.
            /// \param[in] eventProcessor_ Callback to process Xlib XEvent events.
            /// \param[in] userData_ Optional user data passed to eventProcessor.
            /// \param[in] window_ Xlib window.
            /// \param[in] displays_ A list of displays to listen to.
            SystemRunLoop (
                const std::string &name = std::string (),
                Type type = TYPE_FIFO,
                ui32 maxPendingJobs = UI32_MAX,
                bool done = true,
                WorkerCallback *workerCallback_ = 0,
                EventProcessor eventProcessor_ = 0,
                void *userData_ = 0,
                XlibWindow::Ptr window_ = CreateThreadWindow (0),
                const std::vector<Display *> &displays_ = std::vector<Display *> ());

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
            /// \param[in] name RunLoop name.
            /// \param[in] type RunLoop queue type.
            /// \param[in] maxPendingJobs Max pending run loop jobs.
            /// \param[in] done true = must call Start before Enq.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the worker thread.
            /// \param[in] runLoop_ OS X run loop object.
            /// NOTE: if runLoop_ == 0, SystemRunLoop will use \see{CocoaStart) \see{CocoaStop)
            /// from OSXUtils.[h | mm]. \see{MainRunLoopCreateInstance} takes care of the details
            /// if you call \see{MainRunLoopCreateInstance::Parametrize} with runLoop == 0.
            SystemRunLoop (
                const std::string &name = std::string (),
                Type type = TYPE_FIFO,
                ui32 maxPendingJobs = UI32_MAX,
                bool done = true,
                WorkerCallback *workerCallback_ = 0,
                CFRunLoopRef runLoop_ = CFRunLoopGetCurrent ());

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
            /// Enqueue a job to be performed on the run loop thread.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// NOTE: Same constraint applies to EnqJob as Stop. Namely, you can't call EnqJob
            /// from the same thread that called Start.
            /// \return true == !wait || WaitForJob (...)
            virtual bool EnqJob (
                Job::Ptr job,
                bool wait = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
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
                Job::Ptr job,
                bool wait = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);

        private:
            /// \brief
            /// Used internally to execute the pending jobs.
            void ExecuteJobs ();

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
