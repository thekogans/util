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

#if !defined (__thekogans_util_LockGuard_h)
#define __thekogans_util_LockGuard_h

#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct LockGuard LockGuard.h thekogans/util/LockGuard.h
        ///
        /// \brief
        /// A very simple lock management template.
        ///
        /// Use this template to achieve exception safty in your
        /// code. Here's how:
        ///
        /// \code
        /// using namespace thekogans;
        ///
        /// void foo (...) {
        ///     util::LockGuard<util::Mutex> guard (mutex);
        ///     // function body with potentially many exit points,
        ///     // and exceptional conditions.
        /// }
        /// \endcode
        ///
        /// This function will correctly release the mutex no mater
        /// what exit point is used.

        template<typename Lock>
        struct LockGuard {
        private:
            /// \brief
            /// Lock to use to guard access to a shared resource.
            Lock &lock;
            /// \brief
            /// Release was called.
            bool released;

        public:
            /// \brief
            /// ctor. Acquire the lock.
            /// \param[in] lock_ Lock to acquire.
            /// \param[in] acquire true = acquire the lock,
            /// false = don't acquire the lock.
            /// NOTE: That bool acquire flags is there to help you
            /// write code like this:
            ///
            /// \code{.cpp}
            /// thekogans::util::Mutex mutex;
            /// if (mutex.TryAcquire ()) {
            ///     thekogans::util::LockGuard<thekogans::util::Mutex> guard (mutex, false);
            ///     ...
            /// }
            /// else {
            ///     // Couldn't acquire the mutex.
            ///     ...
            /// }
            /// \endcode
            ///
            /// That mutex will be released no mater how many
            /// exits that if statement has.
            explicit LockGuard (
                    Lock &lock_,
                    bool acquire = true) :
                    lock (lock_),
                    released (false) {
                if (acquire) {
                    lock.Acquire ();
                }
            }
            /// \brief
            /// dtor. Release the lock.
            ~LockGuard () {
                if (!released) {
                    lock.Release ();
                }
            }

            /// \brief
            /// Reacquire the lock.
            inline void Acquire () {
                if (released) {
                    lock.Acquire ();
                    released = false;
                }
            }

            /// \brief
            /// Release the lock.
            inline void Release () {
                if (!released) {
                    lock.Release ();
                    released = true;
                }
            }

            /// \brief
            /// LockGuard is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (LockGuard)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_LockGuard_h)
