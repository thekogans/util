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

#include "thekogans/util/SlabAllocatorDetail.h"

namespace thekogans {
    namespace util {

        // 1. The Scoped / Local version. Will clean up after itself.
        template <typename T, typename... Policies>
        using ScopedSlabAllocator = detail::SlabAllocator<T, Policies...>;

        // 2. The Global Singleton version. Will NOT clean up after itself.
        template <typename T, typename... Policies>
        using GlobalSlabAllocator = Singleton<
            detail::SlabAllocator<T, Policy::IsSingleton<true>, Policies...>,
            typename detail::GetTypePolicy<Policy::Lock, SpinLock, Policies...>::type,
            typename detail::GetInstanceCreatorPolicy<T, Policies...>::type,
            NullInstanceDestroyer<detail::SlabAllocator<T, Policy::IsSingleton<true>, Policies...>>>;

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SlabAllocator_h)
