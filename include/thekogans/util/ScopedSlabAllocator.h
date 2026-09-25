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

#if !defined (__thekogans_util_ScopedSlabAllocator_h)
#define __thekogans_util_ScopedSlabAllocator_h

#include <new>
#include <type_traits>
#include "thekogans/util/AlignedAllocator.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/SlabAllocatorDetail.h"

namespace thekogans {
    namespace util {

        /// \struct ScopedSlabAllocator ScopedSlabAllocator.h thekogans/util/ScopedSlabAllocator.h
        ///
        /// \brief
        /// A ScopedSlabAllocator's purpose is to allocate memory for a single type for one or more
        /// local thread groups (workers). It's design very much parallels that of \see{SlabAllocator}.
        /// The one big feature ScopedSlabAllocator brings to the table is cleanup. Where as
        /// \see{SlabAllocator} is desgned to be a system wide \see{Singleton} with very strict
        /// lifetime implications (the hard coded NullInstanceDestroyer), ScopedSlabAllocator is
        /// designed to be used in controlled environrmnts shared by a cooperating group of threads.
        /// When the task on which that group is working on is done, and the context that supports
        /// and manages their work and lifetimes joins with them, it will be safe to destroy the
        /// local ScopedSlabAllocator and reclaim the resources it used back to the OS. Otherwise,
        /// save for managing a singly linked masterPageList, the two implementations are identical.
        /// To learn all about the cool tuning knobs you have at your disposal, go read \see{SlabAllocator}.
        ///
        /// \tparam T The type that we are allocating for.
        /// \tparam SlotsPerPage a tuning knob to control page size.
        /// \tparam TLCThreshold a tuning knob for TLC (Gemini came up with that!)
        /// \tparam CacheLineSize a tuning knob for alignment.
        /// \tparam Id a tuning knob for type fragmentation.
        /// \tparam PageAllocator a tuning knob used for page allocation.
        /// ***************************
        /// WARNING: It is absolutely critical that your supplied page allocator
        /// return a pointer aligned on a pageSize boundary. Our entire architecture
        /// is built on that requirement. See \see{DefaultPageAllocator} for an example.
        /// ***************************
        /// \tparam Lock Any of the standard locks to use for synchronization.
        template <
            typename T,
            std::size_t SlotsPerPage = detail::DEFAULT_SLOTS_PER_PAGE,
            std::size_t TLCThreshold = detail::DEFAULT_TLC_THRESHOLD,
            std::size_t CacheLineSize = detail::DEFAULT_CACHE_LINE_SIZE,
            std::size_t Id = 0,
            typename PageAllocator = detail::DefaultPageAllocator,
            typename Lock = SpinLock>
        struct ScopedSlabAllocator {
            /// \brief
            /// Validate template parameters.
            static_assert (SlotsPerPage > 0, "SlotsPerPage must be > 0.");
            static_assert (IsPowerOf2 (CacheLineSize), "CacheLineSize must be a power of 2.");

        private:
            /// \brief
            /// Compile time function to calculate the slot size.
            /// Wrapped in a function because of the compiler scope evaluation rules.
            /// \return Slot size.
            static constexpr std::size_t calcSlotSize () noexcept {
                return std::max ((sizeof (T) + alignof (T) - 1) & ~(alignof (T) - 1), sizeof (typename Page::Slot));
            }

            // The following consts are allocator invariants. We calculate them
            // once at compile time and use them at run time as 'magic' numbers.

            /// \brief
            /// Slot size.
            static constexpr std::size_t slotSize = calcSlotSize ();
            /// \brief
            /// Page size (header + slots). Aligned to the next power of 2.
            static constexpr std::size_t pageSize = Align (CacheLineSize + slotSize * SlotsPerPage);
            /// \brief
            /// Page mask used to turn raw void * in to Page * (see Free).
            static constexpr std::size_t pageMask = pageSize - 1;
            /// \brief
            /// Maximum slots per page.
            static constexpr std::size_t maxSlots = (pageSize - CacheLineSize) / slotSize;

            /// \struct ScopedSlabAllocator::Page ScopedSlabAllocator.h thekogans/util/ScopedSlabAllocator.h
            ///
            /// \brief
            /// The page (aka slab) from which we allocate slots. It's aligned
            /// on and occupies an entire cache line to prevent false sharing
            /// and cache thrashing. Pages form a singly linked list rooted in
            /// pageList.
            struct alignas (CacheLineSize) Page {
                /// \brief
                /// Next page in the master list.
                Page *masterNext{nullptr};
                /// \brief
                /// Next page in the partial list.
                Page *partialNext{nullptr};
                /// \brief
                /// Number of slots allocated from this page.
                std::size_t slotCount{0};
                /// \struct ScopedSlabAllocator::Page::Slot ScopedSlabAllocator.h thekogans/util/ScopedSlabAllocator.h
                ///
                /// \brief
                /// Slot overlays our free slot list on top of released user data.
                struct Slot {
                    /// \brief
                    /// Free slots form a singly linked list rooted at freeList.
                    /// Pointer to the next free slot in the list.
                    Slot *next;
                } *freeList{nullptr};

                /// \brief
                /// Calculate exactly how many bytes are left in the single cache line.
                /// 4 pointers/counters = 32 bytes on 64-bit systems.
                ////////////////////////////// VERY IMPORTANT //////////////////////////////
                /// If you add new members to Page you must add their sizes to metadataSize.
                ////////////////////////////// VERY IMPORTANT //////////////////////////////
                static constexpr std::size_t metadataSize =
                    sizeof (Page *) +
                    sizeof (Page *) +
                    sizeof (std::size_t) +
                    sizeof (Slot *);
                // Guard the Metadata Header Block
                static_assert (
                    metadataSize <= CacheLineSize,
                    "Page metadata header size has exceeded a single CacheLineSize block.");

                // ************************************************************************
                // The following bit of c++foo was all Gemini.
                /// \brief
                /// Calculated padding size.
                static constexpr std::size_t paddingSize = CacheLineSize - metadataSize;
                /// \struct ScopedSlabAllocator::Page::EmptyTag ScopedSlabAllocator.h thekogans/util/ScopedSlabAllocator.h
                ///
                /// \brief
                /// This empty struct is a conditional placeholder for PaddingType in case paddingSize == 0.
                struct EmptyTag {};
                /// \brief
                /// Declare padding type to be either char[] if paddingSize > 0, or EmptyTag if paddingSize == 0.
                using PaddingType = std::conditional_t<(paddingSize > 0), char[paddingSize], EmptyTag>;
                /// \brief
                /// Declare a conditional PaddingType using a standards compliant (c++17 and >) compiler attribute.
                /// [[no_unique_address]] ensures EmptyTag takes up absolutely zero bytes of space!
                [[no_unique_address]] PaddingType padding;
                // ************************************************************************

                /// \brief
                /// ctor.
                /// if paddingSize is > 0, this ctor will generate code to clear it.
                /// if paddingSize is == 0, this ctor will be a constexpr noop and
                /// removed entirely by the compiler.
                constexpr Page () noexcept {
                    if constexpr (paddingSize > 0) {
                        for (std::size_t i = 0; i < paddingSize; ++i) {
                            reinterpret_cast<char *> (&padding)[i] = 0;
                        }
                    }
                }

                /// \brief
                /// Allocate a slot.
                /// \return A new slot.
                void *Alloc () noexcept {
                    if (freeList != nullptr) {
                        Slot *slot = freeList;
                        freeList = freeList->next;
                        ++slotCount;
                        return reinterpret_cast<void *> (slot);
                    }
                    return reinterpret_cast<char *> (this) + CacheLineSize + slotCount++ * slotSize;
                }

                /// \brief
                /// Return a previously Alloc(ated) slot back to the free list.
                /// \param[in] ptr Slot pointer to free.
                void Free (void *ptr) noexcept {
                    Slot *slot = reinterpret_cast<Slot *> (ptr);
                    slot->next = freeList;
                    freeList = slot;
                    --slotCount;
                }
            };

            // Validate our assumptions and perform sanity checks.
            static_assert (
                sizeof (Page) == CacheLineSize,
                "sizeof (Page) must be EXACTLY equal to CacheLineSize.");
            static_assert (IsPowerOf2 (slotSize), "slotSize must be a power of 2.");
            static_assert (IsPowerOf2 (pageSize), "pageSize must be a power of 2.");

            /// \brief
            /// Master page list.
            Page *masterPageList;
            /// \brief
            /// Partially allocated page list.
            Page *partialPageList;
            /// \brief
            /// Protect access to page lists.
            /// Align the lock to it's own cache line to prevent false sharing with page lists.
            alignas (CacheLineSize) Lock lock;

            /// \struct ScopedSlabAllocator::TLC ScopedSlabAllocator.h thekogans/util/ScopedSlabAllocator.h
            ///
            /// \brief
            /// Thread Local Cache (TLC). We keep a small (TLCThreshold) number of slots
            /// per thread. This optimization allows us to bypass the costly lock
            /// acquisition. In real load testing (see test_ScopedSlabAllocator) this
            /// results in ~65% speedup!
            struct TLC {
                /// \brief
                /// Our own local slot list.
                typename Page::Slot *slotList{nullptr};
                /// \brief
                /// Number of slots currently in the slotList.
                std::size_t slotCount{0};
            };

            /// \brief
            /// Return the TLC.
            /// \return tlc.
            static TLC &GetTLC () noexcept {
                thread_local TLC tlc;
                return tlc;
            }

        public:
            /// \brief
            /// ctor.
            ScopedSlabAllocator () noexcept :
                masterPageList (nullptr),
                partialPageList (nullptr) {}
            /// \brief
            /// dtor.
            ~ScopedSlabAllocator () noexcept {
                Page *page = masterPageList;
                while (page != nullptr) {
                    Page *nextPage = page->masterNext;
                    PageAllocator::Free (page, pageSize);
                    page = nextPage;
                }
            }

            /// \brief
            /// Allocate a new slot.
            /// \return Pointer to the newly allocated slot.
            void *Alloc () noexcept {
                if constexpr (TLCThreshold > 0) {
                    // Try the fastest route. See if we have a free slot in our local cache.
                    TLC &tlc = GetTLC ();
                    if (tlc.slotList != nullptr) {
                        typename Page::Slot *slot = tlc.slotList;
                        tlc.slotList = tlc.slotList->next;
                        --tlc.slotCount;
                        return reinterpret_cast<void *> (slot);
                    }
                }
                // No banana. See if we have a partial page that can supply the slot.
                LockGuard<Lock> guard (lock);
                if (partialPageList != nullptr) {
                    void *ptr = partialPageList->Alloc ();
                    // Page is full. Evict it from the partial list so that no one
                    // asks it for slots again.
                    if (partialPageList->slotCount == maxSlots) {
                        partialPageList = partialPageList->partialNext;
                    }
                    return ptr;
                }
                // Nothing in the cache. No partial pages available that can supply the slot.
                // Time to dip down to the os level and ask it for a new page. This of course,
                // is the worst of all possible worlds, but we can be comforted knowing that
                // it's very rare (1 in SlotsPerPage) and will get amortized across many allocations.
                Page *page = new (PageAllocator::Alloc (pageSize)) Page ();
                page->masterNext = masterPageList;
                masterPageList = page;
                void *ptr = page->Alloc ();
                // Unless someone decided to have one slot/page, wire it in to our partial page
                // list for further allocation requests.
                if (page->slotCount < maxSlots) {
                    page->partialNext = partialPageList;
                    partialPageList = page;
                }
                return ptr;
            }

            /// \brief
            /// Try to cache a previously allocated slot. If our cache is full,
            /// return it back to the page it came from.
            /// \param[in] ptr Slot pointer to free.
            void Free (void *ptr) noexcept {
                if (ptr != nullptr) {
                    if constexpr (TLCThreshold > 0) {
                        // If we have room in our local cache, stash the slot there for fast reallocation.
                        TLC &tlc = GetTLC ();
                        if (tlc.slotCount < TLCThreshold) {
                            typename Page::Slot *slot = reinterpret_cast<typename Page::Slot *> (ptr);
                            slot->next = tlc.slotList;
                            tlc.slotList = slot;
                            ++tlc.slotCount;
                            return;
                        }
                    }
                    LockGuard<Lock> guard (lock);
                    // The magic! This is why we can guarntee wall to wall O(1) performance.
                    // By aligning the page size to the next power of 2 and then using that
                    // size as page memory placement alignment, we can use a simple pointer
                    // masking trick to find the page address given any address it allocated.
                    // After all the syntactic sugar is stripped away this line boils down
                    // to a single 'and' instruction in hardware.
                    Page *page = reinterpret_cast<Page *> (reinterpret_cast<uintptr_t> (ptr) & ~pageMask);
                    page->Free (ptr);
                    // NOTE: Empty pages are not given back to the OS.
                    // they go right back on the partial list to be
                    // recycled. To do otherwise would require me to
                    // be able to unlink a page from the middle of the
                    // list. Singly linked lists have O(n) complexity
                    // to do that. To transition from a simple singly
                    // linked list to a double one would be way too
                    // much to pay, performance wise, for so little
                    // gain. As it's name implies, this is a scoped
                    // allocator. Treat it like one and it will give
                    // top notch performance with cleanup capabilities.
                    //
                    // The page transitioned from full to partial.
                    // Wire it back in to our partial list.
                    if (page->slotCount + 1 == maxSlots) {
                        page->partialNext = partialPageList;
                        partialPageList = page;
                    }
                }
            }

            /// \brief
            /// RefCounted is neither copy or move constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (ScopedSlabAllocator)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_ScopedSlabAllocator_h)
