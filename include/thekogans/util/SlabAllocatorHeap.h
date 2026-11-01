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
#include "thekogans/util/SlabAllocatorImpl.h"

namespace thekogans {
    namespace util {

        /// \struct tk_mem_t
        /// \brief The native return token for the high-performance heap.
        /// maps perfectly to ARM64 registers (x0/x1) for zero-copy passing.
        struct tk_mem_t {
            void *ptr{nullptr};
            std::size_t poolIndex{0};
        };

        /// \struct SlabAllocatorHeap SlabAllocatorHeap.h thekogans/util/SlabAllocatorHeap.h
        ///
        /// \brief
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
                { 64, 8},
                { 32, 4},  // 1KiB - 8KiB
                { 32, 4},
                { 16, 2},
                { 16, 2},  // 16KiB - 64KiB
                {  8, 1},
                {  8, 1},  // 128KiB - 256KiB
                {  4, 1},
                {  2, 1},
                {  2, 1},
                {  2, 1}   // 512KiB - 4MiB
            };

            explicit SlabAllocatorHeap (
                std::size_t minExp_ = DEFAULT_MIN_EXP,
                std::size_t maxExp_ = DEFAULT_MAX_EXP,
                const std::pair<std::size_t, std::size_t> poolConfig[] = DEFAULT_POOL_CONFIG);
            /// \brief
            /// dtor.
            ~SlabAllocatorHeap ();

            /// \brief
            /// Allocate a block from system heap.
            /// \param[in] size Size of block to allocate.
            /// \return Token (tk_mem_t) containing the block and the SlabAllocator index.
            tk_mem_t Alloc (std::size_t size);
            /// \brief
            /// Return the true allocated block size (alligned to power of 2).
            /// \param[in] mem tk_mem_t returned by Alloc.
            /// \return True allocated block size.
            std::size_t GetMemSize (tk_mem_t mem);
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// \param[in] mem Same tk_mem_t previously returned by Alloc.
            void Free (tk_mem_t mem);
        };

        /// \brief
        /// Initialize the GlobalSlabAllocatorHeap \see{Singleton}.
        /// \param[in] minExp The smallest object (2^minExp).
        /// \param[in] maxExp The largest object (2^maxExp).
        /// \param[in] poolConfig An array of maxExp - minExp + 1 pairs of <SlotsPerPage, TLCSize>.
        void tk_malloc_init (
            std::size_t minExp,
            std::size_t maxExp,
            const std::pair<std::size_t, std::size_t> poolConfig[]);

        /// \brief
        /// Direct lock-free replacement for std::malloc.
        /// \param[in] size Size of block to allocate.
        /// \return A token containing the block pointer and the pool index.
        [[nodiscard]] tk_mem_t tk_malloc (std::size_t size) noexcept;

        /// \brief
        /// Return the true allocated block size (alligned to power of 2).
        /// \param[in] mem tk_mem_t returned by tk_malloc.
        /// \return True allocated block size.
        std::size_t tk_mem_size (tk_mem_t mem) noexcept;

        /// \brief
        /// Companion to tk_malloc.
        /// \param[in] mem tk_mem_t returned by tk_malloc.
        void tk_free (tk_mem_t mem) noexcept;

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SlabAllocatorHeap_h)
