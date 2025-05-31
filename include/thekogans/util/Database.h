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

#if !defined (__thekogans_util_Database_h)
#define __thekogans_util_Database_h

#include <string>
#include "thekogans/util/BufferedFile.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/FileAllocatorRegistry.h"
#include "thekogans/util/DefaultAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct Database Database.h thekogans/util/Database.h
        ///
        /// \brief
        /// Database puts all the relavant pieces together in to a convenient
        /// base class.

        struct _LIB_THEKOGANS_UTIL_DECL Database {
        protected:
            BufferedFile::SharedPtr file;
            FileAllocator::SharedPtr fileAllocator;
            FileAllocatorRegistry::SharedPtr registry;

        public:
            Database (
                const std::string &path,
                bool secure = false,
                std::size_t btreeEntriesPerNode = FileAllocator::DEFAULT_BTREE_ENTRIES_PER_NODE,
                std::size_t btreeNodesPerPage = FileAllocator::DEFAULT_BTREE_NODES_PER_PAGE,
                std::size_t registryEntriesPerNode =
                    FileAllocatorRegistry::DEFAULT_REGISTRY_ENTRIES_PER_NODE,
                std::size_t registryNodesPerPage =
                    FileAllocatorRegistry::DEFAULT_REGISTRY_NODES_PERP_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            virtual ~Database () {}

            inline BufferedFile::SharedPtr GetFile () const {
                return file;
            };

            inline FileAllocator::SharedPtr GetFileAllocator () const {
                return fileAllocator;
            };

            inline FileAllocatorRegistry::SharedPtr GetRegistry () const {
                return registry;
            };
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Database_h)
