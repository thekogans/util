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
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
    #include <winsock2.h>
    #include <iphlpapi.h>
    #if defined (THEKOGANS_UTIL_HAVE_WTS)
        #include <wtsapi32.h>
    #endif // defined (THEKOGANS_UTIL_HAVE_WTS)
    #include <VersionHelpers.h>
#elif defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
    #include <ifaddrs.h>
    #include <net/ethernet.h>
    #if defined (TOOLCHAIN_OS_Linux)
        #include <net/if_arp.h>
        #include <linux/if_packet.h>
        #include <unistd.h>
        #include <pwd.h>
        #include <climits>
    #elif defined (TOOLCHAIN_OS_OSX)
        #include <IOKit/IOKitLib.h>
        #include <net/if_dl.h>
        #include <net/if_types.h>
        #include <libproc.h>
        #include <sys/sysctl.h>
    #endif // defined (TOOLCHAIN_OS_Windows)
#endif // defined (TOOLCHAIN_OS_Windows)
#include <string>
#include <set>
#include "thekogans/util/Constants.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/SHA2.h"
#include "thekogans/util/StringUtils.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
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

            THEKOGANS_UTIL_SESSION_ID GetSessionIdImpl () {
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD sessionId;
                if (!ProcessIdToSessionId (GetCurrentProcessId (), &sessionId)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
                return static_cast<THEKOGANS_UTIL_SESSION_ID> (
                #if defined (TOOLCHAIN_OS_Windows)
                    sessionId);
                #else // defined (TOOLCHAIN_OS_Windows)
                    getsid (0));
                #endif // defined (TOOLCHAIN_OS_Windows)
            }

            std::string GetProcessPathImpl () {
            #if defined (TOOLCHAIN_OS_Windows)
                wchar_t path[MAX_PATH];
                std::size_t length = GetModuleFileNameW (0, path, MAX_PATH);
                if (length > 0) {
                    return os::windows::UTF16ToUTF8 (path, length);
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #elif defined (TOOLCHAIN_OS_Linux)
                char path[PATH_MAX];
                ssize_t count =
                    readlink (FormatString ("/proc/%d/exe", getpid ()).c_str (), path, PATH_MAX);
                if (count < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                path[count] = '\0';
                return path;
            #elif defined (TOOLCHAIN_OS_OSX)
                char path[PROC_PIDPATHINFO_MAXSIZE];
                if (proc_pidpath (getpid (), path, PROC_PIDPATHINFO_MAXSIZE) <= 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                return path;
            #endif // defined (TOOLCHAIN_OS_Windows)
            }

            THEKOGANS_UTIL_PROCESS_ID GetProcessIdImpl () {
                return static_cast<THEKOGANS_UTIL_PROCESS_ID> (
                #if defined (TOOLCHAIN_OS_Windows)
                    GetCurrentProcessId ());
                #else // defined (TOOLCHAIN_OS_Windows)
                    getpid ());
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
                if (IsWindows8OrGreater ()) {
                    // Get WinSock module handle that is already
                    // mapped into process virtual space, find a
                    // routine entry address and call it.
                    const HMODULE hmodule = GetModuleHandleW (L"WS2_32.DLL");
                    if (hmodule != 0) {
                        typedef int (WINAPI *GetHostNameWProc) (
                            PWSTR name,
                            int namelen);
                        const GetHostNameWProc getHostNameW = reinterpret_cast<GetHostNameWProc> (
                            GetProcAddress (hmodule, "GetHostNameW"));
                        if (getHostNameW != nullptr) {
                            wchar_t name[256];
                            SecureZeroMemory (name, 256);
                            if (getHostNameW (name, 256) == 0) {
                                return os::windows::UTF16ToUTF8 (name);
                            }
                            else {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                    WSAGetLastError ());
                            }
                        }
                        else {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE);
                        }
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                }
                else {
                    // Pre-Windows 8 host name is always ACP encoded.
                    char name[256];
                    SecureZeroMemory (name, 256);
                    if (gethostname (name, 256) == 0) {
                        // There is no direct way to convert ACP into
                        // UTF8, so perform the conversion in two steps.
                        return os::windows::UTF16ToUTF8 (os::windows::ACPToUTF16 (name));
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            WSAGetLastError ());
                    }
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                char name[256];
                SecureZeroMemory (name, 256);
                if (gethostname (name, 256) == 0) {
                    return name;
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }

        #if defined (TOOLCHAIN_OS_OSX)
            namespace {
                struct CFStringRefDeleter {
                    void operator () (CFStringRef stringRef) {
                        if (stringRef != nullptr) {
                            CFRelease (stringRef);
                        }
                    }
                };
                using CFStringRefPtr = std::unique_ptr<const __CFString, CFStringRefDeleter>;
            }
        #endif // defined (TOOLCHAIN_OS_OSX)

            std::string GetHostIdImpl () {
            #if defined (TOOLCHAIN_OS_Windows)
                wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
                DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
                if (GetComputerNameW (computerName, &size)) {
                    DWORD serialNum = 0;
                    if (GetVolumeInformationW (L"c:\\", 0, 0, &serialNum, 0, 0, 0, 0)) {
                        Hash::Digest digest;
                        {
                            SHA2 sha2;
                            sha2.Init (SHA2::DIGEST_SIZE_256);
                            sha2.Update (&computerName[0], size * WCHAR_T_SIZE);
                            sha2.Update (&serialNum, sizeof (serialNum));
                            sha2.Final (digest);
                        }
                        return HexEncodeBuffer (digest.data (), digest.size ());
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #elif defined (TOOLCHAIN_OS_Linux)
                uuid_t uuid;
                timespec wait;
                wait.tv_sec = 0;
                wait.tv_nsec = 0;
                if (gethostuuid (uuid, &wait) == 0) {
                    Hash::Digest digest;
                    {
                        SHA2 sha2;
                        sha2.Init (SHA2::DIGEST_SIZE_256);
                        sha2.Update (uuid, sizeof (uuid_t));
                        sha2.Final (digest);
                    }
                    return HexEncodeBuffer (digest.data (), digest.size ());
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #elif defined (TOOLCHAIN_OS_OSX)
                #if (MAC_OS_X_VERSION_MAX_ALLOWED >= 120000) // Before macOS 12 Monterey
                    #define kIOMasterPortDefault kIOMainPortDefault
                #endif
                struct io_registry_entry_tPtr {
                    io_registry_entry_t registryEntry;
                    io_registry_entry_tPtr (io_registry_entry_t registryEntry_) :
                        registryEntry (registryEntry_) {}
                    ~io_registry_entry_tPtr () {
                        IOObjectRelease (registryEntry);
                    }
                } ioRegistryRoot (IORegistryEntryFromPath (kIOMasterPortDefault, "IOService:/"));
                if (ioRegistryRoot.registryEntry != 0) {
                    CFStringRefPtr uuid (
                        (CFStringRef)IORegistryEntryCreateCFProperty (
                            ioRegistryRoot.registryEntry,
                            CFSTR (kIOPlatformUUIDKey),
                            kCFAllocatorDefault,
                            0));
                    if (uuid != nullptr) {
                        char buffer[1024];
                        CFStringGetCString (uuid.get (), buffer, 1024, kCFStringEncodingMacRoman);
                        Hash::Digest digest;
                        {
                            SHA2 sha2;
                            sha2.Init (SHA2::DIGEST_SIZE_256);
                            sha2.Update (buffer, strlen (buffer));
                            sha2.Final (digest);
                        }
                        return HexEncodeBuffer (digest.data (), digest.size ());
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Unable to retrieve property: %s",
                            "kIOPlatformUUIDKey");
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Unable to retrieve registry entry: %s",
                        "IOService:/");
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }

            std::string GetUserNameImpl () {
                std::string result;
            #if defined (TOOLCHAIN_OS_Windows)
            #if defined (THEKOGANS_UTIL_HAVE_WTS)
                struct UserName {
                    LPWSTR name;
                    DWORD length;
                    UserName () :
                            name (nullptr),
                            length (0) {
                        if (!WTSQuerySessionInformationW (
                                WTS_CURRENT_SERVER_HANDLE,
                                WTS_CURRENT_SESSION,
                                WTSUserName,
                                &name,
                                &length)) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE);
                        }
                    }
                    ~UserName () {
                        if (name != nullptr) {
                            WTSFreeMemory (name);
                        }
                    }
                    operator std::string () {
                        return name != nullptr ?
                            os::windows::UTF16ToUTF8 (std::wstring (name)) :
                            std::string ();
                    }
                } userName;
                result = userName;
            #else // defined (THEKOGANS_UTIL_HAVE_WTS)
                WCHAR name[UNLEN + 1];
                DWORD length = UNLEN + 1;
                if (GetUserNameW (name, &length)) {
                    result = os::windows::UTF16ToUTF8 (std::wstring (name));
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #endif // defined (THEKOGANS_UTIL_HAVE_WTS)
            #elif defined (TOOLCHAIN_OS_Linux)
                struct passwd *pw = getpwuid (geteuid ());
                if (pw != nullptr) {
                    result = pw->pw_name;
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #elif defined (TOOLCHAIN_OS_OSX)
                CFStringRefPtr consoleUser (
                    SCDynamicStoreCopyConsoleUser (nullptr, nullptr, nullptr));
                if (consoleUser != nullptr) {
                    struct CFDataRefDeleter {
                        void operator () (CFDataRef dataRef) {
                            if (dataRef != nullptr) {
                                CFRelease (dataRef);
                            }
                        }
                    };
                    using CFDataRefPtr = std::unique_ptr<const __CFData, CFDataRefDeleter>;
                    CFDataRefPtr UTF8String (
                        CFStringCreateExternalRepresentation (
                            nullptr, consoleUser.get (), kCFStringEncodingUTF8, '?'));
                    if (UTF8String != nullptr) {
                        const UInt8 *data = CFDataGetBytePtr (UTF8String.get ());
                        CFIndex length = CFDataGetLength (UTF8String.get ());
                        result = std::string (data, data + length);
                    }
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
                return result;
            }

            std::string osTostring (ui8 os) {
                return os == SystemInfo::Windows ? "Windows" :
                    os == SystemInfo::Linux ? "Linux" :
                    os == SystemInfo::OSX ? "OS X" : "Unknown";
            }
        }

        std::string SystemInfo::processStartDirectory = Path::GetCurrDirectory ();
        TimeSpec SystemInfo::processStartTime = GetCurrentTime ();

        SystemInfo::SystemInfo () :
            endianness (GetEndiannessImpl ()),
            cpuCount (GetCPUCountImpl ()),
            pageSize (GetPageSizeImpl ()),
            memorySize (GetMemorySizeImpl ()),
            sessionId (GetSessionIdImpl ()),
            processPath (GetProcessPathImpl ()),
            processId (GetProcessIdImpl ()),
            hostName (GetHostNameImpl ()),
            hostId (GetHostIdImpl ()),
            userName (GetUserNameImpl ()),
        #if defined (TOOLCHAIN_OS_Windows)
            os (Windows) {}
        #elif defined (TOOLCHAIN_OS_Linux)
            os (Linux) {}
        #elif defined (TOOLCHAIN_OS_OSX)
            os (OSX) {}
        #else // defined (TOOLCHAIN_OS_Windows)
            os (Unknown) {}
        #endif // defined (TOOLCHAIN_OS_Windows)

        void SystemInfo::Dump (std::ostream &stream) const {
            stream <<
                "Endianness: " << EndiannessToString (endianness) << std::endl <<
                "CPU count: " << cpuCount << std::endl <<
                "Page size: " << pageSize << std::endl <<
                "Memory size: " << memorySize << std::endl <<
                "Session id: " << sessionId << std::endl <<
                "Process path: " << processPath << std::endl <<
                "Process id: " << processId << std::endl <<
                "Process start directory: " << processStartDirectory << std::endl <<
                "Process start time: " << FormatTimeSpec (processStartTime) << std::endl <<
                "Host name: " << hostName << std::endl <<
                "Host Id: " << hostId << std::endl <<
                "User name: " << userName << std::endl <<
                "OS: " << osTostring (os)  << std::endl;
        }

    } // namespace util
} // namespace thekogans
