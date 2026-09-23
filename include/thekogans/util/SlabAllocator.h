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
#include "thekogans/util/AlignedAllocator.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"

namespace thekogans {
    namespace util {

        /// \struct SlabAllocator SlabAllocator.h thekogans/util/SlabAllocator.h
        ///
        /// \brief
        /// SlabAllocator is my first collaboration with an AI (Google's Gemini). I posed a question;
        /// Is there a practical way to implement a slab allocator with 'true' O(1) performance
        /// guarantees? I didn't want average or amortized O(1). I wanted true O(1). After many
        /// iterations and blind alley ventures, this is what we both came up with. The architecture
        /// is all mine. The nuts and bolts of alignment, constexpr and TLC cache are all AI's. This
        /// implementation evolved over just two days of back and forth. Looking back on my earlier
        /// efforts I can honestly say that to achieve this level of sophistication in the past would
        /// take me significantly longer. Digging through old chats. Looking for exact documentation
        /// I needed would have consumed an enormous amount of time and effort. On top of all this.
        /// Once we were done designing, and I was done implementing it took the AI mere seconds to
        /// generate a burn the earth down validation suite. Again, something that would take me a
        /// day or two to do by myself. All in all, I am absolutely sold on the idea of pair programming
        /// with AI. To be sure it's not all roses. There were times it was missing context and tried
        /// to lead me down blind alleys. But that's why it's a collaboration. It's not an all knowing,
        /// all seeing oracle that will flawlessly do your work for you. It's a fantastically powerful
        /// tool that in the right hands creates an unbeatable team.
        ///
        /// \tparam T The type that we are allocating for.
        /// \tparam SlotsPerPage a tuning knob to control page size.
        /// \tparam Threshold a tuning knob for TLC (Gemini came up with that!)
        /// \tparam Lock Any of the standard locks to use for synchronization.
        template <
            typename T,
            std::size_t SlotsPerPage = 256,
            std::size_t Threshold = 32,
            typename Lock = SpinLock>
        struct SlabAllocator : public
            Singleton<
                SlabAllocator<T, SlotsPerPage, Threshold, Lock>,
                Lock,
                DefaultInstanceCreator<SlabAllocator<T, SlotsPerPage, Threshold, Lock>>,
                NullInstanceDestroyer<SlabAllocator<T, SlotsPerPage, Threshold, Lock>>> {
        private:
            /// \brief
            /// Hardware cache line size.
        #if defined (__cpp_lib_hardware_interference_size)
            /// \brief
            /// With C++17 and higher the standard library provides this value.
            static constexpr std::size_t cacheLineSize = std::hardware_destructive_interference_size;
        #else // defined (__cpp_lib_hardware_interference_size)
            /// \brief
            /// Since we need to use it to calculate the page padding, we need
            /// this value to be available at compile time.
            static constexpr std::size_t cacheLineSize = 64;
        #endif // defined (__cpp_lib_hardware_interference_size)

            static constexpr std::size_t calcHeaderSize () noexcept {
                return (sizeof (Page) + cacheLineSize - 1) & ~(cacheLineSize - 1);
            }
            static constexpr std::size_t calcSlotSize() noexcept {
                return std::max ((sizeof (T) + alignof (T) - 1) & ~(alignof (T) - 1), sizeof (typename Page::Slot));
            }
            static constexpr std::size_t calcPageSize () noexcept {
                return Align (calcHeaderSize () + calcSlotSize () * SlotsPerPage);
            }

            // The following consts are allocator invariants. We calculate
            // them once (in the ctor) and use the values at run time.

            /// \brief
            /// Page metadata size.
            static constexpr std::size_t headerSize = calcHeaderSize ();
            /// \brief
            /// Slot size.
            static constexpr std::size_t slotSize = calcSlotSize ();
            /// \brief
            /// Aligned page size.
            static constexpr std::size_t pageSize = calcPageSize ();
            /// \brief
            /// Page mask used to turn raw void * in to Page * (see Free).
            static constexpr std::size_t pageMask = pageSize - 1;
            /// \brief
            /// Maximum slots per page.
            static constexpr std::size_t maxSlots = (pageSize - headerSize) / slotSize;

            /// \struct SlabAllocator::Page SlabAllocator.h thekogans/util/SlabAllocator.h
            ///
            /// \brief
            /// The page (aka slab) from which we allocate slots. It's aligned on and occupies
            /// an entire cache line to prevent false sharing and cache thrashing. Pages form
            /// a singly linked list rooted in pageList.
            struct alignas (cacheLineSize) Page {
                /// \brief
                /// Next page in the list.
                Page *next;
                /// \brief
                /// Number of slots allocated from this page.
                std::size_t slotCount;
                /// \struct SlabAllocator::Page::Slot SlabAllocator.h thekogans/util/SlabAllocator.h
                ///
                /// \brief
                /// Slot overlays our free slot list on top of released user data.
                struct Slot {
                    /// \brief
                    /// Free slots form a singly linked list rooted at freeList.
                    /// Pointer to the next free slot in the list.
                    Slot *next;
                } *freeList;
                /// \brief
                /// Pad the page header to be the size of a single cache line.
                /// This prevents any user data from sharing our page metadata
                /// cache causing false sharing and cache line eviction.
                alignas (cacheLineSize) char padding = 0;

                /// \brief
                /// ctor.
                /// \param[in] headerSize_ Since all pages are identical, we do the calculation
                /// once (in the SlabAllocator ctor) and pass it in to each page to be used to
                /// calculate slot addresses in Alloc.
                /// \param[in] slotSize_ Just like headerSize_ above, slot size is an invariant
                /// and is calculated once in the SlabAllocator ctor.
                constexpr Page () noexcept :
                    next (nullptr),
                    slotCount (0),
                    freeList (nullptr) {}

                /// \brief
                /// Allocate a slot.
                /// \return A new slot.
                void *Alloc () noexcept {
                    if (freeList != nullptr) {
                        Slot *slot = freeList;
                        freeList = freeList->next;
                        ++slotCount;
                        return slot;
                    }
                    return reinterpret_cast<char *> (this) + headerSize + slotCount++ * slotSize;
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

            /// \brief
            /// Partially allocated page list.
            Page *pageList;
            /// \brief
            /// Protect access to pageList.
            /// Align the lock to it's own cache line to prevent false sharing with pageList.
            alignas (cacheLineSize) Lock lock;

            /// \struct SlabAllocator::TLC SlabAllocator.h thekogans/util/SlabAllocator.h
            ///
            /// \brief
            /// Thread Local Cache (TLC). We keep a small (Threshold) number of slots
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
            /// Allocate a new slot.
            /// \return Pointer to the newly allocated slot.
            void *Alloc () noexcept {
                TLC &tlc = GetTLC ();
                // Try the fastest route. See if we have a free slot in our local cache.
                if (tlc.slotList != nullptr) {
                    typename Page::Slot *slot = tlc.slotList;
                    tlc.slotList = tlc.slotList->next;
                    --tlc.slotCount;
                    return reinterpret_cast<void *> (slot);
                }
                // No banana. See if we have a partial page that can supply the slot.
                LockGuard<Lock> guard (lock);
                if (pageList != nullptr) {
                    void *ptr = pageList->Alloc ();
                    // Page is full. Evict it from the list so that no one asks it
                    // for slots again until a slot is freed from it (see Free).
                    if (pageList->slotCount == maxSlots) {
                        pageList = pageList->next;
                    }
                    return ptr;
                }
                // Nothing in the cache. No partial pages available that can supply the slot.
                // Time to dip down to the os level and ask it for a new page. This of course,
                // is the worst of all possible worlds, but we can be comforted knowing that
                // it's very rare and will get amortized across many allocations.
                Page *page = new (
                    ::operator new [] (pageSize, std::align_val_t{pageSize})) Page ();
                void *ptr = page->Alloc ();
                // Unless someone decided to have one slot/page, wire it in to our page list.
                if (page->slotCount < maxSlots) {
                    page->next = pageList;
                    pageList = page;
                }
                return ptr;
            }

            /// \brief
            /// Return a previously Alloc(ated) slot back to the page it came from.
            /// \param[in] ptr Slot pointer to free.
            void Free (void *ptr) noexcept {
                if (ptr != nullptr) {
                    // If we have room in our local cache, stash the slot for fast reallocation.
                    TLC &tlc = GetTLC ();
                    if (tlc.slotCount < Threshold) {
                        typename Page::Slot *slot = reinterpret_cast<typename Page::Slot *> (ptr);
                        slot->next = tlc.slotList;
                        tlc.slotList = slot;
                        ++tlc.slotCount;
                    }
                    else {
                        LockGuard<Lock> guard (lock);
                        Page *page = reinterpret_cast<Page *> (reinterpret_cast<uintptr_t> (ptr) & ~pageMask);
                        page->Free (ptr);
                        // The page transitioned from full to partial. Wire it in to our list.
                        if (page->slotCount + 1 == maxSlots) {
                            page->next = pageList;
                            pageList = page;
                        }
                    }
                }
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SlabAllocator_h)
