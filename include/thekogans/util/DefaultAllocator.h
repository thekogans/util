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

#include <cstddef>
#include "thekogans/util/Config.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        /// \struct DefaultAllocator DefaultAllocator.h thekogans/util/DefaultAllocator.h
        ///
        /// \brief
        /// Uses system new/delete to allocate from the global heap.
        /// DefaultAllocator is part of the \see{Allocator} framework.

        struct _LIB_THEKOGANS_UTIL_DECL DefaultAllocator :
                public Allocator,
                public RefCountedSingleton<DefaultAllocator> {
            /// \brief
            /// DefaultAllocator participates in the \see{DynamicCreatable}
            /// dynamic discovery and creation.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (DefaultAllocator)

            /// \brief
            /// Allocate a block from system heap.
            /// \param[in] size Size of block to allocate.
            /// \return Pointer to the allocated block (0 if out of memory).
            virtual void *Alloc (std::size_t size) override;
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// \param[in] ptr Pointer to the block returned by Alloc.
            /// \param[in] size Same size parameter previously passed in to Alloc.
            virtual void Free (
                void *ptr,
                std::size_t /*size*/) override;
        };

        /// \def THEKOGANS_UTIL_IMPLEMENT_DEFAULT_ALLOCATOR_FUNCTIONS(_T)
        /// Macro to implement DefaultAllocator functions.
        #define THEKOGANS_UTIL_IMPLEMENT_DEFAULT_ALLOCATOR_FUNCTIONS(_T)\
        void *_T::operator new (std::size_t size) {\
            assert (size == sizeof (_T));\
            return thekogans::util::DefaultAllocator::Instance ()->Alloc (size);\
        }\
        void *_T::operator new (\
                std::size_t size,\
                std::nothrow_t) noexcept {\
            assert (size == sizeof (_T));\
            return thekogans::util::DefaultAllocator::Instance ()->Alloc (size);\
        }\
        void *_T::operator new (\
                std::size_t size,\
                void *ptr) {\
            assert (size == sizeof (_T));\
            return ptr;\
        }\
        void _T::operator delete (void *ptr) {\
            thekogans::util::DefaultAllocator::Instance ()->Free (ptr, sizeof (_T));\
        }\
        void _T::operator delete (\
                void *ptr,\
                std::nothrow_t) noexcept {\
            thekogans::util::DefaultAllocator::Instance ()->Free (ptr, sizeof (_T));\
        }\
        void _T::operator delete (\
            void *,\
            void *) {}

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_DefaultAllocator_h)
