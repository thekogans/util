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


#include "thekogans/util/LockGuard.h"
#include "thekogans/util/FixedSizeFileAllocator.h"

namespace thekogans {
    namespace util {

        FileAllocator::PtrType FixedSizeFileAllocator::Alloc (std::size_t size) {
            PtrType offset = nullptr;
            if (size <= header.blockSize) {
                LockGuard<SpinLock> guard (spinLock);
                if (header.freeBlock != nullptr) {
                    offset = header.freeBlock;
                    file.Seek ((ui64)offset, SEEK_SET);
                    file >> header.freeBlock;
                    Save ();
                }
                else {
                    offset = (PtrType)file.GetSize ();
                    file.SetSize ((ui64)offset + header.blockSize);
                }
            }
            return offset;
        }

        void FixedSizeFileAllocator::Free (
                PtrType offset,
                std::size_t size) {
            if (size <= header.blockSize) {
                bool save = false;
                LockGuard<SpinLock> guard (spinLock);
                if (header.rootBlock == offset) {
                    header.rootBlock = nullptr;
                    save = true;
                }
                if ((ui64)offset + header.blockSize < file.GetSize ()) {
                    file.Seek ((ui64)offset, SEEK_SET);
                    file << header.freeBlock;
                    header.freeBlock = offset;
                    save = true;
                }
                else {
                    while ((ui64)header.freeBlock == (ui64)offset - header.blockSize) {
                        offset = header.freeBlock;
                        file.Seek ((ui64)header.freeBlock, SEEK_SET);
                        file >> header.freeBlock;
                        save = true;
                    }
                    file.SetSize ((ui64)offset);
                }
                if (save) {
                    Save ();
                }
            }
        }

    } // namespace util
} // namespace thekogans
