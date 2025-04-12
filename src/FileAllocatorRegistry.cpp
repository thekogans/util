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

#include "thekogans/util/FileAllocatorRegistry.h"

namespace thekogans {
    namespace util {

        FileAllocatorRegistry::FileAllocatorRegistry (
                FileAllocator::SharedPtr fileAllocator,
                std::size_t entriesPerNode,
                std::size_t nodesPerPage,
                Allocator::SharedPtr allocator) :
                BTree (
                    fileAllocator,
                    fileAllocator->GetRootOffset (),
                    BTree::GUIDKey::TYPE,
                    BTree::PtrValue::TYPE,
                    entriesPerNode,
                    nodesPerPage,
                    allocator) {
            if (fileAllocator->GetRootOffset () != GetOffset ()) {
                fileAllocator->SetRootOffset (GetOffset ());
            }
        }

        void FileAllocatorRegistry::Delete (FileAllocator &fileAllocator) {
            BTree::Delete (fileAllocator, fileAllocator.GetRootOffset ());
            fileAllocator.SetRootOffset (0);
        }

    } // namespace util
} // namespace thekogans
