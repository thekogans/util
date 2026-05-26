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

        void TransactedFile::TransactionParticipant::Delete () {
            Reset ();
            if (!IsDeleted ()) {
                SetDeleted (true);
                AddRef ();
            }
        }

        void TransactedFile::TransactionParticipant::OnTransactedFileTransactionCommit (
                TransactedFile::SharedPtr /*file*/,
                int phase) noexcept {
            THEKOGANS_UTIL_TRY {
                if (phase == COMMIT_PHASE_1) {
                    assert (IsDeleted () || IsDirty ());
                    if (IsDeleted ()) {
                        Free ();
                    }
                    if (IsDirty ()) {
                        Alloc ();
                    }
                    if (IsDeleted ()) {
                        SetDeleted (false);
                        Release ();
                    }
                }
                else if (phase == COMMIT_PHASE_2) {
                    assert (!IsDeleted () && IsDirty ());
                    Flush ();
                    SetDirty (false);
                }
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        void TransactedFile::TransactionParticipant::OnTransactedFileTransactionAbort (
                TransactedFile::SharedPtr /*file*/) noexcept {
            THEKOGANS_UTIL_TRY {
                assert (flags);
                Reload ();
                SetFlag (FLAGS_DIRTY | FLAGS_DELETED, false);
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        void TransactedFile::TransactionParticipant::SetFlag (
                ui32 flag,
                bool on) {
            ui32 oldFlags = flags.SetAll (flag, on);
            if (oldFlags != flags) {
                if (oldFlags == 0 && flags != 0) {
                    Subscriber<TransactedFileEvents>::Subscribe (*file);
                }
                else if (oldFlags != 0 && flags == 0) {
                    Subscriber<TransactedFileEvents>::Unsubscribe (*file);
                }
            }
        }

        TransactedFile::Allocator::PtrType TransactedFile::Object::ForceFlush () {
            SetDirty (true);
            Alloc ();
            Flush ();
            return GetOffset ();
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
            assert (GetOffset () != 0);
            TransactedFile::BlockRange buffer (*file, GetOffset (), false);
            Write (buffer);
            if (GetAllocator ()->IsSecure ()) {
                buffer.Seek (
                    SecureZeroMemory (buffer.GetDataPtr (), buffer.GetDataAvailable ()),
                    SEEK_CUR);
            }
        }

        void TransactedFile::Object::Reload () {
            Reset ();
            if (GetOffset () != 0) {
                TransactedFile::BlockRange buffer (*file, GetOffset ());
                Read (buffer);
            }
        }

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (TransactedFile::SerializableObject)

        Serializable::SharedPtr TransactedFile::SerializableObject::GetObject () {
            if (object == nullptr) {
                Reload ();
            }
            return object;
        }

        void TransactedFile::SerializableObject::SetObject (Serializable::SharedPtr object_) {
            object = object_;
        }

        void TransactedFile::SerializableObject::Read (Serializer &serializer) {
            Serializer::ContextGuard guard (serializer, context, factory, parameters);
            serializer >> object;
        }

        void TransactedFile::SerializableObject::Write (Serializer &serializer) {
            Serializer::ContextGuard guard (serializer, context, factory, parameters);
            serializer << *object;
        }

    } // namespace util
} // namespace thekogans
