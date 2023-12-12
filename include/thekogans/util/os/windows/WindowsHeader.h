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

#if !defined (__thekogans_util_os_windows_WindowsHeader_h)
#define __thekogans_util_os_windows_WindowsHeader_h

#if !defined (_WINDOWS_)
    #if !defined (NOMINMAX)
        #define NOMINMAX
    #endif // !defined (NOMINMAX)
    #if !defined (WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif // !defined (WIN32_LEAN_AND_MEAN)
    #if !defined (_WIN32_WINDOWS)
        #define _WIN32_WINDOWS 0x0602
    #endif // !defined (_WIN32_WINDOWS)
    #if !defined (_WIN32_WINNT)
        #define _WIN32_WINNT 0x0602
    #endif // !defined (_WIN32_WINNT)
    #if !defined (WINVER)
        #define WINVER 0x0602
    #endif // !defined (WINVER)
    #if !defined (UNICODE)
        #define UNICODE 1
    #endif // !defined (UNICODE)
    #if !defined (_UNICODE)
        #define _UNICODE 1
    #endif // !defined (_UNICODE)
    #include <windows.h>
#endif // !defined (_WINDOWS_)

#endif // !defined (__thekogans_util_os_windows_WindowsHeader_h)
