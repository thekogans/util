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
    #include "thekogans/util/File.h"
    #include "thekogans/util/BufferedFile.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)
#include "thekogans/util/RandomSeekSerializer.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (
            thekogans::util::RandomSeekSerializer)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void RandomSeekSerializer::StaticInit () {
            File::StaticInit ();
            BufferedFile::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

    } // namespace util
} // namespace thekogans
