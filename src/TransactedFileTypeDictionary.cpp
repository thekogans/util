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

#include <string>
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/TransactedFileTypeDictionary.h"

namespace thekogans {
    namespace util {

        struct SystemTypeDictionary {
            static constexpr DynamicCreatable::TypeDictionary::IdType TypeDictionary = 1;

            static const char *ResolveTypeId (DynamicCreatable::TypeDictionary::IdType id) noexcept {
                switch (id) {
                    case TypeDictionary: {
                        return TransactedFileTypeDictionary::TYPE;
                    }
                    default: {
                        return nullptr;
                    }
                }
            }

            static DynamicCreatable::TypeDictionary::IdType GetReservedTypeId (const char *type) noexcept {
                if (type == TransactedFileTypeDictionary::TYPE) {
                    return TypeDictionary;
                }
                return DynamicCreatable::TypeDictionary::None;
            }
        };

        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT (thekogans::util::TransactedFileTypeDictionary, 1, 0)

        TransactedFileTypeDictionary::TransactedFileTypeDictionary (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset) :
                TransactedFile::Object (file, offset) {
            Reload ();
        }

        DynamicCreatable::TypeDictionary::IdType TransactedFileTypeDictionary::GetOrCreateTypeId (const char *type) {
            if (THEKOGANS_UTIL_UNLIKELY (type == nullptr || *type == '\0')) {
                return 0;
            }
            IdType index = SystemTypeDictionary::GetReservedTypeId (type);
            if (THEKOGANS_UTIL_UNLIKELY (index != None)) {
                return index;
            }
            LockGuard<SpinLock> guard (spinLock);
            auto it = hashMap.find (type);
            if (it != hashMap.end ()) {
                return it->second;
            }
            index = types.size ();
            types.push_back (type);
            hashMap[types[index].c_str ()] = FirstUserId + index;
            SetDirty ();
            return FirstUserId + index;
        }

        const char *TransactedFileTypeDictionary::ResolveTypeId (IdType typeId) {
            if (THEKOGANS_UTIL_UNLIKELY (typeId < FirstUserId)) {
                return SystemTypeDictionary::ResolveTypeId (typeId);
            }
            typeId -= FirstUserId;
            LockGuard<SpinLock> guard (spinLock);
            return typeId < types.size () ? types[typeId].c_str () : nullptr;
        }

        std::size_t TransactedFileTypeDictionary::Size (Serializer &serializer) noexcept {
            LockGuard<SpinLock> guard (spinLock);
            std::size_t size = UI32_SIZE + SizeT (types.size ()).Size ();
            for (const auto &type : types) {
                size += Serializer::Size (type);
            }
            return size;
        }

        void TransactedFileTypeDictionary::Read (Serializer &serializer) {
            LockGuard<SpinLock> guard (spinLock);
            ui32 magic;
            serializer >> magic;
            if (THEKOGANS_UTIL_LIKELY (magic == MAGIC32)) {
                types.clear ();
                hashMap.clear ();
                SizeT count;
                serializer >> count;
                types.resize (count);
                for (ui32 i = 0; i < count; ++i) {
                    std::string type;
                    serializer  >> type;
                    types[i] = type;
                    hashMap[types[i].c_str ()] = i;
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFileTypeDictionary in (%s).",
                    file->GetPath ().c_str ());
            }
        }

        void TransactedFileTypeDictionary::Write (Serializer &serializer) {
            LockGuard<SpinLock> guard (spinLock);
            serializer << MAGIC32 << SizeT (types.size ());
            for (const auto &type : types) {
                serializer << type;
            }
        }

    } // namespace util
} // namespace thekogans
