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

#if !defined (__thekogans_util_os_windows_HeapAllocator_h)
#define __thekogans_util_os_windows_HeapAllocator_h

#include "thekogans/util/Environment.h"

#if defined (TOOLCHAIN_OS_Windows)

#include <cstddef>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {
        namespace os {
            namespace windows {

                /// \struct HeapAllocator HeapAllocator.h thekogans/util/os/windows/HeapAllocator.h
                ///
                /// \brief
                /// Uses Windows HeapCreate/HeapAlloc/HeapFree to allocate from the application heap.
                /// HeapAllocator is part of the \see{Allocator} framework.

                struct _LIB_THEKOGANS_UTIL_DECL HeapAllocator :
                        public Allocator,
                        public RefCountedSingleton<HeapAllocator> {
                private:
                    HANDLE handle;

                public:
                    /// \brief
                    /// HeapAllocator participates in the \see{DynamicCreatable} dynamic
                    /// discovery and creation.
                    THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (HeapAllocator)

                    HeapAllocator () :
                            handle (HeapCreate (0, 0, 0)) {
                        if (handle == NULL) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE);
                        }
                    }
                    ~HeapAllocator () {
                        HeapDestroy (handle);
                    }


                    /// \brief
                    /// Allocate a block from system heap (GMEM_FIXED).
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

                /// \def THEKOGANS_UTIL_IMPLEMENT_Heap_ALLOCATOR_FUNCTIONS(_T)
                /// Macro to implement HGLOBALAllocator functions.
                #define THEKOGANS_UTIL_IMPLEMENT_Heap_ALLOCATOR_FUNCTIONS(_T)\
                void *_T::operator new (std::size_t size) {\
                    assert (size == sizeof (_T));\
                    return thekogans::util::HeapAllocator::Instance ()->Alloc (size);\
                }\
                void *_T::operator new (\
                        std::size_t size,\
                        std::nothrow_t) noexcept {\
                    assert (size == sizeof (_T));\
                    return thekogans::util::HeapAllocator::Instance ()->Alloc (size);\
                }\
                void *_T::operator new (\
                        std::size_t size,\
                        void *ptr) {\
                    assert (size == sizeof (_T));\
                    return ptr;\
                }\
                void _T::operator delete (void *ptr) {\
                    thekogans::util::HeapAllocator::Instance ()->Free (ptr, sizeof (_T));\
                }\
                void _T::operator delete (\
                        void *ptr,\
                        std::nothrow_t) noexcept {\
                    thekogans::util::HeapAllocator::Instance ()->Free (ptr, sizeof (_T));\
                }\
                void _T::operator delete (\
                    void *,\
                    void *) {}

            } // namespace windows
        } // namespace os
    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Windows)

#endif // !defined (__thekogans_util_os_windows_HeapAllocator_h)
