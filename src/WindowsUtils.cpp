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

#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/WindowsUtils.h"

_LIB_THEKOGANS_UTIL_DECL int _LIB_THEKOGANS_UTIL_API pipe (
        THEKOGANS_UTIL_HANDLE fildes[2]) {
    static THEKOGANS_UTIL_ATOMIC<ULONG> serialNumber (1);
    std::string name =
        thekogans::util::FormatString (
            "\\\\.\\Pipe\\thekogans_util.%08x.%08x",
            GetCurrentProcessId (),
            serialNumber++);
    SECURITY_ATTRIBUTES securityAttributes;
    securityAttributes.nLength = sizeof (SECURITY_ATTRIBUTES);
    securityAttributes.bInheritHandle = TRUE;
    securityAttributes.lpSecurityDescriptor = 0;
    fildes[0] = CreateNamedPipeA (
        name.c_str (),
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_WAIT,
        1,
        4096,
        4096,
        120 * 1000,
        &securityAttributes);
    if (fildes[0] == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
        return -1;
    }
    fildes[1] = CreateFileA (
        name.c_str (),
        GENERIC_WRITE,
        0,
        &securityAttributes,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        0);
    if (fildes[1] == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
        THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
        CloseHandle (fildes[0]);
        SetLastError (errorCode);
        return -1;
    }
    return 0;
}

namespace thekogans {
    namespace util {

        _LIB_THEKOGANS_UTIL_DECL FILETIME _LIB_THEKOGANS_UTIL_API i64ToFILETIME (i64 value) {
            ULARGE_INTEGER ul;
            ul.QuadPart = (value + THEKOGANS_UTIL_UI64_LITERAL (11644473600)) *
                THEKOGANS_UTIL_UI64_LITERAL (10000000);
            FILETIME ft;
            ft.dwHighDateTime = ul.HighPart;
            ft.dwLowDateTime = ul.LowPart;
            return ft;
        }

        _LIB_THEKOGANS_UTIL_DECL i64 _LIB_THEKOGANS_UTIL_API FILETIMEToi64 (const FILETIME &value) {
            ULARGE_INTEGER ul;
            ul.LowPart = value.dwLowDateTime;
            ul.HighPart = value.dwHighDateTime;
            return ul.QuadPart / THEKOGANS_UTIL_UI64_LITERAL (10000000) -
                THEKOGANS_UTIL_UI64_LITERAL (11644473600);
        }

        HGLOBALPtr &HGLOBALPtr::operator = (HGLOBALPtr &&other) {
            if (this != &other) {
                HGLOBALPtr temp (std::move (other));
                swap (temp);
            }
            return *this;
        }

        void HGLOBALPtr::swap (HGLOBALPtr &other) {
            std::swap (hglobal, other.hglobal);
            std::swap (owner, other.owner);
            std::swap (ptr, other.ptr);
            std::swap (length, other.length);
        }

        void HGLOBALPtr::Attach (
                HGLOBAL hglobal_,
                bool owner_) {
            if (hglobal != 0) {
                GlobalUnlock (hglobal);
                if (owner) {
                    GlobalFree (hglobal);
                    owner = false;
                }
                ptr = 0;
                length = 0;
            }
            hglobal = hglobal_;
            owner = owner_;
            if (hglobal != 0) {
                ptr = GlobalLock (hglobal);
                if (ptr != 0) {
                    length = GlobalSize (hglobal);
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
        }

        HGLOBAL HGLOBALPtr::Release () {
            HGLOBAL result = 0;
            if (hglobal != 0) {
                result = hglobal;
                GlobalUnlock (hglobal);
                hglobal = 0;
                owner = false;
                ptr = 0;
                length = 0;
            }
            return result;
        }

        WindowClass::WindowClass (
                const std::string &name_,
                WNDPROC wndProc,
                UINT style,
                HICON icon,
                HCURSOR cursor,
                HBRUSH background,
                LPCTSTR menu,
                HINSTANCE instance_) :
                name (name_),
                instance (instance_),
                atom (0) {
            if (!name.empty () && wndProc != 0 && instance != 0) {
                WNDCLASSEX wndClassEx;
                wndClassEx.cbSize = sizeof (WNDCLASSEX);
                wndClassEx.style = style;
                wndClassEx.lpfnWndProc = wndProc;
                wndClassEx.cbClsExtra = 0;
                wndClassEx.cbWndExtra = 0;
                wndClassEx.hInstance = instance;
                wndClassEx.hIcon = icon;
                wndClassEx.hCursor = cursor;
                wndClassEx.hbrBackground = background;
                wndClassEx.lpszMenuName = menu;
                wndClassEx.lpszClassName = name.c_str ();
                wndClassEx.hIconSm = 0;
                atom = RegisterClassEx (&wndClassEx);
                if (atom == 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        WindowClass::~WindowClass () {
            UnregisterClass (name.c_str (), instance);
        }

        Window::Window (
                const WindowClass &windowClass,
                const Rectangle &rectangle,
                const std::string &name,
                DWORD style,
                DWORD extendedStyle,
                HWND parent,
                HMENU menu,
                void *userInfo) :
                wnd (
                    CreateWindowEx (
                        extendedStyle,
                        windowClass.name.c_str (),
                        name.c_str (),
                        style,
                        rectangle.origin.x,
                        rectangle.origin.y,
                        rectangle.extents.width,
                        rectangle.extents.height,
                        parent,
                        menu,
                        windowClass.instance,
                        userInfo)),
                owner (true) {
            if (wnd == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        }

        Window::~Window () {
            if (owner) {
                DestroyWindow (wnd);
            }
        }

    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Windows)
