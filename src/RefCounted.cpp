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

#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/operations_lockfree.hpp>
#include <boost/memory_order.hpp>
#include "thekogans/util/Singleton.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/RefCounted.h"

namespace thekogans {
    namespace util {

    #if !defined (THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_HEAP_ITEMS_IN_PAGE)
        #define THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_HEAP_ITEMS_IN_PAGE 8192
    #endif // !defined (THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_HEAP_ITEMS_IN_PAGE)

        struct RefCounted::References::Heap : Singleton<Heap> {
        protected:
            struct Page;
            enum {
                PAGE_LIST_ID
            };
            typedef IntrusiveList<Page, PAGE_LIST_ID> PageList;
            struct Page : public PageList::Node {
                const std::size_t maxItems;
                std::size_t itemCount;
                union Item {
                    Item *next;
                    ui8 block[sizeof (RefCounted::References)];
                } *freeItem, items[1];

                Page (std::size_t maxItems_) :
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

            const std::size_t itemsInPage;
            const std::size_t pageSize;
            PageList fullPages;
            PageList partialPages;
            SpinLock spinLock;

        public:
            Heap (std::size_t itemsInPage_ =
                    THEKOGANS_UTIL_DEFAULT_REF_COUNED_REFERENCES_HEAP_ITEMS_IN_PAGE) :
                itemsInPage (itemsInPage_),
                pageSize (sizeof (Page) + sizeof (Page::Item) * (itemsInPage - 1)) {}

            void *Alloc () {
                LockGuard<SpinLock> guard (spinLock);
                Page *page = GetPage ();
                void *ptr = page->Alloc ();
                if (page->IsFull ()) {
                    partialPages.erase (page);
                    fullPages.push_back (page);
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
                    delete [] (ui8 *)page;
                }
            }

        private:
            inline Page *GetPage () {
                if (partialPages.empty ()) {
                    partialPages.push_back (new (new ui8[pageSize]) Page (itemsInPage));
                }
                return partialPages.front ();
            }

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
                    FormatString ("%s : %u\n", typeid (*this).name (), references->GetSharedCount ());
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
