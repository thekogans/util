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
        private:
            /// \brief
            /// Host endianness.
            Endianness endianness;
            /// \brief
            /// Host cpu count.
            ui32 cpuCount;
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
            /// Process start directory.
            static std::string processStartDirectory;
            /// \brief
            /// Host name.
            std::string hostName;
            /// \brief
            /// Host id.
            std::string hostId;
            /// \brief
            /// User name.
            std::string userName;

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
            /// Return process start directory.
            /// \return Process start directory.
            inline std::string GetProcessStartDirectory () const {
                return processStartDirectory;
            }

            /// \brief
            /// Return host name.
            /// \return Host name.
            inline std::string GetHostName () const {
                return hostName;
            }

            /// \brief
            /// Return host id.
            /// \return Host id.
            inline std::string GetHostId () const {
                return hostId;
            }

            /// \brief
            /// Return user name.
            /// \return User name.
            inline std::string GetUserName () const {
                return userName;
            }

            /// \brief
            /// Dump system info to std::ostream.
            /// \param[in] stream std::ostream to dump the info to.
            void Dump (std::ostream &stream = std::cout) const;

            /// \brief
            /// SystemInfo is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SystemInfo)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SystemInfo_h)
