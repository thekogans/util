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

#include <vector>
#include <bitset>
#include <array>
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

        struct _LIB_THEKOGANS_UTIL_DECL CPUInfo : public Singleton<CPUInfo, SpinLock> {
        private:
            std::string vendor_;
            std::string brand_;
            bool isIntel_;
            bool isAMD_;
            std::bitset<32> f_1_ECX_;
            std::bitset<32> f_1_EDX_;
            std::bitset<32> f_7_EBX_;
            std::bitset<32> f_7_ECX_;
            std::bitset<32> f_81_ECX_;
            std::bitset<32> f_81_EDX_;

        public:
            CPUInfo ();

            inline const std::string &Vendor () const {
                return vendor_;
            }
            inline const std::string &Brand () const {
                return brand_;
            }

            inline bool Intel () const {
                return isIntel_;
            }
            inline bool AMD () const {
                return isAMD_;
            }

            inline bool SSE3 () const {
                return f_1_ECX_[0];
            }
            inline bool PCLMULQDQ () const {
                return f_1_ECX_[1];
            }
            inline bool MONITOR () const {
                return f_1_ECX_[3];
            }
            inline bool SSSE3 () const {
                return f_1_ECX_[9];
            }
            inline bool FMA () const {
                return f_1_ECX_[12];
            }
            inline bool CMPXCHG16B () const {
                return f_1_ECX_[13];
            }
            inline bool SSE41 () const {
                return f_1_ECX_[19];
            }
            inline bool SSE42 () const {
                return f_1_ECX_[20];
            }
            inline bool MOVBE () const {
                return f_1_ECX_[22];
            }
            inline bool POPCNT () const {
                return f_1_ECX_[23];
            }
            inline bool AES () const {
                return f_1_ECX_[25];
            }
            inline bool XSAVE () const {
                return f_1_ECX_[26];
            }
            inline bool OSXSAVE () const {
                return f_1_ECX_[27];
            }
            inline bool AVX () const {
                return f_1_ECX_[28];
            }
            inline bool F16C () const {
                return f_1_ECX_[29];
            }
            inline bool RDRAND () const {
                return f_1_ECX_[30];
            }

            inline bool MSR () const {
                return f_1_EDX_[5];
            }
            inline bool CX8 () const {
                return f_1_EDX_[8];
            }
            inline bool SEP () const {
                return f_1_EDX_[11];
            }
            inline bool CMOV () const {
                return f_1_EDX_[15];
            }
            inline bool CLFSH () const {
                return f_1_EDX_[19];
            }
            inline bool MMX () const {
                return f_1_EDX_[23];
            }
            inline bool FXSR () const {
                return f_1_EDX_[24];
            }
            inline bool SSE () const {
                return f_1_EDX_[25];
            }
            inline bool SSE2 () const {
                return f_1_EDX_[26];
            }

            inline bool FSGSBASE () const {
                return f_7_EBX_[0];
            }
            inline bool BMI1 () const {
                return f_7_EBX_[3];
            }
            inline bool HLE () const {
                return isIntel_ && f_7_EBX_[4];
            }
            inline bool AVX2 () const {
                return f_7_EBX_[5];
            }
            inline bool BMI2 () const {
                return f_7_EBX_[8];
            }
            inline bool ERMS () const {
                return f_7_EBX_[9];
            }
            inline bool INVPCID () const {
                return f_7_EBX_[10];
            }
            inline bool RTM () const {
                return isIntel_ && f_7_EBX_[11];
            }
            inline bool AVX512F () const {
                return f_7_EBX_[16];
            }
            inline bool RDSEED () const {
                return f_7_EBX_[18];
            }
            inline bool ADX () const {
                return f_7_EBX_[19];
            }
            inline bool AVX512PF () const {
                return f_7_EBX_[26];
            }
            inline bool AVX512ER () const {
                return f_7_EBX_[27];
            }
            inline bool AVX512CD () const {
                return f_7_EBX_[28];
            }
            inline bool SHA () const {
                return f_7_EBX_[29];
            }

            inline bool PREFETCHWT1 () const {
                return f_7_ECX_[0];
            }

            inline bool LAHF () const {
                return f_81_ECX_[0];
            }
            inline bool LZCNT () const {
                return isIntel_ && f_81_ECX_[5];
            }
            inline bool ABM () const {
                return isAMD_ && f_81_ECX_[5];
            }
            inline bool SSE4a () const {
                return isAMD_ && f_81_ECX_[6];
            }
            inline bool XOP () const {
                return isAMD_ && f_81_ECX_[11];
            }
            inline bool TBM () const {
                return isAMD_ && f_81_ECX_[21];
            }

            inline bool SYSCALL () const {
                return isIntel_ && f_81_EDX_[11];
            }
            inline bool MMXEXT () const {
                return isAMD_ && f_81_EDX_[22];
            }
            inline bool RDTSCP () const {
                return isIntel_ && f_81_EDX_[27];
            }
            inline bool _3DNOWEXT () const {
                return isAMD_ && f_81_EDX_[30];
            }
            inline bool _3DNOW () const {
                return isAMD_ && f_81_EDX_[31];
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_CPUInfo_h)
