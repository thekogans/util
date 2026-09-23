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

#ifdef _MSC_VER
    #include <intrin.h>
#endif
#include <cstddef>
#include <climits>
#include "thekogans/util/Exception.h"
#include "thekogans/util/AlignedAllocator.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (
            thekogans::util::AlignedAllocator,
            Allocator::TYPE)

        AlignedAllocator::AlignedAllocator (
                std::size_t alignment_,
                Allocator::SharedPtr allocator_) :
                alignment (alignment_),
                allocator (allocator_) {
            if (!IsPowerOf2 (alignment) || allocator == nullptr) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void AlignedAllocator::Free (
                void *ptr,
                std::size_t size) {
            if (ptr != nullptr) {
                Footer *footer = (Footer *)((std::size_t)ptr + size);
                footer->~Footer ();
                allocator->Free (footer->ptr, footer->size);
            }
        }

        void *AlignedAllocator::AllocHelper (
                std::size_t &size,
                bool useMax) {
            ui8 *ptr = nullptr;
            if (size > 0) {
                // Calculate additional space required to align the block.
                // NOTE: For very large alignments, we can have very
                // inefficient use of resources.
                std::size_t rawSize = alignment + size + sizeof (Footer);
                void *rawPtr = allocator->Alloc (rawSize);
                if (rawPtr != nullptr) {
                    ptr = (ui8 *)rawPtr;
                    // To minimize waste, return to the caller the
                    // maximum amount of space available for use
                    // after alignment restrictions are satisfied.
                    std::size_t amountMisaligned = ((std::size_t)ptr & (alignment - 1));
                    assert (alignment >= amountMisaligned);
                    if (useMax) {
                        size += amountMisaligned;
                    }
                    if (amountMisaligned > 0) {
                        // Align the raw pointer.
                        ptr += alignment - amountMisaligned;
                    }
                    // Stash it away in the footer to be freed later.
                    new ((void *)((std::size_t)ptr + size)) Footer (rawPtr, rawSize);
                }
            }
            return ptr;
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API IsPowerOf2 (std::size_t value) {
            return value > 0 && (value & (value - 1)) == 0;
        }

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API ZeroBitCount (std::size_t value) {
            return BitWidth<std::size_t>::value - OneBitCount (value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API OneBitCount (std::size_t value) {
        #if defined (__GNUC__) || defined (__clang__)
            if constexpr (sizeof (std::size_t) == 8) {
                return __builtin_popcountll (value);
            }
            else {
                return __builtin_popcount (value);
            }
        #elif defined(_MSC_VER)
            if constexpr (sizeof (std::size_t) == 8) {
                return __popcnt64 (value));
            }
            else {
                return __popcnt (value);
            }
        #else // defined (__GNUC__) || defined (__clang__)
            // Fallback SWAR (SIMD Within A Register) algorithm
            value = value - ((value >> 1) & (std::size_t)~(std::size_t)0 / 3);
            value = (value & (std::size_t)~(std::size_t)0 / 15 * 3) + ((value >> 2) & (std::size_t)~(std::size_t)0 / 15 * 3);
            value = (value + (value >> 4)) & (std::size_t)~(std::size_t)0 / 255 * 15;
            return (std::size_t)(value * ((std::size_t)~(std::size_t)0 / 255)) >> (sizeof (std::size_t) - 1) * CHAR_BIT;
        #endif // defined (__GNUC__) || defined (__clang__)
        }

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API TrailingZeroBitCount (std::size_t value) {
            if (value == 0) {
                return sizeof (std::size_t) * 8; // Handle 0 safely
            }
        #if defined (__GNUC__) || defined (__clang__)
            if constexpr (sizeof (std::size_t) == 8) {
                return __builtin_ctzll (value);
            }
            else {
                return __builtin_ctz (value);
            }
        #elif defined (_MSC_VER)
            unsigned long index;
        #if defined (_WIN64)
            _BitScanForward64 (&index, value);
        #else // defined (_WIN64)
            _BitScanForward (&index, value);
        #endif // defined (_WIN64)
            return index;
        #else // defined (__GNUC__) || defined (__clang__)
            // Fallback cross-platform bit-twiddling if no intrinsic is found
            std::size_t count = 0;
            if constexpr (sizeof (std::size_t) >= 8) {
                if ((value & 0xFFFFFFFF) == 0) {
                    count += 32;
                    value >>= 32;
                }
            }
            if ((value & 0xFFFF) == 0) {
                count += 16;
                value >>= 16;
            }
            if ((value & 0xFF) == 0) {
                count += 8;
                value >>= 8;
            }
            if ((value & 0xF) == 0) {
                count += 4;
                value >>= 4;
            }
            if ((value & 0x3) == 0) {
                count += 2;
                value >>= 2;
            }
            if ((value & 0x1) == 0) {
                count += 1;
            }
            return count;
        #endif // defined (__GNUC__) || defined (__clang__)
        }

    } // namespace util
} // namespace thekogans
