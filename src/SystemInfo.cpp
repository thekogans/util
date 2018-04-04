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

#include <string>
#if defined (TOOLCHAIN_OS_Windows)
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
        #include <winsock2.h>
        #include <windows.h>
    #endif // !defined (_WINDOWS_)
#elif defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
    #if defined (TOOLCHAIN_OS_Linux)
        #include <unistd.h>
        #include <climits>
    #elif defined (TOOLCHAIN_OS_OSX)
        #include <libproc.h>
        #include <sys/sysctl.h>
    #endif // defined (TOOLCHAIN_OS_Windows)
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Exception.h"
#include "thekogans/util/SystemInfo.h"

namespace thekogans {
    namespace util {

        namespace {
            Endianness GetEndiannessImpl () {
                return HostEndian;
            }

            ui32 GetCPUCountImpl () {
            #if defined (TOOLCHAIN_OS_Windows)
                SYSTEM_INFO systemInfo = {0};
                GetSystemInfo (&systemInfo);
                return systemInfo.dwNumberOfProcessors;
            #elif defined (TOOLCHAIN_OS_Linux)
                return sysconf (_SC_NPROCESSORS_ONLN);
            #elif defined (TOOLCHAIN_OS_OSX)
                ui32 cpuCount = 1;
                int selectors[2] = {CTL_HW, HW_NCPU};
                size_t length = sizeof (cpuCount);
                sysctl (selectors, 2, &cpuCount, &length, 0, 0);
                return cpuCount;
            #endif // defined (TOOLCHAIN_OS_Windows)
            }

            ui32 GetPageSizeImpl () {
            #if defined (TOOLCHAIN_OS_Windows)
                SYSTEM_INFO systemInfo;
                GetSystemInfo (&systemInfo);
                return systemInfo.dwPageSize;
            #elif defined (TOOLCHAIN_OS_Linux)
                return sysconf (_SC_PAGESIZE);
            #elif defined (TOOLCHAIN_OS_OSX)
                ui32 pageSize = 0;
                int selectors[2] = {CTL_HW, HW_PAGESIZE};
                size_t length = sizeof (pageSize);
                sysctl (selectors, 2, &pageSize, &length, 0, 0);
                return pageSize;
            #endif // defined (TOOLCHAIN_OS_Windows)
            }

            ui64 GetMemorySizeImpl () {
            #if defined (TOOLCHAIN_OS_Windows)
                MEMORYSTATUSEX memoryStatus = {0};
                memoryStatus.dwLength = sizeof (MEMORYSTATUSEX);
                if (!GlobalMemoryStatusEx (&memoryStatus)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                return memoryStatus.ullTotalPhys;
            #elif defined (TOOLCHAIN_OS_Linux)
                return (ui64)sysconf (_SC_PHYS_PAGES) * (ui64)sysconf (_SC_PAGESIZE);
            #elif defined (TOOLCHAIN_OS_OSX)
                ui64 memorySize = 0;
                int selectors[2] = {CTL_HW, HW_MEMSIZE};
                size_t length = sizeof (memorySize);
                sysctl (selectors, 2, &memorySize, &length, 0, 0);
                return memorySize;
            #endif // defined (TOOLCHAIN_OS_Windows)
            }

            std::string GetProcessPathImpl () {
            #if defined (TOOLCHAIN_OS_Windows)
                char path[MAX_PATH];
                if (GetModuleFileName (0, path, sizeof (path)) == 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                return path;
            #elif defined (TOOLCHAIN_OS_Linux)
                char path[PATH_MAX];
                ssize_t count =
                    readlink (FormatString ("/proc/%d/exe", getpid ()).c_str (), path, sizeof (path));
                if (count < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                path[count] = '\0';
                return path;
            #elif defined (TOOLCHAIN_OS_OSX)
                char path[PROC_PIDPATHINFO_MAXSIZE];
                if (proc_pidpath (getpid (), path, sizeof (path)) <= 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                return path;
            #endif // defined (TOOLCHAIN_OS_Windows)
            }

            std::string GetHostNameImpl () {
            #if defined (TOOLCHAIN_OS_Windows)
                struct WinSockInit {
                    WinSockInit () {
                        WSADATA data;
                        WSAStartup (MAKEWORD (2, 2), &data);
                    }
                    ~WinSockInit () {
                        WSACleanup ();
                    }
                } winSockInit;
            #endif // defined (TOOLCHAIN_OS_Windows)
                char name[256] = {0};
                if (gethostname (name, 256) != 0) {
                #if defined (TOOLCHAIN_OS_Windows)
                    THEKOGANS_UTIL_ERROR_CODE errorCode = WSAGetLastError ();
                #else // defined (TOOLCHAIN_OS_Windows)
                    THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                #endif // defined (TOOLCHAIN_OS_Windows)
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
                return name;
            }
        }

        SystemInfo::SystemInfo () :
            endianness (GetEndiannessImpl ()),
            cpuCount (GetCPUCountImpl ()),
            pageSize (GetPageSizeImpl ()),
            memorySize (GetMemorySizeImpl ()),
            processPath (GetProcessPathImpl ()),
            hostName (GetHostNameImpl ()) {}

    } // namespace util
} // namespace thekogans
