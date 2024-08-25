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
#include "thekogans/util/Constants.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/RefCounted.h"

namespace thekogans {
    namespace util {

    #if !defined (THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_HEAP_ITEMS_IN_PAGE)
        #define THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_HEAP_ITEMS_IN_PAGE 8192
    #endif // !defined (THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_HEAP_ITEMS_IN_PAGE)

        // Heap implemented from first principles because RefCounted::References
        // is that far down in the firmament. Practically every other sub-system
        // uses it. So I wrote this 'tuned' version to provide lightning fast
        // RefCounted::References allocations making RefCounted a very attractive
        // solution in many different contexts. Paired with RefCountedRegistry
        // it's an unbeatable combination for async work that interfaces with the
        // OS API.
        // NOTE: Unlike other, public facing code, this heap implementation is
        // tuned for one purpose, performance. For this reason the safe and defensive
        // coding standards I use everywhere else do not apply here. I did this
        // because this is not user facing code. There are no external interfaces.
        // And in the interest of performance I removed all redundant checking and
        // parameterezation. If there's a crash in this code it is most likely
        // because of heap corruption which happened somewhere else. This code is
        // very input sensative and as long as you give it back (Free) what it
        // gave you (Alloc) should never cause any problems.

        struct RefCounted::References::Heap : public Singleton<Heap> {
        private:
            struct Page {
                // Page is designed to properly align the shared and
                // weak counters in RefCounted::References on both 32
                // and 64 bit architectures. It's even future proof
                // for 128 bit architectures if they ever become common.
                Page *next;
                union Item {
                    Item *next;
                    ui8 block[sizeof (RefCounted::References)];
                } *freeItem, *lastItem, items[1];

                Page (std::size_t itemsInPage) :
                        next (nullptr),
                        freeItem (items),
                        lastItem (items + itemsInPage - 1) {
                    // Chain the items together to create the free list.
                    for (Item *item = items; item != lastItem; ++item) {
                        item->next = item + 1;
                    }
                    lastItem->next = nullptr;
                }

                // Size of raw block of memory to allocate for the page.
                static std::size_t Size (std::size_t itemsInPage) {
                    // -1 is because of the items[1] above.
                    return sizeof (Page) + sizeof (Item) * (itemsInPage - 1);
                }

                inline bool IsFull () const {
                    return freeItem == nullptr;
                }
                // Check to see if the given pointer belongs to this page.
                inline bool IsItem (const void *ptr) const {
                    return ptr >= items && ptr <= lastItem;
                }

                inline void *Alloc () {
                    // Note the lack of any and all safety features.
                    // This code is used only by Heap::Alloc below
                    // and it makes sure that this method is never
                    // called on a full page.
                    Item *item = freeItem;
                    freeItem = freeItem->next;
                    return item->block;
                }

                inline void Free (void *ptr) {
                    Item *item = (Item *)ptr;
                    item->next = freeItem;
                    freeItem = item;
                }
            };

            struct PageList {
                Page *head;

                PageList () :
                    head (nullptr) {}

                inline bool empty () const {
                    return head == nullptr;
                }
                inline Page *front () const {
                    return head;
                }

                inline void push_front (Page *page) {
                    page->next = head;
                    head = page;
                }

                inline void erase (
                        Page *prev,
                        Page *page) {
                    if (prev != nullptr) {
                        prev->next = page->next;
                    }
                    else {
                        head = page->next;
                    }
                }

                inline Page *find (
                        const void *ptr,
                        Page *&prev) const {
                    prev = nullptr;
                    for (Page *page = head; page != nullptr; prev = page, page = page->next) {
                        if (page->IsItem (ptr)) {
                            return page;
                        }
                    }
                    return nullptr;
                }
            };

            std::size_t itemsInPage;
            PageList fullPages;
            PageList partialPages;
            // RefCounted::References uses boost primitives to atomicaly
            // increment/decrement the shared and weak counters. On some
            // platforms these primitives use machine instructions to
            // achieve good performance. Sometimes those instructins
            // have various alignment requirements. In order to guarntee
            // that we satisfy these requirements we allocate pages with
            // AlignedAllocator. The alignement used is UI32_SIZE which
            // happens to be the type of the above mentioned counters.
            struct AlignedAllocator {
                void *Alloc (std::size_t size) {
                    std::size_t rawSize = UI32_SIZE + size + sizeof (ui8 *);
                    ui8 *rawPtr = new ui8[rawSize];
                    ui8 *ptr = rawPtr;
                    std::size_t amountMisaligned = (std::size_t)ptr & (UI32_SIZE - 1);
                    if (amountMisaligned > 0) {
                        ptr += UI32_SIZE - amountMisaligned;
                    }
                    *(ui8 **)((std::size_t)ptr + size) = rawPtr;
                    return ptr;
                }

                inline void Free (
                        void *ptr,
                        std::size_t size) {
                    delete [] *(ui8 **)((std::size_t)ptr + size);
                }
            } allocator;
            SpinLock spinLock;

        public:
            Heap (std::size_t itemsInPage_ =
                    THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_HEAP_ITEMS_IN_PAGE) :
                itemsInPage (itemsInPage_) {}

            void *Alloc () {
                LockGuard<SpinLock> guard (spinLock);
                Page *page = GetPage ();
                void *ptr = page->Alloc ();
                if (page->IsFull ()) {
                    partialPages.erase (nullptr, page);
                    fullPages.push_front (page);
                }
                return ptr;
            }

            void Free (void *ptr) {
                LockGuard<SpinLock> guard (spinLock);
                Page *prev;
                Page *page = GetPage (ptr, prev);
                if (page->IsFull ()) {
                    // If the page is full, it must have come
                    // from the fullPages list.
                    fullPages.erase (prev, page);
                    // Since we're removing an item from the
                    // page it will no longer be full and needs
                    // to go to the head of partialPages list.
                    partialPages.push_front (page);
                }
                page->Free (ptr);
            }

        private:
            inline Page *GetPage () {
                if (partialPages.empty ()) {
                    partialPages.push_front (
                        new (allocator.Alloc (Page::Size (itemsInPage))) Page (itemsInPage));
                    // Similar to the algorithm in RefCountedRegistry, Heap
                    // grows the page size with every page allocation.
                    // By growing the page size we keep the page count
                    // relatively small for GetPage (below).
                    // ASIDE: It has not escaped me that this is a policy and,
                    // from the design perspective, should be treated as such and
                    // be parametarized. Perhaps, in the future, if the need arizes
                    // Heap can be turned in to a template taking a policy type.
                    itemsInPage <<= 1;
                }
                return partialPages.front ();
            }

            // The heap has only two publc facing methods, Alloc and Free.
            // Alloc uses the above GetPage which runs in O(1). Free uses
            // this GetPage which runs in O(n) where n is the sum of both
            // partial and full page lists. If you're profiling your code
            // and you see that it spends a lot of its time here there's a
            // knob you can tune to substantially improve performance. Rebuild
            // util and supply your own:
            // THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_HEAP_ITEMS_IN_PAGE.
            // Its default is 8192 which should be acceptable in most
            // situations. By increasing it to match the needs of your
            // application you can greatly reduce the time it spends here.
            inline Page *GetPage (
                    const void *ptr,
                    Page *&prev) const {
                Page *page = partialPages.find (ptr, prev);
                if (page == nullptr) {
                    page = fullPages.find (ptr, prev);
                }
                return page;
            }
        };

        void *RefCounted::References::operator new (std::size_t) {
            return Heap::Instance ()->Alloc ();
        }

        void RefCounted::References::operator delete (void *ptr) {
            Heap::Instance ()->Free (ptr);
        }

        namespace {
            typedef boost::atomics::detail::operations<4u, false> operations;
        }

        ui32 RefCounted::References::AddWeakRef () {
            return operations::fetch_add (weak, 1, boost::memory_order_release) + 1;
        }

        ui32 RefCounted::References::ReleaseWeakRef () {
            ui32 newWeak = operations::fetch_sub (weak, 1, boost::memory_order_release) - 1;
            if (newWeak == 0) {
                delete this;
            }
            return newWeak;
        }

        ui32 RefCounted::References::GetWeakCount () const {
            return operations::load (weak, boost::memory_order_relaxed);
        }

        ui32 RefCounted::References::AddSharedRef () {
            return operations::fetch_add (shared, 1, boost::memory_order_release) + 1;
        }

        ui32 RefCounted::References::ReleaseSharedRef (RefCounted *object) {
            ui32 newShared = operations::fetch_sub (shared, 1, boost::memory_order_release) - 1;
            if (newShared == 0) {
                object->Harakiri ();
            }
            return newShared;
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
                        boost::memory_order_release,
                        boost::memory_order_relaxed)) {
                    return true;
                }
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
