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

#include "thekogans/kcd/Database.h"

namespace thekogans {
    namespace kcd {

        Database::Database (
                const std::string &path,
                bool secure,
                std::size_t btreeEntriesPerNode,
                std::size_t btreeNodesPerPage,
                std::size_t registryEntriesPerNode_,
                std::size_t registryNodesPerPage_,
                util::Allocator::SharedPtr allocator_) :
                file (
                    new util::SimpleBufferedFile (
                        util::HostEndian,
                        path,
                        util::SimpleFile::ReadWrite | util::SimpleFile::Create)),
                registryEntriesPerNode (registryEntriesPerNode_),
                registryNodesPerPage (registryNodesPerPage_),
                allocator (allocator_) {
            util::BufferedFile::Transaction transaction (file);
            fileAllocator.Reset (
                new util::FileAllocator (
                    file,
                    secure,
                    btreeEntriesPerNode,
                    btreeNodesPerPage,
                    allocator));
            transaction.Commit ();
        }

    } // namespace kcd
} // namespace thekogans
