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
#include "thekogans/util/SizeT.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/TransactedFileTypeDictionary.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT (thekogans::util::TransactedFileTypeDictionary, 1, 0)

        TransactedFileTypeDictionary::TransactedFileTypeDictionary (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset) :
                TransactedFile::Object (file, offset) {
            Reload ();
        }

        namespace {
            struct SystemTypeDictionary :
                    public DynamicCreatable::DefaultTypeDictionary,
                    public Singleton<SystemTypeDictionary> {
                SystemTypeDictionary () {
                    types.push_back (TransactedFileTypeDictionary::TYPE);
                    // Add system types here.
                    for (std::size_t i = 0, count = types.size (); i < count; ++i) {
                        hashMap[types[i].c_str ()] = FirstSystemId + i;
                    }
                }

                virtual IdType GetOrCreateTypeId (const char *type) noexcept override {
                    auto it = hashMap.find (type);
                    return it != hashMap.end () ? it->second : None;
                }

                virtual const char *ResolveTypeId (IdType id) noexcept override {
                    if (id > MaxReservedId) {
                        return nullptr;
                    }
                    id -= FirstSystemId;
                    return id < types.size () ? types[id].c_str () : nullptr;
                }
            };
        }

        DynamicCreatable::TypeDictionary::IdType TransactedFileTypeDictionary::GetOrCreateTypeId (const char *type) {
            IdType id = SystemTypeDictionary::Instance ()->GetOrCreateTypeId (type);
            if (THEKOGANS_UTIL_UNLIKELY (id != None)) {
                return id;
            }
            LockGuard<SpinLock> guard (spinLock);
            std::size_t size = types.size ();
            id = DynamicCreatable::DefaultTypeDictionary::GetOrCreateTypeId (type);
            if (size != types.size ()) {
                SetDirty ();
            }
            return id;
        }

        const char *TransactedFileTypeDictionary::ResolveTypeId (IdType id) {
            if (THEKOGANS_UTIL_UNLIKELY (id < FirstUserId)) {
                return SystemTypeDictionary::Instance ()->ResolveTypeId (id);
            }
            LockGuard<SpinLock> guard (spinLock);
            return DynamicCreatable::DefaultTypeDictionary::ResolveTypeId (id);
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
                    hashMap[types[i].c_str ()] = FirstUserId + i;
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
