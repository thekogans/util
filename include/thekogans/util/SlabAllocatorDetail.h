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

#if !defined (__thekogans_util_SlabAllocatorDetail_h)
#define __thekogans_util_SlabAllocatorDetail_h

#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        namespace detail {
            /// \brief
            /// Default slots per page. A tuning knob meant to limit page
            /// allocations for a heavily allocated type.
            static constexpr std::size_t DEFAULT_SLOTS_PER_PAGE = 256;
            /// \brief
            /// Default Thread Local Cache (TLC) size.
            /// A tuning knob meant to limit lock contention on a heavily contested type.
            static constexpr std::size_t DEFAULT_TLC_THRESHOLD = 32;
            /// \brief
            /// Default cache line size.
            /// VERY IMPORTANT: This is a very critical parameter. On modern megacore
            /// architectures (M5) cache eviction is perhaps the single biggest drain
            /// on application performance. This is why it's exposed as a template parameter.
            /// If, for some reason, the compiler can't get it right, you have the power to
            /// force it in to a particular value. It goes without saying that this value
            /// must be a power of 2.
            static constexpr std::size_t DEFAULT_CACHE_LINE_SIZE = SYSTEM_CACHE_LINE_SIZE;

            /// \struct DefaultPageAllocator SlabAllocator.h thekogans/util/SlabAllocator.h
            ///
            /// \brief
            /// The default page allocator. Yet another tuning knob to allow you to
            /// control how pages are allocated. This one uses direct OS services to
            /// return aligned and clean (0 filled) pages.
            struct _LIB_THEKOGANS_UTIL_DECL DefaultPageAllocator {
                /// \brief
                /// Allocate a pageSize aligned and 0 filled page.
                /// \param[in] pageSize Page size and alignement.
                /// \return pageSize aligned and 0 filled page.
                static void *Alloc (std::size_t pageSize) noexcept;
                /// \brief
                /// Free a previously Alloc'ed page.
                /// \param[in] ptr Page pointer returned by Alloc above.
                /// \param[in] pageSize Same pageSize you passed to Alloc.
                static void Free (
                    void *ptr,
                    std::size_t pageSize) noexcept;
            };

            /// \struct StdPageAllocator SlabAllocator.h thekogans/util/SlabAllocator.h
            ///
            /// \brief
            /// If the DefaultPageAllocator proves to be a bottleneck on your system,
            /// use StdPageAllocator. It uses operator new and delete and zero fills
            /// the buffers they return and release.
            struct _LIB_THEKOGANS_UTIL_DECL StdPageAllocator {
                /// \brief
                /// Allocate a pageSize aligned and 0 filled page.
                /// \param[in] pageSize Page size and alignement.
                /// \return pageSize aligned and 0 filled page.
                static void *Alloc (std::size_t pageSize) noexcept;
                /// \brief
                /// Free a previously Alloc'ed page.
                /// \param[in] ptr Page pointer returned by Alloc above.
                /// \param[in] pageSize Same pageSize you passed to Alloc.
                static void Free (
                    void *ptr,
                    std::size_t pageSize) noexcept;
            };
        } // namespace detail

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SlabAllocatorDetail_h)
