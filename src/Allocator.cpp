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

#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/Allocator.h"
#if defined (THEKOGANS_UTIL_TYPE_Static)
    #include "thekogans/util/Environment.h"
    #include "thekogans/util/DefaultAllocator.h"
    #include "thekogans/util/SecureAllocator.h"
    #if defined (TOOLCHAIN_OS_Windows)
        #include "thekogans/util/os/windows/HGLOBALAllocator.h"
    #endif // defined (TOOLCHAIN_OS_Windows)
    //#include "thekogans/util/AlignedAllocator.h"
    //#include "thekogans/util/SharedAllocator.h"
    #include "thekogans/util/NullAllocator.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE (Allocator)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void Allocator::StaticInit () {
            DefaultAllocator::StaticInit ();
            SecureAllocator::StaticInit ();
        #if defined (TOOLCHAIN_OS_Windows)
            os::windows::HGLOBALAllocator::StaticInit ();
        #endif // defined (TOOLCHAIN_OS_Windows)
            //AlignedAllocator::StaticInit ();
            //SharedAllocator::StaticInit ();
            NullAllocator::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        std::string Allocator::GetSerializedName () const {
            std::string allocatorName = Type ();
            if (CreateType (allocatorName).Get () == nullptr) {
                allocatorName = DefaultAllocator::Instance ()->Type ();
            }
            return allocatorName;
        }

    } // namespace util
} // namespace thekogans
