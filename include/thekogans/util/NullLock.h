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

#if !defined (__thekogans_util_NullLock_h)
#define __thekogans_util_NullLock_h

#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct NullLock NullLock.h thekogans/util/NullLock.h
        ///
        /// \brief
        /// NullLock is a noop lock. It is used in single threaded
        /// systems in place where a real lock (mutex, spin lock...)
        /// would be used. It is designed to give optimizing compilers
        /// a chance to optimize it away at build time.

        struct _LIB_THEKOGANS_UTIL_DECL NullLock {
            /// \brief
            /// ctor.
            NullLock ();
            /// \brief
            /// dtor.
            ~NullLock ();

            /// \brief
            /// Try to acquire the lock (noop)
            bool TryAcquire ();
            /// \brief
            /// Acquire the lock (noop)
            void Acquire ();
            /// \brief
            /// Release the lock (noop)
            void Release ();
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_NullLock_h)
