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

#if !defined (__thekogans_util_CPUInfo_h)
#define __thekogans_util_CPUInfo_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

        /// \struct CPUInfo CPUInfo.h thekogans/util/CPUInfo.h
        ///
        /// \brief
        /// CPUInfo is a system wide singleton that provides available
        /// cpu features in a platform independent manner.

        // This class was heavily borrowed from: https://msdn.microsoft.com/en-us/library/hskdteyh.aspx

        struct _LIB_THEKOGANS_UTIL_DECL CPUInfo :
                public Singleton<CPUInfo, SpinLock> {
        private:
        #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
            /// \brief
            /// Vendor string ("GenuineIntel", "AuthenticAMD"...).
            std::string vendor;
            /// \brief
            /// CPU brand.
            std::string brand;
            /// \brief
            /// true == Intel cpu.
            bool isIntel;
            /// \brief
            /// true == AMD cpu.
            bool isAMD;
            /// \brief
            /// L1 cache line size.
            ui32 l1CacheLineSize;
            /// \brief
            /// cpuid function 1 ecx register value.
            Flags32 f_1_ECX;
            /// \brief
            /// cpuid function 1 edx register value.
            Flags32 f_1_EDX;
            /// \brief
            /// cpuid function 7 ebx register value.
            Flags32 f_7_EBX;
            /// \brief
            /// cpuid function 7 ecx register value.
            Flags32 f_7_ECX;
            /// \brief
            /// cpuid function 81 ecx register value.
            Flags32 f_81_ECX;
            /// \brief
            /// cpuid function 81 edx register value.
            Flags32 f_81_EDX;
        #elif defined (TOOLCHAIN_ARCH_ppc) || defined (TOOLCHAIN_ARCH_ppc64)
            /// \brief
            /// true == AltiVec is supported.
            bool isAltiVec;
        #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)

        public:
            /// \brief
            /// ctor.
            CPUInfo ();

        #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
            /// \brief
            /// Return the vendor string ("GenuineIntel", "AuthenticAMD"...).
            /// \return Vendor string.
            inline const std::string &Vendor () const {
                return vendor;
            }
            /// \brief
            /// Return the cpu brand.
            /// \return CPU brand.
            inline const std::string &Brand () const {
                return brand;
            }

            /// \brief
            /// Return true if it's an Intel cpu.
            /// \return true == Intel cpu.
            inline bool Intel () const {
                return isIntel;
            }
            /// \brief
            /// Return true if it's an AMD cpu.
            /// \return true == AMD cpu.
            inline bool AMD () const {
                return isAMD;
            }

            /// \brief
            /// Return the size (in bytes) of the L1 cache line.
            /// \return The size (in bytes) of the L1 cache line.
            inline ui32 L1CacheLineSize () const {
                return l1CacheLineSize;
            }

            /// \brief
            /// Return true if SSE3 is supported.
            /// \return true == SSE3 is supported.
            inline bool SSE3 () const {
                return f_1_ECX.Test (0);
            }
            /// \brief
            /// Return true if PCLMULQDQ is supported.
            /// \return true == PCLMULQDQ is supported.
            inline bool PCLMULQDQ () const {
                return f_1_ECX.Test (1);
            }
            /// \brief
            /// Return true if MONITOR is supported.
            /// \return true == MONITOR is supported.
            inline bool MONITOR () const {
                return f_1_ECX.Test (3);
            }
            /// \brief
            /// Return true if SSSE3 is supported.
            /// \return true == SSSE3 is supported.
            inline bool SSSE3 () const {
                return f_1_ECX.Test (9);
            }
            /// \brief
            /// Return true if FMA is supported.
            /// \return true == FMA is supported.
            inline bool FMA () const {
                return f_1_ECX.Test (12);
            }
            /// \brief
            /// Return true if CMPXCHG16B is supported.
            /// \return true == CMPXCHG16B is supported.
            inline bool CMPXCHG16B () const {
                return f_1_ECX.Test (13);
            }
            /// \brief
            /// Return true if  is supported.
            /// \return true ==  is supported.
            inline bool SSE41 () const {
                return f_1_ECX.Test (19);
            }
            /// \brief
            /// Return true if SSE42 is supported.
            /// \return true == SSE42 is supported.
            inline bool SSE42 () const {
                return f_1_ECX.Test (20);
            }
            /// \brief
            /// Return true if MOVBE is supported.
            /// \return true == MOVBE is supported.
            inline bool MOVBE () const {
                return f_1_ECX.Test (22);
            }
            /// \brief
            /// Return true if POPCNT is supported.
            /// \return true == POPCNT is supported.
            inline bool POPCNT () const {
                return f_1_ECX.Test (23);
            }
            /// \brief
            /// Return true if AES is supported.
            /// \return true == AES is supported.
            inline bool AES () const {
                return f_1_ECX.Test (25);
            }
            /// \brief
            /// Return true if XSAVE is supported.
            /// \return true == XSAVE is supported.
            inline bool XSAVE () const {
                return f_1_ECX.Test (26);
            }
            /// \brief
            /// Return true if OSXSAVE is supported.
            /// \return true == OSXSAVE is supported.
            inline bool OSXSAVE () const {
                return f_1_ECX.Test (27);
            }
            /// \brief
            /// Return true if AVX is supported.
            /// \return true == AVX is supported.
            inline bool AVX () const {
                return f_1_ECX.Test (28);
            }
            /// \brief
            /// Return true if F16C is supported.
            /// \return true == F16C is supported.
            inline bool F16C () const {
                return f_1_ECX.Test (29);
            }
            /// \brief
            /// Return true if RDRAND is supported.
            /// \return true == RDRAND is supported.
            inline bool RDRAND () const {
                return f_1_ECX.Test (30);
            }

            /// \brief
            /// Return true if MSR is supported.
            /// \return true == MSR is supported.
            inline bool MSR () const {
                return f_1_EDX.Test (5);
            }
            /// \brief
            /// Return true if CX8 is supported.
            /// \return true == CX8 is supported.
            inline bool CX8 () const {
                return f_1_EDX.Test (8);
            }
            /// \brief
            /// Return true if SEP is supported.
            /// \return true == SEP is supported.
            inline bool SEP () const {
                return f_1_EDX.Test (11);
            }
            /// \brief
            /// Return true if CMOV is supported.
            /// \return true == CMOV is supported.
            inline bool CMOV () const {
                return f_1_EDX.Test (15);
            }
            /// \brief
            /// Return true if RDTSC is supported.
            /// \return true == RDTSC is supported.
            inline bool RDTSC () const {
                return isIntel && f_1_EDX.Test (16);
            }
            /// \brief
            /// Return true if CLFSH is supported.
            /// \return true == CLFSH is supported.
            inline bool CLFSH () const {
                return f_1_EDX.Test (19);
            }
            /// \brief
            /// Return true if MMX is supported.
            /// \return true == MMX is supported.
            inline bool MMX () const {
                return f_1_EDX.Test (23);
            }
            /// \brief
            /// Return true if FXSR is supported.
            /// \return true == FXSR is supported.
            inline bool FXSR () const {
                return f_1_EDX.Test (24);
            }
            /// \brief
            /// Return true if SSE is supported.
            /// \return true == SSE is supported.
            inline bool SSE () const {
                return f_1_EDX.Test (25);
            }
            /// \brief
            /// Return true if SSE2 is supported.
            /// \return true == SSE2 is supported.
            inline bool SSE2 () const {
                return f_1_EDX.Test (26);
            }

            /// \brief
            /// Return true if FSGSBASE is supported.
            /// \return true == FSGSBASE is supported.
            inline bool FSGSBASE () const {
                return f_7_EBX.Test (0);
            }
            /// \brief
            /// Return true if BMI1 is supported.
            /// \return true == BMI1 is supported.
            inline bool BMI1 () const {
                return f_7_EBX.Test (3);
            }
            /// \brief
            /// Return true if HLE is supported.
            /// \return true == HLE is supported.
            inline bool HLE () const {
                return isIntel && f_7_EBX.Test (4);
            }
            /// \brief
            /// Return true if AVX2 is supported.
            /// \return true == AVX2 is supported.
            inline bool AVX2 () const {
                return f_7_EBX.Test (5);
            }
            /// \brief
            /// Return true if BMI2 is supported.
            /// \return true == BMI2 is supported.
            inline bool BMI2 () const {
                return f_7_EBX.Test (8);
            }
            /// \brief
            /// Return true if ERMS is supported.
            /// \return true == ERMS is supported.
            inline bool ERMS () const {
                return f_7_EBX.Test (9);
            }
            /// \brief
            /// Return true if INVPCID is supported.
            /// \return true == INVPCID is supported.
            inline bool INVPCID () const {
                return f_7_EBX.Test (10);
            }
            /// \brief
            /// Return true if RTM is supported.
            /// \return true == RTM is supported.
            inline bool RTM () const {
                return isIntel && f_7_EBX.Test (11);
            }
            /// \brief
            /// Return true if AVX512F is supported.
            /// \return true == AVX512F is supported.
            inline bool AVX512F () const {
                return f_7_EBX.Test (16);
            }
            /// \brief
            /// Return true if RDSEED is supported.
            /// \return true == RDSEED is supported.
            inline bool RDSEED () const {
                return f_7_EBX.Test (18);
            }
            /// \brief
            /// Return true if ADX is supported.
            /// \return true == ADX is supported.
            inline bool ADX () const {
                return f_7_EBX.Test (19);
            }
            /// \brief
            /// Return true if AVX512PF is supported.
            /// \return true == AVX512PF is supported.
            inline bool AVX512PF () const {
                return f_7_EBX.Test (26);
            }
            /// \brief
            /// Return true if AVX512ER is supported.
            /// \return true == AVX512ER is supported.
            inline bool AVX512ER () const {
                return f_7_EBX.Test (27);
            }
            /// \brief
            /// Return true if AVX512CD is supported.
            /// \return true == AVX512CD is supported.
            inline bool AVX512CD () const {
                return f_7_EBX.Test (28);
            }
            /// \brief
            /// Return true if SHA is supported.
            /// \return true == SHA is supported.
            inline bool SHA () const {
                return f_7_EBX.Test (29);
            }

            /// \brief
            /// Return true if PREFETCHWT1 is supported.
            /// \return true == PREFETCHWT1 is supported.
            inline bool PREFETCHWT1 () const {
                return f_7_ECX.Test (0);
            }

            /// \brief
            /// Return true if LAHF is supported.
            /// \return true == LAHF is supported.
            inline bool LAHF () const {
                return f_81_ECX.Test (0);
            }
            /// \brief
            /// Return true if LZCNT is supported.
            /// \return true == LZCNT is supported.
            inline bool LZCNT () const {
                return isIntel && f_81_ECX.Test (5);
            }
            /// \brief
            /// Return true if ABM is supported.
            /// \return true == ABM is supported.
            inline bool ABM () const {
                return isAMD && f_81_ECX.Test (5);
            }
            /// \brief
            /// Return true if SSE4a is supported.
            /// \return true == SSE4a is supported.
            inline bool SSE4a () const {
                return isAMD && f_81_ECX.Test (6);
            }
            /// \brief
            /// Return true if XOP is supported.
            /// \return true == XOP is supported.
            inline bool XOP () const {
                return isAMD && f_81_ECX.Test (11);
            }
            /// \brief
            /// Return true if TBM is supported.
            /// \return true == TBM is supported.
            inline bool TBM () const {
                return isAMD && f_81_ECX.Test (21);
            }

            /// \brief
            /// Return true if SYSCALL is supported.
            /// \return true == SYSCALL is supported.
            inline bool SYSCALL () const {
                return isIntel && f_81_EDX.Test (11);
            }
            /// \brief
            /// Return true if MMXEXT is supported.
            /// \return true == MMXEXT is supported.
            inline bool MMXEXT () const {
                return isAMD && f_81_EDX.Test (22);
            }
            /// \brief
            /// Return true if RDTSCP is supported.
            /// \return true == RDTSCP is supported.
            inline bool RDTSCP () const {
                return isIntel && f_81_EDX.Test (27);
            }
            /// \brief
            /// Return true if _3DNOWEXT is supported.
            /// \return true == _3DNOWEXT is supported.
            inline bool _3DNOWEXT () const {
                return isAMD && f_81_EDX.Test (30);
            }
            /// \brief
            /// Return true if _3DNOW is supported.
            /// \return true == _3DNOW is supported.
            inline bool _3DNOW () const {
                return isAMD && f_81_EDX.Test (31);
            }
        #elif defined (TOOLCHAIN_ARCH_ppc) || defined (TOOLCHAIN_ARCH_ppc64)
            /// \brief
            /// Return true if AltiVec is supported.
            /// \return true == AltiVec is supported.
            inline bool AltiVec () const {
                return isAltiVec;
            }
        #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_CPUInfo_h)
