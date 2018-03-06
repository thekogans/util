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

#if defined (TOOLCHAIN_OS_Windows)

#if !defined (__thekogans_util_WindowsUtils_h)
#define __thekogans_util_WindowsUtils_h

#if !defined (_WINDOWS_)
    #if !defined (WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif // !defined (WIN32_LEAN_AND_MEAN)
    #if !defined (NOMINMAX)
        #define NOMINMAX
    #endif // !defined (NOMINMAX)
    #include <windows.h>
#endif // !defined (_WINDOWS_)
#if defined (_MSC_VER)
    #include <direct.h>
    #include <cstdlib>
    #include <cstring>
    #include <cstdio>

    #define atoll(str) _strtoui64 (str, 0, 10)
    #define strtoll _strtoi64
    #define strtoull _strtoui64
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
    #define snprintf sprintf_s
    #define vsnprintf vsprintf_s
    #define unlink _unlink
    #define rmdir _rmdir
#endif // defined (_MSC_VER)
#include <memory>
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Rectangle.h"

/// \brief
/// Create both ends of an anonymous pipe. Useful
/// if you're planning on using it for overlapped io.
/// Adapted from: http://www.davehart.net/remote/PipeEx.c
/// \param[out] fildes fildes[0] = read end of the pipe,
/// fildes[1] = write end of the pipe.
/// \return 0 = success, -1 = errno contains the error.
_LIB_THEKOGANS_UTIL_DECL int _LIB_THEKOGANS_UTIL_API pipe (
    THEKOGANS_UTIL_HANDLE fildes[2]);

namespace thekogans {
    namespace util {

        /// \brief
        /// Convert a given i64 value to Windows FILETIME.
        /// \param[in] value i64 value to convert.
        /// \return Converted FILETIME.
        _LIB_THEKOGANS_UTIL_DECL FILETIME _LIB_THEKOGANS_UTIL_API i64ToFILETIME (i64 value);
        /// \brief
        /// Convert a given Windows FILETIME value to i64.
        /// \param[in] value FILETIME value to convert.
        /// \return Converted i64.
        _LIB_THEKOGANS_UTIL_DECL i64 _LIB_THEKOGANS_UTIL_API FILETIMEToi64 (const FILETIME &value);

        /// \struct WindowClass WindowsUtils.h thekogans/util/WindowsUtils.h
        ///
        /// \brief
        /// A helper for creating window classes.

        struct _LIB_THEKOGANS_UTIL_DECL WindowClass {
            /// \brief
            /// Class name.
            std::string name;
            /// \brief
            /// Module instance handle.
            HINSTANCE instance;
            /// \brief
            /// Registerd class atom.
            ATOM atom;

            /// \brief
            /// ctor.
            /// \param[in] name_ Class name.
            /// \param[in] wndProc Window message handler.
            /// \param[in] style Window class style.
            /// \param[in] icon = Window class icon.
            /// \param[in] cursor = Window class cursor.
            /// \param[in] background Window class background brush.
            /// \param[in] instance_ Module instance handle.
            WindowClass (
                const std::string &name_,
                WNDPROC wndProc,
                UINT style = CS_HREDRAW | CS_VREDRAW,
                HICON icon = 0,
                HCURSOR cursor = LoadCursor (0, IDC_ARROW),
                HBRUSH background = (HBRUSH)(COLOR_WINDOW + 1),
                HINSTANCE instance_ = GetModuleHandle (0));
            /// \brief
            /// dtor.
            virtual ~WindowClass ();
        };

        /// \struct Window WindowsUtils.h thekogans/util/WindowsUtils.h
        ///
        /// \brief
        /// A helper for creating windows. Hides a lot of Windows specific code and
        /// defaults almost everything. Used by \see{SystemRunLoop}.

        struct _LIB_THEKOGANS_UTIL_DECL Window {
            /// \brief
            /// Convenient typedef for std::unique_ptr<Window>.
            typedef std::unique_ptr<Window> Ptr;

            /// \brief
            /// Windows window.
            HWND wnd;
            /// \brief
            /// true == call DestroyWindow in the dtor.
            bool owner;

            /// \brief
            /// ctor.
            /// \param[in] wnd_ Window handle to wrap.
            /// \param[in] owner_ true == call DestroyWindow in the dtor.
            Window (
                HWND wnd_,
                bool owner_ = false) :
                wnd (wnd_),
                owner (owner_) {}
            /// \brief
            /// ctor.
            /// \param[in] windowClass An instance of \see{WindowClass}.
            /// \param[in] rectangle Window origin and extents.
            /// \param[in] name Window name.
            /// \param[in] style Window style.
            /// \param[in] extendedStyle Extended window style.
            /// \param[in] parent Window parent.
            /// \param[in] menu Window menu.
            /// \param[in] userInfo Passed to window proc in WM_CREATE message.
            Window (
                const WindowClass &windowClass,
                const Rectangle &rectangle = Rectangle (),
                const std::string &name = std::string (),
                DWORD style = WS_POPUP | WS_VISIBLE,
                DWORD extendedStyle = WS_EX_TOOLWINDOW,
                HWND parent = 0,
                HMENU menu = 0,
                void *userInfo = 0);
            /// \brief
            /// dtor.
            virtual ~Window ();
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_WindowsUtils_h)

#endif // defined (TOOLCHAIN_OS_Windows)
