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

#include "thekogans/util/Exception.h"
#include "thekogans/util/NullAllocator.h"

namespace thekogans {
    namespace util {

        NullAllocator NullAllocator::Global;

        const char *NullAllocator::GetName () const {
            return "NullAllocator";
        }

        void *NullAllocator::Alloc (std::size_t size) {
            if (size > 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_ENOMEM);
            }
            return 0;
        }

        void NullAllocator::Free (
            void * /*ptr*/,
            std::size_t /*size*/) {
        }

    } // namespace util
} // namespace thekogans
