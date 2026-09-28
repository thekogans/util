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

#if !defined (__thekogans_util_SlabAllocator_h)
#define __thekogans_util_SlabAllocator_h

#include <cstddef>
#include <new>
#include <utility>
#include "thekogans/util/Config.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/NullLock.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SlabAllocatorDetail.h"

namespace thekogans {
    namespace util {

        // The Scoped / Local version. Will clean up after itself.
        template <typename T, typename... Policies>
        using ScopedSlabAllocator = detail::SlabAllocator<T, Policies...>;

        // The Global Singleton version. Will NOT clean up after itself.
        template <typename T, typename... Policies>
        using GlobalSlabAllocator = Singleton<
            detail::SlabAllocator<T, Policy::IsSingleton<true>, Policies...>,
            typename detail::GetTypePolicy<Policy::Lock, SpinLock, Policies...>::type,
            typename detail::GetInstanceCreatorPolicy<T, Policies...>::type,
            NullInstanceDestroyer<detail::SlabAllocator<T, Policy::IsSingleton<true>, Policies...>>>;

        // Scoped / Single Threaded version. Will clean up after itself.
        template <typename T, typename... Policies>
        using PersonalScopedSlabAllocator =
            ScopedSlabAllocator<T, Policy::TLCThreshold<0>, Policy::Lock<NullLock>, Policies...>;

        // The Global / Single Threaded version. Will NOT clean up after itself.
        template <typename T, typename... Policies>
        using PersonalGlobalSlabAllocator =
            GlobalSlabAllocator<T, Policy::TLCThreshold<0>, Policy::Lock<NullLock>, Policies...>;

        #define THEKOGANS_UTIL_IMPLEMENT_GLOBAL_SLAB_ALLOCATOR_FUNCTIONS(_T, ...)\
        public:\
            static void *operator new (std::size_t /*size*/) {\
                return thekogans::util::GlobalSlabAllocator<_T, ##__VA_ARGS__>::Instance ()->Alloc ();\
            }\
            static void *operator new (\
                    std::size_t /*size*/,\
                    const std::nothrow_t &) noexcept {\
                try {\
                    return thekogans::util::GlobalSlabAllocator<_T, ##__VA_ARGS__>::Instance ()->Alloc ();\
                }\
                catch (...) {\
                    return nullptr;\
                }\
            }\
            static void *operator new (\
                    std::size_t size,\
                    void *ptr) noexcept {\
                return ::operator new (size, ptr);\
            }\
            static void operator delete (void *ptr) noexcept {\
                thekogans::util::GlobalSlabAllocator<_T, ##__VA_ARGS__>::Instance ()->Free (ptr);\
            }\
            static void operator delete (\
                    void *ptr,\
                    const std::nothrow_t &) noexcept {\
                thekogans::util::GlobalSlabAllocator<_T, ##__VA_ARGS__>::Instance ()->Free (ptr);\
            }\
            static void operator delete (\
                    void *ptr,\
                    void *target) noexcept {\
                ::operator delete (ptr, target);\
            }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SlabAllocator_h)
