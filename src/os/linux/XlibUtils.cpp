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

#include "thekogans/util/Environment.h"

#if defined (TOOLCHAIN_OS_Linux) && defined (THEKOGANS_UTIL_HAVE_XLIB)

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <cstring>
#include <cstdio>
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Directory.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/os/linux/XlibUtils.h"

namespace thekogans {
    namespace util {
        namespace os {
            namespace linux {

                namespace {
                    int ErrorHandler (
                            Display *display,
                            XErrorEvent *errorEvent) {
                        char buffer[1024];
                        XGetErrorText (display, errorEvent->error_code, buffer, 1024);
                        THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (
                            THEKOGANS_UTIL,
                            "%s\n", buffer);
                        return 0;
                    }

                    int IOErrorHandler (Display *display) {
                        THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (
                            THEKOGANS_UTIL,
                            "%s\n", "Fatal Xlib IO error.");
                        return 0;
                    }
                }

                void XlibInit () {
                    XInitThreads ();
                    XSetErrorHandler (ErrorHandler);
                    XSetIOErrorHandler (IOErrorHandler);
                }

                XlibDispays::XlibDispays (
                        const char *path,
                        const char *pattern) {
                    Directory directory (path);
                    Directory::Entry entry;
                    for (bool gotEntry = directory.GetFirstEntry (entry);
                         gotEntry; gotEntry = directory.GetNextEntry (entry)) {
                        i32 dispayNumber;
                        if (sscanf (entry.name.c_str (), pattern, &dispayNumber) == 1) {
                            Display *display = XOpenDisplay (FormatString (":%d", dispayNumber).c_str ());
                            if (display != 0) {
                                displays.push_back (display);
                            }
                            else {
                                THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING (
                                    THEKOGANS_UTIL,
                                    "Unable to open display: %s\n",
                                    entry.name.c_str ());
                            }
                        }
                    }
                }

                XlibDisplayGuard::XlibDisplayGuard (Display *display_) :
                        display (display_) {
                    if (display != 0) {
                        XLockDisplay (display);
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }

                XlibDisplayGuard::~XlibDisplayGuard () {
                    XUnlockDisplay (display);
                }

                namespace {
                    struct XlibWindowMap :
                            public Singleton<XlibWindowMap, SpinLock>,
                            private std::map<Window, XlibWindowRegistry::Token::ValueType> {
                        SpinLock spinLok;

                        void Add (
                                Window window,
                                XlibWindowRegistry::Token::ValueType token) {
                            LockGuard<SpinLock> guard (spinLock);
                            insert (value_type (window, token));
                        }
                        void Remove (Window window) {
                            LockGuard<SpinLock> guard (spinLock);
                            erase (window);
                        }
                        XlibWindow::SharedPtr Get (Window window) {
                            LockGuard<SpinLock> guard (spinLock);
                            const_iterator it = find (window);
                            return it != end () ?
                                XlibWindowRegistry::Instance ().Get (it->second) :
                                XlibWindow::SharedPtr ();
                        }
                    };
                }

                XlibWindow::XlibWindow (
                        Display *display_,
                        Window window_) :
                        display (display_),
                        window (window_),
                        token (this) {
                    // Register the window with the XlibWindowMap so that XlibRunLoop::Begin
                    // can find it and route events to it.
                    XlibWindowMap::Instance ().Add (window, token.GetValue ());
                }

                XlibWindow::~XlibWindow () {
                    XlibWindowMap::Instance ().Remove (window);
                    XlibDisplayGuard guard (display);
                    XDestroyWindow (display, window);
                }


                XlibRunLoop::XlibRunLoop () {
                    if (!XlibDisplays::Instance ().displays.empty ()) {
                        Display *display = XlibDisplays::Instance ().displays[0];
                        XlibDisplayGuard guard (display);
                        window.Reset (
                            new XlibWindow (
                                display,
                                XCreateSimpleWindow (
                                    display,
                                    DefaultRootWindow (display),
                                    0, 0, 0, 0, 0,
                                    BlackPixel (display, DefaultScreen (display)),
                                    BlackPixel (display, DefaultScreen (display)))));
                        message_type = XInternAtom (display, MESSAGE_TYPE_NAME, False);
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "%s", "No connections to Xlib server found.");
                    }
                }

                namespace {
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

                void XlibRunLoop::Begin () {
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
                    for (std::size_t i = 0, count = XlibDisplays::Instance ().displays.size (); i < count; ++i) {
                        epoll_event event = {0};
                        event.events = EPOLLIN;
                        event.data.ptr = displays[i];
                        if (epoll_ctl (epoll.handle, EPOLL_CTL_ADD,
                                ConnectionNumber (XlibDisplays::Instance ().displays[i]), &event) < 0) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE);
                        }
                    }
                    while (1) {
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
                                            if (display == window->display &&
                                                    event.type == ClientMessage &&
                                                    event.xclient.window == window->window &&
                                                    event.xclient.message_type == message_type) {
                                                if (event.xclient.data.l[0] == ID_RUN_LOOP_EXECUTE_JOB) {
                                                    ExecuteJob ();
                                                }
                                                else if (event.xclient.data.l[0] == ID_RUN_LOOP_STOP) {
                                                    return;
                                                }
                                            }
                                            else {
                                                // Unlike Windows there's no window class registering a
                                                // callback function for a particular Xlib window on Linux.
                                                // Therefore we provide our own version of DispatchMessage.
                                                // For this to work user Xlib windows need to inherit from
                                                // XlibWindow.
                                                XlibWindow::SharedPtr window =
                                                    XlibWindowMap::Instance ().Get (event.xclient.window);
                                                if (window.Get () != 0) {
                                                    window->OnEvent (event);
                                                }
                                                else {
                                                    // As a last ditch effort to process the event call our OnEvent
                                                    // method. Derivatives should override this to provide catch all
                                                    // processing.
                                                    OnEvent (event);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                void XlibRunLoop::End () {
                    PostEvent (ID_RUN_LOOP_STOP);
                }

                void XlibRunLoop::ScheduleJob () {
                    PostEvent (ID_RUN_LOOP_EXECUTE_JOB);
                }

                void XlibRunLoop::PostEvent (long id) {
                    XClientMessageEvent event;
                    memset (&event, 0, sizeof (event));
                    event.type = ClientMessage;
                    event.message_type = message_type;
                    event.format = 32;
                    event.data.l[0] = id;
                    DisplayGuard guard (display);
                    XSendEvent (window->display, window->window, False, 0, (XEvent *)&event);
                }

            } // namespace linux
        } // namespace os
    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Linux) && defined (THEKOGANS_UTIL_HAVE_XLIB)
