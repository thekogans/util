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

#endif // defined (TOOLCHAIN_OS_Windows)
