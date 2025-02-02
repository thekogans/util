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

#if !defined (__thekogans_util_BTree_h)
#define __thekogans_util_BTree_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/BlockAllocator.h"
#include "thekogans/util/FileBlockAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct BTree BTree.h thekogans/util/BTree.h
        ///
        /// \brief

        struct _LIB_THEKOGANS_UTIL_DECL BTree {
            // {size, offset}
            /// \brief
            /// Keys are structured to allow for greater flexibility.
            using Key = std::pair<ui64, ui64>;

            enum {
                /// \brief
                /// Default number of entries per node.
                DEFAULT_ENTRIES_PER_NODE = 32
            };

        private:
            /// \brief
            FileBlockAllocator fileBlockAllocator;
            /// \struct BTree::Header BTree.h thekogans/util/BTree.h
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

                enum {
                    SIZE = UI32_SIZE + UI64_SIZE
                };

                /// \brief
                /// ctor.
                /// \param[in] entriesPerNode_ Entries per node.
                Header (ui32 entriesPerNode_ = DEFAULT_ENTRIES_PER_NODE) :
                    entriesPerNode (entriesPerNode_),
                    rootOffset (NIDX64) {}
            } header;
            Allocator::SharedPtr fileNodeAllocator;
            Allocator::SharedPtr nodeAllocator;
            /// \struct BTree::Node BTree.h thekogans/util/BTree.h
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
                    Entry (const Key &key_ = Key (NIDX64, NIDX64)) :
                        key (key_),
                        rightOffset (NIDX64),
                        rightNode (nullptr) {}
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
                    ui64 offset_ = NIDX64);
                /// \brief
                /// dtor.
                ~Node ();

                /// \brief
                /// Given the number of entries, return the node file size in bytes.
                static std::size_t FileSize (ui32 entriesPerNode);
                /// \brief
                /// Given the number of entries, return the node size in bytes.
                static std::size_t Size (ui32 entriesPerNode);
                /// \brief
                /// Allocate a node.
                /// \param[in] btree BTree to which this node belongs.
                /// \param[in] offset Node offset.
                static Node *Alloc (
                    BTree &btree,
                    ui64 offset = NIDX64);
                /// \brief
                /// Free the given node.
                /// \param[in] node Node to free.
                static void Free (Node *node);
                /// \brief
                /// Delete the file associated with and free the given empty node.
                /// If the node is not empty, throw exception.
                /// \param[in] node Node to delete.
                static void Delete (Node *node);

                void Save ();
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
            } *root;

        public:
            /// \brief
            /// ctor.
            /// \param[in] path Directory where the btree files are stored.
            /// \param[in] entriesPerNode If the btree is just being created
            /// use this value for entries per node.
            BTree (
                const std::string &path,
                ui32 entriesPerNode = DEFAULT_ENTRIES_PER_NODE,
                std::size_t nodesPerPage = BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            /// \brief
            /// dtor.
            ~BTree ();

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

            friend Serializer &operator << (
                Serializer &serializer,
                const Node::Entry &entry);
            friend Serializer &operator >> (
                Serializer &serializer,
                Node::Entry &header);
            friend Serializer &operator << (
                Serializer &serializer,
                const Header &header);
            friend Serializer &operator >> (
                Serializer &serializer,
                Header &header);
        };

        inline bool operator == (
                const BTree::Key &key1,
                const BTree::Key &key2) {
            return key1.first == key2.first && key1.second == key2.second;
        }
        inline bool operator != (
                const BTree::Key &key1,
                const BTree::Key &key2) {
            return key1.first != key2.first || key1.second != key2.second;
        }

        inline bool operator < (
                const BTree::Key &key1,
                const BTree::Key &key2) {
            return key1.first < key2.first ||
                (key1.first == key2.first && key1.second < key2.second);
        }

    } // namespace util
} // namespace thekogans

#endif // __thekogans_util_BTree_h
