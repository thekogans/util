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
#include "thekogans/util/TransactedFile.h"
#include "thekogans/util/TransactedFileTypeDictionary.h"
#include "thekogans/util/TransactedFileBTreeAllocator.h"
#include "thekogans/util/TransactedFileBTreeRegistry.h"
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
            /// \see{util::TransactedFile} where the database lives.
            util::TransactedFile::SharedPtr file;

        public:
            /// \brief
            /// ctor.
            /// \param[in] path Path to database file.
            /// \param[in] secure true == \see{util::FileAllocator} will zero fill freed blocks.
            /// \param[in] allocatorEntriesPerNode Number of entries per \see{util::TransactedFileBTreeAllocator::Node}.
            /// \param[in] registryValueAsObject
            /// \param[in] registryEntriesPerNode Number of entries per \see{util::TransactedFileBTree::Node}.
            Database (
                    const std::string &path = Options::Instance ()->dbPath,
                    bool secure = false,
                    std::size_t allocatorEntriesPerNode =
                        util::TransactedFileBTreeAllocatorBTree::DEFAULT_ENTRIES_PER_NODE,
                    bool registryValueAsObject = true,
                    std::size_t registryEntriesPerNode =
                        util::TransactedFileBTree::DEFAULT_ENTRIES_PER_NODE) :
                    file (util::TransactedFile::Open (path)) {
                if (file == nullptr) {
                    file = util::TransactedFile::Create (
                        util::HostEndian,
                        path,
                        secure,
                        new util::TransactedFileTypeDictionary (nullptr, 0),
                        new util::TransactedFileBTreeAllocator (
                            nullptr,
                            0,
                            allocatorEntriesPerNode),
                        new util::TransactedFileBTreeRegistry (
                            nullptr,
                            0,
                            registryValueAsObject,
                            registryEntriesPerNode));
                }
            }

            /// \brief
            /// Return the file.
            /// \return file.
            inline util::TransactedFile::SharedPtr GetFile () const {
                return file;
            }

            /// \brief
            /// Return the allocator.
            /// \return allocator.
            inline util::TransactedFile::Allocator::SharedPtr GetAllocator () const {
                return file->GetAllocator ();
            }

            /// \brief
            /// Return the registry.
            /// \return registry.
            inline util::TransactedFile::Registry::SharedPtr GetRegistry () const {
                return file->GetRegistry ();
            }
        };

    } // namespace pathfinder
} // namespace thekogans

#endif // !defined (__thekogans_pathfinder_Database_h)
