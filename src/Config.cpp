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

#if defined (THEKOGANS_UTIL_TYPE_Static)
    #include "thekogans/util/DynamicCreatable.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/ConsoleLogger.h"
#include "thekogans/util/Console.h"

namespace thekogans {
    namespace util {

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void StaticInit () {
            DynamicCreatable::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API SecureZeroMemory (
                volatile void *data,
                std::size_t size) {
            if (data != nullptr && size > 0) {
                // Cast to a volatile character pointer so every byte write is legally un-optimizable.
                volatile char *p = static_cast<volatile char *>(const_cast<void *>(data));
                std::size_t count = size;
                while (count--) {
                    *p++ = 0;
                }
                return size;
            }
            return 0;
        }

    } // namespace util
} // namespace thekogans
