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

#if !defined (__thekogans_util_Allocator_h)
#define __thekogans_util_Allocator_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/DynamicCreatable.h"

namespace thekogans {
    namespace util {

        /// \struct Allocator Allocator.h thekogans/util/Allocator.h
        ///
        /// \brief
        /// Allocator is a base class for all allocators. It defines
        /// the interface and lets concrete classes handle implementation
        /// details.

        struct _LIB_THEKOGANS_UTIL_DECL Allocator : public DynamicCreatable {
            /// \brief
            /// Declare \see{DynamicCreatable} boilerplate.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE (Allocator)

        #if defined (THEKOGANS_UTIL_TYPE_Static)
            /// \brief
            /// Because Allocator uses dynamic initialization, when using
            /// it in static builds call this method to have the Allocator
            /// explicitly include all internal allocator types. Without
            /// calling this api, the only allocators that will be available
            /// to your application are the ones you explicitly link to.
            static void StaticInit ();
        #endif // defined (THEKOGANS_UTIL_TYPE_Static)

            /// \brief
            /// Return a serializable allocator type (one that can be dynamically creatable).
            /// \return A serializable allocator type (one that can be dynamically creatable).
            std::string GetSerializedType () const;

            /// \brief
            /// Allocate a block.
            /// NOTE: Allocator policy is to return nullptr if size == 0.
            /// if size > 0 and an error occurs, Allocator will throw an exception.
            /// \param[in] size Size of block to allocate.
            /// \return Pointer to the allocated block (nullptr if size == 0).
            virtual void *Alloc (std::size_t size) = 0;
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// NOTE: Allocator policy is to do nothing if ptr == nullptr.
            /// \param[in] ptr Pointer to the block returned by Alloc.
            /// \param[in] size Same size parameter previously passed in to Alloc.
            virtual void Free (
                void *ptr,
                std::size_t size) = 0;
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Allocator_h)
