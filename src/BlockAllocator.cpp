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

#include "thekogans/util/LockGuard.h"
#include "thekogans/util/BlockAllocator.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (thekogans::util::BlockAllocator)

        BlockAllocator::BlockAllocator (
                std::size_t blockSize_,
                std::size_t blocksPerPage_,
                Allocator::SharedPtr allocator_) :
                blockSize (blockSize_),
                blocksPerPage (blocksPerPage_),
                allocator (allocator_),
                blockCount (0) {
            if (blockSize < sizeof (Page::Block)) {
                blockSize = sizeof (Page::Block);
            }
            if (blocksPerPage == 0) {
                blocksPerPage = 1;
            }
            if (allocator == nullptr) {
                allocator = DefaultAllocator::Instance ();
            }
        }

        BlockAllocator::~BlockAllocator () {
            // We're going out of scope. If there are still
            // pages remaining, we have a memory leak.
            if (!fullPages.empty () || !partialPages.empty ()) {
                // FIXME throw
                assert (0);
            }
        }

        bool BlockAllocator::IsValidPtr (void *ptr) noexcept {
            if (ptr != nullptr) {
                LockGuard<SpinLock> guard (spinLock);
                // To honor the no throw promise, we can't assume the
                // pointer came from this heap. We can't even assume
                // that it is valid (we cannot de-reference it). We
                // therefore search through our pages to see if the
                // given pointer lies within range.
                auto callback = [ptr] (Page *page) -> bool {
                    return !page->IsValidPtr (ptr);
                };
                return !fullPages.for_each (callback) || !partialPages.for_each (callback);
            }
            return false;
        }

        void *BlockAllocator::Alloc (std::size_t /*size*/) {
            LockGuard<SpinLock> guard (spinLock);
            Page *page = GetPage ();
            assert (page != nullptr);
            if (page != nullptr) {
                void *ptr = page->Alloc ();
                assert (ptr != nullptr);
                if (page->IsFull ()) {
                    // GetPage will always return a page from the
                    // partialPages list.
                    partialPages.erase (page);
                    fullPages.push_back (page);
                }
                ++blockCount;
                return ptr;
            }
            return nullptr;
        }

        void BlockAllocator::Free (
                void *ptr,
                std::size_t /*size*/) {
            LockGuard<SpinLock> guard (spinLock);
            Page *page = GetPage (ptr);
            assert (page != nullptr);
            if (page != nullptr) {
                // This logic is necessary to accommodate pages
                // with one block. They become full after one
                // allocation, and empty after one deletion.
                if (page->IsFull ()) {
                    fullPages.erase (page);
                    // Put the page at the head of the partial
                    // pages list. If the next allocation happens
                    // soon enough, this page should still be in
                    // cache.
                    partialPages.push_front (page);
                }
                page->Free (ptr);
                --blockCount;
                if (page->IsEmpty ()) {
                    partialPages.erase (page);
                    page->~Page ();
                    allocator->Free (page, Page::Size (page->blockSize, page->blocksPerPage));
                    blocksPerPage >>= 1;
                }
            }
        }

        Allocator::SharedPtr BlockAllocator::Pool::GetBlockAllocator (
                std::size_t blockSize,
                std::size_t blocksPerPage,
                Allocator::SharedPtr allocator) {
            LockGuard<SpinLock> guard (spinLock);
            std::pair<Map::iterator, bool> result =
                map.insert (Map::value_type (blockSize, nullptr));
            if (result.second) {
                result.first->second.Reset (
                    new BlockAllocator (blockSize, blocksPerPage, allocator));
            }
            return result.first->second;
        }

        BlockAllocator::Page *BlockAllocator::GetPage () {
            if (partialPages.empty ()) {
                void *page = allocator->Alloc (Page::Size (blockSize, blocksPerPage));
                assert (page != nullptr);
                if (page != nullptr) {
                    // This is safe, as neither placement new, nor
                    // Page ctor, nor push_back will throw.
                    partialPages.push_back (new (page) Page (blockSize, blocksPerPage));
                }
                blocksPerPage <<= 1;
            }
            return partialPages.front ();
        }

        BlockAllocator::Page *BlockAllocator::GetPage (void *ptr) const {
            auto callback = [ptr] (Page *page) -> bool {
                return page->IsValidPtr (ptr);
            };
            Page *page = partialPages.find (callback);
            if (page == nullptr) {
                page = fullPages.find (callback);
            }
            return page;
        }

    } // namespace util
} // namespace thekogans
