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

#include "thekogans/util/Constants.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/GlobalSlabAllocator.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_S (
            thekogans::util::GlobalSlabAllocator,
            Allocator::TYPE)

        GlobalSlabAllocator::GlobalSlabAllocator (
                std::size_t minExp_,
                std::size_t maxExp_,
                const std::pair<std::size_t, std::size_t> poolConfig[]) :
                minExp (minExp_),
                maxExp (maxExp_),
                numPools (maxExp - minExp),
                minSize (1ULL << minExp),
                maxSize (1ULL << maxExp) {
            // Validate input.
            if (minExp < DEFAULT_MIN_EXP || minExp > MAX_POOLS || minExp >= maxExp ||
                    maxExp > MAX_POOLS || numPools > MAX_POOLS) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
            for (std::size_t i = 0, slotSize = minSize; i < numPools; ++i) {
                pools[i] = new SlabAllocatorImpl (slotSize, poolConfig[i].first, poolConfig[i].second);
                slotSize <<= 1;
            }
        }

        void *GlobalSlabAllocator::Alloc (size_t size) {
            if (THEKOGANS_UTIL_UNLIKELY (size == 0 || size > maxSize)) {
                return nullptr;
            }
            if (THEKOGANS_UTIL_UNLIKELY (size < minSize)) {
                size = minSize;
            }
            return pools[TrailingZeroBitCount (Align (size)) - minExp]->Alloc ();
        }

        void GlobalSlabAllocator::Free (
                void *ptr,
                std::size_t size) {
            if (THEKOGANS_UTIL_UNLIKELY (ptr == nullptr || size == 0 || size > maxSize)) {
                return;
            }
            if (THEKOGANS_UTIL_UNLIKELY (size < minSize)) {
                size = minSize;
            }
            pools[TrailingZeroBitCount (Align (size)) - minExp]->Free (ptr);
        }

        namespace {
            static GlobalSlabAllocator &allocator = *GlobalSlabAllocator::Instance ();
        }

        void *thekogans_malloc (std::size_t size) {
            return allocator.Alloc (size);
        }

        void thekogans_free (
                void *ptr,
                std::size_t size) {
            allocator.Free (ptr, size);
        }

    } // namespace util
} // namespace thekogans
