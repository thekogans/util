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
#if defined (THEKOGANS_UTIL_TYPE_Static)
    #include "thekogans/util/TransactedFileTypeDictionary.h"
    #include "thekogans/util/TransactedFileBTreeAllocator.h"
    #include "thekogans/util/TransactedFileBTreeRegistry.h"
    #include "thekogans/util/TransactedFileBTree.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (thekogans::util::TransactedFile::Object)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void TransactedFile::Object::StaticInit () {
            TransactedFileTypeDictionary::StaticInitEx ();
            TransactedFileBTreeAllocatorBTree::StaticInitEx ();
            TransactedFileBTreeAllocator::StaticInitEx ();
            TransactedFileBTreeRegistry::StaticInitEx ();
            TransactedFileBTree::StaticInitEx ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        DynamicCreatable::SharedPtr TransactedFile::Object::CreateObject (
                const char *type,
                TransactedFile::SharedPtr file,
                AddressType offset) {
            return CreateType (type,
                [file, offset] (DynamicCreatable::SharedPtr dynamicCreatable) {
                    Object::SharedPtr object = dynamicCreatable;
                    if (object != nullptr) {
                        object->Init (file, offset);
                    }
                });
        }

        void TransactedFile::Object::DestroyObject (
                TransactedFile::SharedPtr file,
                AddressType offset) {
            if (file != nullptr && offset != 0) {
                BlockRange range (file, offset);
                ui32 magic;
                range >> magic;
                if (magic == TransactedFile::Object::MAGIC) {
                    SerializableHeader header;
                    range >> header;
                    DeleterMapType::const_iterator it = GetDeleters ().find (header.type.c_str ());
                    if (it != GetDeleters ().end ()) {
                        it->second (file, offset);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("Corrupt Object header (%x).", magic);
                }
            }
        }

        void TransactedFile::Object::FreeObject (
                TransactedFile::SharedPtr file,
                AddressType offset) {
            file->GetAllocator ()->Free (offset);
            file->Producer<TransactedFileStructureEvents>::Produce (
                std::bind (
                    &TransactedFileStructureEvents::OnTransactedFileOffsetDestroyed,
                    std::placeholders::_1,
                    file,
                    offset));
        }

        void TransactedFile::Object::SetDirty () {
            THEKOGANS_UTIL_ASSERT (!IsDestroyed (),
                "Calling TransactedFile::Object::SetDirty on a destroyed object.");
            file->Producer<TransactedFileTransactionEvents>::Subscribe (*this);
            Serializable::SetDirty ();
        }

        void TransactedFile::Object::Init (
                TransactedFile::SharedPtr file_,
                AddressType offset_) noexcept {
            file = file_;
            offset = offset_;
            if (file != nullptr && offset == 0) {
                SetDirty ();
            }
        }

        void TransactedFile::Object::AllocBlock () {
            THEKOGANS_UTIL_ASSERT (!IsDestroyed (),
                "Calling TransactedFile::Object::AllocBlock on a destroyed object.");
            std::size_t size = UI32_SIZE + GetSize (*file);
            if (offset == 0 || size > TransactedFile::Block::GetSize (file, offset)) {
                AddressType newOffset = file->GetAllocator ()->Realloc (offset, size, false);
                if (offset != newOffset) {
                    if (offset != 0) {
                        Producer<ObjectEvents>::Produce (
                            std::bind (
                                &ObjectEvents::OnTransactedFileObjectFreeBlock,
                                std::placeholders::_1,
                                this));
                    }
                    offset = newOffset;
                    Producer<ObjectEvents>::Produce (
                        std::bind (
                            &ObjectEvents::OnTransactedFileObjectAllocBlock,
                            std::placeholders::_1,
                            this));
                }
            }
        }

        void TransactedFile::Object::FreeBlock () {
            THEKOGANS_UTIL_ASSERT (!IsDestroyed (),
                "Calling TransactedFile::Object::FreeBlock on a destroyed object.");
            if (offset != 0) {
                // Can't call FreeObject here. it's not virtual.
                DestroyObject (file, offset);
                Producer<ObjectEvents>::Produce (
                    std::bind (
                        &ObjectEvents::OnTransactedFileObjectFreeBlock,
                        std::placeholders::_1,
                        this));
                offset = 0;
                destroyed = true;
            }
        }

        void TransactedFile::Object::Flush () {
            THEKOGANS_UTIL_ASSERT (!IsDestroyed (),
                "Calling TransactedFile::Object::Flush on a destroyed object.");
            if (offset != 0) {
                BlockRange range (file, offset, false);
                range << MAGIC << *this;
                if (file->IsSecure ()) {
                    range.Seek (
                        SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable ()),
                        SEEK_CUR);
                }
            }
        }

        void TransactedFile::Object::Reload () {
            THEKOGANS_UTIL_ASSERT (!IsDestroyed (),
                "Calling TransactedFile::Object::Reload on a destroyed object.");
            if (offset != 0) {
                BlockRange range (file, offset);
                ui32 magic;
                range >> magic;
                if (magic == TransactedFile::Object::MAGIC) {
                    range >> *this;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("Corrupt Object header (%x).", magic);
                }
            }
        }

        void TransactedFile::Object::OnTransactedFileTransactionCommit (
                TransactedFile::SharedPtr /*file*/,
                int phase) noexcept {
            THEKOGANS_UTIL_ASSERT (!IsDestroyed (),
                "Calling TransactedFile::Object::OnTransactedFileTransactionCommit on a destroyed object.");
            if (phase == TransactedFile::Transaction::COMMIT_PHASE_1) {
                AllocBlock ();
            }
            else if (phase == TransactedFile::Transaction::COMMIT_PHASE_2) {
                Flush ();
            }
        }

        void TransactedFile::Object::OnTransactedFileTransactionAbort (
                TransactedFile::SharedPtr /*file*/) noexcept {
            THEKOGANS_UTIL_ASSERT (!IsDestroyed (),
                "Calling TransactedFile::Object::OnTransactedFileTransactionAbort on a destroyed object.");
            Reload ();
        }

        _LIB_THEKOGANS_UTIL_DECL TransactedFile::Range & _LIB_THEKOGANS_UTIL_API operator >> (
                TransactedFile::Range &range,
                TransactedFile::Object::SharedPtr &object) {
            ui32 magic;
            range >> magic;
            if (magic == TransactedFile::Object::MAGIC) {
                SerializableHeader header;
                range >> header;
                if (range.factory != nullptr) {
                    object = range.factory (range.parameters);
                }
                else {
                    object = TransactedFile::Object::CreateObject (
                        header.type.c_str (), range.GetFile (), range.GetOffset ());
                }
                if (object != nullptr) {
                    if (header.version <= object->Version ()) {
                        Serializer::ContextGuard guard (range, header);
                        object->Read (range);
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Header version (%u) is greater then the TransactedFile::Object version (%u).",
                            header.version,
                            object->Version ());
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Unable to create a TransactedFile::Object of type %s.",
                        header.type.c_str ());
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("Corrupt Object header (%x).", magic);
            }
            return range;
        }

    } // namespace util
} // namespace thekogans
