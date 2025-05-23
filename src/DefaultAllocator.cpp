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

#include <memory>
#include "thekogans/util/Exception.h"
#include "thekogans/util/DefaultAllocator.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_S (
            thekogans::util::DefaultAllocator,
            Allocator::TYPE)

        void *DefaultAllocator::Alloc (std::size_t size) {
            return allocator->Alloc (size);
        }

        void DefaultAllocator::Free (
                void *ptr,
                std::size_t size) {
            allocator->Free (ptr, size);
        }

    } // namespace util
} // namespace thekogans
