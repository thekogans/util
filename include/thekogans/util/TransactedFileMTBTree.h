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

#if !defined (__thekogans_util_TransactedFileMTBTree_h)
#define __thekogans_util_TransactedFileMTBTree_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/TransactedFileBTree.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        /// \struct TransactedFileMTBTree TransactedFileMTBTree.h thekogans/util/TransactedFileMTBTree.h
        ///
        /// \brief
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileMTBTree :
                public virtual TransactedFile::Object,
                private TransactedFileBTree {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT (TransactedFileMTBTree)

        private:
            /// \brief
            /// Synchronization lock.
            Mutex mutex;

        public:
            static const std::size_t DEFAULT_ENTRIES_PER_NODE = 256;
            static const std::size_t DEFAULT_NODES_PER_PAGE = 16;

            TransactedFileMTBTree (
                const SerializableHeader &keyContext = SerializableHeader (),
                const SerializableHeader &valueContext = SerializableHeader (),
                bool valueIsObject = false,
                std::size_t entriesPerNode = DEFAULT_ENTRIES_PER_NODE,
                std::size_t nodesPerPage = DEFAULT_NODES_PER_PAGE) :
                TransactedFileBTree (keyContext, valueContext, valueIsObject, entriesPerNode, nodesPerPage) {}

            /// \brief
            /// Search for the given key in the btree.
            /// \param[in] key \see{Key} to search for.
            /// \return true == found.
            Serializable::SharedPtr Find (const Key &key);
            /// \brief
            /// Insert the given key in to the btree.
            /// \param[in] key Key to insert.
            /// \param[in] value Value associated with the given key.
            /// \return true == added. false == duplicate. If false, return
            /// the current entry in it.
            bool Insert (
                Key::SharedPtr key,
                Serializable::SharedPtr value);
            /// \brief
            /// Delete the given key from the btree.
            /// \param[in] key Key to delete.
            /// \return true == entry deleted. false == entry not found.
            bool Remove (const Key &key);

            // Forward scan over [firstPrefix, lastPrefix) (or full tree when both are nullptr):
            // Acquires `first` and `last` once, steps `batchSize` entries per lock acquisition,
            // releases the lock while invoking `callback(key, value)`, and resumes directly from `first`.
            template<typename Callback>
            void ForEach (
                    TransactedFileBTree::Key::SharedPtr firstPrefix,
                    TransactedFileBTree::Key::SharedPtr lastPrefix,
                    Callback &&callback,
                    std::size_t batchSize = 64) {
                THEKOGANS_UTIL_ASSERT (batchSize > 0, "batchSize must be > 0.");
                using Entry = std::pair<TransactedFileBTree::Key::SharedPtr, Serializable::SharedPtr>;
                std::vector<Entry> batch;
                batch.reserve (batchSize);
                TransactedFileBTree::Iterator first;
                TransactedFileBTree::Iterator last;
                {
                    LockGuard<Mutex> guard (mutex);
                    first = LowerBound (firstPrefix);
                    last = UpperBound (lastPrefix);
                }
                while (true) {
                    batch.clear ();
                    {
                        LockGuard<Mutex> guard (mutex);
                        for (; first != last && batch.size () < batchSize; ++first) {
                            batch.emplace_back (first.GetKey (), first.GetValue ());
                        }
                    }
                    if (batch.empty ()) {
                        break;
                    }
                    for (const auto &[key, value] : batch) {
                        if (!callback (key, value)) {
                            return;
                        }
                    }
                    if (batch.size () < batchSize) {
                        break;
                    }
                }
            }

            // Symmetric reverse scan over `prefix` (or full tree when `prefix == nullptr`):
            template<typename Callback>
            void ForEachReverse (
                    TransactedFileBTree::Key::SharedPtr prefix,
                    Callback &&callback,
                    std::size_t batchSize = 64) {
                THEKOGANS_UTIL_ASSERT (batchSize > 0, "batchSize must be > 0.");
                using Entry = std::pair<TransactedFileBTree::Key::SharedPtr, Serializable::SharedPtr>;
                std::vector<Entry> batch;
                batch.reserve (batchSize);
                TransactedFileBTree::Iterator it;
                {
                    LockGuard<Mutex> guard (mutex);
                    it = FindLast (prefix);
                }
                while (true) {
                    batch.clear ();
                    {
                        LockGuard<Mutex> guard (mutex);
                        for (; !it.IsFinished () && batch.size () < batchSize; it.Prev ()) {
                            batch.emplace_back (it.GetKey (), it.GetValue ());
                        }
                    }
                    if (batch.empty ()) {
                        break;
                    }
                    for (const auto &[key, value] : batch) {
                        if (!callback (key, value)) {
                            return;
                        }
                    }
                    if (batch.size () < batchSize) {
                        break;
                    }
                }
            }

            virtual void Init (
                TransactedFile *file,
                TransactedFile::AddressType offset) override;

            /// \brief
            /// TransactedFileMTBTree is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (TransactedFileMTBTree)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_TransactedFileMTBTree_h)
