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

#if !defined (__thekogans_util_SpinRWLock_h)
#define __thekogans_util_SpinRWLock_h

#include <atomic>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"

namespace thekogans {
    namespace util {

        /// \struct SpinRWLock SpinRWLock.h thekogans/util/SpinRWLock.h
        ///
        /// \brief
        /// SpinRWLock wraps a std::atomic so that it can be used
        /// with the rest of the util synchronization machinery.
        ///
        /// This implementation was adapted from Intel TBB.

        struct _LIB_THEKOGANS_UTIL_DECL SpinRWLock {
        private:
            /// \brief
            /// Default max pause iterations before giving up the time slice.
            static const ui32 DEFAULT_MAX_PAUSE_BEFORE_YIELD = 16;
            /// \brief
            /// Flag indicating the presence of a writer.
            static const ui32 WRITER = 1;
            /// \brief
            /// \see{Thread::Backoff} parameter.
            ui32 maxPauseBeforeYield;
            /// \brief
            /// Flag indicating that a writer is waiting
            /// for the readers to exit.
            static const ui32 WRITER_PENDING = 2;
            /// \brief
            /// All other bits are used to keep count of readers.
            static const ui32 READERS = ~(WRITER | WRITER_PENDING);
            /// \brief
            /// Test if a single reader has the lock.
            static const ui32 ONE_READER = 4;
            /// \brief
            /// Mask to test if the lock is busy with readers or writer.
            static const ui32 BUSY = WRITER | READERS;
            /// \brief
            /// Lock state.
            std::atomic<ui32> state;

        public:
            /// \brief
            /// ctor. Initialize to unlocked.
            /// \param[in] maxPauseBeforeYield_ \see{Thread::Backoff} parameter.
            SpinRWLock (ui32 maxPauseBeforeYield_ = DEFAULT_MAX_PAUSE_BEFORE_YIELD) :
                maxPauseBeforeYield (maxPauseBeforeYield_),
                state (0) {}

            /// \brief
            /// Try to acquire the lock.
            /// \param[in] read true = acqure for reading, acquire for writing.
            /// \return true = acquierd, false = failed to acquire.
            bool TryAcquire (bool read);
            /// \brief
            /// Acquire the lock.
            /// \param[in] read true = acqure for reading, acquire for writing.
            void Acquire (bool read);
            /// \brief
            /// Release the lock.
            /// \param[in] read true = release for reading, release for writing.
            void Release (bool read);

            /// \brief
            /// SpinRWLock is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SpinRWLock)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SpinRWLock_h)
