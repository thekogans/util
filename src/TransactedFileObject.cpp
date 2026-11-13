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
            TransactedFileBTreeAllocator::StaticInitEx ();
            TransactedFileBTreeRegistry::StaticInitEx ();
            TransactedFileBTree::StaticInitEx ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        void TransactedFile::Object::DestroyObject (
                TransactedFile::SharedPtr file,
                AddressType offset) {
            if (THEKOGANS_UTIL_LIKELY (file != nullptr && offset != 0)) {
                if (THEKOGANS_UTIL_UNLIKELY (!file->CheckCanMutate ())) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Structural Logic Error: TransactedFile::Object::DestroyObject can only be "
                        "called durring transaction in progress and allocation phases (%s).",
                        TransactedFile::Transaction::phaseName[file->GetTransactionPhase ()]);
                }
                ObjectRange range (file, offset);
                DeleterMapType::const_iterator it = GetDeleters ().find (range.context.type.c_str ());
                if (it != GetDeleters ().end ()) {
                    it->second (file, offset);
                }
            }
        }

        void TransactedFile::Object::FreeObject (
                TransactedFile::SharedPtr file,
                AddressType offset) {
            if (THEKOGANS_UTIL_LIKELY (file != nullptr && offset != 0)) {
                if (THEKOGANS_UTIL_UNLIKELY (!file->CheckCanMutate ())) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Structural Logic Error: TransactedFile::Object::FreeObject can only be "
                        "called durring transaction in progress and allocation phases (%s).",
                        TransactedFile::Transaction::phaseName[file->GetTransactionPhase ()]);
                }
                file->GetAllocator ()->Free (offset);
                file->Producer<TransactedFileStructureEvents>::Produce (
                    std::bind (
                        &TransactedFileStructureEvents::OnTransactedFileOffsetDestroyed,
                        std::placeholders::_1,
                        file,
                        offset));
            }
        }

        void TransactedFile::Object::SetDirty () {
            THEKOGANS_UTIL_ASSERT (!IsDestroyed (),
                "Calling TransactedFile::Object::SetDirty on a destroyed object.");
            if (THEKOGANS_UTIL_LIKELY (file != nullptr)) {
                if (THEKOGANS_UTIL_UNLIKELY (!file->CheckCanMutate ())) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Structural Logic Error: TransactedFile::Object::SetDirty can only be "
                        "called durring transaction in progress and allocation phases (%s).",
                        TransactedFile::Transaction::phaseName[file->GetTransactionPhase ()]);
                }
                file->Producer<TransactedFileTransactionEvents>::Subscribe (*this);
                Serializable::SetDirty ();
            }
        }

        void TransactedFile::Object::Init (
                TransactedFile::SharedPtr file_,
                AddressType offset_) noexcept {
            THEKOGANS_UTIL_ASSERT (offset_ == 0 || file_ != nullptr,
                "Invalid Object Construction: Non-zero physical offset provided without a valid TransactedFile.");
            file = file_;
            offset = offset_;
            header.type = Type ();
            header.version = Version ();
            if (file != nullptr) {
                if (offset != 0) {
                    Reload ();
                }
                else {
                    SetDirty ();
                }
            }
        }

        void TransactedFile::Object::AllocBlock () {
            THEKOGANS_UTIL_ASSERT (!IsDestroyed (),
                "Calling TransactedFile::Object::AllocBlock on a destroyed object.");
            if (THEKOGANS_UTIL_LIKELY (file != nullptr)) {
                header.size = Size (*file);
                std::size_t size = 0;
                {
                    Serializer::ContextGuard guard (*file);
                    size = UI32_SIZE + file->Size (header) + header.size;
                }
                if (offset == 0 || size > Block::GetSize (file, offset)) {
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
        }

        void TransactedFile::Object::FreeBlock () {
            THEKOGANS_UTIL_ASSERT (!IsDestroyed (),
                "Calling TransactedFile::Object::FreeBlock on a destroyed object.");
            if (THEKOGANS_UTIL_LIKELY (file != nullptr && offset != 0)) {
                if (THEKOGANS_UTIL_UNLIKELY (!file->CheckCanMutate ())) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Structural Logic Error: TransactedFile::Object::FreeBlock can only be "
                        "called durring transaction in progress and allocation phases (%s).",
                        TransactedFile::Transaction::phaseName[file->GetTransactionPhase ()]);
                }
                // Can't call FreeObject here. it's not virtual.
                DestroyObject (file, offset);
                Producer<ObjectEvents>::Produce (
                    std::bind (
                        &ObjectEvents::OnTransactedFileObjectFreeBlock,
                        std::placeholders::_1,
                        this));
                offset = 0;
                header.size = 0;
                destroyed = true;
            }
        }

        void TransactedFile::Object::Flush () {
            THEKOGANS_UTIL_ASSERT (!IsDestroyed (),
                "Calling TransactedFile::Object::Flush on a destroyed object.");
            if (THEKOGANS_UTIL_LIKELY (file != nullptr && offset != 0)) {
                ObjectRange range (file, offset, *this);
                Write (range);
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
            if (THEKOGANS_UTIL_LIKELY (file != nullptr && offset != 0)) {
                ObjectRange range (file, offset, header);
                header.size = range.context.size;
                Read (range);
            }
        }

        void TransactedFile::Object::OnTransactedFileTransactionCommit (
                TransactedFile::SharedPtr /*file*/,
                int phase) noexcept {
            if (phase == TransactedFile::Transaction::PHASE_ALLOC) {
                AllocBlock ();
            }
            else if (phase == TransactedFile::Transaction::PHASE_FLUSH) {
                Flush ();
            }
        }

        void TransactedFile::Object::OnTransactedFileTransactionAbort (
                TransactedFile::SharedPtr /*file*/) noexcept {
            Reload ();
        }

        _LIB_THEKOGANS_UTIL_DECL TransactedFile::ObjectRange & _LIB_THEKOGANS_UTIL_API operator >> (
                TransactedFile::ObjectRange &range,
                TransactedFile::Object::SharedPtr &object) {
            if (range.factory != nullptr) {
                object = range.factory (range.parameters);
            }
            else {
                object = TransactedFile::Object::CreateType (range.context.type.c_str (), range.parameters);
            }
            if (object != nullptr) {
                object->Init (range.GetFile (), range.GetOffset ());
            }
            return range;
        }

    } // namespace util
} // namespace thekogans
