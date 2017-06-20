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

#include <memory>
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct Allocator Allocator.h thekogans/util/Allocator.h
        ///
        /// \brief
        /// Allocator is a base class for all allocators. It defines
        /// the interface and lets concrete classes handle implementation
        /// details.

        struct _LIB_THEKOGANS_UTIL_DECL Allocator {
            /// \brief
            /// dtor.
            virtual ~Allocator () {}

            /// \brief
            /// Allocate a block.
            /// NOTE: Allocator policy is to return (void *)0 if size == 0.
            /// if size > 0 and an error occurs, Allocator will throw an exception.
            /// \param[in] size Size of block to allocate.
            /// \return Pointer to the allocated block ((void *)0 if size == 0).
            virtual void *Alloc (std::size_t size) = 0;
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// NOTE: Allocator policy is to do nothing if ptr == 0.
            /// \param[in] ptr Pointer to the block returned by Alloc.
            /// \param[in] size Same size parameter previously passed in to Alloc.
            virtual void Free (
                void *ptr,
                std::size_t size) = 0;
        };

        /// \def THEKOGANS_UTIL_DECLARE_ALLOCATOR_FUNCTIONS
        /// Macro to declare allocator functions.
        #define THEKOGANS_UTIL_DECLARE_ALLOCATOR_FUNCTIONS\
        public:\
            static void *operator new (std::size_t size);\
            static void *operator new (std::size_t size, std::nothrow_t) throw ();\
            static void *operator new (std::size_t, void *ptr);\
            static void operator delete (void *ptr);\
            static void operator delete (void *ptr, std::nothrow_t) throw ();\
            static void operator delete (void *, void *);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Allocator_h)
