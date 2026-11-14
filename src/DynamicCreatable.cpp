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
                const HashedString bases[],
                const HashedString &type,
                FactoryType factory) {
            BaseMapType &baseMap = GetBases ();
            while (*bases != HashedString ()) {
                baseMap[*bases++][type] = factory;
            }
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        DynamicCreatable::TypeDictionary::IdType DynamicCreatable::DefaultTypeDictionary::GetOrCreateTypeId (const HashedString &type) {
            if (THEKOGANS_UTIL_UNLIKELY (type.empty ())) {
                return None;
            }
            THEKOGANS_UTIL_ASSERT (type.str == DynamicCreatable::TypeForType (type).str,
                "DynamicCreatable::DefaultTypeDictionary::GetOrCreateTypeId called on noncanonical type %s.",
                type.str);
            auto it = hashMap.find (type);
            if (THEKOGANS_UTIL_LIKELY (it != hashMap.end ())) {
                return it->second;
            }
            IdType id = static_cast<IdType> (types.size ());
            types.push_back (type);
            hashMap.emplace (type, FirstUserId + id);
            return FirstUserId + id;
        }

        const HashedString &DynamicCreatable::DefaultTypeDictionary::ResolveTypeId (IdType id) {
            if (id < FirstUserId) {
                return HashedString::Empty;
            }
            id -= FirstUserId;
            return id < types.size () ? types[id] : HashedString::Empty;
        }

        bool DynamicCreatable::IsDerivedFrom (const HashedString &base) const noexcept {
            if (base != nullptr) {
                BaseMapType::const_iterator it = GetBases ().find (base);
                return it != GetBases ().end () &&
                    it->second.find (Type ()) != it->second.end ();
            }
            return false;
        }

        bool DynamicCreatable::IsKindOf (const HashedString &type) const noexcept {
            return Type () == type || IsDerivedFrom (type);
        }

        DynamicCreatable::BaseMapType &DynamicCreatable::GetBases () {
            static BaseMapType bases;
            return bases;
        }

        void DynamicCreatable::DumpBase (const HashedString &base) {
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
                std::cout << it->first.str << ":" << std::endl;
                for (TypeMapType::const_iterator
                        jt = it->second.begin (),
                        end = it->second.end (); jt != end; ++jt) {
                    std::cout << "  " << jt->first.str << std::endl;
                }
            }
        }

    } // namespace util
} // namespace thekogans
