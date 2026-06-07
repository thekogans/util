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

#include "thekogans/util/Heap.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        void TransactedFile::TransactionParticipant::OnTransactedFileTransactionCommit (
                TransactedFile::SharedPtr /*file*/,
                int phase) noexcept {
            THEKOGANS_UTIL_TRY {
                assert (IsDirty ());
                if (phase == COMMIT_PHASE_1) {
                    Alloc ();
                }
                else if (phase == COMMIT_PHASE_2) {
                    Flush ();
                    SetDirty (false);
                }
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        void TransactedFile::TransactionParticipant::OnTransactedFileTransactionAbort (
                TransactedFile::SharedPtr /*file*/) noexcept {
            THEKOGANS_UTIL_TRY {
                assert (IsDirty ());
                Reload ();
                SetDirty (false);
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        bool TransactedFile::TransactionParticipant::SetDirty (bool dirty) {
            // Only subscribe @the transition from clean to dirty.
            if (!flags.Set (FLAGS_DIRTY, dirty) && dirty) {
                Subscriber<TransactedFileEvents>::Subscribe (*file);
                return true;
            }
            return false;
        }

        TransactedFile::Allocator::PtrType TransactedFile::Object::ForceFlush () {
            if (IsDirty ()) {
                Subscriber<TransactedFileEvents>::Unsubscribe (*file);
                Alloc ();
                Flush ();
                SetDirty (false);
            }
            return GetOffset ();
        }

        bool TransactedFile::Object::SetDirty (bool dirty) {
            if (TransactionParticipant::SetDirty (dirty)) {
                Produce (
                    std::bind (
                        &ObjectEvents::OnTransactedFileObjectDirty,
                        std::placeholders::_1,
                        this));
                return true;
            }
            return false;
        }

        void TransactedFile::Object::Alloc () {
            if (!IsFixedSize () || offset == 0) {
                Allocator::PtrType newOffset =
                    GetAllocator ()->Realloc (offset, Size (), false);
                if (offset != newOffset) {
                    if (offset != 0) {
                        Produce (
                            std::bind (
                                &ObjectEvents::OnTransactedFileObjectFree,
                                std::placeholders::_1,
                                this));
                    }
                    offset = newOffset;
                    Produce (
                        std::bind (
                            &ObjectEvents::OnTransactedFileObjectAlloc,
                            std::placeholders::_1,
                            this));
                }
            }
        }

        void TransactedFile::Object::Free () {
            if (offset != 0) {
                GetAllocator ()->Free (offset);
                Produce (
                    std::bind (
                        &ObjectEvents::OnTransactedFileObjectFree,
                        std::placeholders::_1,
                        this));
                offset = 0;
            }
        }

        void TransactedFile::Object::Flush () {
            assert (IsDirty ());
            assert (offset != 0);
            BlockRange range (*file, offset, false);
            Write (range);
            if (GetAllocator ()->IsSecure ()) {
                range.Seek (
                    SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable ()),
                    SEEK_CUR);
            }
        }

        void TransactedFile::Object::Reload () {
            if (offset != 0) {
                BlockRange range (*file, offset);
                Read (range);
            }
        }

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (TransactedFile::SerializableObject)

        Serializable::SharedPtr TransactedFile::SerializableObject::GetSerializable () {
            if (serializable == nullptr) {
                Reload ();
            }
            return serializable;
        }

        void TransactedFile::SerializableObject::SetSerializable (Serializable::SharedPtr serializable_) {
            serializable = serializable_;
        }

        void TransactedFile::SerializableObject::Read (Serializer &serializer) {
            Serializer::ContextGuard guard (serializer, context, factory, parameters);
            serializer >> serializable;
        }

        void TransactedFile::SerializableObject::Write (Serializer &serializer) {
            if (serializable != nullptr) {
                Serializer::ContextGuard guard (serializer, context, factory, parameters);
                serializer << *serializable;
            }
        }

    } // namespace util
} // namespace thekogans
