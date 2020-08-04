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
#include "thekogans/util/TimeSpec.h"
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
            enum : ui8 {
                /// \brief
                /// Unknown OS.
                Unknown = 0,
                /// \brief
                /// We're running on Windows.
                Windows = 1,
                /// \brief
                /// We're running on Linux.
                Linux = 2,
                /// \brief
                /// We're running on OS X.
                OSX = 3
            };

        private:
            /// \brief
            /// Host endianness.
            Endianness endianness;
            /// \brief
            /// Host cpu count.
            std::size_t cpuCount;
            /// \brief
            /// Memory page size.
            std::size_t pageSize;
            /// \brief
            /// Total size of physical memory.
            ui64 memorySize;
            /// \brief
            /// Session id.
            THEKOGANS_UTIL_SESSION_ID sessionId;
            /// \brief
            /// Process path.
            std::string processPath;
            /// \brief
            /// Process id.
            THEKOGANS_UTIL_PROCESS_ID processId;
            /// \brief
            /// Process start directory.
            static std::string processStartDirectory;
            /// \brief
            /// Process start time.
            static TimeSpec processStartTime;
            /// \brief
            /// Host name.
            std::string hostName;
            /// \brief
            /// Host id.
            std::string hostId;
            /// \brief
            /// User name.
            std::string userName;
            /// \brief
            /// OS type we're running on (Windows, Linux or OSX).
            ui8 os;

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
            inline std::size_t GetCPUCount () const {
                return cpuCount;
            }

            /// \brief
            /// Return memory page size.
            /// \return Memory page size.
            inline std::size_t GetPageSize () const {
                return pageSize;
            }

            /// \brief
            /// Return physical memory size.
            /// \return Physical memory size.
            inline ui64 GetMemorySize () const {
                return memorySize;
            }

            /// \brief
            /// Return session id.
            /// \return Session id.
            inline THEKOGANS_UTIL_SESSION_ID GetSessionId () const {
                return sessionId;
            }

            /// \brief
            /// Return process path.
            /// \return Process path.
            inline const std::string &GetProcessPath () const {
                return processPath;
            }

            /// \brief
            /// Return process id.
            /// \return Process id.
            inline THEKOGANS_UTIL_PROCESS_ID GetProcessId () const {
                return processId;
            }

            /// \brief
            /// Return process start directory.
            /// \return Process start directory.
            inline const std::string &GetProcessStartDirectory () const {
                return processStartDirectory;
            }

            /// \brief
            /// Return process start time.
            /// \return Process start time.
            inline TimeSpec GetProcessStartTime () const {
                return processStartTime;
            }

            /// \brief
            /// Return host name.
            /// \return Host name.
            inline const std::string &GetHostName () const {
                return hostName;
            }
            /// \brief
            /// Set host name that will be returned by GetHostName above.
            /// NOTE: Call this method to set the name you want returned
            /// when calling GetHostName. While it's not necessary (GetHostName
            /// will query the OS to get the host name), it's sometimes
            /// beneficial because host names (especially on the mac) can
            /// vary depending on the network.
            /// \param[in] hostName_ Host name to set.
            inline void SetHostName (const std::string &hostName_) {
                hostName = hostName_;
            }

            /// \brief
            /// Return host id.
            /// \return Host id.
            inline const std::string &GetHostId () const {
                return hostId;
            }
            /// \brief
            /// Set host id that will be returned by GetHostId above.
            /// NOTE: This call exists for similar reasons to SetHostName.
            /// \param[in] hostId_ Host id to set.
            inline void SetHostId (const std::string &hostId_) {
                hostId = hostId_;
            }

            /// \brief
            /// Return user name.
            /// \return User name.
            inline const std::string &GetUserName () const {
                return userName;
            }
            /// \brief
            /// Set user name that will be returned by GetUserName above.
            /// NOTE: This call exists for similar reasons to SetHostName.
            /// \param[in] userName_ User name to set.
            inline void SetUserName (const std::string &userName_) {
                userName = userName_;
            }

            /// \brief
            /// Return OS we're running on (Windows, Linux or OS X).
            /// \return OS we're running on.
            inline ui8 GetOS () const {
                return os;
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
