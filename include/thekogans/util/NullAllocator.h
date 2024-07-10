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

#if !defined (__thekogans_util_NullAllocator_h)
#define __thekogans_util_NullAllocator_h

#include <cstddef>
#include <memory>
#include "thekogans/util/Config.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        /// \struct NullAllocator NullAllocator.h thekogans/util/NullAllocator.h
        ///
        /// \brief
        /// NullAllocator is a noop allocator. It's designed to be used in
        /// situations where an allocator instance is required but is not
        /// used. Specifically it's very useful with \see{TenantReadBuffer}
        /// and \see{TenantWriteBuffer} instances created from raw buffers.

        struct _LIB_THEKOGANS_UTIL_DECL NullAllocator :
                public Allocator,
                public Singleton<
                    NullAllocator,
                    SpinLock,
                    RefCountedInstanceCreator<NullAllocator>,
                    RefCountedInstanceDestroyer<NullAllocator>> {
            /// \brief
            /// NullAllocator participates in the \see{DynamicCreatable}
            /// dynamic discovery and creation.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (NullAllocator)

            /// \brief
            /// Allocate a block.
            /// \param[in] size Size of block to allocate.
            /// \return 0.
            virtual void *Alloc (std::size_t size);
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// \param[in] ptr Pointer to the block returned by Alloc.
            /// \param[in] size Same size parameter previously passed in to Alloc.
            virtual void Free (
                void * /*ptr*/,
                std::size_t /*size*/);
        };

        /// \def THEKOGANS_UTIL_IMPLEMENT_NULL_ALLOCATOR_FUNCTIONS(_T)
        /// Macro to implement NullAllocator functions.
        #define THEKOGANS_UTIL_IMPLEMENT_NULL_ALLOCATOR_FUNCTIONS(_T)\
        void *_T::operator new (std::size_t size) {\
            assert (size == sizeof (_T));\
            return thekogans::util::NullAllocator::Instance ().Alloc (size);\
        }\
        void *_T::operator new (\
                std::size_t size,\
                std::nothrow_t) throw () {\
            assert (size == sizeof (_T));\
            return thekogans::util::NullAllocator::Instance ().Alloc (size);\
        }\
        void *_T::operator new (\
                std::size_t size,\
                void *ptr) {\
            assert (size == sizeof (_T));\
            return ptr;\
        }\
        void _T::operator delete (void *ptr) {\
            thekogans::util::NullAllocator::Instance ().Free (ptr, sizeof (_T));\
        }\
        void _T::operator delete (\
                void *ptr,\
                std::nothrow_t) throw () {\
            thekogans::util::NullAllocator::Instance ().Free (ptr, sizeof (_T));\
        }\
        void _T::operator delete (\
            void *,\
            void *) {}

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_NullAllocator_h)
