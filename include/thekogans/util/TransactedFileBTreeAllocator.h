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

#if !defined (__thekogans_util_TransactedFileBTreeAllocator_h)
#define __thekogans_util_TransactedFileBTreeAllocator_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/TransactedFile.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/BlockAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct TransactedFileBTreeAllocator TransactedFileBTreeAllocator.h
        /// thekogans/util/TransactedFileBTreeAllocator.h
        ///
        /// \brief
        /// TransactedFileBTreeAllocator is a general purpose \see{TransactedFile::Allocator}.
        /// It uses a hand tuned \see{TransactedFileBTreeAllocator::BTree} to manage the free list.
        /// It is specifically tuned and is very sesitive to the fact that blocks that straddle
        /// page boundaries (see \see{PageMap::pageSize} incur a heavy performance penalty
        /// (see \see{TransactedFile::Range}). Every attempt is made to avoid that.
        /// TransactedFileBTreeAllocator is thread safe.
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileBTreeAllocator :
                public TransactedFile::Allocator,
                public Subscriber<TransactedFile::ObjectEvents> {
            /// \brief
            /// TransactedFileBTreeAllocator is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (TransactedFileBTreeAllocator)

        private:
            /// \brief
            /// Number of entries per \see{BTree::Node}.
            const std::size_t btreeEntriesPerNode;
            /// \brief
            /// Number of \see{BTree::Node}s that will fit in to a \see{BlockAllocator} page.
            const std::size_t btreeNodesPerPage;
            /// \brief
            /// \see{util::Allocator} for \see{BTree::Node}.
            util::Allocator::SharedPtr allocator;
            /// \struct TransactedFileBTreeAllocator::Header TransactedFileBTreeAllocator.h
            /// thekogans/util/TransactedFileBTreeAllocator.h
            ///
            /// \brief
            /// Header contains the global heap values.
            struct _LIB_THEKOGANS_UTIL_DECL Header {
                /// \brief
                /// Contains the offset of the \see{BTree::Header}.
                PtrType btreeOffset;

                /// \brief
                /// The size of the header on disk.
                static const std::size_t SIZE =
                    UI32_SIZE +     // magic
                    PTR_TYPE_SIZE;  // btreeOffset

                /// \brief
                /// ctor.
                Header () :
                    btreeOffset (0) {}
            } header;
            /// \brief
            /// Include the \see{BTree} header.
            /// I split it out because this file was getting too big to maintain.
            #include "thekogans/util/TransactedFileBTreeAllocatorBTree.h"
            /// \brief
            /// \see{BTree} to manage heap free space.
            BTree::SharedPtr btree;
            /// \brief
            /// Cache the size of the BTree::Node on disk so that we
            /// don't pay the price of calculating it everytime \see{Alloc}
            /// is called.
            std::size_t btreeNodeFileSize;
            /// \brief
            /// Synchronization lock.
            mutable SpinLock spinLock;

        public:
            /// \brief
            /// Allocator size on disk.
            static const std::size_t SIZE = Allocator::Header::SIZE + Header::SIZE;

            // NOTE: The following constants are meant to be tuned during
            // system integration to provide the best performance for your
            // needs.
            /// \brief
            /// Default number of entries per \see{BTree::Node}.
            static const std::size_t DEFAULT_BTREE_ENTRIES_PER_NODE = 256;
            /// \brief
            /// Number of \see{BTree::Node}s that will fit in to a
            /// \see{BlockAllocator} page.
            static const std::size_t DEFAULT_BTREE_NODES_PER_PAGE = 10;

            /// \brief
            /// ctor.
            /// \param[in] secure true == zero out free blocks.
            /// \param[in] btreeEntriesPerNode Number of entries per \see{BTree::Node}.
            /// \param[in] btreeNodesPerPage Number of \see{BTree::Node}s that will fit
            /// in to a \see{BlockAllocator} page.
            /// \param[in] allocator_ \see{util::Allocator} for \see{BTree::Node}.
            TransactedFileBTreeAllocator (
                bool secure = false,
                std::size_t btreeEntriesPerNode_ = DEFAULT_BTREE_ENTRIES_PER_NODE,
                std::size_t btreeNodesPerPage_ = DEFAULT_BTREE_NODES_PER_PAGE,
                util::Allocator::SharedPtr allocator_ = DefaultAllocator::Instance ()) :
                Allocator (secure),
                btreeEntriesPerNode (btreeEntriesPerNode_),
                btreeNodesPerPage (btreeNodesPerPage_),
                allocator (allocator_),
                btreeNodeFileSize (BTree::Node::FileSize (btreeNodesPerPage)) {}

            // Allocator
            /// \brief
            /// Allocate a block.
            /// \param[in] size Size of block to allocate.
            /// \return Offset to the allocated block.
            virtual PtrType Alloc (std::size_t size) override;
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// \param[in] offset Offset of block to free.
            virtual void Free (PtrType offset) override;
            /// \brief
            /// Resize a block. Make it bigger or smaller.
            /// \param[in] offset Offset of \see{Block} to resize.
            /// \param[in] newSize New \see{Block} size.
            /// \param[in] moveData true == Copy the data to the new block.
            /// \return If newSize is greater than current size, return the
            /// new \see{Block} offset. If not, return the old \see{Block}
            /// offset.
            virtual PtrType Realloc (
                PtrType offset,
                std::size_t newSize,
                bool moveData = true) override;

        private:
            // Serializable
            /// \brief
            /// Read the \see{Header} from the given \see{Serializer}.
            virtual void Read (
                const SerializableHeader & /*header*/,
                Serializer &serializer) override;
            /// \brief
            /// Write the \see{Header} to the given \see{Serializer}.
            virtual void Write (Serializer &serializer) const override;

            // TransactedFileEvents
            /// \brief
            /// Transaction is committing:
            /// Phase 1: Nothing to do as we're the special first block.
            /// Phase 2: Flush our and \see{Allocator::Header} to file.
            /// \param[in] file \see{TransactedFile} commiting the transaction.
            /// \param[in] phase Either COMMIT_PHASE_1 or COMMIT_PHASE_2.
            virtual void OnTransactedFileTransactionCommit (
                TransactedFile::SharedPtr file,
                int phase) noexcept override;
            /// \brief
            /// Transaction is aborting:
            /// Reload our and \see{Allocator::Header} from disk.
            /// \param[in] file \see{TransactedFile} aborting the transaction.
            virtual void OnTransactedFileTransactionAbort (
                TransactedFile::SharedPtr file) noexcept override;

            // TransactedFile::ObjectEvents
            /// \brief
            /// \see{BTree} allocated it's header block.
            /// \param[in] object \see{BTree} that allocated it's header block.
            virtual void OnTransactedFileObjectAlloc (
                TransactedFile::Object::SharedPtr object) noexcept override;
            /// \brief
            /// \see{BTree} freed it's header block.
            /// \param[in] object \see{BTree} that freed it's header block.
            virtual void OnTransactedFileObjectFree (
                TransactedFile::Object::SharedPtr /*object*/) noexcept override;

            /// \brief
            /// Needs access to private members.
            friend Serializer &operator << (
                Serializer &serializer,
                const Header &header);
            /// \brief
            /// Needs access to private members.
            friend Serializer &operator >> (
                Serializer &serializer,
                Header &header);

            /// \brief
            /// Needs access to private members.
            friend bool operator == (
                const BTree::KeyType &key1,
                const BTree::KeyType &key2);
            /// \brief
            /// Needs access to private members.
            friend bool operator != (
                const BTree::KeyType &key1,
                const BTree::KeyType &key2);
            /// \brief
            /// Needs access to private members.
            friend bool operator < (
                const BTree::KeyType &key1,
                const BTree::KeyType &key2);
            /// \brief
            /// Needs access to private members.
            friend bool operator > (
                const BTree::KeyType &key1,
                const BTree::KeyType &key2);
            /// \brief
            /// Needs access to private members.
            friend Serializer &operator << (
               Serializer &serializer,
               const BTree::KeyType &key);
            /// \brief
            /// Needs access to private members.
            friend Serializer &operator >> (
                Serializer &serializer,
                BTree::KeyType &key);

            /// \brief
            /// Needs access to private members.
            friend Serializer &operator << (
                Serializer &serializer,
                const BTree::Node::Entry &entry);
            /// \brief
            /// Needs access to private members.
            friend Serializer &operator >> (
                Serializer &serializer,
                BTree::Node::Entry &entry);

            /// \brief
            /// Needs access to private members.
            friend Serializer &operator << (
                Serializer &serializer,
                const BTree::Header &header);
            /// \brief
            /// Needs access to private members.
            friend Serializer &operator >> (
                Serializer &serializer,
                BTree::Header &header);

            /// \brief
            /// TransactedFileBTreeAllocator is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (TransactedFileBTreeAllocator)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_TransactedFileBTreeAllocator_h)
