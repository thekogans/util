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

#if defined (TOOLCHAIN_OS_Linux) && defined (THEKOGANS_UTIL_HAVE_XLIB)

#if !defined (__thekogans_util_XlibUtils_h)
#define __thekogans_util_XlibUtils_h

#include <X11/Xlib.h>
#include <vector>

namespace thekogans {
    namespace util {

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

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_XlibUtils_h)

#endif // defined (TOOLCHAIN_OS_Linux) && defined (THEKOGANS_UTIL_HAVE_XLIB)
