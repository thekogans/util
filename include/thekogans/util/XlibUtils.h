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

#if !defined (__thekogans_util_XlibUtils_h)
#define __thekogans_util_XlibUtils_h

#include "thekogans/util/Environment.h"

#if defined (TOOLCHAIN_OS_Linux) && defined (THEKOGANS_UTIL_HAVE_XLIB)

#include <X11/Xlib.h>
#include <vector>

namespace thekogans {
    namespace util {
        namespace xlib {

        /// \brief
        /// Call XInitThreads and setup error handling callbacks.
        /// IMPORTANT: This method must be called before any other
        /// calls to Xlib.
        void XlibInit ();

        /// \struct DisplayGuard XlibUtils.h thekogans/util/XlibUtils.h
        ///
        /// \brief
        /// Xlib is not thread safe. This Display guard will lock the display
        /// in it's ctor, and unlock it in it's dtor. Use it anytime you use
        /// Xlib functions that take a Display parameter.

        struct DisplayGuard {
            /// \brief
            /// Xlib Display to Lock/Unlock.
            Display *display;

            /// \brief
            /// ctor. Call XLockDisplay (display);
            /// \param[in] display Xlib Display to Lock/Unlock.
            DisplayGuard (Display *display_);
            /// \brief
            /// dtor. Call XUnlockDisplay (display);
            ~DisplayGuard ();
        };

        /// \brief
        /// Return a list of connections (Display) to all X-servers running on the system.
        /// \param[in] path Path where displays are located.
        /// \param[in] pattern Display file name pattern.
        /// NOTE: More often than not, displays have the following pattern: "X%d". If you
        /// have a custom X11 install, supply the pattern that works for you. Keep in mind
        /// that your pattern needs to expose a display number.
        /// \return A list of connections (Display) to all X-servers running on the system.
        std::vector<Display *> EnumerateDisplays (
            const char *path = "/tmp/.X11-unix",
            const char *pattern = "X%d");

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
                typedef std::unique_ptr<XlibWindow> UniquePtr;

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

        } // namespace xlib
    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Linux) && defined (THEKOGANS_UTIL_HAVE_XLIB)

#endif // !defined (__thekogans_util_XlibUtils_h)
