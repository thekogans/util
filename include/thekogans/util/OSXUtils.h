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

#if defined (TOOLCHAIN_OS_OSX)

#if !defined (__thekogans_util_OSXUtils_h)
#define __thekogans_util_OSXUtils_h

#include <sys/stat.h>
#include <CoreFoundation/CFError.h>
#include <IOKit/IOReturn.h>
#include <string>

#define STAT_STRUCT struct stat
#define STAT_FUNC stat
#define LSTAT_FUNC lstat
#define FSTAT_FUNC fstat
#define LSEEK_FUNC lseek
#define FTRUNCATE_FUNC ftruncate

#if defined (TOOLCHAIN_ARCH_i386)
    #define keventStruct kevent
    #define keventFunc(kq, changelist, nchanges, eventlist, nevents, timeout)\
        kevent (kq, changelist, nchanges, eventlist, nevents, timeout)
    #define keventSet(kev, ident, filter, flags, fflags, data, udata)\
        EV_SET (kev, ident, filter, flags, fflags, data, (void *)udata)
#elif defined (TOOLCHAIN_ARCH_x86_64)
    #define keventStruct kevent64_s
    #define keventFunc(kq, changelist, nchanges, eventlist, nevents, timeout)\
        kevent64 (kq, changelist, nchanges, eventlist, nevents, 0, timeout)
    #define keventSet(kev, ident, filter, flags, fflags, data, udata)\
        EV_SET64 (kev, ident, filter, flags, fflags, data, (uint64_t)udata, 0, 0)
#else // defined (TOOLCHAIN_ARCH_i386)
    #error Unknown TOOLCHAIN_ARCH.
#endif // defined (TOOLCHAIN_ARCH_i386)

namespace thekogans {
    namespace util {

        /// \brief
        /// Return error description from the given OSStatus.
        /// \param[in] errorCode OSStatus to return description for.
        /// \return Error description from the given OSStatus.
        std::string DescriptionFromOSStatus (OSStatus errorCode);

        /// \brief
        /// Return error description from the given CFError.
        /// \param[in] error CFError to return description for.
        /// \return Error description from the given CFError.
        std::string DescriptionFromCFErrorRef (CFErrorRef error);

        /// \brief
        /// Return error description from the given IOReturn.
        /// \param[in] errorCode IOReturn to return description for.
        /// \return Error description from the given IOReturn.
        std::string DescriptionFromIOReturn (IOReturn errorCode);

        /// \brief
        /// Return the current user home directory path.
        /// \return Current user home directory path.
        std::string GetHomeDirectory ();

        /// \brief
        /// typedef for KQueueTimer alarm callback.
        typedef void (*KQueueTimerCallback) (void * /*userData*/);

        /// \brief
        /// Forward declaration for KQueueTimer.
        struct KQueueTimer;
        /// \brief
        /// Forward declaration for \see{TimeSpec}.
        struct TimeSpec;

        /// \brief
        /// Create a KQueueTimer.
        /// \param[in] timerCallback Timer alarm callback.
        /// \param[in] userData Parameter passed to callback.
        /// \return A new KQueueTimer struct.
        KQueueTimer *CreateKQueueTimer (
            KQueueTimerCallback timerCallback,
            void *userData);
        /// \brief
        /// Destroy the given KQueueTimer.
        /// \param[in] timer KQueueTimer to destroy.
        void DestroyKQueueTimer (KQueueTimer *timer);
        /// \brief
        /// Start the given KQueueTimer.
        /// \param[in] timer KQueueTimer to start.
        /// \param[in] timeSpec \see{TimeSpec} representing the timer interval.
        /// \param[in] periodic true == the timer will fire every timeSpec milliseconds,
        /// false == the timer will fire once after timeSpec milliseconds.
        void StartKQueueTimer (
            KQueueTimer *timer,
            const TimeSpec &timeSpec,
            bool periodic);
        /// \brief
        /// Stop the given KQueueTimer.
        /// \param[in] timer KQueueTimer to stop.
        void StopKQueueTimer (KQueueTimer *timer);
        /// \brief
        /// Return true if the given KQueueTimer is running.
        /// \param[in] timer KQueueTimer to check if running.
        /// \return true == the given KQueueTimer is running.
        bool IsKQueueTimerRunning (KQueueTimer *timer);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_OSXUtils_h)

#endif // defined (TOOLCHAIN_OS_OSX)
