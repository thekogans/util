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

namespace thekogans {
    namespace util {

        /// \struct SpinLock SpinLock.h thekogans/util/SpinLock.h
        ///
        /// \brief
        /// SpinLock wraps a std::atomic so that it can be used
        /// with the rest of the util synchronization machinery.
        ///
        /// This implementation was adapted from:
        /// http://www.boost.org/doc/libs/1_53_0/doc/html/atomic/usage_examples.html

        struct _LIB_THEKOGANS_UTIL_DECL SpinLock {
        private:
            /// \enum
            /// SpinLock state type.
            typedef enum {
                /// \brief
                /// Locked.
                Locked,
                /// \brief
                /// Unlocked.
                Unlocked
            } LockState;
            /// \brief
            /// SpinLock state.
            THEKOGANS_UTIL_ATOMIC<LockState> state;

        public:
            /// \brief
            /// Default ctor. Initialize to unlocked.
            SpinLock () :
                state (Unlocked) {}

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
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SpinLock)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SpinLock_h)
