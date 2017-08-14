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
#include <list>
#include "thekogans/util/Config.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Condition.h"
#include "thekogans/util/JobQueue.h"
#include "thekogans/util/RunLoop.h"

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
        /// #if defined (TOOLCHAIN_OS_Linux)
        ///     static bool EventProcessor (
        ///             const XEvent &event,
        ///             void * /*userData*/) {
        ///         switch (event.type) {
        ///             ...
        ///         }
        ///         return true;
        ///     }
        /// #endif // defined (TOOLCHAIN_OS_Linux)
        ///
        ///     // util::Thread
        ///     virtual void Run () {
        ///         THEKOGANS_UTIL_TRY {
        ///         #if defined (TOOLCHAIN_OS_Linux)
        ///             runLoop.reset (new util::SystemRunLoop (EventProcessor, this));
        ///         #elif // defined (TOOLCHAIN_OS_Linux)
        ///             runLoop.reset (new util::SystemRunLoop);
        ///         #endif // defined (TOOLCHAIN_OS_Linux)
        ///             runLoop->Start ();
        ///             // This call to reset is very important as it allows the thread that
        ///             // created the SystemRunLoop to destroy it too. This is especially
        ///             // important under X as Xlib is not thread safe.
        ///             runLoop.reset ();
        ///         }
        ///         THEKOGANS_UTIL_CATCH_AND_LOG
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
            /// Convenient typedef for bool (*) (const XEvent &, void *).
            /// NOTE: Returning false from this callback will cause
            /// the event loop to terminate.
            typedef bool (*EventProcessor) (
                const XEvent & /*event*/,
                void * /*userData*/);
        #endif // defined (TOOLCHAIN_OS_Windows)

        private:
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
            /// Windows window handle.
            HWND wnd;
        #elif defined (TOOLCHAIN_OS_Linux)
            /// \brief
            /// Callback to process Xlib XEvent events.
            EventProcessor eventProcessor;
            /// \brief
            /// Optional user data passed to eventProcessor.
            void *userData;
            /// \brief
            /// Xlib server display name.
            const char *displayName;
            /// \brief
            /// Xlib server connection.
            Display *display;
            /// \brief
            /// Xlib window id.
            Window window;
            /// \brief
            /// Xlib atom for our custom ClientMessage.
            Atom message_type;
        #elif defined (TOOLCHAIN_OS_OSX)
            /// \brief
            /// OS X run loop object.
            CFRunLoopRef runLoop;
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Job queue.
            std::list<JobQueue::Job::UniquePtr> jobs;
            /// \brief
            /// Synchronization lock for jobs.
            Mutex jobsMutex;
            /// \brief
            /// Synchronization condition variable.
            Condition jobFinished;

        public:
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// ctor.
            /// \param[in] eventProcessor_ Callback to process window events.
            /// \param[in] userData_ Optional user data passed to eventProcessor.
            /// \param[in] wnd_ Windows window handle.
            /// NOTE: SystemRunLoop takes ownership of the passed in wnd_ and
            /// will destroy it in it's dtor.
            SystemRunLoop (
                EventProcessor eventProcessor_ = 0,
                void *userData_ = 0,
                HWND wnd_ = CreateThreadWindow ());
            /// \brief
            /// dtor.
            virtual ~SystemRunLoop ();
        #elif defined (TOOLCHAIN_OS_Linux)
            /// \brief
            /// ctor.
            /// \param[in] eventProcessor_ Callback to process Xlib XEvent events.
            /// \param[in] userData_ Optional user data passed to eventProcessor.
            /// \param[in] displayName_ Xlib server display name.
            /// NOTE: More often then not you call XOpenDisplay (0) to open the
            /// connection to the main X server. If that's the case, leave
            /// displayName_ = 0. It exists to enable those cases where you
            /// open a connection to a specific server. In that case, pass a
            /// valid display name to SystemRunLoop so that it can dispatch
            /// events correctly.
            SystemRunLoop (
                EventProcessor eventProcessor_,
                void *userData_ = 0,
                const char *displayName_ = 0);
            /// \brief
            /// dtor.
            virtual ~SystemRunLoop ();
        #elif defined (TOOLCHAIN_OS_OSX)
            /// \brief
            /// ctor.
            /// \param[in] runLoop_ OS X run loop object.
            SystemRunLoop (CFRunLoopRef runLoop_ = CFRunLoopGetCurrent ());
        #endif // defined (TOOLCHAIN_OS_Windows)

        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Create a run loop window to service job requests.
            /// \return Window that will process job requests.
            static HWND CreateThreadWindow ();
            /// \brief
            /// Return the window associated with this run loop.
            /// \return Window associated with this run loop.
            inline HWND GetWindow () const {
                return wnd;
            }
        #elif defined (TOOLCHAIN_OS_Linux)
            /// \brief
            /// Dispatch an event to the given run loop.
            /// \param[in] event Event to dispatch.
            /// \param[in] runLoop SystemRunLoop this event belongs to.
            /// \return true = continue dispatching events,
            /// false = break out of the run loop.
            static bool DispatchEvent (
                const XEvent &event,
                RunLoop &runLoop);
        #elif defined (TOOLCHAIN_OS_OSX)
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
            virtual void Stop ();

            /// \brief
            /// Return true if Start was called.
            /// \return true if Start was called.
            virtual bool IsRunning ();

            /// \brief
            /// Enqueue a job to be performed on the run loop's thread.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish.
            /// Used for synchronous job execution.
            virtual void Enq (
                JobQueue::Job::UniquePtr job,
                bool wait = false);

        private:
        #if defined (TOOLCHAIN_OS_Linux)
            enum {
                /// \brief
                /// Execute the next waiting job.
                ID_RUN_LOOP,
                /// \brief
                /// Stop the run loop.
                ID_STOP
            };
            /// \brief
            /// Send an event with the given id.
            /// \param[in] id Event id.
            void SendEvent (long id);
        #endif // defined (TOOLCHAIN_OS_Linux)
            /// \brief
            /// Used internally to execute the next job.
            void ExecuteJob ();

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
