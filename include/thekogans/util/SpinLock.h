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

#if !defined (__thekogans_util_SpinLock_h)
#define __thekogans_util_SpinLock_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"

namespace thekogans {
    namespace util {

        /// \struct StorageSpinLock SpinLock.h thekogans/util/SpinLock.h
        ///
        /// \brief
        /// StorageSpinLock wraps a provided ui32 & so that it can be used
        /// with the rest of the util synchronization machinery.
        ///
        /// This implementation was adapted from:
        /// http://www.boost.org/doc/libs/1_53_0/doc/html/atomic/usage_examples.html

        struct _LIB_THEKOGANS_UTIL_DECL StorageSpinLock {
        public:
            /// \enum
            /// SpinLock state type.
            /// \brief
            /// Unlocked.
            static const ui32 Unlocked = 0;
            /// \brief
            /// Locked.
            static const ui32 Locked = 1;
            /// \brief
            /// Default max pause iterations before giving up the time slice.
            static const ui32 DEFAULT_MAX_PAUSE_BEFORE_YIELD = 16;

        private:
            /// \brief
            /// SpinLock state.
            ui32 &state;
            /// \brief
            /// \see{Thread::Backoff} parameter.
            ui32 maxPauseBeforeYield;

        public:
            /// \brief
            /// Default ctor. Initialize to unlocked.
            /// \param[in] state_ Storage for spin lock state.
            /// \param[in] maxPauseBeforeYield_ \see{Thread::Backoff} parameter.
            StorageSpinLock (
                    ui32 &state_,
                    ui32 maxPauseBeforeYield_ = DEFAULT_MAX_PAUSE_BEFORE_YIELD) :
                    state (state_),
                    maxPauseBeforeYield (maxPauseBeforeYield_) {
                state = Unlocked;
            }

            /// \brief
            /// Return true if locked.
            /// \return true if locked.
            bool IsLocked () const;

            /// \brief
            /// Try to acquire the lock.
            /// \return true = acquired, false = failed to acquire
            bool TryAcquire ();

            /// \brief
            /// Acquire the lock.
            void Acquire ();

            /// \brief
            /// Release the lock.
            void Release ();

            /// \brief
            /// SpinLock is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (StorageSpinLock)
        };

        /// \struct SpinLock SpinLock.h thekogans/util/SpinLock.h
        ///
        /// \brief
        /// SpinLock wraps a ui32 so that it can be used
        /// with the rest of the util synchronization machinery.
        ///
        /// This implementation was adapted from:
        /// http://www.boost.org/doc/libs/1_53_0/doc/html/atomic/usage_examples.html

        struct _LIB_THEKOGANS_UTIL_DECL SpinLock : private StorageSpinLock {
        private:
            /// \brief
            /// SpinLock state.
            ui32 state;

        public:
            /// \brief
            /// Default ctor. Initialize to unlocked.
            /// \param[in] maxPauseBeforeYield \see{Thread::Backoff} parameter.
            SpinLock (ui32 maxPauseBeforeYield = DEFAULT_MAX_PAUSE_BEFORE_YIELD) :
                StorageSpinLock (state, maxPauseBeforeYield) {}

            /// \brief
            /// Return true if locked.
            /// \return true if locked.
            inline bool IsLocked () const {
                return StorageSpinLock::IsLocked ();
            }

            /// \brief
            /// Try to acquire the lock.
            /// \return true = acquired, false = failed to acquire
            inline bool TryAcquire () {
                return StorageSpinLock::TryAcquire ();
            }

            /// \brief
            /// Acquire the lock.
            inline void Acquire () {
                StorageSpinLock::Acquire ();
            }

            /// \brief
            /// Release the lock.
            inline void Release () {
                StorageSpinLock::Release ();
            }

            /// \brief
            /// SpinLock is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SpinLock)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SpinLock_h)
