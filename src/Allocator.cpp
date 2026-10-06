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

#include <cstddef>
#include <cstdint>
#include <bit>
#include <algorithm>
#include "thekogans/util/Environment.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/Allocator.h"
#if defined (THEKOGANS_UTIL_TYPE_Static)
    #include "thekogans/util/DefaultAllocator.h"
    #include "thekogans/util/SecureAllocator.h"
    #if defined (TOOLCHAIN_OS_Windows)
        #include "thekogans/util/os/windows/HGLOBALAllocator.h"
        #include "thekogans/util/os/windows/HeapAllocator.h"
    #endif // defined (TOOLCHAIN_OS_Windows)
    //#include "thekogans/util/AlignedAllocator.h"
    #include "thekogans/util/SharedAllocator.h"
    #include "thekogans/util/NullAllocator.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (thekogans::util::Allocator)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void Allocator::StaticInit () {
            DefaultAllocator::StaticInit ();
            SecureAllocator::StaticInit ();
        #if defined (TOOLCHAIN_OS_Windows)
            os::windows::HGLOBALAllocator::StaticInit ();
            os::windows::HeapAllocator::StaticInit ();
        #endif // defined (TOOLCHAIN_OS_Windows)
            //AlignedAllocator::StaticInit ();
            GlobalSharedAllocator::StaticInit ();
            NullAllocator::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        std::string Allocator::GetSerializedType () const {
            const char *type = Type ();
            if (CreateType (type) == nullptr) {
                type = DefaultAllocator::TYPE;
            }
            return type;
        }

        MasterAllocator::MasterAllocator (
                std::size_t minExp_,
                std::size_t maxExp_,
                const std::pair<std::size_t, std::size_t> poolConfig[],
                std::size_t sizeCap_) :
                minExp (minExp_),
                maxExp (maxExp_),
                numPools (maxExp - minExp),
                minSize (1ULL << minExp),
                maxSize (1ULL << maxExp),
                sizeCap (sizeCap_) {
            // Validate input.
            if (minExp < DEFAULT_MIN_EXP || minExp > MAX_POOLS || minExp >= maxExp ||
                    maxExp > MAX_POOLS || numPools > MAX_POOLS || sizeCap < maxSize) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
            for (std::size_t i = 0, slotSize = minSize; i < numPools; ++i) {
                pools[i] = new SlabAllocatorImpl (slotSize, poolConfig[i].first, poolConfig[i].second);
                slotSize <<= 1;
            }
        }

        void *MasterAllocator::Alloc (size_t size) {
            if (THEKOGANS_UTIL_UNLIKELY (size == 0 || size > maxSize)) {
                if (THEKOGANS_UTIL_UNLIKELY (size == 0 || size > sizeCap)) {
                    return nullptr;
                }
                return ::operator new (size);
            }
            if (THEKOGANS_UTIL_UNLIKELY (size < minSize)) {
                size = minSize;
            }
            return pools[TrailingZeroBitCount (Align (size)) - minExp]->Alloc ();
        }

        void MasterAllocator::Free (
                void *ptr,
                std::size_t size) {
            if (THEKOGANS_UTIL_UNLIKELY (ptr == nullptr)) {
                return;
            }
            if (THEKOGANS_UTIL_UNLIKELY (size > maxSize)) {
                if (THEKOGANS_UTIL_LIKELY (size <= sizeCap)) {
                    ::operator delete (ptr);
                }
                return;
            }
            if (THEKOGANS_UTIL_UNLIKELY (size < minSize)) {
                size = minSize;
            }
            pools[TrailingZeroBitCount (Align (size)) - minExp]->Free (ptr);
        }

        namespace {
            static MasterAllocator *allocator = MasterAllocator::Instance ();
        }

        void *thekogans_malloc (std::size_t size) {
            return allocator->Alloc (size);
        }

        void thekogans_free (
                void *ptr,
                std::size_t size) {
            allocator->Free (ptr, size);
        }

    } // namespace util
} // namespace thekogans
