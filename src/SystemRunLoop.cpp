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

namespace thekogans {
    namespace util {

    #if defined (TOOLCHAIN_OS_Windows)
        SystemRunLoop::SystemRunLoop (
                const std::string &name_,
                JobQueue::Type type_,
                ui32 maxPendingJobs_,
                JobQueue::WorkerCallback *workerCallback_,
                bool done_,
                EventProcessor eventProcessor_,
                void *userData_,
                HWND wnd_) :
                name (name_),
                type (type_),
                maxPendingJobs (maxPendingJobs_),
                workerCallback (workerCallback_),
                done (done_),
                eventProcessor (eventProcessor_),
                userData (userData_),
                wnd (wnd_),
                jobFinished (jobsMutex),
                idle (jobsMutex),
                state (Idle),
                busyWorkers (0) {
            if (maxPendingJobs != 0 && wnd != 0) {
                SetWindowLongPtr (wnd, GWLP_USERDATA, (LONG_PTR)this);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        SystemRunLoop::~SystemRunLoop () {
            DestroyWindow (wnd);
            CancelAll ();
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
                const std::string &name_,
                JobQueue::Type type_,
                ui32 maxPendingJobs_,
                JobQueue::WorkerCallback *workerCallback_,
                bool done_,
                EventProcessor eventProcessor_,
                void *userData_,
                XlibWindow::Ptr window_,
                const std::vector<Display *> &displays_) :
                name (name_),
                type (type_),
                maxPendingJobs (maxPendingJobs_),
                workerCallback (workerCallback_),
                done (done_),
                eventProcessor (eventProcessor_),
                userData (userData_),
                window (std::move (window_)),
                displays (displays_),
                jobFinished (jobsMutex),
                idle (jobsMutex),
                state (Idle),
                busyWorkers (0) {
            if (maxPendingJobs == 0 || window.get () == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        SystemRunLoop::~SystemRunLoop () {
            CancelAll ();
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
        SystemRunLoop::SystemRunLoop (
                const std::string &name_,
                JobQueue::Type type_,
                ui32 maxPendingJobs_,
                JobQueue::WorkerCallback *workerCallback_,
                bool done_,
                CFRunLoopRef runLoop_) :
                name (name_),
                type (type_),
                maxPendingJobs (maxPendingJobs_),
                workerCallback (workerCallback_),
                done (done_),
                runLoop (runLoop_),
                jobFinished (jobsMutex),
                idle (jobsMutex),
                state (Idle),
                busyWorkers (0) {
            if (maxPendingJobs == 0 || runLoop == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        SystemRunLoop::~SystemRunLoop () {
            CancelAll ();
        }

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
    #endif // defined (TOOLCHAIN_OS_Windows)

        void SystemRunLoop::Start () {
            if (SetDone (false)) {
                struct WorkerInitializer {
                    SystemRunLoop &runLoop;
                    explicit WorkerInitializer (SystemRunLoop &runLoop_) :
                            runLoop (runLoop_) {
                        if (runLoop.workerCallback != 0) {
                            runLoop.workerCallback->InitializeWorker ();
                        }
                    }
                    ~WorkerInitializer () {
                        if (runLoop.workerCallback != 0) {
                            runLoop.workerCallback->UninitializeWorker ();
                        }
                    }
                } workerInitializer (*this);
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
                    int count = epoll_wait (epoll.handle, &events[0], DEFAULT_MAX_SIZE, -1);
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
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        }

        void SystemRunLoop::Stop (bool cancelPendingJobs) {
            if (SetDone (true)) {
            #if defined (TOOLCHAIN_OS_Windows)
                PostMessage (wnd, WM_CLOSE, 0, 0);
            #elif defined (TOOLCHAIN_OS_Linux)
                window->PostEvent (XlibWindow::ID_STOP);
            #elif defined (TOOLCHAIN_OS_OSX)
                CFRunLoopStop (runLoop);
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            if (cancelPendingJobs) {
                CancelAll ();
            }
        }

        void SystemRunLoop::Enq (
                JobQueue::Job &job,
                bool wait) {
            LockGuard<Mutex> guard (jobsMutex);
            if (stats.jobCount < maxPendingJobs) {
                job.finished = false;
                if (type == JobQueue::TYPE_FIFO) {
                    jobs.push_back (&job);
                }
                else {
                    jobs.push_front (&job);
                }
                job.AddRef ();
                ++stats.jobCount;
                state = Busy;
            #if defined (TOOLCHAIN_OS_Windows)
                PostMessage (wnd, RUN_LOOP_MESSAGE, 0, 0);
            #elif defined (TOOLCHAIN_OS_Linux)
                window->PostEvent (XlibWindow::ID_RUN_LOOP);
            #elif defined (TOOLCHAIN_OS_OSX)
                CFRunLoopPerformBlock (
                    runLoop,
                    kCFRunLoopCommonModes,
                    ^(void) {
                        ExecuteJobs ();
                    });
                CFRunLoopWakeUp (runLoop);
            #endif // defined (TOOLCHAIN_OS_Windows)
                if (wait) {
                    while (!job.ShouldStop (done) && !job.finished) {
                        jobFinished.Wait ();
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "SystemRunLoop (%s) max jobs (%u) reached.",
                    !name.empty () ? name.c_str () : "no name",
                    maxPendingJobs);
            }
        }

        bool SystemRunLoop::Cancel (const JobQueue::Job::Id &jobId) {
            LockGuard<Mutex> guard (jobsMutex);
            for (JobQueue::Job *job = jobs.front (); job != 0; job = jobs.next (job)) {
                if (job->GetId () == jobId) {
                    jobs.erase (job);
                    job->Cancel ();
                    job->Release ();
                    --stats.jobCount;
                    jobFinished.SignalAll ();
                    if (busyWorkers == 0 && jobs.empty ()) {
                        state = Idle;
                        idle.SignalAll ();
                    }
                    return true;
                }
            }
            return false;
        }

        void SystemRunLoop::CancelAll () {
            LockGuard<Mutex> guard (jobsMutex);
            if (!jobs.empty ()) {
                assert (state == Busy);
                struct Callback : public JobQueue::JobList::Callback {
                    typedef JobQueue::JobList::Callback::result_type result_type;
                    typedef JobQueue::JobList::Callback::argument_type argument_type;
                    virtual result_type operator () (argument_type job) {
                        job->Cancel ();
                        job->Release ();
                        return true;
                    }
                } callback;
                jobs.clear (callback);
                stats.jobCount = 0;
                jobFinished.SignalAll ();
                if (busyWorkers == 0) {
                    state = Idle;
                    idle.SignalAll ();
                }
            }
        }

        void SystemRunLoop::WaitForIdle () {
            LockGuard<Mutex> guard (jobsMutex);
            while (state == Busy) {
                idle.Wait ();
            }
        }

        JobQueue::Stats SystemRunLoop::GetStats () {
            LockGuard<Mutex> guard (jobsMutex);
            return stats;
        }

        bool SystemRunLoop::IsRunning () {
            LockGuard<Mutex> guard (jobsMutex);
            return !done;
        }

        bool SystemRunLoop::IsEmpty () {
            LockGuard<Mutex> guard (jobsMutex);
            return jobs.empty ();
        }

        bool SystemRunLoop::IsIdle () {
            LockGuard<Mutex> guard (jobsMutex);
            return state == Idle;
        }

        void SystemRunLoop::ExecuteJobs () {
            while (!done) {
                JobQueue::Job::Ptr job (Deq ());
                if (job.Get () == 0) {
                    break;
                }
                ui64 start = HRTimer::Click ();
                job->Prologue (done);
                job->Execute (done);
                job->Epilogue (done);
                ui64 end = HRTimer::Click ();
                FinishedJob (*job, start, end);
            }
        }

        JobQueue::Job *SystemRunLoop::Deq () {
            LockGuard<Mutex> guard (jobsMutex);
            JobQueue::Job *job = 0;
            if (!done && !jobs.empty ()) {
                job = jobs.pop_front ();
                --stats.jobCount;
                ++busyWorkers;
            }
            return job;
        }

        void SystemRunLoop::FinishedJob (
                JobQueue::Job &job,
                ui64 start,
                ui64 end) {
            LockGuard<Mutex> guard (jobsMutex);
            assert (state == Busy);
            stats.Update (job.id, start, end);
            job.finished = true;
            jobFinished.SignalAll ();
            if (--busyWorkers == 0 && jobs.empty ()) {
                state = Idle;
                idle.SignalAll ();
            }
        }

        bool SystemRunLoop::SetDone (bool value) {
            LockGuard<Mutex> guard (jobsMutex);
            if (done != value) {
                done = value;
                return true;
            }
            return false;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (TOOLCHAIN_OS_Linux) || defined (THEKOGANS_UTIL_HAVE_XLIB)
