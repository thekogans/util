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

#include <new>
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/CPU.h"
#include "thekogans/util/SlabAllocatorDetail.h"
#include "thekogans/util/SlabAllocatorImpl.h"

namespace thekogans {
    namespace util {

        SlabAllocatorImpl::Page::Slot *SlabAllocatorImpl::Page::Alloc () noexcept {
            Slot *slot = nullptr;
            if (header.freeList != nullptr) {
                slot = header.freeList;
                header.freeList = slot->next;
            }
            else {
                slot = reinterpret_cast<Slot *> (
                    reinterpret_cast<std::byte *> (this + 1) + header.slotCount * header.allocator.slotSize);
            }
            if (THEKOGANS_UTIL_UNLIKELY (++header.slotCount == header.allocator.slotsPerPage)) {
                // Page is full. Evict it from the partialPageList so that no one
                // asks it for slots again. This works because Alloc is only called
                // on header.allocator.partialPageList in SlabAllocatorImpl::Alloc.
                header.allocator.partialPageList = header.partialNext;
                header.partialNext = nullptr;
            }
            return slot;
        }

        void SlabAllocatorImpl::Page::Free (Slot *slot) noexcept {
            slot->next = header.freeList;
            header.freeList = slot;
            --header.slotCount;
            if (THEKOGANS_UTIL_UNLIKELY (header.slotCount == 0)) {
                // If the page is empty reset the free list pointer
                // so that Alloc grabs contiguous, cache friendly
                // slots instead of pointer hoping.
                header.freeList = nullptr;
            }
            // The page transitioned from full to partial.
            // Wire it back in to partialPageList list.
            if (THEKOGANS_UTIL_UNLIKELY (header.slotCount + 1 == header.allocator.slotsPerPage)) {
                header.partialNext = header.allocator.partialPageList;
                header.allocator.partialPageList = this;
            }
        }

        namespace {
            std::atomic<std::size_t> instanceCounter{0};
        }

        SlabAllocatorImpl::SlabAllocatorImpl (
                std::size_t slotSize_,
                std::size_t slotsPerPage_,
                std::size_t tlcSize_) :
                instanceId (instanceCounter.fetch_add (1, std::memory_order_relaxed)),
                slotSize (slotSize_),
                pageSize (Align (sizeof (Page) + slotSize * slotsPerPage_)),
                pageMask (~(pageSize - 1)),
                slotsPerPage ((pageSize - sizeof (Page)) / slotSize),
                tlcSize (tlcSize_),
                tlcBatchSize (tlcSize / 2) {
            // Validate input.
            if (slotSize < sizeof (Page::Slot *) || slotsPerPage == 0 ||
                    tlcSize == 0 || tlcSize > slotsPerPage) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        SlabAllocatorImpl::~SlabAllocatorImpl () noexcept {
            Page *page = masterPageList;
            while (page != nullptr) {
                Page *next = page->header.masterNext;
                detail::DefaultPageAllocator::Free (page, pageSize);
                page = next;
            }
        }

        void *SlabAllocatorImpl::Alloc () {
            TLC &tlc = GetTLC ();
            // See if we have a free slot in our local cache.
            if (THEKOGANS_UTIL_UNLIKELY (tlc.slotCount == 0)) {
                // No banana. Let's seed it.
                LockGuard<SpinLock> guard (lock);
                while (tlc.slotCount < tlcBatchSize) {
                    // As long as there are partial pages to harvest and
                    // we haven't drank are fill...
                    while (tlc.slotCount < tlcBatchSize && partialPageList != nullptr) {
                        tlc.Push (partialPageList->Alloc ());
                    }
                    // ...the above loop broke because, a) we're full or
                    // b) no more partial pages to harvest. If it's the
                    // later, grab a fresh page from the OS.
                    if (THEKOGANS_UTIL_UNLIKELY (tlc.slotCount < tlcBatchSize)) {
                        AllocPage ();
                    }
                }
            }
            return tlc.Pop ();
        }

        void SlabAllocatorImpl::Free (void *ptr) noexcept {
            if (THEKOGANS_UTIL_LIKELY (ptr != nullptr)) {
                Page::Slot *slot = reinterpret_cast<Page::Slot *> (ptr);
                TLC &tlc = GetTLC ();
                tlc.Push (slot);
                // If we have no more room in our local cache batch
                // release a batch of slots back to their pages.
                if (THEKOGANS_UTIL_UNLIKELY (tlc.slotCount == tlcSize)) {
                    LockGuard<SpinLock> guard (lock);
                    for (std::size_t i = 0; i < tlcBatchSize; ++i) {
                        tlc.Pop ()->Free (pageMask);
                    }
                }
            }
        }

        void SlabAllocatorImpl::AllocPage () noexcept {
            // Wait until a page is guaranteed to be available or...
            while (THEKOGANS_UTIL_LIKELY (partialPageList == nullptr && pageAllocationInFlight)) {
                lock.Release ();
                CPU::YieldSlice ();
                lock.Acquire ();
            }
            // ...we need to allocate it.
            if (THEKOGANS_UTIL_LIKELY (partialPageList == nullptr)) {
                // Set the in flight flag before releasing the lock so that no other thread
                // tries to allocate a page too.
                pageAllocationInFlight = true;
                // Release the lock before dropping down to the OS.
                // This wont help waiting allocators but if there are
                // waiting freeers it will alow them to let go of their
                // slots while we're waiting on the OS.
                lock.Release ();
                void *ptr = detail::DefaultPageAllocator::Alloc (pageSize);
                // We're back from the OS land. Reaquire the lock so that we
                // can wire the freshly minted page in to the lists.
                lock.Acquire ();
                // NOTE: Between the Release and Acquire above other threads
                // could have released slots and rewired partial pages back
                // in to the pool. The reason we don't check and potentially
                // give back this page is 1. It takes time to check and 2.
                // even if a page or two are back with one or two empty slots
                // having a completely empty page is better for cache harvesting.
                new (ptr) Page (*this);
                // Now that a fresh page is available the upstream Alloc will
                // be able to satisfy harvesting or allocating. Since we hold
                // the lock there's no chance that this page will be stolen
                // from under us by another thread. We can now safely clear
                // the in flight flag so that the spinning waiters drop out
                // and either have a fresh page to harvest/allocate from or
                // permission to allocate.
                pageAllocationInFlight = false;
            }
        }

    } // namespace util
} // namespace thekogans
