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
    #include "thekogans/util/TransactedFile.h"
    #include "thekogans/util/TransactedFileTypeDictionary.h"
    #include "thekogans/util/TransactedFileBTreeAllocatorBTree.h"
    #include "thekogans/util/TransactedFileBTreeAllocator.h"
    #include "thekogans/util/TransactedFileBTreeRegistry.h"
    #include "thekogans/util/TransactedFileBTree.h"
    #include "thekogans/util/TransactedFileObjects.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (thekogans::util::TransactedFile::Object)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void TransactedFile::Object::StaticInit () {
            TransactedFile::Header::StaticInitEx ();
            TransactedFileTypeDictionary::StaticInitEx ();
            TransactedFileBTreeAllocator::StaticInitEx ();
            TransactedFileBTreeRegistry::StaticInitEx ();
            TransactedFileBTreeAllocatorBTree::Node::StaticInitEx ();
            TransactedFileBTreeAllocatorBTree::StaticInitEx ();
            TransactedFileBTree::Node::StaticInitEx ();
            TransactedFileBTree::StaticInitEx ();
            StringObject::StaticInitEx ();
            StringArrayObject::StaticInitEx ();
            AddressArrayObject::StaticInitEx ();
            GUIDArrayObject::StaticInitEx ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        void TransactedFile::Object::DestroyObject (
                TransactedFile *file,
                AddressType offset) {
            if (THEKOGANS_UTIL_LIKELY (file != nullptr && offset != 0)) {
                if (THEKOGANS_UTIL_UNLIKELY (!file->CheckCanMutate ())) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Structural Logic Error: TransactedFile::Object::DestroyObject can only be "
                        "called durring transaction in progress and allocation phases (%s).",
                        TransactedFile::Transaction::phaseName[file->GetTransactionPhase ()]);
                }
                ObjectRange range (file, offset);
                DeleterMapType::const_iterator it = GetDeleters ().find (range.context.type);
                if (THEKOGANS_UTIL_LIKELY (it != GetDeleters ().end ())) {
                    it->second (file, offset);
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Unable to locate a deleter for %s", range.context.type.c_str ());
                }
            }
        }

        void TransactedFile::Object::FreeObject (
                TransactedFile *file,
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

        void TransactedFile::Object::SetDestroyed () {
            if (THEKOGANS_UTIL_UNLIKELY (!file->CheckCanMutate ())) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Structural Logic Error: TransactedFile::Object::SetDestroyed can only be "
                    "called durring transaction in progress and allocation phases (%s).",
                    TransactedFile::Transaction::phaseName[file->GetTransactionPhase ()]);
            }
            if (file != nullptr) {
                file->Producer<TransactedFileTransactionEvents>::Unsubscribe (*this);
                file->Producer<TransactedFileStructureEvents>::Unsubscribe (*this);
            }
            offset = 0;
            size = 0;
            destroyed = true;
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
                TransactedFile *file_,
                AddressType offset_) {
            THEKOGANS_UTIL_ASSERT (!IsDestroyed (),
                "Calling TransactedFile::Object::Init on a destroyed object.");
            THEKOGANS_UTIL_ASSERT (file == nullptr,
                "TransactedFile::Object::Init called on an already initialized object.");
            THEKOGANS_UTIL_ASSERT (file_ != nullptr,
                "TransactedFile::Object::Init called with nullptr for TransactedFile.");
            file = file_;
            offset = offset_;
            if (offset != 0) {
                Reload ();
            }
            else {
                SetDirty ();
            }
        }

        void TransactedFile::Object::AllocBlock () {
            THEKOGANS_UTIL_ASSERT (!IsDestroyed (),
                "Calling TransactedFile::Object::AllocBlock on a destroyed object.");
            if (THEKOGANS_UTIL_LIKELY (file != nullptr)) {
                std::size_t objectSize = 0;
                {
                    Range range (file, 0, 0);
                    size = Size (range);
                    SerializableHeader header{Type (), Version (), size};
                    objectSize = UI32_SIZE + file->Size (header) + size;
                }
                bool needsRealloc = true;
                if (THEKOGANS_UTIL_LIKELY (offset != 0)) {
                    SizeType currentBlockSize = Block::GetSize (file, offset);
                    needsRealloc =
                        objectSize > currentBlockSize ||
                        (objectSize < (currentBlockSize >> 1) &&
                            (currentBlockSize - objectSize) >= TransactedFile::MIN_BLOCK_SIZE);
                }
                if (needsRealloc) {
                    AddressType newOffset = file->GetAllocator ()->Realloc (offset, objectSize, false);
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
                SetDestroyed ();
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
                ObjectRange range (file, offset);
                if (THEKOGANS_UTIL_LIKELY (range.context.WeakCompare (SerializableHeader{Type (), Version ()}))) {
                    size = range.context.size;
                    Read (range);
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Error reloading TransactedFile::Object, header mismatch. "
                        "On disk: type %s, version " THEKOGANS_UTIL_UI64_FORMAT ". "
                        "Expecting: type %s, version " THEKOGANS_UTIL_SIZE_T_FORMAT ". ",
                        range.context.type.c_str (), range.context.version.value,
                        Type ().c_str (), Version ());
                }
            }
        }

        void TransactedFile::Object::RecordReference (
                TransactedFile *file,
                AddressType offset,
                BlockMap &blockMap) {
            if (file != nullptr && offset != 0) {
                // Go straight to the source.
                Block block (file, offset);
                block.Read ();
                // We're collecting diagnostics here. An empty range context is appropriate.
                ObjectRange range (file, offset, true);
                auto [it, inserted] = blockMap.try_emplace (block, BlockInfo{range.context});
                if (!inserted) {
                    ++it->second.count;
                }
            }
        }

        void TransactedFile::Object::OnTransactedFileTransactionCommit (
                TransactedFile */*file*/,
                int phase) noexcept {
            // It shouldn't happen as SetDestroyed unsubscribes us from files events list.
            // We guard here in case we got pulled out of the list (by \see{Transaction})
            // before we were able to unsubscribe.
            if (THEKOGANS_UTIL_LIKELY (!IsDestroyed ())) {
                if (phase == TransactedFile::Transaction::PHASE_ALLOC) {
                    AllocBlock ();
                }
                else if (phase == TransactedFile::Transaction::PHASE_FLUSH) {
                    Flush ();
                }
            }
        }

        void TransactedFile::Object::OnTransactedFileTransactionAbort (
                TransactedFile */*file*/) noexcept {
            // Same reasoning applies here as in OnTransactedFileTransactionCommit above.
            if (THEKOGANS_UTIL_LIKELY (!IsDestroyed ())) {
                Reload ();
            }
        }

        _LIB_THEKOGANS_UTIL_DECL TransactedFile::ObjectRange & _LIB_THEKOGANS_UTIL_API operator >> (
                TransactedFile::ObjectRange &range,
                TransactedFile::Object::SharedPtr &object) {
            if (range.factory != nullptr) {
                object = range.factory (range.parameters);
            }
            else {
                object = TransactedFile::Object::CreateType (range.context.type, range.parameters);
            }
            if (THEKOGANS_UTIL_LIKELY (object != nullptr)) {
                object->Init (range.GetFile (), range.GetOffset ());
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Unable to create %s.", range.context.type.c_str ());
            }
            return range;
        }

        TransactedFile::ObjectRange::ObjectRange (
                TransactedFile *file,
                TransactedFile::AddressType offset,
                bool nothrow,
                util::Allocator::SharedPtr allocator) :
                BlockRange (file, offset, true, allocator) {
            context.Clear ();
            ui32 magic = 0;
            try {
                *this >> magic;
            }
            catch (...) {
                if (!nothrow) {
                    throw;
                }
            }
            if (THEKOGANS_UTIL_LIKELY (magic == Object::MAGIC)) {
                try {
                    *this >> context;
                }
                catch (...) {
                    if (!nothrow) {
                        throw;
                    }
                    return;
                }
                if (THEKOGANS_UTIL_UNLIKELY (context.NeedType ()) && THEKOGANS_UTIL_LIKELY (!nothrow)) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Invalid TransactedFile::Object type @" THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
                if (THEKOGANS_UTIL_UNLIKELY (context.NeedVersion ()) && THEKOGANS_UTIL_LIKELY (!nothrow)) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Invalid TransactedFile::Object version for type %s @" THEKOGANS_UTIL_UI64_FORMAT,
                        context.type.c_str (), offset);
                }
                if (THEKOGANS_UTIL_UNLIKELY (context.size > GetDataAvailable ()) && THEKOGANS_UTIL_LIKELY (!nothrow)) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Invalid TransactedFile::Object size for type %s @" THEKOGANS_UTIL_UI64_FORMAT,
                        context.type.c_str (), offset);
                }
            }
            else if (THEKOGANS_UTIL_LIKELY (!nothrow)) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFile::Object: @" THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        }

        TransactedFile::ObjectRange::ObjectRange (
                TransactedFile *file,
                TransactedFile::AddressType offset,
                Object &object,
                util::Allocator::SharedPtr allocator) :
                BlockRange (file, offset, false, allocator) {
            SerializableHeader header{object.Type (), object.Version (), object.size};
            *this << Object::MAGIC << header;
            context = header;
        }

    } // namespace util
} // namespace thekogans
