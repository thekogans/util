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

#if !defined (__thekogans_util_os_linux_XlibUtils_h)
#define __thekogans_util_os_linux_XlibUtils_h

#include "thekogans/util/Environment.h"

#if defined (TOOLCHAIN_OS_Linux) && defined (THEKOGANS_UTIL_HAVE_XLIB)

#include <X11/Xlib.h>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/RefCountedRegistry.h"
#include "thekogans/util/os/RunLoop.h"


namespace thekogans {
    namespace util {
        namespace os {
            namespace linux {

                /// \brief
                /// Call XInitThreads and setup error handling callbacks.
                /// IMPORTANT: This method must be called before any other
                /// calls to Xlib.
                void XlibInit ();

                /// \struct XlibDisplays XlibUtils.h thekogans/util/XlibUtils.h
                ///
                /// \brief
                /// List of all X-servers running on the system.

                struct XlibDispays : public Singleton<XlibDispays> {
                    /// \brief
                    /// A list of connections (Display) to all X-servers running on the system.
                    std::vector<Display *> displays;

                    /// \brief
                    /// ctor.
                    /// \param[in] path Path where displays are located.
                    /// \param[in] pattern Display file name pattern.
                    /// NOTE: More often than not, displays have the following pattern: "X%d". If you
                    /// have a custom X11 install, supply the pattern that works for you. Keep in mind
                    /// that your pattern needs to expose a display number.
                    XlibDispays (
                        const char *path = "/tmp/.X11-unix",
                        const char *pattern = "X%d");
                };

                /// \struct XlibDisplayGuard XlibUtils.h thekogans/util/XlibUtils.h
                ///
                /// \brief
                /// Xlib is not thread safe. This Display guard will lock the display
                /// in it's ctor, and unlock it in it's dtor. Use it anytime you use
                /// Xlib functions that take a Display parameter.

                struct XlibDisplayGuard {
                    /// \brief
                    /// Xlib Display to Lock/Unlock.
                    Display *display;

                    /// \brief
                    /// ctor. Call XLockDisplay (display);
                    /// \param[in] display Xlib Display to Lock/Unlock.
                    XlibDisplayGuard (Display *display_);
                    /// \brief
                    /// dtor. Call XUnlockDisplay (display);
                    ~XlibDisplayGuard ();
                };

                /// \struct SystemRunLoop::XlibWindow SystemRunLoop.h thekogans/util/SystemRunLoop.h
                ///
                /// \brief
                /// Encapsulates an Xlib Window. XlibWindow provides barebones functionality needed
                /// to wire an Xlib window in to our \see{XlibRunLoop}. The only interface it exposes
                /// is OnEvent. An XlibWindow derivative will override this method to react to events
                /// sent to this window. If you're planning on using \see{MainRunLoop} in your main
                /// thread and you're going to create Xlib windows you must derive your windows from
                /// XlibWindow.

                struct XlibWindow : public virtual RefCounted {
                    /// \brief
                    /// Declare \see{util::RefCounted} pointers.
                    THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (XlibWindow)

                    /// \brief
                    /// Alias for RefCountedRegistry<XlibWindow>.
                    using Registry = RefCountedRegistry<XlibWindow>;

                protected:
                    /// \brief
                    /// Xlib Display.
                    Display *display;
                    /// \brief
                    /// Xlib Window.
                    Window window;
                    /// \brief
                    /// Registry token.
                    const Registry::Token token;

                public:
                    /// \brief
                    /// ctor.
                    /// \param[in] display_ Xlib Display.
                    /// \param[in] window_ Xlib Window.
                    /// IMPORTANT: The Window is owned by this XlibWindow
                    /// and it will call XDestroyWindow in it's dtor.
                    XlibWindow (
                        Display *display_,
                        Window window_);
                    /// \brief
                    /// dtor.
                    virtual ~XlibWindow ();

                    /// \brief
                    /// Override this method in your derivatives to react to
                    /// events sent to your window.
                    /// \param[in] event XEvent sent.
                    virtual void OnEvent (const XEvent & /*event*/) noexcept {}

                    /// \brief
                    /// XlibWindow is neither copy constructable, nor assignable.
                    THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (XlibWindow)

                    /// \brief
                    /// XlibRunLoop needs access to protected members.
                    friend struct XlibRunLoop;
                };

                /// \struct XlibRunLoop XlibUtils.h thekogans/util/os/linux/XlibUtils.h
                ///
                /// \brief
                /// Xlib based run loop.

                struct XlibRunLoop : public os::RunLoop {
                private:
                    /// \brief
                    /// Run loop custom message type.
                    static const char * const MESSAGE_TYPE_NAME =
                        "thekogans_util_os_linux_XlibRunLoop_message_type";
                    enum {
                        /// \brief
                        /// Execute the next waiting job.
                        ID_RUN_LOOP_EXECUTE_JOB,
                        /// \brief
                        /// Stop the run loop.
                        ID_RUN_LOOP_STOP
                    };
                    /// \brief
                    /// Window that will receive run loop notifications.
                    XlibWindow::SharedPtr window;
                    /// \brief
                    /// A custom Xlib message type used to signal our run loop.
                    Atom message_type;

                public:
                    /// \brief
                    /// ctor.
                    XlibRunLoop ();

                    /// \brief
                    /// Start the run loop.
                    virtual void Begin () override;
                    /// \brief
                    /// Stop the run loop.
                    virtual void End () override;
                    /// \brief
                    /// Post ID_RUN_LOOP_EXECUTE_JOB to the thread to signal
                    /// that job processing should take place.
                    virtual void ScheduleJob () override;

                    /// \brief
                    /// Override this method in your derivatives to react to
                    /// events sent to your windows.
                    /// \param[in] event XEvent sent.
                    virtual void OnEvent (const XEvent & /*event*/) noexcept {}

                private:
                    /// \brief
                    /// Post an event with the given id.
                    /// \param[in] id Event id.
                    void PostEvent (long id);
                };

            } // namespace linux
        } // namespace os
    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Linux) && defined (THEKOGANS_UTIL_HAVE_XLIB)

#endif // !defined (__thekogans_util_os_linux_XlibUtils_h)
