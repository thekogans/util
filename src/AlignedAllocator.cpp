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

#include <cstddef>
#include "thekogans/util/AlignedAllocator.h"

namespace thekogans {
    namespace util {

        AlignedAllocator::AlignedAllocator (
                Allocator &allocator_,
                std::size_t alignment_) :
                allocator (allocator_),
                alignment (alignment_) {
            assert (OneBitCount (alignment) == 1);
        }

        void AlignedAllocator::Free (
                void *ptr,
                std::size_t size) {
            if (ptr != nullptr) {
                Footer *footer = (Footer *)((std::size_t)ptr + size);
                footer->~Footer ();
                allocator.Free (footer->ptr, footer->size);
            }
        }

        void *AlignedAllocator::AllocHelper (
                std::size_t &size,
                bool useMax) {
            void *ptr = nullptr;
            if (size > 0) {
                // Calculate additional space required to align the block.
                // NOTE: For very large alignments, we can have very
                // inefficient use of resources.
                std::size_t rawSize = alignment + size + sizeof (Footer);
                void *rawPtr = allocator.Alloc (rawSize);
                if (rawPtr != nullptr) {
                    // To minimize waste, return to the caller the
                    // maximum amount of space available for use
                    // after alignment restrictions are satisfied.
                    std::size_t amountMisaligned =
                        alignment - ((std::size_t)rawPtr & (alignment - 1));
                    assert (alignment >= amountMisaligned);
                    if (useMax) {
                        size += alignment - amountMisaligned;
                    }
                    // Align the raw pointer.
                    ptr = (void *)((std::size_t)rawPtr + amountMisaligned);
                    // Stash it away in the footer to be freed later.
                    new ((void *)((std::size_t)ptr + size)) Footer (rawPtr, rawSize);
                }
            }
            return ptr;
        }

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API ZeroBitCount (
                std::size_t value) {
            static const std::size_t nibleZeroBitCount[] = {
                4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0
            };
            std::size_t count = 0;
            for (std::size_t shift = sizeof (std::size_t) * 8 - 4,
                     mask = (std::size_t)0xf << shift; mask != 0; mask >>= 4, shift -= 4) {
                count += nibleZeroBitCount[(value & mask) >> shift];
            }
            return count;
        }

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API OneBitCount (
                std::size_t value) {
            static const std::size_t nibleOneBitCount[] = {
                0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
            };
            std::size_t count = 0;
            for (std::size_t shift = sizeof (std::size_t) * 8 - 4,
                     mask = (std::size_t)0xf << shift; mask != 0; mask >>= 4, shift -= 4) {
                count += nibleOneBitCount[(value & mask) >> shift];
            }
            return count;
        }

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API Align (
                std::size_t value) {
            static const std::size_t nibleHighBit[] = {
                0, 1, 2, 2, 4, 4, 4, 4, 8, 8, 8, 8, 8, 8, 8, 8
            };
            for (std::size_t shift = sizeof (std::size_t) * 8 - 4,
                     mask = (std::size_t)0xf << shift; mask != 0; mask >>= 4, shift -= 4) {
                std::size_t highBit = nibleHighBit[(value & mask) >> shift];
                if (highBit != 0) {
                    highBit <<= shift;
                    if (highBit < value) {
                        highBit <<= 1;
                    }
                    assert (highBit >= value);
                    return highBit;
                }
            }
            return 0;
        }

    } // namespace util
} // namespace thekogans
