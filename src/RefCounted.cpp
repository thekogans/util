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

        struct RefCounted::References::Heap : Singleton<Heap> {
        private:
            struct Page {
                // Page is designed to properly align the shared and
                // weak counters in RefCounted::References on both 32
                // and 64 bit architectures. It's even future proof
                // for 128 bit architectures if they ever become common.
                Page *prev;
                Page *next;
                const std::size_t maxItems;
                std::size_t itemCount;
                union Item {
                    Item *next;
                    ui8 block[sizeof (RefCounted::References)];
                } *freeItem, items[1];

                Page (std::size_t maxItems_) :
                    prev (nullptr),
                    next (nullptr),
                    maxItems (maxItems_),
                    itemCount (0),
                    freeItem (nullptr) {}

                inline bool IsEmpty () const {
                    return itemCount == 0;
                }
                inline bool IsFull () const {
                    return itemCount == maxItems;
                }

                inline void *Alloc () {
                    Item *item;
                    if (freeItem != nullptr) {
                        item = freeItem;
                        freeItem = freeItem->next;
                    }
                    else {
                        item = &items[itemCount];
                    }
                    ++itemCount;
                    return item->block;
                }

                inline void Free (void *ptr) {
                    Item *item = (Item *)ptr;
                    item->next = freeItem;
                    freeItem = item;
                    --itemCount;
                }

                // Check to see if the given pointer is within the
                // items list.
                inline bool IsItem (const void *item) const {
                    return item >= items && item < &items[maxItems];
                }
            };

            struct PageList {
                Page *head;
                Page *tail;

                PageList () :
                    head (nullptr),
                    tail (nullptr) {}

                inline bool empty () const {
                    return head == nullptr;
                }
                inline Page *front () const {
                    return head;
                }

                inline void push_front (Page *page) {
                    if (head == nullptr) {
                        page->prev = page->next = nullptr;
                        head = tail = page;
                    }
                    else {
                        page->prev = nullptr;
                        page->next = head;
                        head = head->prev = page;
                    }
                }

                inline void erase (Page *page) {
                    if (page->prev != nullptr) {
                        page->prev->next = page->next;
                    }
                    else {
                        head = page->next;
                        if (head != nullptr) {
                            head->prev = nullptr;
                        }
                    }
                    if (page->next != nullptr) {
                        page->next->prev = page->prev;
                    }
                    else {
                        tail = page->prev;
                        if (tail != nullptr) {
                            tail->next = nullptr;
                        }
                    }
                    page->prev = page->next = nullptr;
                }

                inline Page *find (const std::function<bool (Page *page)> &callback) const {
                    for (Page *page = head; page != nullptr; page = page->next) {
                        if (callback (page)) {
                            return page;
                        }
                    }
                    return nullptr;
                }
            };

            const std::size_t itemsInPage;
            const std::size_t pageSize;
            // RefCounted::References uses boost primitives to atomicaly
            // increment/decrement the shared and weak counters. On some
            // platforms these primitives use machine instructions to
            // achieve good performance. Sometimes those instructins
            // have various alignment requirements. In order to guarntee
            // that we satisfy these requirements we allocate pages with
            // AlignedAllocator (above). The alignement used is UI32_SIZE
            // which happens to be the type of the above mentioned counters.
            struct AlignedAllocator {
            private:
                std::size_t alignment;

            public:
                AlignedAllocator (std::size_t alignment_) :
                    alignment (alignment_) {}

                void *Alloc (std::size_t size) {
                    std::size_t rawSize = alignment + size + sizeof (ui8 *);
                    ui8 *rawPtr = new ui8[rawSize];
                    ui8 *ptr = rawPtr;
                    std::size_t amountMisaligned = ((std::size_t)ptr & (alignment - 1));
                    if (amountMisaligned > 0) {
                        ptr += alignment - amountMisaligned;
                    }
                    *(ui8 **)((std::size_t)ptr + size) = rawPtr;
                    return ptr;
                }

                void Free (
                        void *ptr,
                        std::size_t size) {
                    delete [] *(ui8 **)((std::size_t)ptr + size);
                }
            } allocator;
            PageList fullPages;
            PageList partialPages;
            SpinLock spinLock;

        public:
            Heap (std::size_t itemsInPage_ =
                    THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_HEAP_ITEMS_IN_PAGE) :
                itemsInPage (itemsInPage_),
                pageSize (sizeof (Page) + sizeof (Page::Item) * (itemsInPage - 1)),
                allocator (UI32_SIZE) {}

            void *Alloc () {
                LockGuard<SpinLock> guard (spinLock);
                Page *page = GetPage ();
                void *ptr = page->Alloc ();
                if (page->IsFull ()) {
                    partialPages.erase (page);
                    fullPages.push_front (page);
                }
                return ptr;
            }

            void Free (void *ptr) {
                LockGuard<SpinLock> guard (spinLock);
                Page *page = GetPage (ptr);
                if (page->IsFull ()) {
                    fullPages.erase (page);
                    partialPages.push_front (page);
                }
                page->Free (ptr);
                if (page->IsEmpty ()) {
                    partialPages.erase (page);
                    page->~Page ();
                    allocator.Free (page, pageSize);
                }
            }

        private:
            inline Page *GetPage () {
                if (partialPages.empty ()) {
                    partialPages.push_front (
                        new (allocator.Alloc (pageSize)) Page (itemsInPage));
                }
                return partialPages.front ();
            }

            // The heap has only two publc facing methods, Alloc and Free.
            // Alloc uses the above GetPage which runs in O(1). Free uses
            // this GetPage which runs in O(n) where n is the sum of both
            // partial and full page lists. If you're profiling your code
            // and you see that it spends a lot of its time here theres a
            // knob you can tune to substantially improve performance. Rebuild
            // util and supply your own:
            // THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_HEAP_ITEMS_IN_PAGE.
            // Its default is 8192 which should be acceptable in most
            // situations. By increasing it to match the needs of your
            // application you can greatly reduce the time it spends here.
            inline Page *GetPage (void *ptr) const {
                auto findPage = [ptr] (Page *page) -> bool {
                    return page->IsItem (ptr);
                };
                Page *page = partialPages.find (findPage);
                if (page == nullptr) {
                    page = fullPages.find (findPage);
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
            else if (newWeak == UI32_MAX) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "weak counter underflow in an instance of %s.",
                    typeid (*this).name ());
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
            else if (newShared == UI32_MAX) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "shared counter underflow in an instance of %s.",
                    typeid (*this).name ());
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

        RefCounted::~RefCounted () {
            // We're going out of scope. If there are still
            // shared references remaining, we have a problem.
            if (references->GetSharedCount () != 0) {
                // Here we both log the problem and assert to give the
                // engineer the best chance of figuring out what happened.
                std::string message =
                    FormatString ("%s : %u\n",
                        typeid (*this).name (),
                        references->GetSharedCount ());
                Log (
                    SubsystemAll,
                    THEKOGANS_UTIL,
                    Error,
                    __FILE__,
                    __FUNCTION__,
                    __LINE__,
                    __DATE__ " " __TIME__,
                    "%s",
                    message.c_str ());
                THEKOGANS_UTIL_ASSERT (references->GetSharedCount () == 0, message);
            }
            references->ReleaseWeakRef ();
        }

    } // namespace util
} // namespace thekogans
