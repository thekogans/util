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

#if !defined (__thekogans_util_DefaultAllocator_h)
#define __thekogans_util_DefaultAllocator_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Allocator.h"

namespace thekogans {
    namespace util {

        /// \struct DefaultAllocator DefaultAllocator.h thekogans/util/DefaultAllocator.h
        ///
        /// \brief
        /// Uses system new/delete to allocate from the global
        /// heap. DefaultAllocator is part of the \se{Allocate}
        /// framework.

        struct _LIB_THEKOGANS_UTIL_DECL DefaultAllocator : public Allocator {
            /// \brief
            /// Global DefaultAllocator. Used by default in \see{Heap} and \see{Buffer}.
            static DefaultAllocator Global;

            /// \brief
            /// Allocate a block from system heap.
            /// \param[in] size Size of block to allocate.
            /// \return Pointer to the allocated block (0 if out of memory).
            virtual void *Alloc (std::size_t size);
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// \param[in] ptr Pointer to the block returned by Alloc.
            /// \param[in] size Same size parameter previously passed in to Alloc.
            virtual void Free (
                void *ptr,
                std::size_t /*size*/);
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_DefaultAllocator_h)
