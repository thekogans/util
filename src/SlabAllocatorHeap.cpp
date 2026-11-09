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
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SlabAllocatorHeap.h"

namespace thekogans {
    namespace util {

        SlabAllocatorHeap::SlabAllocatorHeap (
                std::size_t minExp_,
                std::size_t maxExp_,
                const std::pair<std::size_t, std::size_t> poolConfig[]) :
                minExp (minExp_),
                maxExp (maxExp_),
                numPools (maxExp - minExp),
                minSize (1ULL << minExp),
                maxSize (1ULL << maxExp) {
            // Validate input.
            if (minExp < DEFAULT_MIN_EXP || minExp >= maxExp || numPools > MAX_POOLS) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
            for (std::size_t i = 0, slotSize = minSize; i < numPools; ++i) {
                pools[i] = new SlabAllocatorImpl (slotSize, poolConfig[i].first, poolConfig[i].second);
                slotSize <<= 1;
            }
        }

        SlabAllocatorHeap::~SlabAllocatorHeap () {
            for (std::size_t i = 0; i < numPools; ++i) {
                delete pools[i];
            }
        }

        tk_mem_t SlabAllocatorHeap::Alloc (std::size_t size) {
            if (THEKOGANS_UTIL_UNLIKELY (size < minSize || size > maxSize)) {
                return tk_mem_t {nullptr, 0};
            }
            std::size_t poolIndex = GetMemPoolIndex (size);
            return tk_mem_t{pools[poolIndex]->Alloc (), poolIndex};
        }

        std::size_t SlabAllocatorHeap::GetMemSize (std::size_t poolIndex) {
            return 1ULL << (poolIndex + minExp);
        }

        std::size_t SlabAllocatorHeap::GetMemPoolIndex (std::size_t size) {
            return TrailingZeroBitCount (Align (size)) - minExp;
        }

        void SlabAllocatorHeap::Free (tk_mem_t mem) {
            if (THEKOGANS_UTIL_UNLIKELY (mem.ptr == nullptr || mem.poolIndex >= numPools)) {
                return;
            }
            pools[mem.poolIndex]->Free (mem.ptr);
        }

        using GlobalSlabAllocatorHeap = Singleton<SlabAllocatorHeap>;

        void tk_malloc_init (
                std::size_t minExp,
                std::size_t maxExp,
                const std::pair<std::size_t, std::size_t> poolConfig[]) {
            GlobalSlabAllocatorHeap::CreateInstance (minExp, maxExp, poolConfig);
        }

        tk_mem_t tk_malloc (std::size_t size) noexcept {
            return GlobalSlabAllocatorHeap::Instance ()->Alloc (size);
        }

        std::size_t tk_mem_size (std::size_t poolIndex) noexcept {
            return GlobalSlabAllocatorHeap::Instance ()->GetMemSize (poolIndex);
        }

        std::size_t tk_mem_pool_index (std::size_t size) noexcept {
            return GlobalSlabAllocatorHeap::Instance ()->GetMemPoolIndex (size);
        }

        void tk_free (tk_mem_t mem) noexcept {
            GlobalSlabAllocatorHeap::Instance ()->Free (mem);
        }

    } // namespace util
} // namespace thekogans
