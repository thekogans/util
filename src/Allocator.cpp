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
#include "thekogans/util/SlabAllocator.h"
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
            // --- Configuration Knobs ---
            constexpr size_t MIN_ALIGN = sizeof (void *); // 8 Bytes
            constexpr size_t MIN_EXP   = 3;             // 2^3 = 8 Bytes
            constexpr size_t MAX_EXP   = 22;            // 2^22 = 4 MiB
            constexpr size_t NUM_POOLS = MAX_EXP - MIN_EXP + 1; // 20 Pools

            // The Tuning Knob: Defines the ideal memory footprint target for a pool's page
            // Small pools target standard OS pages (4KB), large pools scale up to avoid OS overhead.
            constexpr size_t get_target_page_footprint (size_t slot_size) {
                if (slot_size <= 128)      return 4 * 1024;       // 4 KiB target (~512 to 32 slots)
                if (slot_size <= 2048)     return 16 * 1024;      // 16 KiB target
                if (slot_size <= 65536)    return 256 * 1024;     // 256 KiB target
                if (slot_size <= 1048576)  return 4 * 1024 * 1024;// 4 MiB target (~4 slots)
                return 16 * 1024 * 1024;                          // 16 MiB target for >= 2MB slots (4 slots)
            }

            // --- Compile-Time Pool Configuration Generator ---
            template <size_t PoolIdx>
            struct PoolConfig {
                static constexpr size_t SLOT_SIZE = 1ULL << (PoolIdx + MIN_EXP);
                static constexpr size_t TARGET_FOOTPRINT = get_target_page_footprint (SLOT_SIZE);
                // Calculate how many slots fit. Ensure at least 2 slots per page for large objects
                static constexpr size_t SLOT_COUNT = std::max (size_t (2), TARGET_FOOTPRINT / SLOT_SIZE);
                // The exact allocation size needed from the OS for this page type
                static constexpr size_t PAGE_SIZE = SLOT_COUNT * SLOT_SIZE;
            };

            // Base interface so the master allocator can hold an array of different pools uniformly
            struct IPool {
                virtual ~IPool () = default;
                virtual void *Alloc () = 0;
                virtual void Free (void *ptr) = 0;
            };

            template <size_t PoolIdx>
            class StaticPool : public IPool {
                using Config = PoolConfig<PoolIdx>;
                using SlotType = std::byte[Config::SLOT_SIZE];
                using AllocatorType = ScopedSlabAllocator<
                    SlotType,
                    Policy::IsSingleton<true>,
                    Policy::SlotsPerPage<Config::SLOT_COUNT * 100>,
                    Policy::DeriveTLCThreshold<Config::SLOT_COUNT * 40>>;
                AllocatorType allocator;

            public:
                void *Alloc () override {
                    return allocator.Alloc ();
                }

                void Free (void *ptr) override {
                    allocator.Free (ptr);
                }
            };

            class MasterAllocator : public Singleton<MasterAllocator> {
            private:
                StaticPool<0> pool0;
                StaticPool<1> pool1;
                StaticPool<2> pool2;
                StaticPool<3> pool3;
                StaticPool<4> pool4;
                StaticPool<5> pool5;
                StaticPool<6> pool6;
                StaticPool<7> pool7;
                StaticPool<8> pool8;
                StaticPool<9> pool9;
                StaticPool<10> pool10;
                StaticPool<11> pool11;
                StaticPool<12> pool12;
                StaticPool<13> pool13;
                StaticPool<14> pool14;
                StaticPool<15> pool15;
                StaticPool<16> pool16;
                StaticPool<17> pool17;
                StaticPool<18> pool18;
                StaticPool<19> pool19;
                IPool *pools[NUM_POOLS] = {
                    &pool0, &pool1, &pool2, &pool3, &pool4, &pool5, &pool6, &pool7, &pool8, &pool9,
                    &pool10, &pool11, &pool12, &pool13, &pool14, &pool15, &pool16, &pool17, &pool18, &pool19
                };

            public:
                void *Alloc (size_t size) {
                    if (THEKOGANS_UTIL_UNLIKELY (size > (1ULL << MAX_EXP))) {
                        return std::malloc (size);
                    }
                    if (THEKOGANS_UTIL_UNLIKELY (size < MIN_ALIGN)) {
                        size = MIN_ALIGN;
                    }
                    return pools[TrailingZeroBitCount (Align (size)) - MIN_EXP]->Alloc ();
                }
                void Free (
                        void *ptr,
                        size_t size) {
                    if (THEKOGANS_UTIL_UNLIKELY (size > (1ULL << MAX_EXP))) {
                        std::free (ptr);
                    }
                    if (THEKOGANS_UTIL_UNLIKELY (size < MIN_ALIGN)) {
                        size = MIN_ALIGN;
                    }
                    return pools[TrailingZeroBitCount (Align (size)) - MIN_EXP]->Free (ptr);
                }
            };
        }

        void *thekogans_malloc (std::size_t size) {
            static MasterAllocator &allocator = *MasterAllocator::Instance ();
            return allocator.Alloc (size);
        }

        void thekogans_free (
                void *ptr,
                std::size_t size) {
            static MasterAllocator &allocator = *MasterAllocator::Instance ();
            allocator.Free (ptr, size);
        }

    } // namespace util
} // namespace thekogans
