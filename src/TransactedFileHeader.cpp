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

#include "thekogans/util/TransactedFile.h"
#include "thekogans/util/TransactedFileTypeDictionary.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT (
            thekogans::util::TransactedFile::Header, 1, TransactedFile::Header::SIZE)

        void TransactedFile::Header::Read (Serializer &serializer) {
            ui32 magic;
            serializer >> magic;
            if (THEKOGANS_UTIL_LIKELY (magic == MAGIC32)) {
                LockGuard<SpinLock> guard (spinLock);
                if (file->dictionary != nullptr) {
                    Object::SharedPtr object = file->dictionary;
                    if (object != nullptr) {
                        object->Producer<ObjectEvents>::Unsubscribe (*this);
                    }
                    file->dictionary.Reset ();
                }
                if (file->allocator != nullptr) {
                    Object::SharedPtr object = file->allocator;
                    if (object != nullptr) {
                        object->Producer<ObjectEvents>::Unsubscribe (*this);
                    }
                    file->allocator.Reset ();
                }
                if (file->registry != nullptr) {
                    Object::SharedPtr object = file->registry;
                    if (object != nullptr) {
                        object->Producer<ObjectEvents>::Unsubscribe (*this);
                    }
                    file->registry.Reset ();
                }
                serializer >> flags >> dictionaryOffset >> allocatorOffset >> registryOffset;
                file->dictionary.Reset (new TransactedFileTypeDictionary);
                {
                    ObjectRange dictionaryRange (file, dictionaryOffset);
                    Object::SharedPtr object;
                    dictionaryRange >> object;
                    file->dictionary = object;
                    assert (file->dictionary != nullptr);
                    object->Producer<ObjectEvents>::Subscribe (*this);
                }
                {
                    ObjectRange allocatorRange (file, allocatorOffset);
                    Object::SharedPtr object;
                    allocatorRange >> object;
                    file->allocator = object;
                    assert (file->allocator != nullptr);
                    object->Producer<ObjectEvents>::Subscribe (*this);
                }
                {
                    ObjectRange registryRange (file, registryOffset);
                    Object::SharedPtr object;
                    registryRange >> object;
                    file->registry = object;
                    assert (file->registry != nullptr);
                    object->Producer<ObjectEvents>::Subscribe (*this);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFile::Header in (%s).",
                    file->GetPath ().c_str ());
            }
        }

        void TransactedFile::Header::Write (Serializer &serializer) {
            LockGuard<SpinLock> guard (spinLock);
            if (file->dictionary != nullptr) {
                Object::SharedPtr object = file->dictionary;
                if (object != nullptr) {
                    dictionaryOffset = object->GetOffset ();
                }
            }
            if (file->allocator != nullptr) {
                Object::SharedPtr object = file->allocator;
                if (object != nullptr) {
                    allocatorOffset = object->GetOffset ();
                }
            }
            if (file->registry != nullptr) {
                Object::SharedPtr object = file->registry;
                if (object != nullptr) {
                    registryOffset = object->GetOffset ();
                }
            }
            serializer << MAGIC32 << flags << dictionaryOffset << allocatorOffset << registryOffset;
        }

        void TransactedFile::Header::Init (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset) noexcept {
            Object::Init (file, offset);
            if (offset == 0) {
                std::size_t size = UI32_SIZE + GetSize (*file);
                this->offset = file->Grow (Block::SIZE + size) + Block::BLOCK_0_OFFSET;
                Block block (file, this->offset, 0, size);
                block.Write ();
                {
                    Object::SharedPtr object = file->dictionary;
                    if (object != nullptr) {
                        object->Producer<ObjectEvents>::Subscribe (*this);
                        object->Init (file, 0);
                    }
                }
                {
                    Object::SharedPtr object = file->allocator;
                    if (object != nullptr) {
                        object->Producer<ObjectEvents>::Subscribe (*this);
                        object->Init (file, 0);
                    }
                }
                {
                    Object::SharedPtr object = file->registry;
                    if (object != nullptr) {
                        object->Producer<ObjectEvents>::Subscribe (*this);
                        object->Init (file, 0);
                    }
                }
            }
        }

    } // namespace util
} // namespace thekogans
