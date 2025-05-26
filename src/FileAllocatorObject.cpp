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

#include "thekogans/util/FileAllocatorObject.h"

namespace thekogans {
    namespace util {

        FileAllocatorObject::FileAllocatorObject (
                FileAllocator::SharedPtr fileAllocator_,
                FileAllocator::PtrType offset_,
                BufferedFile::Transaction::SharedPtr transaction) :
                // If we're a transaction participant, it will tell us when to commit.
                BufferedFileTransactionParticipant (offset_ == 0 ? transaction : nullptr),
                fileAllocator (fileAllocator_),
                offset (offset_) {
            Subscriber<BufferedFileEvents>::Subscribe (*fileAllocator->GetFile ());
            if (offset != 0 && transaction != nullptr) {
                Subscriber<BufferedFile::TransactionEvents>::Subscribe (*transaction);
            }
        }

    } // namespace util
} // namespace thekogans
