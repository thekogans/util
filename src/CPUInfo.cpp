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

    #if !defined (TOOLCHAIN_OS_Windows)
        namespace {
            void __cpuid (
                    ui32 cpu_info[4],
                    ui32 function_id) {
                __asm__ volatile (
                    "pushq %%rbx\n"
                    "cpuid\n"
                    "movq %%rbx, %%rsi\n"
                    "popq %%rbx\n"
                    : "=a" (cpu_info[0]), "=S" (cpu_info[1]), "=c" (cpu_info[2]), "=d" (cpu_info[3])
                    : "a" (function_id));
            }

            void __cpuidex (
                    ui32 cpu_info[4],
                    ui32 function_id,
                    ui32 subfunction_id) {
                __asm__ volatile (
                    "mov %%ebx, %%edi\n"
                    "cpuid\n"
                    "xchg %%edi, %%ebx\n"
                    : "=a" (cpu_info[0]), "=D" (cpu_info[1]), "=c" (cpu_info[2]), "=d" (cpu_info[3])
                    : "a" (function_id), "c" (subfunction_id));
            }

        #if defined (TOOLCHAIN_ARCH_ppc) || defined (TOOLCHAIN_ARCH_ppc64)
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
        #else // defined (TOOLCHAIN_ARCH_ppc) || defined (TOOLCHAIN_ARCH_ppc64)
            bool HaveAltiVec () {
                return false;
            }
        #endif // defined (TOOLCHAIN_ARCH_ppc) || defined (TOOLCHAIN_ARCH_ppc64)
        }
    #endif // !defined (TOOLCHAIN_OS_Windows)

        CPUInfo::CPUInfo () :
                isIntel_ (false),
                isAMD_ (false),
                L1CacheLineSize_ (0),
                f_1_ECX_ (0),
                f_1_EDX_ (0),
                f_7_EBX_ (0),
                f_7_ECX_ (0),
                f_81_ECX_ (0),
                f_81_EDX_ (0),
                AltiVec_ (HaveAltiVec ()) {
            std::array<ui32, 4> cpui;
            // Calling __cpuid with 0x0 as the function_id argument
            // gets the number of the highest valid function ID.
            __cpuid (cpui.data (), 0);
            ui32 nIds_ = cpui[0];
            std::vector<std::array<ui32, 4>> data_;
            for (ui32 i = 0; i <= nIds_; ++i) {
                __cpuidex (cpui.data (), i, 0);
                data_.push_back (cpui);
            }
            // Capture vendor string
            char vendor[0x20];
            memset (vendor, 0, sizeof (vendor));
            *reinterpret_cast<ui32 *> (vendor + 0) = data_[0][1];
            *reinterpret_cast<ui32 *> (vendor + 4) = data_[0][3];
            *reinterpret_cast<ui32 *> (vendor + 8) = data_[0][2];
            vendor_ = vendor;
            if (vendor_ == "GenuineIntel") {
                isIntel_ = true;
            }
            else if (vendor_ == "AuthenticAMD") {
                isAMD_ = true;
            }
            // Load bitset with flags for function 0x00000001
            if (nIds_ >= 1) {
                if (isIntel_) {
                    L1CacheLineSize_ = (((data_[1][1] >> 8) & 0xff) * 8);
                }
                f_1_ECX_ = data_[1][2];
                f_1_EDX_ = data_[1][3];
            }
            // Load bitset with flags for function 0x00000007
            if (nIds_ >= 7) {
                f_7_EBX_ = data_[7][1];
                f_7_ECX_ = data_[7][2];
            }
            // Calling __cpuid with 0x80000000 as the function_id argument
            // gets the number of the highest valid extended ID.
            __cpuid (cpui.data (), 0x80000000);
            ui32 nExIds_ = cpui[0];
            std::vector<std::array<ui32, 4>> extdata_;
            for (ui32 i = 0x80000000; i <= nExIds_; ++i) {
                __cpuidex (cpui.data (), i, 0);
                extdata_.push_back (cpui);
            }
            // Load bitset with flags for function 0x80000001
            if (nExIds_ >= 0x80000001) {
                f_81_ECX_ = extdata_[1][2];
                f_81_EDX_ = extdata_[1][3];
            }
            // Interpret CPU brand string if reported
            if (nExIds_ >= 0x80000004) {
                char brand[0x40];
                memset (brand, 0, sizeof (brand));
                memcpy (brand, extdata_[2].data (), sizeof (cpui));
                memcpy (brand + 16, extdata_[3].data (), sizeof (cpui));
                memcpy (brand + 32, extdata_[4].data (), sizeof (cpui));
                brand_ = brand;
            }
            if (isAMD_ && nExIds_ >= 0x80000005) {
                L1CacheLineSize_ = extdata_[5][2] & 0xff;
            }
        }

    } // namespace util
} // namespace thekogans
