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
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/CPUInfo.h"
#include "thekogans/util/Buffer.h"

namespace thekogans {
    namespace util {

    #if !defined (TOOLCHAIN_OS_Windows)
        namespace {
            void __cpuid (
                    int cpu_info[4],
                    int function_id) {
                __asm__ volatile (
                    "pushq %%rbx\n"
                    "cpuid\n"
                    "movq %%rbx, %%rsi\n"
                    "popq %%rbx\n"
                    : "=a" (cpu_info[0]), "=S" (cpu_info[1]), "=c" (cpu_info[2]), "=d" (cpu_info[3])
                    : "a" (function_id));
            }

            void __cpuidex (
                    int cpu_info[4],
                    int function_id,
                    int subfunction_id) {
                __asm__ volatile (
                    "mov %%ebx, %%edi\n"
                    "cpuid\n"
                    "xchg %%edi, %%ebx\n"
                    : "=a" (cpu_info[0]), "=D" (cpu_info[1]), "=c" (cpu_info[2]), "=d" (cpu_info[3])
                    : "a" (function_id), "c" (subfunction_id));
            }
        }
    #endif // !defined (TOOLCHAIN_OS_Windows)

        CPUInfo::CPUInfo () :
                isIntel_ (false),
                isAMD_ (false),
                f_1_ECX_ (0),
                f_1_EDX_ (0),
                f_7_EBX_ (0),
                f_7_ECX_ (0),
                f_81_ECX_ (0),
                f_81_EDX_ (0) {
            std::array<int, 4> cpui;
            // Calling __cpuid with 0x0 as the function_id argument
            // gets the number of the highest valid function ID.
            __cpuid (cpui.data (), 0);
            int nIds_ = cpui[0];
            std::vector<std::array<int, 4>> data_;
            for (int i = 0; i <= nIds_; ++i) {
                __cpuidex (cpui.data (), i, 0);
                data_.push_back (cpui);
            }
            // Capture vendor string
            char vendor[0x20];
            memset (vendor, 0, sizeof (vendor));
            *reinterpret_cast<int *> (vendor) = data_[0][1];
            *reinterpret_cast<int *> (vendor + 4) = data_[0][3];
            *reinterpret_cast<int *> (vendor + 8) = data_[0][2];
            vendor_ = vendor;
            if (vendor_ == "GenuineIntel") {
                isIntel_ = true;
            }
            else if (vendor_ == "AuthenticAMD") {
                isAMD_ = true;
            }
            // load bitset with flags for function 0x00000001
            if (nIds_ >= 1) {
                f_1_ECX_ = data_[1][2];
                f_1_EDX_ = data_[1][3];
            }
            // load bitset with flags for function 0x00000007
            if (nIds_ >= 7) {
                f_7_EBX_ = data_[7][1];
                f_7_ECX_ = data_[7][2];
            }
            // Calling __cpuid with 0x80000000 as the function_id argument
            // gets the number of the highest valid extended ID.
            __cpuid (cpui.data(), 0x80000000);
            int nExIds_ = cpui[0];
            std::vector<std::array<int, 4>> extdata_;
            for (int i = 0x80000000; i <= nExIds_; ++i) {
                __cpuidex (cpui.data (), i, 0);
                extdata_.push_back (cpui);
            }
            // load bitset with flags for function 0x80000001
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
        }

    } // namespace util
} // namespace thekogans
