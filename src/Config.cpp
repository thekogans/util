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

#include "thekogans/util/Config.h"
#if defined (THEKOGANS_UTIL_TYPE_Static)
    #include "thekogans/util/DynamicCreatable.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

namespace thekogans {
    namespace util {

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void StaticInit () {
            DynamicCreatable::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API SecureZeroMemory (
                volatile void *data,
                std::size_t size,
                char zero) {
            if (data != nullptr && size > 0) {
                using memset_t = void *(*)(void *, int, std::size_t);
                static volatile memset_t volatile_memset = &std::memset;
                volatile_memset (const_cast<void *> (data), static_cast<unsigned char> (zero), size);
                return size;
            }
            return 0;
        }

    } // namespace util
} // namespace thekogans
