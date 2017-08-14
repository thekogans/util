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
    #include <cstring>
#endif // defined (TOOLCHAIN_OS_Linux)
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/SystemRunLoop.h"

namespace thekogans {
    namespace util {

    #if defined (TOOLCHAIN_OS_Windows)
        SystemRunLoop::SystemRunLoop (
                EventProcessor eventProcessor_,
                void *userData_,
                HWND wnd_) :
                done (true),
                eventProcessor (eventProcessor_),
                userData (userData_),
                wnd (wnd_),
                jobFinished (jobsMutex) {
            if (wnd != 0) {
                SetWindowLongPtr (wnd, GWLP_USERDATA, (LONG_PTR)this);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        SystemRunLoop::~SystemRunLoop () {
            DestroyWindow (wnd);
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
                    runLoop->ExecuteJob ();
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

            ATOM RegisterWindowClass () {
                WNDCLASSEX wndClassEx;
                wndClassEx.cbSize = sizeof (WNDCLASSEX);
                wndClassEx.style = CS_HREDRAW | CS_VREDRAW;
                wndClassEx.lpfnWndProc = WndProc;
                wndClassEx.cbClsExtra = 0;
                wndClassEx.cbWndExtra = 0;
                wndClassEx.hInstance = GetModuleHandle (0);
                wndClassEx.hIcon = 0;
                wndClassEx.hCursor = LoadCursor (0, IDC_ARROW);
                wndClassEx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
                wndClassEx.lpszMenuName = 0;
                wndClassEx.lpszClassName = CLASS_NAME;
                wndClassEx.hIconSm = 0;
                ATOM atom = RegisterClassEx (&wndClassEx);
                if (atom != 0) {
                    return atom;
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
        }

        HWND SystemRunLoop::CreateThreadWindow () {
            static const ATOM atom = RegisterWindowClass ();
            HWND wnd = CreateWindow (
                CLASS_NAME,
                WINDOW_NAME,
                WS_POPUP,
                0, 0, 0, 0,
                0, 0,
                GetModuleHandle (0),
                0);
            if (wnd != 0) {
                return wnd;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        }
    #elif defined (TOOLCHAIN_OS_Linux)
        namespace {
            const char * const MESSAGE_TYPE_NAME = "thekogans_util_SystemRunLoop_message_type";
        }

        SystemRunLoop::SystemRunLoop (
                EventProcessor eventProcessor_,
                void *userData_,
                const char *displayName_) :
                done (true),
                eventProcessor (eventProcessor_),
                userData (userData_),
                displayName (displayName_),
                jobFinished (jobsMutex) {
            if (eventProcessor != 0) {
                display = XOpenDisplay (displayName);
                if (display != 0) {
                    window = XCreateSimpleWindow (
                        display,
                        DefaultRootWindow (display),
                        0, 0, 0, 0, 0,
                        BlackPixel (display, DefaultScreen (display)),
                        BlackPixel (display, DefaultScreen (display)));
                    if (window != 0) {
                        XSelectInput (display, window, StructureNotifyMask);
                        message_type = XInternAtom (display, MESSAGE_TYPE_NAME, False);
                        if (message_type == 0) {
                            XDestroyWindow (display, window);
                            XCloseDisplay (display);
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "%s", "Unable to create SystemRunLoop atom.");
                        }
                    }
                    else {
                        XCloseDisplay (display);
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "%s", "Unable to create SystemRunLoop window.");
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "%s", "Unable to create SystemRunLoop display.");
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        SystemRunLoop::~SystemRunLoop () {
            XDestroyWindow (display, window);
            XCloseDisplay (display);
        }

        bool SystemRunLoop::DispatchEvent (
                const XEvent &event,
                RunLoop &runLoop) {
            SystemRunLoop *systemRunLoop = dynamic_cast<SystemRunLoop *> (&runLoop);
            if (systemRunLoop != 0) {
                if (event.type == ClientMessage &&
                        event.xclient.window == systemRunLoop->window &&
                        event.xclient.message_type == systemRunLoop->message_type) {
                    if (event.xclient.data.l[0] == ID_STOP) {
                        return false;
                    }
                    systemRunLoop->ExecuteJob ();
                    return true;
                }
                return systemRunLoop->eventProcessor (event, systemRunLoop->userData);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }
    #elif defined (TOOLCHAIN_OS_OSX)
        SystemRunLoop::SystemRunLoop (CFRunLoopRef runLoop_) :
                done (true),
                runLoop (runLoop_),
                jobFinished (jobsMutex) {
            if (runLoop == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        void SystemRunLoop::Start () {
            if (done) {
                done = false;
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
                while (1) {
                    XEvent event;
                    XNextEvent (display, &event);
                    if (!DispatchEvent (event, *this)) {
                        break;
                    }
                }
            #elif defined (TOOLCHAIN_OS_OSX)
                CFRunLoopRun ();
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        }

        void SystemRunLoop::Stop () {
            if (!done) {
                done = true;
            #if defined (TOOLCHAIN_OS_Windows)
                PostMessage (wnd, WM_CLOSE, 0, 0);
            #elif defined (TOOLCHAIN_OS_Linux)
                SendEvent (ID_STOP);
            #elif defined (TOOLCHAIN_OS_OSX)
                CFRunLoopStop (runLoop);
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        }

        bool SystemRunLoop::IsRunning () {
            return !done;
        }

        void SystemRunLoop::Enq (
                JobQueue::Job::UniquePtr job,
                bool wait) {
            if (job.get () != 0) {
                LockGuard<Mutex> guard (jobsMutex);
                volatile bool finished = false;
                if (wait) {
                    job->finished = &finished;
                }
                jobs.push_back (std::move (job));
            #if defined (TOOLCHAIN_OS_Windows)
                PostMessage (wnd, RUN_LOOP_MESSAGE, 0, 0);
            #elif defined (TOOLCHAIN_OS_Linux)
                SendEvent (ID_RUN_LOOP);
            #elif defined (TOOLCHAIN_OS_OSX)
                CFRunLoopPerformBlock (
                    runLoop,
                    kCFRunLoopCommonModes,
                    ^(void) {
                        ExecuteJob ();
                    });
                CFRunLoopWakeUp (runLoop);
            #endif // defined (TOOLCHAIN_OS_Windows)
                while (wait && !finished) {
                    jobFinished.Wait ();
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    #if defined (TOOLCHAIN_OS_Linux)
        void SystemRunLoop::SendEvent (long id) {
            // FIXME: Read about this here: https://ubuntuforums.org/archive/index.php/t-570702.html
            // If creating and destroying displays is too expensive, we'll need to rethink this.
            Display *display = XOpenDisplay (displayName);
            if (display != 0) {
                XClientMessageEvent event;
                memset (&event, 0, sizeof (event));
                event.type = ClientMessage;
                event.message_type = message_type;
                event.format = 32;
                event.data.l[0] = id;
                XSendEvent (display, window, False, 0, (XEvent *)&event);
                XFlush (display);
                XCloseDisplay (display);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Unable to open display: %s",
                    displayName != 0 ? displayName : "default");
            }
        }
    #endif // defined (TOOLCHAIN_OS_Linux)

        void SystemRunLoop::ExecuteJob () {
            JobQueue::Job::UniquePtr job;
            {
                LockGuard<Mutex> guard (jobsMutex);
                if (!jobs.empty ()) {
                    job = std::move (jobs.front ());
                    jobs.pop_front ();
                }
            }
            if (job.get () != 0) {
                bool done = false;
                job->Prologue (done);
                job->Execute (done);
                job->Epilogue (done);
                if (job->finished != 0) {
                    *job->finished = true;
                    jobFinished.SignalAll ();
                }
            }
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (TOOLCHAIN_OS_Linux) || defined (THEKOGANS_UTIL_HAVE_XLIB)
