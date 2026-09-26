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

#if !defined (__thekogans_util_SlabAllocator_h)
#define __thekogans_util_SlabAllocator_h

#include <new>
#include <type_traits>
#include "thekogans/util/AlignedAllocator.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/SlabAllocatorDetail.h"

namespace thekogans {
    namespace util {

        /// \struct SlabAllocator SlabAllocator.h thekogans/util/SlabAllocator.h
        ///
        /// \brief
        /// A SlabAllocator's purpose is to allocate memory for a single type. Knowing the size
        /// of the type it's allocating for a priori makes all the difference in the world. We
        /// can use that information to design a highly efficient, specifically tuned allocation
        /// engine. This one is designed with all the bells and whistles and to run in 'true' O(1)
        /// time complexity save for the caveat that PageAllocator will do what it will do. As you
        /// can see it's template takes up to 8! parameters. All but the first are defaulted with
        /// sensible values so that you can just leave them alone 90% of the time. But when those
        /// critical corner cases and specialty conditions demand it, it can be tuned to fit any
        /// environment and need. Great care has been taken to make sure alignment requirements
        /// are not only met but are enforced to make this code as performant as possible on modern
        /// cache line driven architectures. Key variables have been isolated in to their own cache
        /// lines to further protect against false sharing. By utilizing it's tuning knobs one can
        /// build highly specific, lock free (NullLock) allocators that execute their Alloc and Free
        /// in just a small handful of machine instructions. The use of SlabAllocator for your
        /// types also greatly aids in avoiding global heap fragmentation as types are allocated
        /// from contiguous pages.
        ///
        /// SlabAllocator is my first collaboration with an AI (Google's Gemini). I posed a question;
        /// Is there a practical way to implement a slab allocator with 'true' O(1) performance
        /// guarantees? I didn't want average or amortized O(1). I wanted true O(1). After many
        /// iterations and blind alley ventures, this is what we both came up with. The architecture
        /// is all mine. The nuts and bolts of alignment, constexpr and TLC are all AI's. This
        /// implementation evolved over just two days of back and forth. Looking back on my earlier
        /// efforts I can honestly say that to achieve this level of sophistication in the past would
        /// take me significantly longer. Digging through old chats. Looking for exact documentation
        /// I needed would have consumed an enormous amount of time and effort. On top of all this,
        /// once we were done designing, and I was done implementing it took the AI mere seconds to
        /// generate a burn the earth down validation suite. Again, something that would take me a
        /// day or two to do by myself. All in all, I am absolutely sold on the idea of pair programming
        /// with AI. To be sure it's not all roses. There were times it was missing context and tried
        /// to lead me down blind alleys. But that's why it's a collaboration. It's not an all knowing,
        /// all seeing oracle that will flawlessly do your work for you. It's a fantastically powerful
        /// tool that in the right hands creates an unbeatable team.
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
        /// \tparam InstanceCreator A \see{Singleton} parameter to control the allocator instance creation.
        template <
            typename T,
            std::size_t SlotsPerPage = detail::DEFAULT_SLOTS_PER_PAGE,
            std::size_t TLCThreshold = detail::DEFAULT_TLC_THRESHOLD,
            std::size_t CacheLineSize = detail::DEFAULT_CACHE_LINE_SIZE,
            std::size_t Id = 0,
            typename PageAllocator = detail::DefaultPageAllocator,
            typename Lock = SpinLock,
            template <typename> typename InstanceCreator = DefaultInstanceCreator>
        struct SlabAllocator : public
            Singleton<
                SlabAllocator<
                    T,
                    SlotsPerPage,
                    TLCThreshold,
                    CacheLineSize,
                    Id,
                    PageAllocator,
                    Lock,
                    InstanceCreator>,
                Lock,
                InstanceCreator<
                    SlabAllocator<
                        T,
                        SlotsPerPage,
                        TLCThreshold,
                        CacheLineSize,
                        Id,
                        PageAllocator,
                        Lock,
                        InstanceCreator>>,
                NullInstanceDestroyer<
                    SlabAllocator<
                        T,
                        SlotsPerPage,
                        TLCThreshold,
                        CacheLineSize,
                        Id,
                        PageAllocator,
                        Lock,
                        InstanceCreator>>> {
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

            /// \struct SlabAllocator::Page SlabAllocator.h thekogans/util/SlabAllocator.h
            ///
            /// \brief
            /// The page (aka slab) from which we allocate slots. It's aligned
            /// on and occupies an entire cache line to prevent false sharing
            /// and cache thrashing. Pages form a singly linked list rooted in
            /// pageList.
            struct alignas (CacheLineSize) Page {
                /// \brief
                /// Next page in the list.
                Page *next{nullptr};
                /// \brief
                /// Number of slots allocated from this page.
                std::size_t slotCount{0};
                /// \struct SlabAllocator::Page::Slot SlabAllocator.h thekogans/util/SlabAllocator.h
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
                /// 3 pointers/counters = 24 bytes on 64-bit systems.
                ////////////////////////////// VERY IMPORTANT //////////////////////////////
                /// If you add new members to Page you must add their sizes to metadataSize.
                ////////////////////////////// VERY IMPORTANT //////////////////////////////
                static constexpr std::size_t metadataSize =
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
                /// \struct SlabAllocator::Page::EmptyTag SlabAllocator.h thekogans/util/SlabAllocator.h
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
                inline void *Alloc () noexcept {
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
                inline void Free (void *ptr) noexcept {
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
            static_assert (IsPowerOf2 (pageSize), "pageSize must be a power of 2.");

            /// \brief
            /// Partially allocated page list.
            Page *pageList;
            /// \brief
            /// Protect access to pageList.
            /// Align the lock to it's own cache line to prevent false sharing with pageList.
            alignas (CacheLineSize) Lock lock;

            /// \struct SlabAllocator::TLC SlabAllocator.h thekogans/util/SlabAllocator.h
            ///
            /// \brief
            /// Thread Local Cache (TLC). We keep a small (TLCThreshold) number of slots
            /// per thread. This optimization allows us to bypass the costly lock
            /// acquisition. In real load testing (see test_SlabAllocator) this
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
            SlabAllocator () :
                pageList (nullptr) {}
            // No dtor. We're a singleton meant to last the lifetime of the application.
            // Let the os cleanup after us. Even if we wanted to (and we don't) we can't
            // get to the full pages that have been evicted from the list and are floating
            // in either.

            /// \brief
            /// Allocate a new slot with streamlined batch refilling.
            /// \return Pointer to the newly allocated slot.
            void *Alloc () noexcept {
                void *ptr = nullptr;
                if constexpr (TLCThreshold > 0) {
                    TLC &tlc = GetTLC ();
                    // See if we have a free slot in our local cache.
                    if (tlc.slotList == nullptr) {
                        // No banana. Let's seed the TLC.
                        static constexpr std::size_t batchTarget = TLCThreshold / 2;
                        // Persistent outer loop forces lock retention until TLC target is met.
                        LockGuard<Lock> guard (lock);
                        while (tlc.slotCount < batchTarget) {
                            // Inner loop aggressively drains whatever pages are currently available.
                            while (tlc.slotCount < batchTarget && pageList != nullptr) {
                                typename Page::Slot *harvested;
                                // Path A: Rapid extraction from recycled slots.
                                if (pageList->freeList != nullptr) {
                                    harvested = pageList->freeList;
                                    pageList->freeList = pageList->freeList->next;
                                }
                                // Path B: Linear extraction from contiguous tail space.
                                else {
                                    harvested = reinterpret_cast<typename Page::Slot *> (
                                        reinterpret_cast<char *> (pageList) + CacheLineSize + pageList->slotCount * slotSize);
                                }
                                // Unified state increment & precise invariant rollover.
                                if (++pageList->slotCount == maxSlots) {
                                    // Page is full. Evict it from the list so that no one asks it
                                    // for slots again. Yes the page is now floating out there in
                                    // the either completely inaccessible until someone decides to
                                    // free one of it's slots.
                                    pageList = pageList->next;
                                }
                                harvested->next = tlc.slotList;
                                tlc.slotList = harvested;
                                ++tlc.slotCount;
                            }
                            // If inner loop broke but target isn't met, the pool is dry.
                            // Seed a fresh page from the OS and let the outer loop repeat the harvest.
                            if (tlc.slotCount < batchTarget) {
                                AllocPage ();
                            }
                        }
                    }
                    ptr = tlc.slotList;
                    tlc.slotList = tlc.slotList->next;
                    --tlc.slotCount;

                }
                else {
                    // TLC compiled out: Single clean allocation tracking.
                    LockGuard<Lock> guard (lock);
                    if (pageList == nullptr) {
                        AllocPage ();
                    }
                    ptr = pageList->Alloc ();
                    if (pageList->slotCount == maxSlots) {
                        pageList = pageList->next;
                    }
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
                        typename Page::Slot *slot = reinterpret_cast<typename Page::Slot *> (ptr);
                        TLC &tlc = GetTLC ();
                        slot->next = tlc.slotList;
                        tlc.slotList = slot;
                        ++tlc.slotCount;
                        // If we have no more room in our local cache batch release a bunch
                        // of slots back to their pages so that we have room for incoming.
                        if (tlc.slotCount > TLCThreshold) {
                            LockGuard<Lock> guard (lock);
                            static constexpr std::size_t flushCount = TLCThreshold / 2;
                            for (std::size_t i = 0; i < flushCount; ++i) {
                                typename Page::Slot *slot = tlc.slotList;
                                tlc.slotList = tlc.slotList->next;
                                --tlc.slotCount;
                                FreeSlot (slot);
                            }
                        }
                    }
                    else {
                        LockGuard<Lock> guard (lock);
                        FreeSlot (ptr);
                    }
                }
            }

        private:
            inline void AllocPage () noexcept {
                // Release the lock before dropping down to the OS.
                // This wont help waiting allocators but if there are
                // waiting freeers it will alow them to let go of their
                // slots while we're waiting on the OS.
                lock.Release ();
                Page *page = new (PageAllocator::Alloc (pageSize)) Page ();
                lock.Acquire ();
                page->next = pageList;
                pageList = page;
            }

            inline void FreeSlot (void *slot) noexcept {
                // The magic! This is why we can guarntee wall to wall O(1) performance.
                // By aligning the page size to the next power of 2 and then using that
                // size as page memory placement alignment, we can use a simple pointer
                // masking trick to find the page address given any address it allocated.
                // After all the syntactic sugar is stripped away this line boils down
                // to a single 'and' instruction in hardware.
                Page *page = reinterpret_cast<Page *> (reinterpret_cast<uintptr_t> (slot) & ~pageMask);
                page->Free (slot);
                // The page transitioned from full to partial.
                // Wire it back in to our list from the either.
                if (page->slotCount + 1 == maxSlots) {
                    page->next = pageList;
                    pageList = page;
                }
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SlabAllocator_h)
