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

#include <iostream>
#include "thekogans/util/Flags.h"
#include "thekogans/util/SystemInfo.h"

using namespace thekogans;

int main (
        int /*argc*/,
        const char * /*argv*/ []) {
    util::Endianness endianness = util::SystemInfo::Instance ().GetEndianness ();
    std::cout << "Endianness: " << (endianness == util::LittleEndian ? "LittleEndian" : "BigEndian") << std::endl;
    util::ui32 cpuCount = util::SystemInfo::Instance ().GetCPUCount ();
    std::cout << "CPU count: " << cpuCount << std::endl;
    util::ui32 cpuL1CacheLineSize = util::SystemInfo::Instance ().GetCPUL1CacheLineSize ();
    std::cout << "CPU L1 cache line size: " << cpuL1CacheLineSize << std::endl;
    util::Flags32 features = util::SystemInfo::Instance ().GetCPUFeatures ();
    std::cout << "Features: ";
    if (features.Test (util::SystemInfo::CPU_HAS_RDTSC)) {
        std::cout << "RDTSC ";
    }
    if (features.Test (util::SystemInfo::CPU_HAS_ALTIVEC)) {
        std::cout << "ALTIVEC ";
    }
    if (features.Test (util::SystemInfo::CPU_HAS_MMX)) {
        std::cout << "MMX ";
    }
    if (features.Test (util::SystemInfo::CPU_HAS_3DNOW)) {
        std::cout << "3DNOW ";
    }
    if (features.Test (util::SystemInfo::CPU_HAS_SSE)) {
        std::cout << "SSE ";
    }
    if (features.Test (util::SystemInfo::CPU_HAS_SSE2)) {
        std::cout << "SSE2 ";
    }
    if (features.Test (util::SystemInfo::CPU_HAS_SSE3)) {
        std::cout << "SSE3 ";
    }
    if (features.Test (util::SystemInfo::CPU_HAS_SSE41)) {
        std::cout << "SSE41 ";
    }
    if (features.Test (util::SystemInfo::CPU_HAS_SSE42)) {
        std::cout << "SSE42 ";
    }
    if (features.Test (util::SystemInfo::CPU_HAS_AVX)) {
        std::cout << "AVX ";
    }
    std::cout << std::endl;
    util::ui32 pageSize = util::SystemInfo::Instance ().GetPageSize ();
    std::cout << "Page size: " << pageSize << std::endl;
    util::ui64 memorySize = util::SystemInfo::Instance ().GetMemorySize ();
    std::cout << "Memory size: " << memorySize << std::endl;
    return 0;
}
