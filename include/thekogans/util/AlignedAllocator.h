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

#if !defined (__thekogans_util_AlignedAllocator_h)
#define __thekogans_util_AlignedAllocator_h

#include <cstddef>
#include <cassert>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Allocator.h"

namespace thekogans {
    namespace util {

        /// \struct AlignedAllocator AlignedAllocator.h thekogans/util/AlignedAllocator.h
        ///
        /// \brief
        /// An adaptor class used to align a block allocated by another allocator.
        /// Take a look at \see{Heap} to see an example of it's usage.

        struct _LIB_THEKOGANS_UTIL_DECL AlignedAllocator : public Allocator {
        private:
            /// \struct AlignedAllocator::Footer AlignedAllocator.h thekogans/util/AlignedAllocator.h
            ///
            /// \brief
            /// Footer at the end of the block that holds the block info.
            struct Footer {
            #if defined (THEKOGANS_UTIL_CONFIG_Debug)
                /// \brief
                /// A watermark.
                std::size_t magic;
            #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
                /// \brief
                /// Pointer to the beginning of the allocated block (unaligned).
                void *ptr;
                /// \brief
                /// Size of the block.
                std::size_t size;
                /// \brief
                /// ctor.
                /// \param[in] ptr_ Pointer to the start of allocated block.
                /// \param[in] size_ Size of block.
                Footer (
                    void *ptr_,
                    std::size_t size_) :
                #if defined (THEKOGANS_UTIL_CONFIG_Debug)
                    magic (MAGIC),
                #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
                    ptr (ptr_),
                    size (size_) {}
                /// \brief
                /// dtor.
                ~Footer () {
                #if defined (THEKOGANS_UTIL_CONFIG_Debug)
                    assert (magic == MAGIC);
                #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
                }
            };

            /// \brief
            /// AlignedAllocator is an adaptor. It will use this allocator
            /// for actual allocations and will align the resulting block.
            Allocator &allocator;
            /// \brief
            /// Alignment boundary (power of 2).
            std::size_t alignment;

        public:
            /// \brief
            /// ctor.
            /// \param[in] allocator_ Allocator to use for actual allocation.
            /// \param[in] alignment_ Alignment boundary (power of 2).
            AlignedAllocator (
                Allocator &allocator_,
                std::size_t alignment_);

            /// \brief
            /// Return allocator name.
            /// \return Allocator name.
            virtual const char *GetName () const {
                return "AlignedAllocator";
            }

            /// \brief
            /// Use Allocator to allocate a block, and align it to the
            /// requested boundary.
            /// \param[in] size Block size to allocate.\n
            /// \return Pointer to the aligned block.
            virtual void *Alloc (std::size_t size) {
                return AllocHelper (size, false);
            }
            /// \brief
            /// Free the block previously allocated with Alloc.
            /// \param[in] ptr Block pointer returned by Alloc
            /// \param[in] size 'true' size returned by Alloc
            virtual void Free (
                void *ptr,
                std::size_t size);

            /// \brief
            /// Use Allocator to allocate a block, and align it to the
            /// requested boundary.
            /// \param[in,out] size in = minimum block size to allocate.\n
            ///                     out = 'true' block size after alignment
            ///                           (at least minimum).
            /// \return Pointer to the aligned block.
            inline void *AllocMax (std::size_t &size) {
                return AllocHelper (size, true);
            }

        private:
            /// \brief
            /// Main allocation function used by Alloc and AllocMax.
            /// \param[in,out] size in = minimum block size to allocate.\n
            ///                     out = 'true' block size after alignment
            ///                           (at least minimum).
            /// \param[in] useMax false = Called by Alloc. true = Called by AllocMax.
            /// \return Pointer to the aligned block.
            void *AllocHelper (
                std::size_t &size,
                bool useMax);

            /// \brief
            /// AlignedAllocator is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (AlignedAllocator)
        };

        /// \brief
        /// Return the count of '0' bits in value.
        /// \param[in] value Value to examine.
        /// \return Number of 0 bits in value.
        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API ZeroBitCount (
            std::size_t value);
        /// \brief
        /// Return the count of '1' bits in value.
        /// \param[in] value Value to examine.
        /// \return Number of 1 bits in value.
        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API OneBitCount (
            std::size_t value);
        /// \brief
        /// Align value to the next power of 2.
        /// If value is already aligned, leave it alone.
        /// \param[in] value Value to align.
        /// \return Value aligned to power of 2 boundary.
        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API Align (
            std::size_t value);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_AlignedAllocator_h)
