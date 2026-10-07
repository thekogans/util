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

#if !defined (__thekogans_util_SlabAllocatorHeap_h)
#define __thekogans_util_SlabAllocatorHeap_h

#include <cstddef>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SlabAllocatorImpl.h"

namespace thekogans {
    namespace util {

        /// \struct SlabAllocatorHeap SlabAllocatorHeap.h thekogans/util/SlabAllocatorHeap.h
        ///
        /// \brief
        /// SlabAllocatorHeap is part of the \see{Allocator} framework.
        struct _LIB_THEKOGANS_UTIL_DECL SlabAllocatorHeap {
        private:
            std::size_t minExp;
            std::size_t maxExp;
            std::size_t numPools;
            std::size_t minSize;
            std::size_t maxSize;
            static constexpr std::size_t MAX_POOLS = BitWidth<ui32>::value;
            SlabAllocatorImpl *pools[MAX_POOLS] = {};

        public:
            static constexpr std::size_t DEFAULT_MIN_EXP = 3; // 2^3
            static constexpr std::size_t DEFAULT_MAX_EXP = 22; // 2^22
            static constexpr std::size_t DEFAULT_MIN_SIZE = 1ULL << DEFAULT_MIN_EXP; // 8 Bytes
            static constexpr std::size_t DEFAULT_MAX_SIZE = 1ULL << DEFAULT_MAX_EXP; // 4 MB
            static constexpr std::pair<std::size_t, std::size_t> DEFAULT_POOL_CONFIG[] = {
                {512, 64},
                {512, 64},
                {512, 64},
                {512, 64},
                {512, 64}, // 8B - 128B
                {512, 64},
                {512, 64}, // 256B - 512B
                {256, 32},
                {128, 16},
                {64,  8},
                {32,  4},  // 1KiB - 8KiB
                {32,  4},
                {16,  2},
                {16,  2},  // 16KiB - 64KiB
                {8,   1},
                {8,   1},  // 128KiB - 256KiB
                {4,   0},
                {2,   0},
                {2,   0},
                {2,   0}   // 512KiB - 4MiB
            };

            explicit SlabAllocatorHeap (
                std::size_t minExp_ = DEFAULT_MIN_EXP,
                std::size_t maxExp_ = DEFAULT_MAX_EXP,
                const std::pair<std::size_t, std::size_t> poolConfig[] = DEFAULT_POOL_CONFIG);
            ~SlabAllocatorHeap ();

            /// \brief
            /// Allocate a block from system heap.
            /// \param[in] size Size of block to allocate.
            /// \return Pointer to the allocated block (0 if out of memory).
            void *Alloc (std::size_t size);
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// \param[in] ptr Pointer to the block returned by Alloc.
            /// \param[in] size Same size parameter previously passed in to Alloc.
            void Free (
                void *ptr,
                std::size_t size);
        };

        struct _LIB_THEKOGANS_UTIL_DECL GlobalSlabAllocatorHeap :
                public Allocator,
                public SlabAllocatorHeap,
                public
            Singleton<
                    GlobalSlabAllocatorHeap,
                    SpinLock,
                    RefCountedInstanceCreator<GlobalSlabAllocatorHeap>,
                    NullInstanceDestroyer<GlobalSlabAllocatorHeap>> {
            /// \brief
            /// SlabAllocatorHeap participates in the \see{DynamicCreatable}
            /// dynamic discovery and creation.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (GlobalSlabAllocatorHeap)

            explicit GlobalSlabAllocatorHeap (
                std::size_t minExp = DEFAULT_MIN_EXP,
                std::size_t maxExp = DEFAULT_MAX_EXP,
                const std::pair<std::size_t, std::size_t> poolConfig[] = DEFAULT_POOL_CONFIG) :
                SlabAllocatorHeap (minExp, maxExp, poolConfig) {}

            /// \brief
            /// Allocate a block from system heap.
            /// \param[in] size Size of block to allocate.
            /// \return Pointer to the allocated block (0 if out of memory).
            virtual void *Alloc (std::size_t size) override {
                return SlabAllocatorHeap::Alloc (size);
            }
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// \param[in] ptr Pointer to the block returned by Alloc.
            /// \param[in] size Same size parameter previously passed in to Alloc.
            virtual void Free (
                    void *ptr,
                    std::size_t size) override {
                SlabAllocatorHeap::Free (ptr, size);
            }
        };

        /// \brief
        /// Direct replacement for std malloc. Uses a pool of \see{SlabAllocatorImpl}.
        /// \param[in] size Size of block to allocate,
        /// \return Allocated block.
        void *tk_malloc (std::size_t size);
        /// \brief
        /// Companion to thekogans_malloc.
        /// \param[in] ptr Pointer return by thekogans_malloc.
        /// \param[in] size Same value passed to thekogans_malloc.
        void tk_free (
            void *ptr,
            std::size_t size);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SlabAllocatorHeap_h)
