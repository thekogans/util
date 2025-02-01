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

        void *BlockAllocator::Page::Alloc () {
            if (freeBlock != nullptr) {
                Block *block = freeBlock;
                freeBlock = freeBlock->next;
                ++blockCount;
                return block->block;
            }
            return blocks + blockSize * blockCount++;
        }

        void BlockAllocator::Page::Free (void *ptr) {
            Block *block = (Block *)ptr;
            block->next = freeBlock;
            freeBlock = block;
            --blockCount;
        }

        bool BlockAllocator::Page::IsValidPtr (void *ptr) {
            Block *block = (Block *)ptr;
            return
                // Verify that the given pointer points to the
                // beginning of an block.
                block->block >= blocks && block->block < blocks + blocksPerPage * blockSize &&
                    (std::ptrdiff_t)((const std::size_t)block->block -
                        (const std::size_t)blocks) % blockSize == 0;
        }

        BlockAllocator::~BlockAllocator () {
            // We're going out of scope. If there are still
            // pages remaining, we have a memory leak.
            assert (fullPages.empty () && partialPages.empty ());
            auto callback = [allocator = allocator] (Page *page) -> bool {
                page->~Page ();
                allocator->Free (page, Page::Size (page->blockSize, page->blocksPerPage));
                return true;
            };
            fullPages.erase (callback);
            partialPages.erase (callback);
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
            void *ptr = page->Alloc ();
            if (page->IsFull ()) {
                partialPages.erase (page);
                fullPages.push_back (page);
            }
            return ptr;
        }

        void BlockAllocator::Free (
                void *ptr,
                std::size_t /*size*/) {
            LockGuard<SpinLock> guard (spinLock);
            Page *page = GetPage (ptr);
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
            if (page->IsEmpty ()) {
                partialPages.erase (page);
                page->~Page ();
                allocator->Free (page, Page::Size (page->blockSize, page->blocksPerPage));
                blocksPerPage >>= 1;
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
                partialPages.push_back (
                    new (allocator->Alloc (Page::Size (blockSize, blocksPerPage)))
                        Page (blockSize, blocksPerPage));
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
