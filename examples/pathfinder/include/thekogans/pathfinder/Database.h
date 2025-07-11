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

#if !defined (__thekogans_pathfinder_Database_h)
#define __thekogans_pathfinder_Database_h

#include <string>
#include "thekogans/util/Singleton.h"
#include "thekogans/util/BufferedFile.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/FileAllocatorRegistry.h"
#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/pathfinder/Options.h"

namespace thekogans {
    namespace pathfinder  {

        /// \struct Database Database.h thekogans/pathfinder/Database.h
        ///
        /// \brief
        /// Database puts all the relavant pieces together in to a convenient
        /// system wide \see{util::Singleton}.
        struct Database : public util::Singleton<Database> {
        protected:
            /// \brief
            /// \see{util::BufferedFile} where the database lives.
            util::BufferedFile::SharedPtr file;
            /// \brief
            /// \see{util::FileAllocator} for managing random size blocks in the file.
            util::FileAllocator::SharedPtr fileAllocator;
            /// \brief
            /// Number of entries per \see{util::BTree::Node}.
            std::size_t registryEntriesPerNode;
            /// \brief
            /// Number of \see{util::BTree::Node}s that will fit in to a \see{util::BlockAllocator} page.
            std::size_t registryNodesPerPage;
            /// \brief
            /// \see{Allocator} for \see{util::FileAllocator::BTree} and \see{util::BTree}.
            util::Allocator::SharedPtr allocator;
            /// \brief
            /// \see{util::FileAllocatorRegistry} for system wide name/value pairs.
            util::FileAllocatorRegistry::SharedPtr registry;
            /// \brief
            /// Protect registry creation.
            util::SpinLock spinLock;

        public:
            /// \brief
            /// ctor.
            /// \param[in] path Path to database file.
            /// \param[in] secure true == \see{util::FileAllocator} will zero fill freed blocks.
            /// \param[in] btreeEntriesPerNode Number of entries per \see{util::FileAllocator::BTree::Node}.
            /// \param[in] btreeNodesPerPage Number of \see{util::FileAllocator::BTree::Node}s that will
            /// fit in to a \see{util::BlockAllocator} page.
            /// \param[in] registryEntriesPerNode Number of entries per \see{util::BTree::Node}.
            /// \param[in] registryNodesPerPage Number of \see{util::BTree::Node}s that will
            /// fit in to a \see{util::BlockAllocator} page.
            /// \param[in] allocator \see{util::Allocator} for
            /// \see{util::FileAllocator::BTree} and \see{util::BTree}.
            Database (
                const std::string &path = Options::Instance ()->dbPath,
                bool secure = false,
                std::size_t btreeEntriesPerNode =
                    util::FileAllocator::DEFAULT_BTREE_ENTRIES_PER_NODE,
                std::size_t btreeNodesPerPage =
                    util::FileAllocator::DEFAULT_BTREE_NODES_PER_PAGE,
                std::size_t registryEntriesPerNode_ =
                    util::FileAllocatorRegistry::DEFAULT_BTREE_ENTRIES_PER_NODE,
                std::size_t registryNodesPerPage_ =
                    util::FileAllocatorRegistry::DEFAULT_BTREE_NODES_PER_PAGE,
                util::Allocator::SharedPtr allocator_ = util::DefaultAllocator::Instance ());

            /// \brief
            /// Return the file.
            /// \return file.
            inline util::BufferedFile::SharedPtr GetFile () const {
                return file;
            }

            /// \brief
            /// Return the fileAllocator.
            /// \return fileAllocator.
            inline util::FileAllocator::SharedPtr GetFileAllocator () const {
                return fileAllocator;
            }

            /// \brief
            /// Return the registry.
            /// NOTE: Registry contains user defined types. These types
            /// will need to read themselves from the file. We have to
            /// create it outside the ctor to prevent potential deadlock
            /// with database creation.
            /// \return registry.
            util::FileAllocatorRegistry::SharedPtr GetRegistry ();
        };

    } // namespace pathfinder
} // namespace thekogans

#endif // !defined (__thekogans_pathfinder_Database_h)
