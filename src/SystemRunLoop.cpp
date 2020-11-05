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

#if !defined (TOOLCHAIN_OS_Linux) || defined (THEKOGANS_UTIL_HAVE_XLIB)

#if defined (TOOLCHAIN_OS_Linux)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/epoll.h>
    #include <cstring>
#endif // defined (TOOLCHAIN_OS_Linux)
#include "thekogans/util/HRTimer.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/SystemRunLoop.h"
#if defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/OSXUtils.h"
#endif // defined (TOOLCHAIN_OS_OSX)

namespace thekogans {
    namespace util {

    #if defined (TOOLCHAIN_OS_Windows)
        SystemRunLoop::SystemRunLoop (
                const std::string &name,
                JobExecutionPolicy::Ptr jobExecutionPolicy,
                EventProcessor eventProcessor_,
                void *userData_,
                Window::Ptr window_) :
                RunLoop (name, jobExecutionPolicy),
                eventProcessor (eventProcessor_),
                userData (userData_),
                window (std::move (window_)) {
            if (window.get () != 0) {
                SetWindowLongPtr (window->wnd, GWLP_USERDATA, (LONG_PTR)this);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        #define RUN_LOOP_MESSAGE WM_USER

        LRESULT CALLBACK WndProc (
                HWND wnd,
                UINT message,
                WPARAM wParam,
                LPARAM lParam) {
            switch (message) {
                case RUN_LOOP_MESSAGE: {
                    SystemRunLoop *runLoop =
                        (SystemRunLoop *)GetWindowLongPtr (wnd, GWLP_USERDATA);
                    runLoop->ExecuteJobs ();
                    return 0;
                }
                case WM_DESTROY:
                case WM_CLOSE: {
                    PostQuitMessage (0);
                    return 0;
                }
            }
            SystemRunLoop *runLoop =
                (SystemRunLoop *)GetWindowLongPtr (wnd, GWLP_USERDATA);
            return runLoop != 0 && runLoop->eventProcessor != 0 ?
                runLoop->eventProcessor (wnd, message, wParam, lParam, runLoop->userData) :
                DefWindowProc (wnd, message, wParam, lParam);
        }

        namespace {
            const char * const CLASS_NAME = "thekogans_util_SystemRunLoop_Window_class";
            const char * const WINDOW_NAME = "thekogans_util_SystemRunLoop_Window_name";
        }

        Window::Ptr SystemRunLoop::CreateThreadWindow () {
            static WindowClass *windowClass = new WindowClass (CLASS_NAME, WndProc);
            return Window::Ptr (new Window (*windowClass, Rectangle (), WINDOW_NAME, WS_POPUP));
        }
    #elif defined (TOOLCHAIN_OS_Linux)
        SystemRunLoop::XlibWindow::_Display::_Display (Display *display_) :
                display (display_ == 0 ? XOpenDisplay (0) : display_),
                owner (display_ == 0) {
            if (display == 0) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s", "Unable to create SystemRunLoop display.");
            }
        }

        SystemRunLoop::XlibWindow::_Display::~_Display () {
            if (owner) {
                XCloseDisplay (display);
            }
        }

        SystemRunLoop::XlibWindow::_Window::_Window (
                Display *display_,
                Window window_) :
                display (display_),
                window (window_ == 0 ? CreateWindow () : window_),
                owner (window_ == 0) {
            if (window == 0) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s", "Unable to create SystemRunLoop window.");
            }
        }

        SystemRunLoop::XlibWindow::_Window::~_Window () {
            if (owner) {
                DisplayGuard guard (display);
                XDestroyWindow (display, window);
            }
        }

        Window SystemRunLoop::XlibWindow::_Window::CreateWindow () {
            DisplayGuard guard (display);
            return XCreateSimpleWindow (
                display,
                DefaultRootWindow (display),
                0, 0, 0, 0, 0,
                BlackPixel (display, DefaultScreen (display)),
                BlackPixel (display, DefaultScreen (display)));
        }

        namespace {
            const char * const MESSAGE_TYPE_NAME = "thekogans_util_SystemRunLoop_message_type";

            bool GetEvent (
                    Display *display,
                    XEvent &event) {
                DisplayGuard guard (display);
                if (XPending (display) > 0) {
                    XNextEvent (display, &event);
                    return true;
                }
                return false;
            }
        }

        SystemRunLoop::XlibWindow::XlibWindow (
                Display *display_,
                Window window_) :
                display (display_),
                window (display.display, window_),
                message_type (XInternAtom (display.display, MESSAGE_TYPE_NAME, False)) {
            if (message_type == 0) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s", "Unable to create SystemRunLoop atom.");
            }
        }

        void SystemRunLoop::XlibWindow::PostEvent (long id) {
            XClientMessageEvent event;
            memset (&event, 0, sizeof (event));
            event.type = ClientMessage;
            event.message_type = message_type;
            event.format = 32;
            event.data.l[0] = id;
            DisplayGuard guard (display.display);
            XSendEvent (display.display, window.window, False, 0, (XEvent *)&event);
        }

        SystemRunLoop::SystemRunLoop (
                const std::string &name,
                JobExecutionPolicy::Ptr jobExecutionPolicy,
                EventProcessor eventProcessor_,
                void *userData_,
                XlibWindow::Ptr window_,
                const std::vector<Display *> &displays_) :
                RunLoop (name, jobExecutionPolicy),
                eventProcessor (eventProcessor_),
                userData (userData_),
                window (std::move (window_)),
                displays (displays_) {
            if (window.get () == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        SystemRunLoop::XlibWindow::Ptr SystemRunLoop::CreateThreadWindow (
                Display *display) {
            return XlibWindow::Ptr (new XlibWindow (display));
        }

        bool SystemRunLoop::DispatchEvent (
                Display *display,
                const XEvent &event) {
            if (display == window->display.display &&
                    event.type == ClientMessage &&
                    event.xclient.window == window->window.window &&
                    event.xclient.message_type == window->message_type) {
                if (event.xclient.data.l[0] == XlibWindow::ID_RUN_LOOP) {
                    ExecuteJobs ();
                }
                else if (event.xclient.data.l[0] == XlibWindow::ID_STOP) {
                    return false;
                }
            }
            else if (eventProcessor == 0) {
                eventProcessor (display, event, userData);
            }
            return true;
        }
    #elif defined (TOOLCHAIN_OS_OSX)
        namespace {
            struct CFRunLoopSourceRefDeleter {
                void operator () (CFRunLoopSourceRef runLoopSourceRef) {
                    if (runLoopSourceRef != 0) {
                        CFRelease (runLoopSourceRef);
                    }
                }
            };
            typedef std::unique_ptr<__CFRunLoopSource, CFRunLoopSourceRefDeleter> CFRunLoopSourceRefPtr;

            void DoNothingRunLoopCallback (void * /*info*/) {
            }
        }

        void SystemRunLoop::CFOSXRunLoop::Start () {
            // Create a dummy source so that CFRunLoopRun has
            // something to wait on.
            CFRunLoopSourceContext context = {0};
            context.perform = DoNothingRunLoopCallback;
            CFRunLoopSourceRefPtr runLoopSource (CFRunLoopSourceCreate (0, 0, &context));
            if (runLoopSource.get () != 0) {
                CFRunLoopAddSource (runLoop, runLoopSource.get (), kCFRunLoopCommonModes);
                CFRunLoopRun ();
                CFRunLoopRemoveSource (runLoop, runLoopSource.get (), kCFRunLoopCommonModes);
            }
            else {
                THEKOGANS_UTIL_THROW_SC_ERROR_CODE_EXCEPTION (SCError ());
            }
        }

        void SystemRunLoop::CFOSXRunLoop::Stop () {
            CFRunLoopStop (runLoop);
        }

        SystemRunLoop::SystemRunLoop (
                const std::string &name,
                JobExecutionPolicy::Ptr jobExecutionPolicy,
                OSXRunLoop::Ptr runLoop_) :
                RunLoop (name, jobExecutionPolicy),
                runLoop (runLoop_) {
            if (runLoop.Get () == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        void SystemRunLoop::Start () {
            done = false;
            ExecuteJobs ();
        #if defined (TOOLCHAIN_OS_Windows)
            BOOL result;
            MSG msg;
            while ((result = GetMessage (&msg, 0, 0, 0)) != 0) {
                if (result != -1) {
                    TranslateMessage (&msg);
                    DispatchMessage (&msg);
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
        #elif defined (TOOLCHAIN_OS_Linux)
            enum {
                DEFAULT_MAX_SIZE = 256
            };
            struct epoll {
                THEKOGANS_UTIL_HANDLE handle;
                explicit epoll (ui32 maxSize) :
                        handle (epoll_create (maxSize)) {
                    if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                }
                ~epoll () {
                    close (handle);
                }
            } epoll (DEFAULT_MAX_SIZE);
            {
                epoll_event event = {0};
                event.events = EPOLLIN;
                event.data.ptr = window->display.display;
                if (epoll_ctl (epoll.handle, EPOLL_CTL_ADD,
                        ConnectionNumber (window->display.display), &event) < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
            for (std::size_t i = 0, count = displays.size (); i < count; ++i) {
                if (displays[i] != window->display.display) {
                    epoll_event event = {0};
                    event.events = EPOLLIN;
                    event.data.ptr = displays[i];
                    if (epoll_ctl (epoll.handle, EPOLL_CTL_ADD,
                            ConnectionNumber (displays[i]), &event) < 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                }
            }
            while (!done) {
                std::vector<epoll_event> events (DEFAULT_MAX_SIZE);
                int count = epoll_wait (epoll.handle, events.data (), DEFAULT_MAX_SIZE, -1);
                if (count < 0) {
                    THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                    // EINTR means a signal interrupted our wait.
                    if (errorCode != EINTR) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
                else {
                    for (int i = 0; i < count; ++i) {
                        Display *display = (Display *)events[i].data.ptr;
                        if (display != 0) {
                            if (events[i].events & EPOLLERR) {
                                THEKOGANS_UTIL_ERROR_CODE errorCode = 0;
                                socklen_t length = sizeof (errorCode);
                                if (getsockopt (
                                        ConnectionNumber (display),
                                        SOL_SOCKET,
                                        SO_ERROR,
                                        &errorCode,
                                        &length) == -1) {
                                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                        THEKOGANS_UTIL_OS_ERROR_CODE);
                                }
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                            }
                            else if (events[i].events & EPOLLIN) {
                                XEvent event;
                                while (GetEvent (display, event)) {
                                    if (!DispatchEvent (display, event)) {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        #elif defined (TOOLCHAIN_OS_OSX)
            runLoop->Start ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        bool SystemRunLoop::Stop (
                bool cancelRunningJobs,
                bool cancelPendingJobs,
                const TimeSpec &timeSpec) {
            TimeSpec deadline = GetCurrentTime () + timeSpec;
            if (!Pause (cancelRunningJobs, deadline - GetCurrentTime ())) {
                return false;
            }
            if (cancelPendingJobs) {
                CancelPendingJobs ();
                Continue ();
                if (!WaitForIdle (deadline - GetCurrentTime ())) {
                    return false;
                }
            }
            done = true;
            Continue ();
        #if defined (TOOLCHAIN_OS_Windows)
            PostMessage (window->wnd, WM_CLOSE, 0, 0);
        #elif defined (TOOLCHAIN_OS_Linux)
            window->PostEvent (XlibWindow::ID_STOP);
        #elif defined (TOOLCHAIN_OS_OSX)
            runLoop->Stop ();
        #endif // defined (TOOLCHAIN_OS_Windows)
            idle.SignalAll ();
            return true;
        }

        bool SystemRunLoop::IsRunning () {
            return !done;
        }

        bool SystemRunLoop::EnqJob (
                Job::Ptr job,
                bool wait,
                const TimeSpec &timeSpec) {
            bool result = RunLoop::EnqJob (job);
            if (result) {
            #if defined (TOOLCHAIN_OS_Windows)
                PostMessage (window->wnd, RUN_LOOP_MESSAGE, 0, 0);
            #elif defined (TOOLCHAIN_OS_Linux)
                window->PostEvent (XlibWindow::ID_RUN_LOOP);
            #elif defined (TOOLCHAIN_OS_OSX)
                CFRunLoopPerformBlock (
                    runLoop->GetCFRunLoop (),
                    kCFRunLoopCommonModes,
                    ^(void) {
                        ExecuteJobs ();
                    });
                CFRunLoopWakeUp (runLoop->GetCFRunLoop ());
            #endif // defined (TOOLCHAIN_OS_Windows)
                result = !wait || WaitForJob (job, timeSpec);
            }
            return result;
        }

        bool SystemRunLoop::EnqJobFront (
                Job::Ptr job,
                bool wait,
                const TimeSpec &timeSpec) {
            bool result = RunLoop::EnqJobFront (job);
            if (result) {
            #if defined (TOOLCHAIN_OS_Windows)
                PostMessage (window->wnd, RUN_LOOP_MESSAGE, 0, 0);
            #elif defined (TOOLCHAIN_OS_Linux)
                window->PostEvent (XlibWindow::ID_RUN_LOOP);
            #elif defined (TOOLCHAIN_OS_OSX)
                CFRunLoopPerformBlock (
                    runLoop->GetCFRunLoop (),
                    kCFRunLoopCommonModes,
                    ^(void) {
                        ExecuteJobs ();
                    });
                CFRunLoopWakeUp (runLoop->GetCFRunLoop ());
            #endif // defined (TOOLCHAIN_OS_Windows)
                result = !wait || WaitForJob (job, timeSpec);
            }
            return result;
        }

        void SystemRunLoop::ExecuteJobs () {
            while (!done) {
                Job *job = DeqJob (false);
                if (job == 0) {
                    break;
                }
                ui64 start = 0;
                ui64 end = 0;
                // Short circuit cancelled pending jobs.
                if (!job->ShouldStop (done)) {
                    start = HRTimer::Click ();
                    job->SetState (Job::Running);
                    job->Prologue (done);
                    job->Execute (done);
                    job->Epilogue (done);
                    job->Succeed (done);
                    end = HRTimer::Click ();
                }
                FinishedJob (job, start, end);
            }
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (TOOLCHAIN_OS_Linux) || defined (THEKOGANS_UTIL_HAVE_XLIB)
