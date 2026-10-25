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
    #include "thekogans/util/TransactedFileBTree.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (thekogans::util::TransactedFile::Object)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void TransactedFile::Object::StaticInit () {
            thekogans::util::TransactedFileBTree::StaticInit ();
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


        void TransactedFile::Object::SetAggregate (bool aggregate_) {
            if (aggregate != aggregate_) {
                aggregate = aggregate_;
                if (aggregate) {
                    file->Unsubscribe (*this);
                }
                else {
                    SetDirty ();
                }
            }
        }

        void TransactedFile::Object::SetDirty () {
            if (!aggregate) {
                file->Subscribe (*this);
            }
            Produce (
                std::bind (
                    &ObjectEvents::OnTransactedFileObjectDirty,
                    std::placeholders::_1,
                    this));
        }

        void TransactedFile::Object::AllocBlock () {
            if (!aggregate) {
                std::size_t size = GetSize ();
                if (offset == 0 || size > TransactedFile::Block::GetSize (file, offset)) {
                    AddressType newOffset =
                        file->GetAllocator ()->Realloc (offset, size, false);
                    if (offset != newOffset) {
                        if (offset != 0) {
                            Produce (
                                std::bind (
                                    &ObjectEvents::OnTransactedFileObjectFreeBlock,
                                    std::placeholders::_1,
                                    this));
                        }
                        offset = newOffset;
                        Produce (
                            std::bind (
                                &ObjectEvents::OnTransactedFileObjectAllocBlock,
                                std::placeholders::_1,
                                this));
                    }
                }
            }
        }

        void TransactedFile::Object::FreeBlock () {
            if (offset != 0 && !aggregate) {
                file->GetAllocator ()->Free (offset);
                Produce (
                    std::bind (
                        &ObjectEvents::OnTransactedFileObjectFreeBlock,
                        std::placeholders::_1,
                        this));
                offset = 0;
            }
        }

        void TransactedFile::Object::Flush () {
            if (offset != 0 && !aggregate) {
                BlockRange range (file, offset, false);
                range << *this;
                if (file->GetAllocator ()->IsSecure ()) {
                    range.Seek (
                        SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable ()),
                        SEEK_CUR);
                }
            }
        }

        void TransactedFile::Object::Reload () {
            if (offset != 0 && !aggregate) {
                BlockRange range (file, offset);
                range >> *this;
            }
        }

        void TransactedFile::Object::OnTransactedFileTransactionCommit (
                TransactedFile::SharedPtr /*file*/,
                int phase) noexcept {
            if (!aggregate) {
                THEKOGANS_UTIL_TRY {
                    if (phase == TransactedFile::COMMIT_PHASE_1) {
                        AllocBlock ();
                    }
                    else if (phase == TransactedFile::COMMIT_PHASE_2) {
                        Flush ();
                    }
                }
                THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
            }
        }

        void TransactedFile::Object::OnTransactedFileTransactionAbort (
                TransactedFile::SharedPtr /*file*/) noexcept {
            if (!aggregate) {
                THEKOGANS_UTIL_TRY {
                    Reload ();
                }
                THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
            }
        }

    } // namespace util
} // namespace thekogans
