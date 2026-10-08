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

#if !defined (__thekogans_util_SlabAllocatorImpl_h)
#define __thekogans_util_SlabAllocatorImpl_h

#include <cstddef>
#include <cassert>
#include <new>
#include <atomic>
#include <type_traits>
#include "thekogans/util/Config.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/CPU.h"

namespace thekogans {
    namespace util {

        /// \struct SlabAllocatorImpl SlabAllocatorImpl.h thekogans/util/SlabAllocatorImpl.h
        ///
        /// \brief
        /// SlabAllocatorImpl
        struct _LIB_THEKOGANS_UTIL_DECL SlabAllocatorImpl {
        private:
            /// \struct SlabAllocatorImpl::Page SlabAllocatorImpl.h thekogans/util/SlabAllocatorImpl.h
            ///
            /// \brief
            /// The page (aka slab) from which we allocate slots. It's aligned
            /// on and occupies an entire cache line to prevent false sharing
            /// and cache thrashing. Pages form a singly linked list rooted in
            /// pageList.
            struct alignas (SYSTEM_CACHE_LINE_SIZE) Page {
                /// \struct SlabAllocatorImpl::Page::Slot SlabAllocatorImpl.h thekogans/util/SlabAllocatorImpl.h
                ///
                /// \brief
                /// Slot overlays our free slot list on top of released user data.
                struct Slot {
                    /// \brief
                    /// Free slots form a singly linked list rooted at freeList.
                    /// Pointer to the next free slot in the list.
                    Slot *next;

                    /// \brief
                    /// Validate that the slot is in fact ours and call its Page::Free.
                    /// \param[in] pageMask Mask to turn the Slot * in to a Page *.
                    inline void Free (std::size_t pageMask) noexcept {
                        Page *page = reinterpret_cast<Page *> (reinterpret_cast<uintptr_t> (this) & pageMask);
                        if (THEKOGANS_UTIL_LIKELY (page->header.magic == MAGIC64)) {
                            page->Free (this);
                        }
                        else {
                            // FIXME: throw something.
                            assert (0);
                        }
                    }
                };

                /// \struct SlabAllocatorImpl::Page::Header SlabAllocatorImpl.h thekogans/util/SlabAllocatorImpl.h
                ///
                /// \brief
                /// We put our metadata in to a header to allow the compiler to align
                /// the fields as it sees fit and to make our padding calculations below
                /// bulletproof. One of the most important performance knobs is preventing
                /// false sharing. By padding out the page header to a cache line size we
                /// prevent sharing it with slots.
                struct Header {
                    /// \brief
                    /// Watermark used by Slot::Free for sanity check.
                    const ui64 magic = MAGIC64;
                    /// \brief
                    /// Backpointer to allocator.
                    SlabAllocatorImpl &allocator;
                    /// \brief
                    /// Master page list structural link.
                    Page *masterNext{nullptr};
                    /// \brief
                    /// Partial page list structural link.
                    Page *partialNext{nullptr};
                    /// \brief
                    /// Number of slots allocated from this page.
                    std::size_t slotCount{0};
                    /// \brief
                    /// Head of the free slot list.
                    Slot *freeList{nullptr};

                    /// \brief
                    /// ctor.
                    /// \param[in] allocator_ Backpointer to allocator.
                    Header (SlabAllocatorImpl &allocator_) noexcept :
                        allocator (allocator_),
                        masterNext (allocator.masterPageList),
                        partialNext (allocator.partialPageList) {}
                } header;

                /// \brief
                /// Calculate the size of the header.
                static constexpr std::size_t headerSize = sizeof (Header);
                // Validate our assumptions.
                static_assert (
                    headerSize <= SYSTEM_CACHE_LINE_SIZE,
                    "Page header header size has exceeded a single SYSTEM_CACHE_LINE_SIZE block.");

                /// \brief
                /// Calculated padding size.
                static constexpr std::size_t paddingSize = SYSTEM_CACHE_LINE_SIZE - headerSize;
                /// \struct SlabAllocatorImpl::Page::EmptyPadding SlabAllocatorImpl.h thekogans/util/SlabAllocatorImpl.h
                ///
                /// \brief
                /// This empty struct is a conditional placeholder for PaddingType in case paddingSize == 0.
                struct EmptyPadding {};
                /// \brief
                /// Declare padding type to be either std::byte[] if paddingSize > 0, or EmptyPadding if paddingSize == 0.
                using PaddingType = std::conditional_t<(paddingSize > 0), std::byte[paddingSize], EmptyPadding>;
                /// \brief
                /// Declare a conditional PaddingType using a standards compliant (c++17 and >) compiler attribute.
                /// [[no_unique_address]] ensures EmptyPadding takes up absolutely zero bytes of space!
                [[no_unique_address]] PaddingType padding = {};

                /// \brief
                /// ctor.
                /// \param[in, out] allocator Backpointer to allocator.
                Page (SlabAllocatorImpl &allocator) noexcept :
                        header (allocator) {
                    allocator.masterPageList = this;
                    allocator.partialPageList = this;
                }

                /// \brief
                /// Allocate a slot.
                /// \return A new slot.
                Slot *Alloc () noexcept;

                /// \brief
                /// Return a previously Alloc(ated) slot back to the free list.
                /// \param[in] slot Slot to free.
                void Free (Slot *slot) noexcept;
            };

            // Validate our assumptions and perform sanity checks.
            static_assert (
                sizeof (Page) == SYSTEM_CACHE_LINE_SIZE,
                "sizeof (Page) must be EXACTLY equal to SYSTEM_CACHE_LINE_SIZE.");

            /// \brief
            /// Our index in to the tlc array.
            const std::size_t instanceId;
            /// \brief
            /// Slot size.
            const std::size_t slotSize;
            /// \brief
            /// Page size.
            const std::size_t pageSize;
            /// \brief
            /// Mask that turns slots in to pages.
            const std::size_t pageMask;
            /// \brief
            /// Number of slots per page.
            const std::size_t slotsPerPage;
            /// \brief
            /// TLC size.
            const std::size_t tlcSize;
            /// \brief
            /// Precomputed tlcSize / 2.
            const std::size_t tlcBatchSize;
            /// \brief
            /// Master immutable page list.
            Page *masterPageList{nullptr};
            /// \brief
            /// Partially allocated page list.
            Page *partialPageList{nullptr};
            /// \brief
            /// Flag to protect from multiple threads calling the page allocator.
            bool pageAllocationInFlight{false};
            /// \brief
            /// Protect access to page lists.
            /// Align the lock to it's own cache line to prevent false sharing.
            alignas (SYSTEM_CACHE_LINE_SIZE) SpinLock lock;

            /// \struct SlabAllocatorImpl::TLC SlabAllocatorImpl.h thekogans/util/SlabAllocatorImpl.h
            ///
            /// \brief
            /// Thread Local Cache (TLC). We keep a small (tlcSize) number of slots
            /// per thread. This optimization allows us to bypass the costly lock
            /// acquisition. In real load testing (see test_SlabAllocator) this
            /// results in ~65% speedup!
            struct TLC {
                /// \brief
                /// Our own local slot cache.
                Page::Slot *slotList{nullptr};
                /// \brief
                /// Number of slots currently in the cache.
                std::size_t slotCount{0};

                /// \brief
                /// Add a slot to the local cache.
                /// \param[in] slot Page::Slot to add to the cache.
                inline void Push (Page::Slot *slot) noexcept {
                    slot->next = slotList;
                    slotList = slot;
                    ++slotCount;
                }

                /// \brief
                /// Remove a slot from the cache.
                /// \return Head of the cache list.
                inline Page::Slot *Pop () noexcept {
                    Page::Slot *slot = slotList;
                    slotList = slotList->next;
                    --slotCount;
                    return slot;
                }
            };

            /// \brief
            /// Return the TLC associated with this allocator.
            inline TLC &GetTLC () const noexcept {
                static constexpr std::size_t MAX_ALLOCATORS = 100;
                thread_local TLC caches[MAX_ALLOCATORS];
                return caches[instanceId];
            }

        public:
            /// \brief
            /// Default slots per page.
            /// A tuning knob meant to limit page allocations for a
            /// heavily allocated type.
            static constexpr std::size_t DEFAULT_SLOTS_PER_PAGE = 512;
            /// \brief
            /// Default Thread Local Cache (TLC) size.
            /// A tuning knob meant to limit lock contention on a heavily
            /// contested type.
            static constexpr std::size_t DEFAULT_TLC_SIZE = 64;

            /// \brief
            /// ctor.
            /// \param[in] slotSize_ Slot size.
            /// \param[in] slotsPerPage_ Number of slots per page.
            /// \param[in] tlcSize_ TLC size.
            SlabAllocatorImpl (
                std::size_t slotSize_,
                std::size_t slotsPerPage_ = DEFAULT_SLOTS_PER_PAGE,
                std::size_t tlcSize_ = DEFAULT_TLC_SIZE);
            /// \brief
            /// dtor. Return pages back to the DefaultPageAllocator.
            ~SlabAllocatorImpl () noexcept;

            /// \brief
            /// Allocate a new slot.
            /// \return A new slot.
            void *Alloc ();

            /// \brief
            /// Try to cache a previously allocated slot. If our cache is full,
            /// return half back to the pages the slots came from.
            /// \param[in] ptr Slot to free.
            void Free (void *ptr) noexcept;

        private:
            /// \brief
            /// Allocate a new page.
            void AllocPage () noexcept;
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SlabAllocatorImpl_h)
