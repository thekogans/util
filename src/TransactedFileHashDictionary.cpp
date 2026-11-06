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
#include "thekogans/util/TransactedFileHashDictionary.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT (
            thekogans::util::TransactedFileHashDictionary,
            1,
            TransactedFileHashDictionary::Header::SIZE)

        TransactedFileHashDictionary::TransactedFileHashDictionary (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset) :
                TransactedFile::Object (file, offset) {
            Reload ();
        }

        TransactedFile::Dictionary::TypeIdType TransactedFileHashDictionary::GetOrCreateTypeId (const std::string &type) {
            if (THEKOGANS_UTIL_UNLIKELY (type.empty ())) {
                return 0;
            }
            TypeIdType index = TransactedFile::SystemDictionary::GetReservedTypeId (type);
            if (index != TransactedFile::SystemDictionary::None) {
                return index;
            }
            LockGuard<SpinLock> guard (spinLock);
            auto it = hashMap.find (type);
            if (it != hashMap.end ()) {
                return it->second;
            }
            index = TransactedFile::SystemDictionary::FirstDictionaryTypeId + types.size ();
            types.push_back (type);
            hashMap[type] = index;
            SetDirty ();
            return index;
        }

        std::string TransactedFileHashDictionary::ResolveTypeId (TypeIdType typeId) {
            if (typeId < TransactedFile::SystemDictionary::FirstDictionaryTypeId) {
                return TransactedFile::SystemDictionary::ResolveTypeId (typeId);
            }
            typeId -= TransactedFile::SystemDictionary::FirstDictionaryTypeId;
            LockGuard<SpinLock> guard (spinLock);
            return typeId < types.size () ? types[typeId] : std::string ();
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileHashDictionary::Header &header) {
            serializer >> header.typesOffset;
            return serializer;
        }

        void TransactedFileHashDictionary::Read (Serializer &serializer) {
            LockGuard<SpinLock> guard (spinLock);
            ui32 magic;
            serializer >> magic;
            if (magic == MAGIC32) {
                serializer >> header;
                types.clear ();
                hashMap.clear ();
                TransactedFile::BlockRange typesRange (file, header.typesOffset);
                serializer >> magic;
                if (magic == MAGIC32) {
                    ui32 count;
                    typesRange >> count;
                    types.resize (count);
                    for (ui32 i = 0; i < count; ++i) {
                        std::string type;
                        typesRange  >> type;
                        types[i] = type;
                        hashMap[types[i]] = i;
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt TransactedFileHashDictionary::types in (%s).",
                        file->GetPath ().c_str ());
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFileHashDictionary in (%s).",
                    file->GetPath ().c_str ());
            }
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const TransactedFileHashDictionary::Header &header) {
            serializer << header.typesOffset;
            return serializer;
        }

        void TransactedFileHashDictionary::Write (Serializer &serializer) {
            LockGuard<SpinLock> guard (spinLock);
            std::size_t typesSize = UI32_SIZE + UI32_SIZE;
            for (const auto &type : types) {
                typesSize += Serializer::Size (type);
            }
            header.typesOffset = file->GetAllocator ()->Realloc (header.typesOffset, typesSize);
            TransactedFile::BlockRange typesRange (file, header.typesOffset, false);
            typesRange << MAGIC32 << (ui32)types.size ();
            for (const auto &type : types) {
                typesRange << type;
            }
            typesRange.Seek (SecureZeroMemory (typesRange.GetDataPtr (), typesRange.GetDataAvailable ()), SEEK_CUR);
            serializer << MAGIC32 << header;
        }

    } // namespace util
} // namespace thekogans
