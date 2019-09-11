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
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
        #include <windows.h>
    #endif // !defined (_WINDOWS_)
    #include <winsock2.h>
    #include <iphlpapi.h>
    #include <wtsapi32.h>
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

            std::string GetHostIdImpl () {
                std::set<std::string> macs;
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD size = 0;
                std::vector<ui8> buffer;
                DWORD rc;
                // This loop is necessary to avoid a race between
                // calls to GetAdaptersAddresses.
                do {
                    if (size > 0) {
                        buffer.resize (size);
                    }
                    rc = ::GetAdaptersAddresses (
                        AF_UNSPEC,
                        GAA_FLAG_SKIP_UNICAST |
                        GAA_FLAG_SKIP_ANYCAST |
                        GAA_FLAG_SKIP_MULTICAST |
                        GAA_FLAG_SKIP_DNS_SERVER |
                        GAA_FLAG_SKIP_FRIENDLY_NAME,
                        0,
                        (PIP_ADAPTER_ADDRESSES)(size > 0 ? buffer.data () : 0),
                        &size);
                } while (rc == ERROR_BUFFER_OVERFLOW && size > 0);
                if (rc == ERROR_SUCCESS) {
                    if (size > 0) {
                        for (PIP_ADAPTER_ADDRESSES
                                ipAdapterAddresses = (PIP_ADAPTER_ADDRESSES)buffer.data ();
                                ipAdapterAddresses != 0; ipAdapterAddresses = ipAdapterAddresses->Next) {
                            if (ipAdapterAddresses->PhysicalAddressLength == MAC_LENGTH) {
                                macs.insert (
                                    HexEncodeBuffer (
                                        ipAdapterAddresses->PhysicalAddress,
                                        ipAdapterAddresses->PhysicalAddressLength));
                            }
                        }
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (rc);
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                struct addrs {
                    ifaddrs *head;
                    addrs () :
                        head (0) {}
                    ~addrs () {
                        if (head != 0) {
                            freeifaddrs (head);
                        }
                    }
                } addrs;
                if (getifaddrs (&addrs.head) == 0) {
                    for (ifaddrs *curr = addrs.head; curr != 0; curr = curr->ifa_next) {
                    #if defined (TOOLCHAIN_OS_Linux)
                        if (curr->ifa_addr->sa_family == AF_PACKET) {
                            const sockaddr_ll *addr = (const sockaddr_ll *)curr->ifa_addr;
                            if (addr->sll_hatype == ARPHRD_ETHER && addr->sll_halen == MAC_LENGTH) {
                                macs.insert (HexEncodeBuffer (addr->sll_addr, addr->sll_halen));
                            }
                        }
                    #else // defined (TOOLCHAIN_OS_Linux)
                        if (curr->ifa_addr->sa_family == AF_LINK) {
                            const sockaddr_dl *addr = (const sockaddr_dl *)curr->ifa_addr;
                            if (addr->sdl_type == IFT_ETHER && addr->sdl_alen == MAC_LENGTH) {
                                macs.insert (HexEncodeBuffer (LLADDR (addr), addr->sdl_alen));
                            }
                        }
                    #endif // defined (TOOLCHAIN_OS_Linux)
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
                SHA2 sha2;
                sha2.Init (SHA2::DIGEST_SIZE_256);
                for (std::set<std::string>::const_iterator
                        it = macs.begin (),
                        end = macs.end (); it != end; ++it) {
                    sha2.Update ((*it).c_str (), (*it).size ());
                }
                Hash::Digest digest;
                sha2.Final (digest);
                return HexEncodeBuffer (digest.data (), digest.size ());
            }

            std::string GetUserNameImpl () {
                std::string result;
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD sessionId = WTSGetActiveConsoleSessionId ();
                if (sessionId != 0xFFFFFFFF) {
                    LPSTR data = NULL;
                    DWORD length = 0;
                    if (WTSQuerySessionInformationA (
                            WTS_CURRENT_SERVER_HANDLE,
                            sessionId,
                            WTSUserName,
                            &data,
                            &length)) {
                        result = data;
                        WTSFreeMemory (data);
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                }
            #elif defined (TOOLCHAIN_OS_Linux)
                struct passwd *pw = getpwuid (geteuid ());
                if (pw != 0) {
                    result = pw->pw_name;
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #elif defined (TOOLCHAIN_OS_OSX)
                struct CFStringRefDeleter {
                    void operator () (CFStringRef stringRef) {
                        if (stringRef != 0) {
                            CFRelease (stringRef);
                        }
                    }
                };
                typedef std::unique_ptr<const __CFString, CFStringRefDeleter> CFStringRefPtr;
                CFStringRefPtr consoleUser (SCDynamicStoreCopyConsoleUser (nullptr, nullptr, nullptr));
                if (consoleUser.get () != 0) {
                    struct CFDataRefDeleter {
                        void operator () (CFDataRef dataRef) {
                            if (dataRef != 0) {
                                CFRelease (dataRef);
                            }
                        }
                    };
                    typedef std::unique_ptr<const __CFData, CFDataRefDeleter> CFDataRefPtr;
                    CFDataRefPtr UTF8String (
                        CFStringCreateExternalRepresentation (
                            nullptr, consoleUser.get (), kCFStringEncodingUTF8, '?'));
                    if (UTF8String.get () != 0) {
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

        SystemInfo::SystemInfo () :
            endianness (GetEndiannessImpl ()),
            cpuCount (GetCPUCountImpl ()),
            pageSize (GetPageSizeImpl ()),
            memorySize (GetMemorySizeImpl ()),
            processPath (GetProcessPathImpl ()),
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
                "Endianness: " << (endianness == util::LittleEndian ? "LittleEndian" : "BigEndian") << std::endl <<
                "CPU count: " << cpuCount << std::endl <<
                "Page size: " << pageSize << std::endl <<
                "Memory size: " << memorySize << std::endl <<
                "Process path: " << processPath << std::endl <<
                "Process start directory: " << processStartDirectory << std::endl <<
                "Host name: " << hostName << std::endl <<
                "Host Id: " << hostId << std::endl <<
                "User name: " << userName << std::endl <<
                "OS: " << osTostring (os)  << std::endl;
        }

    } // namespace util
} // namespace thekogans
