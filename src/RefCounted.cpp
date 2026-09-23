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

#include <functional>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/operations_lockfree.hpp>
#include <boost/memory_order.hpp>
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/SlabAllocator.h"

namespace thekogans {
    namespace util {

    #if !defined (THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_SLAB_ALLOCATOR_SLOTS_PER_PAGE)
        #define THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_SLAB_ALLOCATOR_SLOTS_PER_PAGE 8192
    #endif // !defined (THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_SLAB_ALLOCATOR_SLOTS_PER_PAGE)

    #if !defined (THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_SLAB_ALLOCATOR_THRESHOLD)
        #define THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_SLAB_ALLOCATOR_THRESHOLD 32
    #endif // !defined (THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_SLAB_ALLOCATOR_THRESHOLD)

        void *RefCounted::References::operator new (std::size_t) {
            return SlabAllocator<
                References,
                THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_SLAB_ALLOCATOR_SLOTS_PER_PAGE,
                THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_SLAB_ALLOCATOR_THRESHOLD>::Instance ()->Alloc ();
        }

        void RefCounted::References::operator delete (void *ptr) {
            SlabAllocator<
                References,
                THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_SLAB_ALLOCATOR_SLOTS_PER_PAGE,
                THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_SLAB_ALLOCATOR_THRESHOLD>::Instance ()->Free (ptr);
        }

        namespace {
            using operations = boost::atomics::detail::operations<4u, false>;
        }

        ui32 RefCounted::References::AddWeakRef () {
            return operations::fetch_add (weak, 1, boost::memory_order_relaxed) + 1;
        }

        ui32 RefCounted::References::ReleaseWeakRef () {
            // 1. Use memory_order_release for the decrement to be fast on non-zero drops.
            ui32 oldWeak = operations::fetch_sub (weak, 1, boost::memory_order_release);
            if (oldWeak == 1) {
                // 2. Only issue the acquire barrier if we are the thread destroying 'this'.
                boost::atomics::detail::thread_fence (boost::memory_order_acquire);
                delete this; // Invokes your custom operator delete
                return 0;
            }
            return oldWeak - 1;
        }

        ui32 RefCounted::References::GetWeakCount () const {
            return operations::load (weak, boost::memory_order_relaxed);
        }

        ui32 RefCounted::References::AddSharedRef () {
            return operations::fetch_add (shared, 1, boost::memory_order_relaxed) + 1;
        }

        ui32 RefCounted::References::ReleaseSharedRef (RefCounted *object) {
            // 1. Use memory_order_release for the decrement.
            // fetch_sub returns the OLD value, so we subtract 1 to get the new count.
            ui32 oldShared = operations::fetch_sub (shared, 1, boost::memory_order_release);
            if (oldShared == 1) {
                // 2. Only the thread that drops it to 0 issues an acquire fence.
                // This synchronizes with all previous releasing threads.
                boost::atomics::detail::thread_fence (boost::memory_order_acquire);
                object->Harakiri ();
                return 0;
            }
            return oldShared - 1;
        }

        ui32 RefCounted::References::GetSharedCount () const {
            return operations::load (shared, boost::memory_order_relaxed);
        }

        bool RefCounted::References::LockObject () {
            // This is a classical lock-free algorithm for shared access.
            ui32 count = operations::load (shared, boost::memory_order_relaxed);
            while (count != 0) {
                // If compare_exchange_weak failed, it's because between the load
                // above and the exchange below, we were interupted by another thread
                // that modified the value of shared. Reload and try again.
                if (operations::compare_exchange_weak (
                        shared,
                        count,
                        count + 1,
                        boost::memory_order_acq_rel,
                        boost::memory_order_relaxed)) {
                    return true;
                }
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
