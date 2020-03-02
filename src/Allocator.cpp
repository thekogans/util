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

#include <cassert>
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Allocator.h"
#if defined (THEKOGANS_UTIL_TYPE_Static)
    #include "thekogans/util/DefaultAllocator.h"
    #include "thekogans/util/SecureAllocator.h"
    #if defined (TOOLCHAIN_OS_Windows)
        #include "thekogans/util/HGLOBALAllocator.h"
    #endif // defined (TOOLCHAIN_OS_Windows)
    //#include "thekogans/util/AlignedAllocator.h"
    //#include "thekogans/util/SharedAllocator.h"
    //#include "thekogans/util/NullAllocator.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

namespace thekogans {
    namespace util {

        Allocator::Map &Allocator::GetMap () {
            static Allocator::Map map;
            return map;
        }

        Allocator *Allocator::Get (const std::string &type) {
            Map::iterator it = GetMap ().find (type);
            return it != GetMap ().end () ? it->second () : 0;
        }

        Allocator::MapInitializer::MapInitializer (
                const std::string &type,
                Factory factory) {
            std::pair<Map::iterator, bool> result =
                GetMap ().insert (Map::value_type (type, factory));
            assert (result.second);
            if (!result.second) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s is already registered.", type.c_str ());
            }
        }

        void Allocator::GetAllocators (std::list<std::string> &allocators) {
            for (Map::const_iterator it = GetMap ().begin (),
                    end = GetMap ().end (); it != end; ++it) {
                allocators.push_back (it->first);
            }
        }

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void Allocator::StaticInit () {
            static volatile bool registered = false;
            static SpinLock spinLock;
            LockGuard<SpinLock> guard (spinLock);
            if (!registered) {
                DefaultAllocator::StaticInit ();
                SecureAllocator::StaticInit ();
            #if defined (TOOLCHAIN_OS_Windows)
                HGLOBALAllocator::StaticInit ();
            #endif // defined (TOOLCHAIN_OS_Windows)
                //AlignedAllocator::StaticInit ();
                //SharedAllocator::StaticInit ();
                //NullAllocator::StaticInit ();
                registered = true;
            }
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

    } // namespace util
} // namespace thekogans
