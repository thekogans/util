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

#if !defined (__thekogans_util_SystemInfo_h)
#define __thekogans_util_SystemInfo_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/ByteSwap.h"

namespace thekogans {
    namespace util {

        /// \struct SystemInfo SystemInfo.h thekogans/util/SystemInfo.h
        ///
        /// \brief
        /// SystemInfo is a system wide singleton that provides basic
        /// system stats in a platform independent manner.

        struct _LIB_THEKOGANS_UTIL_DECL SystemInfo :
                public Singleton<SystemInfo, SpinLock> {
            /// \brief
            /// CPU features.
            enum {
                /// \brief
                /// Time stamp counter.
                CPU_HAS_RDTSC = 0x00000001,
                /// \brief
                /// Altivec
                CPU_HAS_ALTIVEC = 0x00000002,
                /// \brief
                /// MMX
                CPU_HAS_MMX = 0x00000004,
                /// \brief
                /// 3DNOW
                CPU_HAS_3DNOW = 0x00000008,
                /// \brief
                /// SSE
                CPU_HAS_SSE = 0x00000010,
                /// \brief
                /// SSE 2
                CPU_HAS_SSE2 = 0x00000020,
                /// \brief
                /// SSE 3
                CPU_HAS_SSE3 = 0x00000040,
                /// \brief
                /// SSE 4.1
                CPU_HAS_SSE41 = 0x00000100,
                /// \brief
                /// SSE 4.2
                CPU_HAS_SSE42 = 0x00000200,
                /// \brief
                /// AVX
                CPU_HAS_AVX = 0x00000400
            };

        private:
            /// \brief
            /// Host endianness.
            Endianness endianness;
            /// \brief
            /// Host cpu count.
            ui32 cpuCount;
            /// \brief
            /// Host level 1 cache line size.
            ui32 cpuL1CacheLineSize;
            /// \brief
            /// Bitflags containing the cpu features.
            ui32 cpuFeatures;
            /// \brief
            /// Memory page size.
            ui32 pageSize;
            /// \brief
            /// Total size of physical memory.
            ui64 memorySize;
            /// \brief
            /// Process path.
            std::string processPath;
            /// \brief
            /// Host name.
            std::string hostName;

        public:
            /// \brief
            /// ctor. Gather system info.
            SystemInfo ();

            /// \brief
            /// Return the host endianness.
            /// \return Host endianness.
            inline Endianness GetEndianness () const {
                return endianness;
            }
            /// \brief
            /// Return CPU count.
            /// \return CPU count.
            inline ui32 GetCPUCount () const {
                return cpuCount;
            }
            /// \brief
            /// Return the size (in bytes) of the cpu L1 cache line.
            /// \return The size (in bytes) of the cpu L1 cache line.
            inline ui32 GetCPUL1CacheLineSize () const {
                return cpuL1CacheLineSize;
            }
            /// \brief
            /// Return the cpu feature set.
            /// \return The cpu feature set.
            inline ui32 GetCPUFeatures () const {
                return cpuFeatures;
            }
            /// \brief
            /// Return memory page size.
            /// \return Memory page size.
            inline ui32 GetPageSize () const {
                return pageSize;
            }
            /// \brief
            /// Return physical memory size.
            /// \return Physical memory size.
            inline ui64 GetMemorySize () const {
                return memorySize;
            }

            /// \brief
            /// Return process path.
            /// \return Process path.
            inline std::string GetProcessPath () const {
                return processPath;
            }

            /// \brief
            /// Return host name.
            /// \return Host name.
            inline std::string GetHostName () const {
                return hostName;
            }

            /// \brief
            /// SystemInfo is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SystemInfo)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SystemInfo_h)
