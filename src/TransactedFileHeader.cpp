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

#include "thekogans/util/TransactedFileTypeDictionary.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT (thekogans::util::TransactedFile::Header, 1, TransactedFile::Header::SIZE)

        TransactedFile::Header::Header (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset,
                ui32 flags_,
                DynamicCreatable::TypeDictionary::SharedPtr dictionary_,
                Allocator::SharedPtr allocator_,
                Registry::SharedPtr registry_) :
                TransactedFile::Object (file, offset),
                flags (flags_),
                dictionary (dictionary_),
                allocator (allocator_),
                registry (registry_) {
            if (file == nullptr) {
                dictionary.Reset (new TransactedFileTypeDictionary);
            }
            else if (offset == 0) {
                std::size_t size = UI32_SIZE + GetSize (*file);
                Block block (file, file->Grow (Block::SIZE + size) + Block::BLOCK_0_OFFSET, 0, size);
                block.Write ();
                {
                    Object::SharedPtr object = dictionary;
                    if (object != nullptr) {
                        object->Producer<ObjectEvents>::Subscribe (*this);
                        object->Init (file, 0);
                    }
                }
                {
                    Object::SharedPtr object = allocator;
                    if (object != nullptr) {
                        object->Producer<ObjectEvents>::Subscribe (*this);
                        object->Init (file, 0);
                    }
                }
                {
                    Object::SharedPtr object = registry;
                    if (object != nullptr) {
                        object->Producer<ObjectEvents>::Subscribe (*this);
                        object->Init (file, 0);
                    }
                }
            }
            Reload ();
        }

        DynamicCreatable::TypeDictionary::SharedPtr TransactedFile::Header::GetDictionary () {
            LockGuard<SpinLock> guard (spinLock);
            if (dictionary == nullptr) {
                BlockRange dictionaryRange (file, dictionaryOffset);
                Object::SharedPtr object;
                dictionaryRange >> object;
                dictionary = object;
                assert (dictionary != nullptr);
                object->Producer<ObjectEvents>::Subscribe (*this);
            }
            return dictionary;
        }

        TransactedFile::Allocator::SharedPtr TransactedFile::Header::GetAllocator () {
            LockGuard<SpinLock> guard (spinLock);
            if (allocator == nullptr) {
                BlockRange allocatorRange (file, allocatorOffset);
                Object::SharedPtr object;
                allocatorRange >> object;
                allocator = object;
                assert (allocator != nullptr);
                object->Producer<ObjectEvents>::Subscribe (*this);
            }
            return allocator;
        }

        TransactedFile::Registry::SharedPtr TransactedFile::Header::GetRegistry () {
            LockGuard<SpinLock> guard (spinLock);
            if (registry == nullptr) {
                BlockRange registryRange (file, registryOffset);
                Object::SharedPtr object;
                registryRange >> object;
                registry = object;
                assert (registry != nullptr);
                object->Producer<ObjectEvents>::Subscribe (*this);
            }
            return registry;
        }

        void TransactedFile::Header::Read (Serializer &serializer) {
            LockGuard<SpinLock> guard (spinLock);
            if (dictionary != nullptr) {
                Object::SharedPtr object = dictionary;
                if (object != nullptr) {
                    object->Producer<ObjectEvents>::Unsubscribe (*this);
                }
                dictionary.Reset ();
            }
            if (allocator != nullptr) {
                Object::SharedPtr object = allocator;
                if (object != nullptr) {
                    object->Producer<ObjectEvents>::Unsubscribe (*this);
                }
                allocator.Reset ();
            }
            if (registry != nullptr) {
                Object::SharedPtr object = registry;
                if (object != nullptr) {
                    object->Producer<ObjectEvents>::Unsubscribe (*this);
                }
                registry.Reset ();
            }
            ui32 magic;
            serializer >> magic;
            if (THEKOGANS_UTIL_LIKELY (magic == MAGIC32)) {
                serializer >> flags >> dictionaryOffset >> allocatorOffset >> registryOffset;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFile::Header in (%s).",
                    file->GetPath ().c_str ());
            }
        }

        void TransactedFile::Header::Write (Serializer &serializer) {
            LockGuard<SpinLock> guard (spinLock);
            if (dictionary != nullptr) {
                Object::SharedPtr object = dictionary;
                if (object != nullptr) {
                    dictionaryOffset = object->GetOffset ();
                }
            }
            if (allocator != nullptr) {
                Object::SharedPtr object = allocator;
                if (object != nullptr) {
                    allocatorOffset = object->GetOffset ();
                }
            }
            if (registry != nullptr) {
                Object::SharedPtr object = registry;
                if (object != nullptr) {
                    registryOffset = object->GetOffset ();
                }
            }
            serializer << MAGIC32 << flags << dictionaryOffset << allocatorOffset << registryOffset;
        }

    } // namespace util
} // namespace thekogans
