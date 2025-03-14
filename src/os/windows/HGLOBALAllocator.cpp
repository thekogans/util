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

#include "thekogans/util/Environment.h"

#if defined (TOOLCHAIN_OS_Windows)

#include "thekogans/util/os/windows/WindowsHeader.h"
#include <memory>
#include "thekogans/util/Exception.h"
#include "thekogans/util/os/windows/HGLOBALAllocator.h"

namespace thekogans {
    namespace util {
        namespace os {
            namespace windows {

                THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_S (
                    thekogans::util::os::windows::HGLOBALAllocator,
                    Allocator::TYPE)

                void *HGLOBALAllocator::Alloc (std::size_t size) {
                    return Alloc (GMEM_FIXED,  size);
                }

                void HGLOBALAllocator::Free (
                        void *ptr,
                        std::size_t /*size*/) {
                    if (ptr != nullptr) {
                        GlobalFree (ptr);
                    }
                }

                void *HGLOBALAllocator::Alloc (
                        ui32 flags,
                        std::size_t size) {
                    void *ptr = nullptr;
                    if (size > 0) {
                        ptr = GlobalAlloc (flags,  size);
                        if (ptr == nullptr) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE);
                        }
                    }
                    return ptr;
                }

            } // namespace windows
        } // namespace os
    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Windows)
