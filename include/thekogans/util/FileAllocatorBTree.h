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

#if !defined (__thekogans_util_FileAllocatorBTree_h)
#define __thekogans_util_FileAllocatorBTree_h

#include <map>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/BlockAllocator.h"
#include "thekogans/util/FileAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct FileAllocator::BTree FileAllocator.h thekogans/util/FileAllocator.h
        ///
        /// \brief
        /// BTree for managing random sized block free list.

        struct FileAllocator::BTree {
            /// \brief
            /// Keys are structured on block {size, offset}
            using Key = std::pair<ui64, ui64>;
            /// \brief
            /// Key size on disk.
            static const std::size_t KEY_SIZE = UI64_SIZE + UI64_SIZE;

        private:
            /// \brief
            /// Reference back to the FileAllocator for
            /// \see{Header} and \see{Node} blocks.
            FileAllocator &fileAllocator;
            /// \brief
            /// Offset of the \see{Header} block.
            ui64 offset;
            /// \struct Fileallocator::BTree::Header FileallocatorBTree.h thekogans/util/FileallocatorBTree.h
            ///
            /// \brief
            /// Header contains global btree info.
            struct Header {
                /// \brief
                /// Entries per node.
                ui32 entriesPerNode;
                /// \brief
                /// Root node offset.
                ui64 rootOffset;

                static const std::size_t SIZE =
                    UI32_SIZE + // magic
                    UI32_SIZE + // entriesPerNode
                    UI64_SIZE;  // rootOffset

                /// \brief
                /// ctor.
                /// \param[in] entriesPerNode_ Entries per node.
                Header (ui32 entriesPerNode_ = FileAllocator::DEFAULT_BTREE_ENTRIES_PER_NODE) :
                    entriesPerNode (entriesPerNode_),
                    rootOffset (0) {}
            } header;
            Allocator::SharedPtr nodeAllocator;
            /// \struct Fileallocator::BTree::Node FileallocatorBTree.h thekogans/util/FileallocatorBTree.h
            ///
            /// \brief
            /// BTree nodes store sorted keys and pointers to children nodes.
            struct Node {
                /// \brief
                /// BTree to whch this node belongs.
                BTree &btree;
                /// \brief
                /// Node block offset.
                ui64 offset;
                /// \brief
                /// Count of entries.
                ui32 count;
                /// \brief
                /// Left most child node offset.
                ui64 leftOffset;
                /// \brief
                /// Left most child node.
                Node *leftNode;
                /// \brief
                /// We accumulate all changes and update the file block in the dtor.
                bool dirty;
                /// \struct BTree::Node::Entry BTree.h thekogans/util/BTree.h
                ///
                /// \brief
                /// Node entries contain keys and right (grater then) children.
                struct Entry {
                    /// \brief
                    /// Entry key.
                    Key key;
                    /// \brief
                    /// Right child node offset.
                    ui64 rightOffset;
                    /// \brief
                    /// Right child node.
                    Node *rightNode;

                    /// \brief
                    /// ctor.
                    /// \param[in] key_ Entry key.
                    Entry (const Key &key_ = Key (0, 0)) :
                        key (key_),
                        rightOffset (0),
                        rightNode (0) {}
                };
                /// \brief
                /// Entry array. The rest of the entries are
                /// allocated when the node is allocated.
                Entry entries[1];

                /// \brief
                /// ctor.
                /// \param[in] btree_ BTree to whch this node belongs.
                /// \param[in] offset_ Node offset.
                Node (
                    BTree &btree_,
                    ui64 offset_ = 0);
                /// \brief
                /// dtor.
                ~Node ();

                /// \brief
                /// Given the number of entries, return the node file size in bytes.
                static std::size_t FileSize (std::size_t entriesPerNode);
                /// \brief
                /// Given the number of entries, return the node size in bytes.
                static std::size_t Size (std::size_t entriesPerNode);
                /// \brief
                /// Allocate a node.
                /// \param[in] btree BTree to which this node belongs.
                /// \param[in] offset Node offset.
                static Node *Alloc (
                    BTree &btree,
                    ui64 offset = 0);
                /// \brief
                /// Free the given node.
                /// \param[in] node Node to free.
                static void Free (Node *node);
                /// \brief
                /// Delete the file associated with and free the given empty node.
                /// If the node is not empty, throw exception.
                /// \param[in] node Node to delete.
                static void Delete (Node *node);

                inline void Save () {
                    dirty = true;
                }
                Node *GetChild (ui32 index);
                bool Search (
                    const Key &key,
                    ui32 &index) const;
                void Split (
                    Node *node,
                    ui32 index);
                void Concatenate (Node *node);
                inline void Concatenate (const Entry &entry) {
                    InsertEntry (entry, count);
                }
                /// \brief
                /// Insert the given entry at the given index.
                void InsertEntry (
                    const Entry &entry,
                    ui32 index);
                /// \brief
                /// Remove the entry at the given index.
                void RemoveEntry (ui32 index);
                /// \brief
                /// Return true if the node is empty.
                inline bool IsEmpty () const {
                    return count == 0;
                }
                /// \brief
                /// Return true if the node is full.
                inline bool IsFull () const {
                    return count == btree.header.entriesPerNode;
                }
                /// \brief
                /// Return true if less than half the node is occupied.
                inline bool IsPoor () const {
                    return count < btree.header.entriesPerNode / 2;
                }
                /// \brief
                /// Return true if more than half the node is occupied.
                inline bool IsPlentiful () const {
                    return count > btree.header.entriesPerNode / 2;
                }

                void Dump ();
            } *root;

        public:
            /// \brief
            /// ctor.
            BTree (
                FileAllocator &fileAllocator_,
                ui64 offset_,
                std::size_t entriesPerNode = FileAllocator::DEFAULT_BTREE_ENTRIES_PER_NODE,
                std::size_t nodesPerPage = BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            /// \brief
            /// dtor.
            ~BTree ();

            inline ui64 GetOffset () const {
                return offset;
            }

            /// \brief
            /// Find the given key in the btree.
            /// \param[in] key Key to find.
            /// \return If found the given key will be returned.
            /// If not found, return the nearest larger key.
            Key Search (const Key &key);
            /// \brief
            /// Add the given key to the btree.
            /// \param[in] key Key to add.
            /// NOTE: Duplicate keys are ignored.
            void Add (const Key &key);
            /// \brief
            /// Delete the given key from the btree.
            /// \param[in] key Key whose entry to delete.
            /// \return true == entry deleted. false == entry not found.
            bool Delete (const Key &key);
            /// \brief
            /// Flush the node cache (used in tight memory situations).
            void Flush ();
            void Dump ();

        private:
            bool Insert (
                Node::Entry &entry,
                Node *node);
            bool Remove (
                const Key &key,
                Node *node);
            void RestoreBalance (
                Node *node,
                ui32 index);
            void RotateRight (
                Node *node,
                ui32 index,
                Node *left,
                Node *right);
            void RotateLeft (
                Node *node,
                ui32 index,
                Node *left,
                Node *right);
            void Merge (
                Node *node,
                ui32 index,
                Node *left,
                Node *right);
            void Save ();
            void SetRoot (Node *node);

            friend struct FileAllocator;

            /// \brief
            /// Needs access to private members.
            friend bool operator == (
                const Key &key1,
                const Key &key2);
            /// \brief
            /// Needs access to private members.
            friend bool operator != (
                const Key &key1,
                const Key &key2);
            /// \brief
            /// Needs access to private members.
            friend bool operator < (
                const Key &key1,
                const Key &key2);

            /// \brief
            /// Needs access to private members.
            friend Serializer &operator << (
               Serializer &serializer,
               const Key &key);
            /// \brief
            /// Needs access to private members.
            friend Serializer &operator >> (
                Serializer &serializer,
                Key &key);

            /// \brief
            /// Needs access to private members.
            friend Serializer &operator << (
                Serializer &serializer,
                const Node::Entry &entry);
            /// \brief
            /// Needs access to private members.
            friend Serializer &operator >> (
                Serializer &serializer,
                Node::Entry &entry);
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
            /// BTree is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BTree)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileAllocatorBTree_h)
