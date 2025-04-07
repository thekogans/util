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
    #include "thekogans/util/BTree2.h"
    #include "thekogans/util/JSON.h"
    #include "thekogans/util/Allocator.h"
    #include "thekogans/util/Hash.h"
    #include "thekogans/util/Logger.h"
    #include "thekogans/util/Serializer.h"
    #include "thekogans/util/Serializable.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)
#include "thekogans/util/DynamicCreatable.h"

namespace thekogans {
    namespace util {

        const char * const *DynamicCreatable::Bases () const noexcept {
            return nullptr;
        }

        std::size_t DynamicCreatable::BasesSize () const noexcept {
            return 0;
        }

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE (thekogans::util::DynamicCreatable)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void DynamicCreatable::StaticInit () {
            BTree2::Key::StaticInit ();
            BTree2::Value::StaticInit ();
            JSON::Value::StaticInit ();
            Allocator::StaticInit ();
            Hash::StaticInit ();
            Logger::StaticInit ();
            Serializer::StaticInit ();
            Serializable::StaticInit ();
        }
    #else // defined (THEKOGANS_UTIL_TYPE_Static)
        DynamicCreatable::BaseMapInitializer::BaseMapInitializer (
                const char * const bases[],
                std::size_t basesSize,
                const char *type,
                FactoryType factory) {
            for (std::size_t i = 0; i < basesSize; ++i) {
                (*BaseMap::Instance ())[bases[i]][type] = factory;
            }
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        bool DynamicCreatable::IsDerivedFrom (const char *base) const noexcept {
            if (base != nullptr) {
                CharPtrEqual compare;
                const char * const *bases = Bases ();
                for (std::size_t i = 0, basesSize = BasesSize (); i < basesSize; ++i) {
                    if (compare (base, bases[i])) {
                        return true;
                    }
                }
            }
            return false;
        }

        void DynamicCreatable::DumpBaseMap (const char *base) {
            BaseMapType::const_iterator it;
            BaseMapType::const_iterator end;
            if (base != nullptr) {
                it = end = BaseMap::Instance ()->find (base);
                if (end != BaseMap::Instance ()->end ()) {
                    ++end;
                }
            }
            else {
                it = BaseMap::Instance ()->begin ();
                end = BaseMap::Instance ()->end ();
            }
            for (; it != end; ++it) {
                std::cout << it->first << ":" << std::endl;
                for (TypeMapType::const_iterator
                         jt = it->second.begin (),
                         end = it->second.end (); jt != end; ++jt) {
                    std::cout << "  " << jt->first << std::endl;
                }
            }
        }

    } // namespace util
} // namespace thekogans
