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
    #include <intrin.h>
#elif defined (TOOLCHAIN_ARCH_ppc) || defined (TOOLCHAIN_ARCH_ppc64)
    #if defined (TOOLCHAIN_OS_Linux)
        #include <signal.h>
        #include <setjmp.h>
    #elif defined (TOOLCHAIN_OS_OSX)
        #include <sys/sysctl.h>
    #endif // defined (TOOLCHAIN_OS_Linux)
#endif // defined (TOOLCHAIN_OS_Windows)
#include <cstring>
#include <vector>
#include "thekogans/util/CPUInfo.h"

namespace thekogans {
    namespace util {

        namespace {
        #if defined (TOOLCHAIN_OS_Windows)
            bool HaveAltiVec () {
                return false;
            }
        #else // defined (TOOLCHAIN_OS_Windows)
        #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
            void __cpuid (
                    int registers[4],
                    int function) {
                __asm__ volatile (
                    "pushq %%rbx\n"
                    "cpuid\n"
                    "movq %%rbx, %%rsi\n"
                    "popq %%rbx\n"
                    : "=a" (registers[0]), "=S" (registers[1]), "=c" (registers[2]), "=d" (registers[3])
                    : "a" (function));
            }

            void __cpuidex (
                    int registers[4],
                    int function,
                    int subfunction) {
                __asm__ volatile (
                    "mov %%ebx, %%edi\n"
                    "cpuid\n"
                    "xchg %%edi, %%ebx\n"
                    : "=a" (registers[0]), "=D" (registers[1]), "=c" (registers[2]), "=d" (registers[3])
                    : "a" (function), "c" (subfunction));
            }

            bool HaveAltiVec () {
                return false;
            }
        #elif defined (TOOLCHAIN_ARCH_ppc) || defined (TOOLCHAIN_ARCH_ppc64)
        #if defined (TOOLCHAIN_OS_Linux)
            jmp_buf jmpbuf;

            void IllegalInstruction (int /*sig*/) {
                longjmp (jmpbuf, 1);
            }

            bool HaveAltiVec () {
                volatile bool altivec = false;
                void (*handler) (int /*sig*/) = signal (SIGILL, IllegalInstruction);
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
                sysctl (selectors, 2, &hasVectorUnit, &length, 0, 0);
                return hasVectorUnit != 0;
            }
        #else // defined (TOOLCHAIN_OS_Linux)
            bool HaveAltiVec () {
                return false;
            }
        #endif // defined (TOOLCHAIN_OS_Linux)
        #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        CPUInfo::CPUInfo () :
                isIntel (false),
                isAMD (false),
                l1CacheLineSize (0),
                f_1_ECX (0),
                f_1_EDX (0),
                f_7_EBX (0),
                f_7_ECX (0),
                f_81_ECX (0),
                f_81_EDX (0),
                isAltiVec (HaveAltiVec ()) {
        #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
            {
                // Calling __cpuid with 0x0 as the function argument
                // gets the number of the highest valid function ID.
                std::array<ui32, 4> registers;
                __cpuid ((int *)registers.data (), 0);
                std::vector<std::array<ui32, 4>> data;
                for (ui32 i = 0, count = registers[0]; i <= count; ++i) {
                    __cpuidex ((int *)registers.data (), i, 0);
                    data.push_back (registers);
                }
                // Capture vendor string.
                char vendor_[0x20];
                memset (vendor_, 0, sizeof (vendor));
                *reinterpret_cast<ui32 *> (vendor_ + 0) = data[0][1];
                *reinterpret_cast<ui32 *> (vendor_ + 4) = data[0][3];
                *reinterpret_cast<ui32 *> (vendor_ + 8) = data[0][2];
                vendor = vendor_;
                if (vendor == "GenuineIntel") {
                    isIntel = true;
                }
                else if (vendor == "AuthenticAMD") {
                    isAMD = true;
                }
                // Load bitset with flags for function 0x00000001.
                if (data.size () > 1) {
                    // If on Intel, get the L1 cache line size.
                    if (isIntel) {
                        l1CacheLineSize = (((data[1][1] >> 8) & 0xff) * 8);
                    }
                    f_1_ECX = data[1][2];
                    f_1_EDX = data[1][3];
                }
                // Load bitset with flags for function 0x00000007.
                if (data.size () > 7) {
                    f_7_EBX = data[7][1];
                    f_7_ECX = data[7][2];
                }
            }
            {
                // Calling __cpuid with 0x80000000 as the function argument
                // gets the number of the highest valid extended ID.
                std::array<ui32, 4> registers;
                __cpuid ((int *)registers.data (), 0x80000000);
                std::vector<std::array<ui32, 4>> data;
                for (ui32 i = 0x80000000, count = registers[0]; i <= count; ++i) {
                    __cpuidex ((int *)registers.data (), i, 0);
                    data.push_back (registers);
                }
                // Load bitset with flags for function 0x80000001.
                if (data.size () > 1) {
                    f_81_ECX = data[1][2];
                    f_81_EDX = data[1][3];
                }
                // Interpret CPU brand string if reported.
                if (data.size () > 4) {
                    char brand_[0x40];
                    memset (brand_, 0, sizeof (brand));
                    memcpy (brand_, data[2].data (), sizeof (registers));
                    memcpy (brand_ + 16, data[3].data (), sizeof (registers));
                    memcpy (brand_ + 32, data[4].data (), sizeof (registers));
                    brand = brand_;
                }
                // If on AMD, get the L1 cache line size.
                if (isAMD && data.size () > 5) {
                    l1CacheLineSize = data[5][2] & 0xff;
                }
            }
        #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
        }

    } // namespace util
} // namespace thekogans
