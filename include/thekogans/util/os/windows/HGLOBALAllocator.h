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

#if !defined (__thekogans_util_os_windows_HGLOBALAllocator_h)
#define __thekogans_util_os_windows_HGLOBALAllocator_h

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

                /// \struct HGLOBALAllocator HGLOBALAllocator.h thekogans/util/os/windows/HGLOBALAllocator.h
                ///
                /// \brief
                /// Uses Windows GlobalAlloc (GMEM_FIXED, ...)/GlobalFree to allocate from the global heap.
                /// HGLOBALAllocator is part of the \see{Allocator} framework.

                struct _LIB_THEKOGANS_UTIL_DECL HGLOBALAllocator :
                        public Allocator,
                        public Singleton<
                            HGLOBALAllocator,
                            SpinLock,
                            RefCountedInstanceCreator<HGLOBALAllocator>,
                            RefCountedInstanceDestroyer<HGLOBALAllocator>> {
                    /// \brief
                    /// HGLOBALAllocator participates in the \see{DynamicCreatable} dynamic
                    /// discovery and creation.
                    THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (HGLOBALAllocator)

                    /// \brief
                    /// Allocate a block from system heap (GMEM_FIXED).
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

                    /// \brief
                    /// Allocate a block from system heap.
                    /// \param[in] flags GlobalAlloc flags.
                    /// \param[in] size Size of block to allocate.
                    /// \return Pointer to the allocated block (0 if out of memory).
                    void *Alloc (
                        ui32 flags,
                        std::size_t size);
                };

                /// \def THEKOGANS_UTIL_IMPLEMENT_HGLOBAL_ALLOCATOR_FUNCTIONS(_T)
                /// Macro to implement HGLOBALAllocator functions.
                #define THEKOGANS_UTIL_IMPLEMENT_HGLOBAL_ALLOCATOR_FUNCTIONS(_T)\
                void *_T::operator new (std::size_t size) {\
                    assert (size == sizeof (_T));\
                    return thekogans::util::HGLOBALAllocator::Instance ().Alloc (size);\
                }\
                void *_T::operator new (\
                        std::size_t size,\
                        std::nothrow_t) throw () {\
                    assert (size == sizeof (_T));\
                    return thekogans::util::HGLOBALAllocator::Instance ().Alloc (size);\
                }\
                void *_T::operator new (\
                        std::size_t size,\
                        void *ptr) {\
                    assert (size == sizeof (_T));\
                    return ptr;\
                }\
                void _T::operator delete (void *ptr) {\
                    thekogans::util::HGLOBALAllocator::Instance ().Free (ptr, sizeof (_T));\
                }\
                void _T::operator delete (\
                        void *ptr,\
                        std::nothrow_t) throw () {\
                    thekogans::util::HGLOBALAllocator::Instance ().Free (ptr, sizeof (_T));\
                }\
                void _T::operator delete (\
                    void *,\
                    void *) {}

            } // namespace windows
        } // namespace os
    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Windows)

#endif // !defined (__thekogans_util_os_windows_HGLOBALAllocator_h)
