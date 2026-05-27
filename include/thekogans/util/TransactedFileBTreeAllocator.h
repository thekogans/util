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
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileBTreeAllocator :
                public TransactedFile::Allocator,
                public Subscriber<TransactedFile::ObjectEvents> {
            /// \brief
            /// TransactedFileBTreeAllocator is a \see{Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (TransactedFileBTreeAllocator)

            /// \struct TransactedFileBTreeAllocator::Block TransactedFileBTreeAllocator.h
            /// thekogans/util/TransactedFileBTreeAllocator.h
            ///
            /// \brief
            /// Extends the \see{TransactedFile::Allocator::Header} to provide a singly linked list
            /// of blocks. These are special blocks allocated and freed by \see{AllocBTreeNode} and
            /// \see{FreeBTreeNode}. Used by \see{TransactedFileBTreeAllocator::Btree} to allocate
            /// and free \see{TransactedFileBTreeAllocator::Btree::Node}.
            struct _LIB_THEKOGANS_UTIL_DECL Block : public Allocator::Block {
                /// \brief
                /// If this flag is set the block is \see{BTree::Node}. Otherwise it's random size.
                static const ui32 FLAGS_BTREE_NODE =
                    TransactedFile::Allocator::Block::Header::FLAGS_FIRST_ALLOCATOR_FLAG;

            private:
                /// \brief
                /// If FLAGS_BTREE_NODE and FLAGS_FREE are set this offset
                /// will point to the next \see{BTree::Node} offset in the free list.
                /// Otherwise this field is ignored. This is the only
                /// difference between header and footer.
                PtrType nextBTreeNodeOffset;

            public:
                /// \brief
                /// ctor.
                /// \param[in] offset Offset in the file where the Block resides.
                /// \param[in] flags Combination of FLAGS_BTREE_NODE and FLAGS_FREE.
                /// \param[in] size Size of the block (not including the size
                /// of the Block itself).
                /// \param[in] nextBTreeNodeOffset_ If FLAGS_FREE and FLAGS_BTREE_NODE are
                /// set, this field contains the next free \see{BTree::Node} offset.
                Block (
                    PtrType offset = 0,
                    Flags32 flags = 0,
                    ui64 size = 0,
                    PtrType nextBTreeNodeOffset_ = 0) :
                    Allocator::Block (offset, flags, size),
                    nextBTreeNodeOffset (nextBTreeNodeOffset_) {}

                /// \brief
                /// Return true if FLAGS_BTREE_NODE set.
                /// \return true == FLAGS_BTREE_NODE set.
                inline bool IsBTreeNode () const {
                    return header.flags.Test (FLAGS_BTREE_NODE);
                }
                /// \brief
                /// Set/clear the FLAGS_BTREE_NODE flag.
                /// \param[in] free true == set, false == clear
                inline void SetBTreeNode (bool btreeNode) {
                    header.flags.Set (FLAGS_BTREE_NODE, btreeNode);
                }

                /// \brief
                /// Return the next free \see{TransactedFileBTreeAllocator::BTree::Node} offset.
                /// \return Next free \see{TransactedFileBTreeAllocator::BTree::Node} offset.
                inline PtrType GetNextBTreeNodeOffset () const {
                    return nextBTreeNodeOffset;
                }
                /// \brief
                /// Set the next free \see{TransactedFileBTreeAllocator::BTree::Node} offset.
                /// This will chain the free \see{TransactedFileBTreeAllocator::BTree::Node}
                /// blocks in to a singly linked list.
                /// \param[in] nextBTreeNodeOffset Next free \see{TransactedFileBTreeAllocator::BTree::Node} offset.
                inline void SetNextBTreeNodeOffset (PtrType nextBTreeNodeOffset_) {
                    nextBTreeNodeOffset = nextBTreeNodeOffset_;
                }

                /// \brief
                /// Read the block.
                /// \param[in] file \see{TransactedFile} containing the block.
                void Read (TransactedFile &file);
                /// \brief
                /// Write the block.
                /// \param[in] file \see{TransactedFile} containing the block.
                void Write (TransactedFile &file) const;
            };

        private:
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
                /// Contains the head of the free \see{BTree::Node} list.
                PtrType freeBTreeNodeOffset;

                /// \brief
                /// The size of the header on disk.
                static const std::size_t SIZE =
                    UI32_SIZE +     // magic
                    PTR_TYPE_SIZE + // btreeOffset
                    PTR_TYPE_SIZE;  // freeBTreeNodeOffset

                /// \brief
                /// ctor.
                Header () :
                    btreeOffset (0),
                    freeBTreeNodeOffset (0) {}
            } header;
            /// \brief
            /// Include the \see{BTree} header.
            /// I split it out because this file was getting too big to maintain.
            #include "thekogans/util/TransactedFileBTreeAllocatorBTree.h"
            /// \brief
            /// Number of entries per \see{BTree::Node}.
            std::size_t btreeEntriesPerNode;
            /// \brief
            /// Number of \see{BTree::Node}s that will fit in to a \see{BlockAllocator} page.
            std::size_t btreeNodesPerPage;
            /// \brief
            /// \see{util::Allocator} for \see{BTree::Node}.
            util::Allocator::SharedPtr allocator;
            /// \brief
            /// \see{BTree} to manage heap free space.
            BTree::SharedPtr btree;
            /// \brief
            /// Cache the size of the BTree::Node on disk so that we
            /// don't pay the price of calculating it everytime \see{Alloc}
            /// is called.
            std::size_t btreeNodeFileSize;

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
            /// Read the \see{Header} from the file.
            virtual void Read (
                const SerializableHeader & /*header*/,
                Serializer &serializer) override;
            /// \brief
            /// Write the \see{Header} to the file.
            virtual void Write (Serializer &serializer) const override;

            // TransactedFile::ObjectEvents
            /// \brief
            /// \see{Object} allocated a block in the file.
            /// \param[in] object \see{Object} whose offset has become valid.
            virtual void OnTransactedFileObjectAlloc (
                    TransactedFile::Object::SharedPtr object) noexcept override {
                header.btreeOffset = object->GetOffset ();
                SetDirty (true);
            }
            /// \brief
            /// \see{Object} freed its block.
            /// \param[in] object \see{Object} whose offset has become invalid.
            virtual void OnTransactedFileObjectFree (
                    TransactedFile::Object::SharedPtr /*object*/) noexcept override {
                header.btreeOffset = 0;
                SetDirty (true);
            }

            /// \brief
            /// Used to allocate \see{BTree::Node} blocks.
            /// This method is used by the \see{BTree::Node}.
            /// \return Offset of allocated block.
            PtrType AllocBTreeNode (std::size_t size);
            /// \brief
            /// Used to free blocks prviously allocated with AllocBTreeNode.
            /// \param[in] offset Offset of \see{BTree::Node} to free.
            void FreeBTreeNode (PtrType offset);

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
