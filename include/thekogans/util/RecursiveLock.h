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

#if !defined (__thekogans_util_RecursiveLock_h)
#define __thekogans_util_RecursiveLock_h

#include "thekogans/util/Config.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Thread.h"

namespace thekogans {
    namespace util {

        /// \struct RecursiveLock RecursiveLock.h thekogans/util/RecursiveLock.h
        ///
        /// \brief
        /// RecursiveLock is an adapter template used to allow one thread
        /// to acquire the lock multiple times without releasing it.
        /// NOTE: RecursiveLock::Release must be called the same number
        /// of times you called RecursiveLock::[Try]Acquire.
        /// WARNING: Given the state required to maintain the RecursiveLock,
        /// their heavy use can result in significant performance and scalability
        /// penalties. Not to mention, more often then not, algorithms that use
        /// recursive locks can probably benefit from re-factoring.

        template<typename Lock>
        struct RecursiveLock {
        private:
            /// \brief
            /// The actual lock.
            Lock lock;
            /// \brief
            /// \see{ThreadHandle} of the \see{Thread}
            /// that currently holds the lock.
            THEKOGANS_UTIL_THREAD_HANDLE thread;
            /// \brief
            /// Recursion count.
            std::size_t count;
            /// \brief
            /// Synchronization \see{SpinLock}
            SpinLock spinLock;

        public:
            /// \brief
            /// ctor.
            RecursiveLock () :
                thread (THEKOGANS_UTIL_INVALID_THREAD_HANDLE_VALUE),
                count (0) {}

            /// \brief
            /// Try to acquire the lock without blocking.
            /// \return true = acquired, false = failed to acquire
            bool TryAcquire () {
                LockGuard<SpinLock> guard (spinLock);
                if (thread != Thread::GetCurrThreadHandle ()) {
                    if (!lock.TryAcquire ()) {
                        return false;
                    }
                    thread = Thread::GetCurrThreadHandle ();
                }
                ++count;
                return true;
            }

            /// \brief
            /// Acquire the lock.
            void Acquire () {
                LockGuard<SpinLock> guard (spinLock);
                if (thread != Thread::GetCurrThreadHandle ()) {
                    lock.Acquire ();
                    thread = Thread::GetCurrThreadHandle ();
                }
                ++count;
            }

            /// \brief
            /// Release the lock.
            void Release () {
                LockGuard<SpinLock> guard (spinLock);
                if (thread == Thread::GetCurrThreadHandle () && --count == 0) {
                    lock.Relese ();
                    thread = THEKOGANS_UTIL_INVALID_THREAD_HANDLE_VALUE;
                }
            }

            /// \brief
            /// RecursiveLock is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (RecursiveLock)
        };

        /// \brief
        /// Alias for RecursiveLock<SpinLock>.
        using RecursiveSpinLock = RecursiveLock<SpinLock>;
        /// \brief
        /// Alias for RecursiveLock<Mutex>.
        using RecursiveMutex = RecursiveLock<Mutex>;

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RecursiveLock_h)
