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
        /// base class. Database would be more accuratelly called a database
        /// kernel or engine in that it does not prescribe any particular data
        /// structure (OO, relational...) but offers the bare minimum foundation
        /// for building those things. Specifically Database is built on top
        /// of \see{BufferedFile} and takes advantage of it's built in transaction
        /// processing. \see{FileAllocator} is used to allocate random size
        /// blocks from the file. \see{FileAllocatorRegistry} is available for
        /// storing system wide named \see{BTree::Value}s. Database is meant to
        /// be derived from.
        struct _LIB_THEKOGANS_UTIL_DECL Database {
        protected:
            /// \brief
            /// \see{BufferedFile} where the database lives.
            BufferedFile::SharedPtr file;
            /// \brief
            /// \see{FileAllocator} for managing random size blocks in the file.
            FileAllocator::SharedPtr fileAllocator;
            /// \brief
            /// \see{FileAllocatorRegistry} for system wide name/value pairs.
            FileAllocatorRegistry::SharedPtr registry;

        public:
            /// \brief
            /// ctor.
            /// \param[in] path Path to database file.
            /// \param[in] secure true == \see{FileAllocator} will zero fill freed blocks.
            /// \param[in] btreeEntriesPerNode Number of entries per \see{FileAllocator::BTree::Node}.
            /// \param[in] btreeNodesPerPage Number of \see{FileAllocator::BTree::Node}s that will
            /// fit in to a \see{BlockAllocator} page.
            /// \param[in] registryEntriesPerNode Number of entries per \see{BTree::Node}.
            /// \param[in] registryNodesPerPage Number of \see{BTree::Node}s that will
            /// fit in to a \see{BlockAllocator} page.
            /// \param[in] allocator \see{Allocator} for \see{FileAllocator::BTree} and \see{BTree}.
            Database (
                const std::string &path,
                bool secure = false,
                std::size_t btreeEntriesPerNode = FileAllocator::DEFAULT_BTREE_ENTRIES_PER_NODE,
                std::size_t btreeNodesPerPage = FileAllocator::DEFAULT_BTREE_NODES_PER_PAGE,
                std::size_t registryEntriesPerNode =
                    FileAllocatorRegistry::DEFAULT_BTREE_ENTRIES_PER_NODE,
                std::size_t registryNodesPerPage =
                    FileAllocatorRegistry::DEFAULT_BTREE_NODES_PERP_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            /// \brief
            /// dtor.
            virtual ~Database () {}

            /// \brief
            /// Return the file.
            /// \return file.
            inline BufferedFile::SharedPtr GetFile () const {
                return file;
            };

            /// \brief
            /// Return the fileAllocator.
            /// \return fileAllocator.
            inline FileAllocator::SharedPtr GetFileAllocator () const {
                return fileAllocator;
            };

            /// \brief
            /// Return the registry
            /// \return registry.
            inline FileAllocatorRegistry::SharedPtr GetRegistry () const {
                return registry;
            };
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Database_h)
