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

#if !defined (__thekogans_util_RWLockGuard_h)
#define __thekogans_util_RWLockGuard_h

#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// A very simple lock management template.
        ///
        /// Use this lock guard to achieve exception safty in your
        /// code. Here's how:
        ///
        /// \code
        /// using namespace thekogans;
        ///
        /// void foo (...) {
        ///     util::RWLockGuard guard (rwlock, true/false);
        ///     // function body with potentially many exit points,
        ///     // and exceptional conditions.
        /// }
        /// \endcode
        ///
        /// This function will correctly release the lock no mater
        /// what exit point is used.

        template<typename T>
        struct RWLockGuard {
        private:
            /// \brief
            /// The lock to guard.
            T &lock;
            /// \brief
            /// Used to figure out how to acquire and release the lock.
            bool read;
            /// \brief
            /// Release was called.
            bool released;

        public:
            /// \brief
            /// ctor. Acquire the lock for reading or writing.
            /// \param[in] lock_ Lock to acquire.
            /// \param[in] read_ true = acquire for reading, false = acquire for writing.
            /// NOTE: That bool acquire flags is there to help you
            /// write code like this:
            ///
            /// \code{.cpp}
            /// thekogans::util::RWLock lock;
            /// if (lock.TryAcquire (true)) {
            ///     thekogans::util::RWLockGuard<thekogans::util::RWLock> guard (lock, true, false);
            ///     ...
            /// }
            /// else {
            ///     // Couldn't acquire the lock.
            ///     ...
            /// }
            /// \endcode
            ///
            /// That lock will be released no mater how many
            /// exits that if statement has.
            RWLockGuard (
                    T &lock_,
                    bool read_,
                    bool acquire = true) :
                    lock (lock_),
                    read (read_),
                    released (false) {
                if (acquire) {
                    lock.Acquire (read);
                }
            }
            /// \brief
            /// dtor. Release the lock.
            ~RWLockGuard () {
                if (!released) {
                    lock.Release (read);
                }
            }

            /// \brief
            /// Reacquire the lock.
            inline void Acquire () {
                if (released) {
                    lock.Acquire (read);
                    released = false;
                }
            }

            /// \brief
            /// Release the lock.
            inline void Release () {
                if (!released) {
                    lock.Release (read);
                    released = true;
                }
            }

            /// \brief
            /// RWLockGuard is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (RWLockGuard)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RWLockGuard_h)
