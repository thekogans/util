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
#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/SlabAllocatorImpl.h"
#include "thekogans/util/SlabAllocatorDetail.h"
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

        namespace {
            constexpr size_t MIN_EXP = 3; // 2^3
            constexpr size_t MAX_EXP = 22; // 2^22
            constexpr size_t NUM_POOLS = MAX_EXP - MIN_EXP + 1; // 20 Pools
            constexpr size_t MIN_SIZE = 1ULL << MIN_EXP; // 8 Bytes
            constexpr size_t MAX_SIZE = 1ULL << MAX_EXP; // 4 MiB
            constexpr size_t ABSOLUTE_POLICY_CAP = 4ULL * 1024ULL * 1024ULL * 1024ULL; // 4 GB

            struct PoolConfig {
                std::size_t slotsPerPage;
                std::size_t TLCThreshold;
            };

            constexpr PoolConfig POOL_CONFIG[NUM_POOLS] = {
                {512, 64},
                {512, 64},
                {512, 64},
                {512, 64},
                {512, 64}, // 8B - 128B
                {512, 64},
                {512, 64}, // 256B - 512B
                {256, 32},
                {128, 16},
                {64,  8},
                {32,  4},  // 1KiB - 8KiB
                {32,  4},
                {16,  2},
                {16,  2},  // 16KiB - 64KiB
                {8,   1},
                {8,   1},  // 128KiB - 256KiB
                {4,   0},
                {2,   0},
                {2,   0},
                {2,   0}   // 512KiB - 4MiB
            };

            class MasterAllocator : public Singleton<MasterAllocator> {
            private:
                SlabAllocatorImpl *pools[NUM_POOLS];

                inline std::size_t SizeToPoolIndex (std::size_t size) const noexcept {
                    // This executes size alignment ceiling check and pool index calculation
                    // simultaneously in exactly ONE native hardware 'CLZ' clock cycle!
                    unsigned long long leading_zeros = __builtin_clzll (size - 1);
                    return (63ULL - leading_zeros) - MIN_EXP;
                }

            public:
                MasterAllocator () {
                    for (std::size_t i = 0, slotSize = MIN_SIZE; i < NUM_POOLS; ++i) {
                        pools[i] = new SlabAllocatorImpl (
                            slotSize,
                            POOL_CONFIG[i].slotsPerPage,
                            POOL_CONFIG[i].TLCThreshold);
                        slotSize <<= 1;
                    }
                }

                void *Alloc (std::size_t size) {
                    if (THEKOGANS_UTIL_LIKELY (size > 0 && size <= MAX_SIZE)) {
                        if (THEKOGANS_UTIL_UNLIKELY (size < MIN_SIZE)) {
                            size = MIN_SIZE;
                        }
                        std::size_t idx = SizeToPoolIndex (size);
                        return pools[idx]->Alloc ();
                    }
                    return THEKOGANS_UTIL_LIKELY (size > 0 && size <= ABSOLUTE_POLICY_CAP) ?
                        ::operator new (size) : nullptr;
                }

                void Free (
                        void *ptr,
                        std::size_t size) {
                    if (THEKOGANS_UTIL_LIKELY (ptr != nullptr && size > 0)) {
                        if (THEKOGANS_UTIL_LIKELY (size <= MAX_SIZE)) {
                            if (THEKOGANS_UTIL_UNLIKELY (size < MIN_SIZE)) {
                                size = MIN_SIZE;
                            }
                            std::size_t idx = SizeToPoolIndex (size);
                            pools[idx]->Free (ptr);
                        }
                        else if (THEKOGANS_UTIL_LIKELY (size <= ABSOLUTE_POLICY_CAP)) {
                            ::operator delete (ptr, size);
                        }
                    }
                }
            };

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
