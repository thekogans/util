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
                FileAllocator::PtrType offset_) :
                // If we're a transaction participant, it will tell us when to commit.
                BufferedFileTransactionParticipant (
                    fileAllocator_->GetFile ()->GetTransaction ()),
                fileAllocator (fileAllocator_),
                offset (offset_) {
            Subscriber<BufferedFileEvents>::Subscribe (*fileAllocator->GetFile ());
        }

        void FileAllocatorObject::Alloc () {
            ui64 blockSize = 0;
            if (offset != 0) {
                FileAllocator::BlockInfo block (offset);
                fileAllocator->GetBlockInfo (block);
                blockSize = block.GetSize ();
            }
            std::size_t size = Size ();
            if (blockSize < size) {
                if (offset != 0) {
                    fileAllocator->Free (offset);
                }
                offset = fileAllocator->Alloc (size);
                Produce (
                    std::bind (
                        &FileAllocatorObjectEvents::OnFileAllocatorObjectOffsetChanged,
                        std::placeholders::_1,
                        this));
            }
        }

    } // namespace util
} // namespace thekogans
