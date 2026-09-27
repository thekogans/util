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

    } // namespace util
} // namespace thekogans
