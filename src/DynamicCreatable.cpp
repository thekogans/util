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
    #include "thekogans/util/Serializable.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)
#include "thekogans/util/DynamicCreatable.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE (thekogans::util::DynamicCreatable)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void DynamicCreatable::StaticInit () {
            JSON::Value::StaticInit ();
            Allocator::StaticInit ();
            Hash::StaticInit ();
            Logger::StaticInit ();
            Serializable::StaticInit ();
        }
    #else // defined (THEKOGANS_UTIL_TYPE_Static)
        DynamicCreatable::BaseMapInitializer::BaseMapInitializer (
                const std::string &base,
                const std::string &type,
                FactoryType factory) {
            (*BaseMap::Instance ())[base][type] = factory;
            if (base != TYPE) {
                (*BaseMap::Instance ())[TYPE][type] = factory;
            }
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        bool DynamicCreatable::IsBaseType (
                const std::string &base,
                const std::string &type) {
            BaseMapType::const_iterator it = BaseMap::Instance ()->find (base);
            return it != BaseMap::Instance ()->end () &&
                it->second.find (type) != it->second.end ();
        }

        const DynamicCreatable::TypeMap &DynamicCreatable::GetBaseTypes (const std::string &base) {
            static const TypeMap empty;
            BaseMapType::const_iterator it = BaseMap::Instance ()->find (base);
            return it != BaseMap::Instance ()->end () ? it->second : empty;
        }

        DynamicCreatable::SharedPtr DynamicCreatable::CreateBaseType (
                const std::string &base,
                const std::string &type,
                Parameters::SharedPtr parameters) {
            BaseMapType::const_iterator it = BaseMap::Instance ()->find (base);
            if (it != BaseMap::Instance ()->end ()) {
                TypeMap::const_iterator jt = it->second.find (type);
                if (jt != it->second.end ()) {
                    return jt->second (parameters);
                }
            }
            return nullptr;
        }

        void DynamicCreatable::DumpBaseMap (const std::string &base) {
            BaseMapType::const_iterator it = BaseMap::Instance ()->end ();
            BaseMapType::const_iterator end = BaseMap::Instance ()->end ();
            if (!base.empty ()) {
                it = end = BaseMap::Instance ()->find (base);
                if (end != BaseMap::Instance ()->end ()) {
                    ++end;
                }
            }
            else {
                it = BaseMap::Instance ()->begin ();
            }
            for (; it != end; ++it) {
                std::cout << it->first << ":" << std::endl;
                for (TypeMap::const_iterator
                         jt = it->second.begin (),
                         end = it->second.end (); jt != end; ++jt) {
                    std::cout << "  " << jt->first << std::endl;
                }
            }
        }

    } // namespace util
} // namespace thekogans
