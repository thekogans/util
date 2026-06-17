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

#include "thekogans/util/Heap.h"
#include "thekogans/util/PageMap.h"

namespace thekogans {
    namespace util {

#if 0
        DWORD GetPhysicalSectorSize (HANDLE hFile) {
            STORAGE_PROPERTY_QUERY query = {
                StorageAccessAlignmentProperty,
                PropertyStandardQuery
            };
            STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR alignment = {0};
            DWORD bytesReturned = 0;
            return DeviceIoControl (
                hFile,
                IOCTL_STORAGE_QUERY_PROPERTY,
                &query, sizeof (query),
                &alignment, sizeof (alignment),
                &bytesReturned,
                NULL) ? alignment.BytesPerPhysicalSector : 0;
        }

        unsigned int GetPhysicalSectorSize (int fd) {
            unsigned int physical_sector_size = 0;
            // BLKPBSZGET returns the physical sector size in bytes
            return ioctl (fd, BLKPBSZGET, &physical_sector_size) == 0 ? physical_sector_size : 0;
        }
#endif

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS_T (PageMap32::Page)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS_T (PageMap64::Page)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS_T (PageMap128::Page)

    } // namespace util
} // namespace thekogans
