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

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (
            thekogans::util::DynamicCreatable)
        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_BEGIN (DynamicCreatable)
        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_END (DynamicCreatable)
        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_Bases (DynamicCreatable)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void DynamicCreatable::StaticInit () {
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
                const char *type,
                FactoryType factory) {
            if (bases != nullptr && type != nullptr) {
                while (*bases != nullptr) {
                    GetBases ()[*bases++][type] = factory;
                }
            }
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        DynamicCreatable::BaseMapType &DynamicCreatable::GetBases () {
            static BaseMapType bases;
            return bases;
        }

        bool DynamicCreatable::IsDerivedFrom (const char *base) const noexcept {
            if (base != nullptr) {
                BaseMapType::const_iterator it = GetBases ().find (base);
                return it != GetBases ().end () &&
                    it->second.find (Type ()) != it->second.end ();
            }
            return false;
        }

        bool DynamicCreatable::IsKindOf (const char *type) const noexcept {
            CharPtrEqual compare;
            return type != nullptr && (compare (type, Type ()) || IsDerivedFrom (type));
        }

        void DynamicCreatable::DumpBaseMap (const char *base) {
            BaseMapType::const_iterator it;
            BaseMapType::const_iterator end;
            if (base != nullptr) {
                it = end = GetBases ().find (base);
                if (end != GetBases ().end ()) {
                    ++end;
                }
            }
            else {
                it = GetBases ().begin ();
                end = GetBases ().end ();
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
