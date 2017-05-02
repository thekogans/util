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
    #include <intrin.h>
#elif defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
    #if defined (TOOLCHAIN_OS_Linux)
        #include <unistd.h>
        #include <signal.h>
        #include <setjmp.h>
    #elif defined (TOOLCHAIN_OS_OSX)
        #include <sys/types.h>
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

            // This code was heavily borrowed from SDL (https://www.libsdl.org).
            // Thanks guys (and gals).
        #if defined (TOOLCHAIN_OS_Windows)
            ui32 GetCPUCountImpl () {
                SYSTEM_INFO systemInfo = {0};
                GetSystemInfo (&systemInfo);
                return systemInfo.dwNumberOfProcessors;
            }

        #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
        #if defined (TOOLCHAIN_ARCH_i386)
            bool HaveCPUID () {
                int haveCPUID = 0;
                __asm {
                    pushfd            ; Get original EFLAGS
                    pop eax
                    mov ecx, eax
                    xor eax, 200000h  ; Flip ID bit in EFLAGS
                    push eax          ; Save new EFLAGS value on stack
                    popfd             ; Replace current EFLAGS value
                    pushfd            ; Get new EFLAGS
                    pop eax           ; Store new EFLAGS in EAX
                    xor eax, ecx      ; Can not toggle ID bit,
                    jz done           ; Processor = 80486
                    mov haveCPUID, 1  ; We have CPUID support
                done:
                }
                return haveCPUID == 1;
            }
        #else // defined (TOOLCHAIN_ARCH_i386)
            bool HaveCPUID () {
                return true;
            }
        #endif // defined (TOOLCHAIN_ARCH_i386)
            #define GetCPUID(func, a, b, c, d) {\
                int cpuInfo[4];\
                __cpuid (cpuInfo, func);\
                a = cpuInfo[0];\
                b = cpuInfo[1];\
                c = cpuInfo[2];\
                d = cpuInfo[3];\
            }
        #else // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
            bool HaveCPUID () {
                return false;
            }

            #define GetCPUID(func, a, b, c, d)\
                a = b = c = d = 0
        #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)

            ui32 GetPageSizeImpl () {
                SYSTEM_INFO systemInfo;
                GetSystemInfo (&systemInfo);
                return systemInfo.dwPageSize;
            }

            ui64 GetMemorySizeImpl () {
                MEMORYSTATUSEX memoryStatus = {0};
                memoryStatus.dwLength = sizeof (MEMORYSTATUSEX);
                if (!GlobalMemoryStatusEx (&memoryStatus)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                return memoryStatus.ullTotalPhys;
            }
        #elif defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
        #if defined (TOOLCHAIN_OS_Linux)
            ui32 GetCPUCountImpl () {
                return sysconf (_SC_NPROCESSORS_ONLN);
            }
        #else // defined (TOOLCHAIN_OS_Linux)
            ui32 GetCPUCountImpl () {
                ui32 cpuCount = 1;
                int selectors[2] = {CTL_HW, HW_NCPU};
                size_t length = sizeof (cpuCount);
                sysctl (selectors, 2, &cpuCount, &length, NULL, 0);
                return cpuCount;
            }
        #endif // defined (TOOLCHAIN_OS_Linux)

        #if defined (TOOLCHAIN_ARCH_i386)
            bool HaveCPUID () {
                int haveCPUID = 0;
                __asm__ (
                    "        pushfl                      # Get original EFLAGS             \n"
                    "        popl    %%eax                                                 \n"
                    "        movl    %%eax,%%ecx                                           \n"
                    "        xorl    $0x200000,%%eax     # Flip ID bit in EFLAGS           \n"
                    "        pushl   %%eax               # Save new EFLAGS value on stack  \n"
                    "        popfl                       # Replace current EFLAGS value    \n"
                    "        pushfl                      # Get new EFLAGS                  \n"
                    "        popl    %%eax               # Store new EFLAGS in EAX         \n"
                    "        xorl    %%ecx,%%eax         # Can not toggle ID bit,          \n"
                    "        jz      1f                  # Processor=80486                 \n"
                    "        movl    $1,%0               # We have CPUID support           \n"
                    "1:                                                                    \n"
                    : "=m" (haveCPUID)
                    :
                    : "%eax", "%ecx"
                );
                return haveCPUID == 1;
            }

            #define GetCPUID(func, a, b, c, d)\
                __asm__ __volatile__ (\
                    "        pushl %%ebx        \n"\
                    "        cpuid              \n"\
                    "        movl %%ebx, %%esi  \n"\
                    "        popl %%ebx         \n" :\
                    "=a" (a), "=S" (b), "=c" (c), "=d" (d) : "a" (func))
        #elif defined (TOOLCHAIN_ARCH_x86_64)
            bool HaveCPUID () {
                int haveCPUID = 0;
                __asm__ (
                    "        pushfq                      # Get original EFLAGS             \n"
                    "        popq    %%rax                                                 \n"
                    "        movq    %%rax,%%rcx                                           \n"
                    "        xorl    $0x200000,%%eax     # Flip ID bit in EFLAGS           \n"
                    "        pushq   %%rax               # Save new EFLAGS value on stack  \n"
                    "        popfq                       # Replace current EFLAGS value    \n"
                    "        pushfq                      # Get new EFLAGS                  \n"
                    "        popq    %%rax               # Store new EFLAGS in EAX         \n"
                    "        xorl    %%ecx,%%eax         # Can not toggle ID bit,          \n"
                    "        jz      1f                  # Processor=80486                 \n"
                    "        movl    $1,%0               # We have CPUID support           \n"
                    "1:                                                                    \n"
                    : "=m" (haveCPUID)
                    :
                    : "%rax", "%rcx"
                );
                return haveCPUID == 1;
            }

            #define GetCPUID(func, a, b, c, d)\
                __asm__ __volatile__ (\
                    "        pushq %%rbx        \n"\
                    "        cpuid              \n"\
                    "        movq %%rbx, %%rsi  \n"\
                    "        popq %%rbx         \n" :\
                    "=a" (a), "=S" (b), "=c" (c), "=d" (d) : "a" (func))
        #else // defined (TOOLCHAIN_ARCH_i386)
            bool HaveCPUID () {
                return false;
            }

            #define GetCPUID(func, a, b, c, d)\
                a = b = c = d = 0
        #endif // defined (TOOLCHAIN_ARCH_i386)

        #if defined (TOOLCHAIN_OS_Linux)
            ui32 GetPageSizeImpl () {
                return sysconf (_SC_PAGESIZE);
            }

            ui64 GetMemorySizeImpl () {
                return (ui64)sysconf (_SC_PHYS_PAGES) * (ui64)sysconf (_SC_PAGESIZE);
            }
        #else // defined (TOOLCHAIN_OS_Linux)
            ui32 GetPageSizeImpl () {
                ui32 pageSize = 0;
                int selectors[2] = {CTL_HW, HW_PAGESIZE};
                size_t length = sizeof (pageSize);
                sysctl (selectors, 2, &pageSize, &length, NULL, 0);
                return pageSize;
            }

            ui64 GetMemorySizeImpl () {
                ui64 memorySize = 0;
                int selectors[2] = {CTL_HW, HW_MEMSIZE};
                size_t length = sizeof (memorySize);
                sysctl (selectors, 2, &memorySize, &length, NULL, 0);
                return memorySize;
            }
        #endif // defined (TOOLCHAIN_OS_Linux)
        #endif // defined (TOOLCHAIN_OS_Windows)

        #if defined (__GNUC__)
            #define THEKOGANS_UTIL_UNUSED __attribute__ ((unused))
        #else // defined (__GNUC__)
            #define THEKOGANS_UTIL_UNUSED
        #endif // defined (__GNUC__)

            std::string GetCPUTypeImpl () {
                char cpuType[13] = {0};
                {
                    ui32 a THEKOGANS_UTIL_UNUSED;
                    ui32 b;
                    ui32 c;
                    ui32 d;
                    GetCPUID (0, a, b, c, d);
                    cpuType[0] = (ui8)(b & 0xff); b >>= 8;
                    cpuType[1] = (ui8)(b & 0xff); b >>= 8;
                    cpuType[2] = (ui8)(b & 0xff); b >>= 8;
                    cpuType[3] = (ui8)(b & 0xff); b >>= 8;
                    cpuType[4] = (ui8)(d & 0xff); d >>= 8;
                    cpuType[5] = (ui8)(d & 0xff); d >>= 8;
                    cpuType[6] = (ui8)(d & 0xff); d >>= 8;
                    cpuType[7] = (ui8)(d & 0xff); d >>= 8;
                    cpuType[8] = (ui8)(c & 0xff); c >>= 8;
                    cpuType[9] = (ui8)(c & 0xff); c >>= 8;
                    cpuType[10] = (ui8)(c & 0xff); c >>= 8;
                    cpuType[11] = (ui8)(c & 0xff); c >>= 8;
                }
                return cpuType;
            }

            ui32 GetCPUL1CacheLineSizeImpl () {
                if (HaveCPUID ()) {
                    std::string cpuType = GetCPUTypeImpl ();
                    if (cpuType == "GenuineIntel") {
                        ui32 a THEKOGANS_UTIL_UNUSED;
                        ui32 b;
                        ui32 c THEKOGANS_UTIL_UNUSED;
                        ui32 d THEKOGANS_UTIL_UNUSED;
                        GetCPUID (0x00000001, a, b, c, d);
                        return (((b >> 8) & 0xff) * 8);
                    }
                    else if (cpuType == "AuthenticAMD") {
                        ui32 a THEKOGANS_UTIL_UNUSED;
                        ui32 b THEKOGANS_UTIL_UNUSED;
                        ui32 c;
                        ui32 d THEKOGANS_UTIL_UNUSED;
                        GetCPUID (0x80000005, a, b, c, d);
                        return (c & 0xff);
                    }
                }
                // Just make a guess here...
                return 128;
            }

            bool HaveRDTSC () {
                ui32 a;
                ui32 b THEKOGANS_UTIL_UNUSED;
                ui32 c THEKOGANS_UTIL_UNUSED;
                ui32 d;
                GetCPUID (0, a, b, c, d);
                if (a >= 1) {
                    GetCPUID (1, a, b, c, d);
                    return (d & 0x00000010) != 0;
                }
                return false;
            }

        #if defined (TOOLCHAIN_ARCH_ppc) || defined (TOOLCHAIN_ARCH_ppc64)
        #if defined (TOOLCHAIN_OS_Linux)
            jmp_buf jmpbuf;
            void IllegalInstruction (int sig) {
                longjmp (jmpbuf, 1);
            }

            bool HaveAltiVec () {
                volatile bool altivec = false;
                void (*handler) (int sig) = signal (SIGILL, IllegalInstruction);
                if (setjmp (jmpbuf) == 0) {
                    asm volatile ("mtspr 256, %0\n\t" "vand %%v0, %%v0, %%v0"::"r" (-1));
                    altivec = true;
                }
                signal (SIGILL, handler);
                return altivec;
            }
        #elif defined (TOOLCHAIN_OS_OSX)
            bool HaveAltiVec () {
                ui32 hasVectorUnit = 0;
                int selectors[2] = {CTL_HW, HW_VECTORUNIT};
                size_t length = sizeof (hasVectorUnit);
                sysctl (selectors, 2, &hasVectorUnit, &length, NULL, 0);
                return hasVectorUnit != 0;
            }
        #else // defined (TOOLCHAIN_OS_Linux)
            bool HaveAltiVec () {
                return false;
            }
        #endif // defined (TOOLCHAIN_OS_Linux)
        #else // defined (TOOLCHAIN_ARCH_ppc) || defined (TOOLCHAIN_ARCH_ppc64)
            bool HaveAltiVec () {
                return false;
            }
        #endif // defined (TOOLCHAIN_ARCH_ppc) || defined (TOOLCHAIN_ARCH_ppc64)

            bool HaveMMX () {
                ui32 a;
                ui32 b THEKOGANS_UTIL_UNUSED;
                ui32 c THEKOGANS_UTIL_UNUSED;
                ui32 d;
                GetCPUID (0, a, b, c, d);
                if (a >= 1) {
                    GetCPUID (1, a, b, c, d);
                    return (d & 0x00800000) != 0;
                }
                return false;
            }

            bool Have3DNow () {
                ui32 a;
                ui32 b THEKOGANS_UTIL_UNUSED;
                ui32 c THEKOGANS_UTIL_UNUSED;
                ui32 d;
                GetCPUID (0x80000000, a, b, c, d);
                if (a >= 0x80000001) {
                    GetCPUID (0x80000001, a, b, c, d);
                    return (d & 0x80000000) != 0;
                }
                return false;
            }

            bool HaveSSE () {
                ui32 a;
                ui32 b THEKOGANS_UTIL_UNUSED;
                ui32 c THEKOGANS_UTIL_UNUSED;
                ui32 d;
                GetCPUID (0, a, b, c, d);
                if (a >= 1) {
                    GetCPUID (1, a, b, c, d);
                    return (d & 0x02000000) != 0;
                }
                return false;
            }

            bool HaveSSE2 () {
                ui32 a;
                ui32 b THEKOGANS_UTIL_UNUSED;
                ui32 c THEKOGANS_UTIL_UNUSED;
                ui32 d;
                GetCPUID (0, a, b, c, d);
                if (a >= 1) {
                    GetCPUID (1, a, b, c, d);
                    return (d & 0x04000000) != 0;
                }
                return false;
            }

            bool HaveSSE3 () {
                ui32 a;
                ui32 b THEKOGANS_UTIL_UNUSED;
                ui32 c;
                ui32 d THEKOGANS_UTIL_UNUSED;
                GetCPUID (0, a, b, c, d);
                if (a >= 1) {
                    GetCPUID (1, a, b, c, d);
                    return (c & 0x00000001) != 0;
                }
                return false;
            }

            bool HaveSSE41 () {
                ui32 a;
                ui32 b THEKOGANS_UTIL_UNUSED;
                ui32 c;
                ui32 d THEKOGANS_UTIL_UNUSED;
                GetCPUID (1, a, b, c, d);
                if (a >= 1) {
                    GetCPUID (1, a, b, c, d);
                    return (c & 0x00080000) != 0;
                }
                return false;
            }

            bool HaveSSE42 () {
                ui32 a;
                ui32 b THEKOGANS_UTIL_UNUSED;
                ui32 c;
                ui32 d THEKOGANS_UTIL_UNUSED;
                GetCPUID (1, a, b, c, d);
                if (a >= 1) {
                    GetCPUID (1, a, b, c, d);
                    return (c & 0x00100000) != 0;
                }
                return false;
            }

            bool HaveAVX () {
                ui32 a;
                ui32 b THEKOGANS_UTIL_UNUSED;
                ui32 c;
                ui32 d THEKOGANS_UTIL_UNUSED;
                GetCPUID (1, a, b, c, d);
                if (a >= 1) {
                    GetCPUID (1, a, b, c, d);
                    return (c & 0x10000000) != 0;
                }
                return false;
            }

            ui32 GetCPUFeaturesImpl () {
                ui32 cpuFeatures = 0;
                if (HaveCPUID ()) {
                    if (HaveRDTSC ()) {
                        cpuFeatures |= SystemInfo::CPU_HAS_RDTSC;
                    }
                    if (HaveAltiVec ()) {
                        cpuFeatures |= SystemInfo::CPU_HAS_ALTIVEC;
                    }
                    if (HaveMMX ()) {
                        cpuFeatures |= SystemInfo::CPU_HAS_MMX;
                    }
                    if (Have3DNow ()) {
                        cpuFeatures |= SystemInfo::CPU_HAS_3DNOW;
                    }
                    if (HaveSSE ()) {
                        cpuFeatures |= SystemInfo::CPU_HAS_SSE;
                    }
                    if (HaveSSE2 ()) {
                        cpuFeatures |= SystemInfo::CPU_HAS_SSE2;
                    }
                    if (HaveSSE3 ()) {
                        cpuFeatures |= SystemInfo::CPU_HAS_SSE3;
                    }
                    if (HaveSSE41 ()) {
                        cpuFeatures |= SystemInfo::CPU_HAS_SSE41;
                    }
                    if (HaveSSE42 ()) {
                        cpuFeatures |= SystemInfo::CPU_HAS_SSE42;
                    }
                    if (HaveAVX ()) {
                        cpuFeatures |= SystemInfo::CPU_HAS_AVX;
                    }
                }
                return cpuFeatures;
            }

            std::string GetHostNameHelper () {
            #if defined (TOOLCHAIN_OS_Windows)
                // This Windows specific initialization happens during
                // static ctor creation. No throwing here. If WSAStartup
                // fails, the clients will know soon enough when we
                // call GetHostNameHelper.
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
            cpuL1CacheLineSize (GetCPUL1CacheLineSizeImpl ()),
            cpuFeatures (GetCPUFeaturesImpl ()),
            pageSize (GetPageSizeImpl ()),
            memorySize (GetMemorySizeImpl ()),
            hostName (GetHostNameHelper ()) {}

    } // namespace util
} // namespace thekogans
