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

#if defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_TYPE_Shared)

#include <string>
#include <map>
#include "thekogans/util/Exception.h"
#include "thekogans/util/Singleton.h"

namespace thekogans {
    namespace util {

        namespace detail {
            _LIB_THEKOGANS_UTIL_DECL Mutex & _LIB_THEKOGANS_UTIL_API GetInstanceLock () {
                static Mutex mutex;
                return mutex;
            }

            namespace {
                typedef std::map<std::string, void *> InstanceMap;

                InstanceMap &GetMap () {
                    static InstanceMap instanceMap;
                    return instanceMap;
                }
            }

            _LIB_THEKOGANS_UTIL_DECL void * _LIB_THEKOGANS_UTIL_API GetInstance (const char *name) {
                InstanceMap::const_iterator it = GetMap ().find (name);
                return it != GetMap ().end () ? it->second : 0;
            }

            _LIB_THEKOGANS_UTIL_DECL void _LIB_THEKOGANS_UTIL_API SetInstance (
                    const char *name,
                    void *instance) {
                std::pair<InstanceMap::iterator, bool> result =
                    GetMap ().insert (InstanceMap::value_type (name, instance));
                if (!result.second) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Unable to insert intance: %s", name);
                }
            }
        } // namespace detail

    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_TYPE_Shared)
