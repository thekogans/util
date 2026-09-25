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
#else // defined (TOOLCHAIN_OS_Windows)
    #include <stdint.h>
    #include <sys/mman.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/SlabAllocatorDetail.h"

namespace thekogans {
    namespace util {

        namespace detail {
            void *DefaultPageAllocator::Alloc (std::size_t pageSize) noexcept {
            #if defined (TOOLCHAIN_OS_Windows)
                // Define the explicit power-of-2 alignment requirement
                MEM_ADDRESS_REQUIREMENTS requirements = {0};
                requirements.Alignment = pageSize; // Guarantees the kernel does the math!
                MEM_EXTENDED_PARAMETER param = {0};
                param.Type = MemExtendedParameterAddressRequirements;
                param.Pointer = &requirements;
                // Direct, perfectly aligned, zero-filled OS page allocation
                return ::VirtualAlloc2 (
                    nullptr,             // Current process
                    nullptr,             // Let OS choose address
                    pageSize,            // Allocation size
                    MEM_RESERVE | MEM_COMMIT,
                    PAGE_READWRITE,
                    &param,
                    1);                  // Number of extended parameters
            #else // defined (TOOLCHAIN_OS_Windows)
                // 1. Allocate a virtual memory sector that is double the needed size.
                // This mathematically guarantees a valid pageSize boundary exists inside it.
                std::size_t totalMapping = pageSize * 2;
                void *rawPtr = ::mmap (
                    nullptr, totalMapping, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (rawPtr == MAP_FAILED) {
                    return nullptr;
                }
                uintptr_t rawAddress = reinterpret_cast<uintptr_t> (rawPtr);
                // 2. Round UP the address to the nearest exact pageSize boundary.
                // Since pageSize is a power of 2, this is a blisteringly fast bitwise op.
                uintptr_t alignedAddress = (rawAddress + pageSize - 1) & ~(pageSize - 1);
                // 3. Calculate how many bytes were wasted at the beginning and the end.
                std::size_t leadWaste = alignedAddress - rawAddress;
                std::size_t trailWaste = totalMapping - pageSize - leadWaste;
                // 4. Return the unneeded "ears" of the mapping back to the OS kernel.
                // This leaves only a perfectly aligned, zero-filled pageSize block intact!
                if (leadWaste > 0) {
                    ::munmap (rawPtr, leadWaste);
                }
                if (trailWaste > 0) {
                    ::munmap (reinterpret_cast<void *> (alignedAddress + pageSize), trailWaste);
                }
                return reinterpret_cast<void *> (alignedAddress);
            #endif // defined (TOOLCHAIN_OS_Windows)
            }

            void DefaultPageAllocator::Free (
                    void *ptr,
                    std::size_t pageSize) noexcept {
                if (ptr != nullptr) {
                #if defined (TOOLCHAIN_OS_Windows)
                    // Windows mandates that MEM_RELEASE passes 0 for size.
                    // It cleanly unmaps the entire virtual reservation matching the base ptr.
                    ::VirtualFree (ptr, 0, MEM_RELEASE);
                #else // defined (TOOLCHAIN_OS_Windows)
                    // For POSIX (Linux/macOS), the kernel tracks the trimmed page area.
                    // We pass the exact aligned block address and its size to vaporize it from the page tables.
                    ::munmap (ptr, pageSize);
                #endif // defined (TOOLCHAIN_OS_Windows)
                }
            }

            void *StdPageAllocator::Alloc (std::size_t pageSize) noexcept {
                void *ptr = ::operator new (pageSize, std::align_val_t{pageSize});
                SecureZeroMemory (ptr, pageSize);
                return ptr;
            }

            void StdPageAllocator::Free (
                    void *ptr,
                    std::size_t pageSize) noexcept {
                SecureZeroMemory (ptr, pageSize);
                ::operator delete (ptr, std::align_val_t{pageSize});
            }
        } // namespace detail

    } // namespace util
} // namespace thekogans
