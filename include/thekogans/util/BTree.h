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
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/BlockAllocator.h"
#include "thekogans/util/GUID.h"

namespace thekogans {
    namespace util {

        /// \struct BTree BTree.h thekogans/util/BTree.h
        ///
        /// \brief
        /// A BTree is a \see{FileAllocator} container. It's attributes are that
        /// all searches, additions and deletions take O(N) where N is the
        /// height of the tree. BTree keys are \see{GUID} and the values are
        /// \see{ui64}.

        struct BTree : public RefCounted {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (BTree)

            /// \brief
            /// Keys are \see{GUID}s.
            using Key = GUID;
            /// \brief
            /// Key size on disk.
            static const std::size_t KEY_SIZE = GUID::SIZE;

        private:
            /// \brief
            /// \see{FileAllocator} where we allocate
            /// \see{Header} and \see{Node} blocks from.
            FileAllocator::SharedPtr fileAllocator;
            /// \brief
            /// Offset of the \see{Header} block.
            ui64 offset;
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

                /// \brief
                /// Size of header on disk.
                static const std::size_t SIZE =
                    UI32_SIZE + // magic
                    UI32_SIZE + // entriesPerNode
                    UI64_SIZE;  // rootOffset

                /// \brief
                /// ctor.
                /// \param[in] entriesPerNode_ Entries per node.
                Header (ui32 entriesPerNode_ = DEFAULT_ENTRIES_PER_NODE) :
                    entriesPerNode (entriesPerNode_),
                    rootOffset (0) {}
            } header;
            /// \brief
            /// An instance of \see{BlockAllocator} to allocate \see{Node}s.
            Allocator::SharedPtr nodeAllocator;
            /// \struct BTree::Node BTree.h thekogans/util/BTree.h
            ///
            /// \brief
            /// BTree nodes store sorted keys and pointers to children nodes.
            struct Node {
                /// \brief
                /// BTree to which this node belongs.
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
                /// Node entries contain keys, values and right (grater then) children.
                struct Entry {
                    /// \brief
                    /// Entry key.
                    Key key;
                    /// \brief
                    /// Entry value.
                    ui64 value;
                    /// \brief
                    /// Right child node offset.
                    ui64 rightOffset;
                    /// \brief
                    /// Right child node.
                    Node *rightNode;

                    /// \brief
                    /// ctor.
                    /// NOTE: Because of the way we allocate Node this ctor
                    /// is here for show. It will never be called as we're
                    /// treating Entry as POT. It actually gets initialized
                    /// in the operator >>.
                    /// \param[in] key_ Entry key.
                    Entry (
                        const Key &key_ = GUID::Empty,
                        ui64 value_ = 0) :
                        key (key_),
                        value (value_),
                        rightOffset (0),
                        rightNode (0) {}
                };
                /// \brief
                /// Entry array. The rest of the entries are
                /// allocated when the node is allocated.
                Entry entries[1];

                /// \brief
                /// ctor.
                /// \param[in] btree_ BTree to which this node belongs.
                /// \param[in] offset_ Node offset.
                Node (
                    BTree &btree_,
                    ui64 offset_ = 0);
                /// \brief
                /// dtor.
                ~Node ();

                /// \brief
                /// Given the number of entries, return the node file size in bytes.
                /// \param[in] entriesPerNode Entries per node.
                /// \return Size of node on disk.
                static std::size_t FileSize (std::size_t entriesPerNode);
                /// \brief
                /// Given the number of entries, return the node size in bytes.
                /// \param[in] entriesPerNode Entries per node.
                /// \return Size of node in memory.
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
                /// \param[in] node Empty node to delete.
                static void Delete (Node *node);
                /// \brief
                /// Delete the node and it's sub-tree.
                /// \param[in] fileAllocator Btree heap.
                /// \param[in] offset Node offset.
                static void Delete (
                    FileAllocator &fileAllocator,
                    ui64 offset);

                /// \brief
                /// Nodes delay writting themselves to disk until they are destroyed.
                /// This way we amortize the cost of disk writes across multiple node
                /// updates. Call BTree::Flush to flush the cache in tight memory
                /// situations.
                inline void Save () {
                    dirty = true;
                }
                /// \brief
                /// Return the child at the given index.
                /// \param[in] index Index of child to retrieve
                /// (0 == left, !0 == entries[index-1].right).
                /// \return Child node at the given index. nullptr if no child at that index exists.
                Node *GetChild (ui32 index);
                /// \brief
                /// Search for a given key.
                /// \param[in] key \see{Key} to search for.
                /// \param[out] index If found will contain the index of the key.
                /// If not found will contain the index of the closest larger key.
                /// \return true == found the key.
                bool Search (
                    const Key &key,
                    ui32 &index) const;
                enum InsertResult {
                    Inserted,
                    Duplicate,
                    Overflow
                };
                /// \brief
                /// Try to recursively insert the given entry.
                /// \param[in] entry \see{Entry} to insert.
                /// \return true == entry inserted. false == the entire sub-tree
                /// rooted at this node is full. Time to split the node.
                InsertResult Insert (Entry &entry);
                /// \brief
                /// Try to recursively delete the given key.
                /// \param[in] key \see{Key} whose entry we want to delete.
                /// \param[in] node \see{Node} to delete from.
                /// \return true == entry was deleted. false == key not found.
                bool Remove (const Key &key);
                /// \brief
                /// Maintain BTree structure.
                void RestoreBalance (ui32 index);
                /// \brief
                /// Maintain BTree structure.
                void RotateRight (
                    ui32 index,
                    Node *left,
                    Node *right);
                /// \brief
                /// Maintain BTree structure.
                void RotateLeft (
                    ui32 index,
                    Node *left,
                    Node *right);
                /// \brief
                /// Maintain BTree structure.
                void Merge (
                    ui32 index,
                    Node *left,
                    Node *right);
                /// \brief
                /// Split the node in the middle (splitIndex = btree.header.entriesPerNode / 2).
                /// \param[out] node Node that will receive entries >= splitIndex.
                void Split (Node *node);
                /// \brief
                /// Concatenate the given node's entries to this one.
                /// \param[in] node Node whose entries are to be concatenated.
                /// The empty node is deleted after.
                void Concatenate (Node *node);
                /// \brief
                /// Concatenate the given entry.
                /// \param[in] entry to concatenate.
                inline void Concatenate (const Entry &entry) {
                    InsertEntry (entry, count);
                }
                /// \brief
                /// Insert the given entry at the given index.
                /// \param[in] entry \see{Entry} to insert.
                /// \param[in] index Index to insert at.
                void InsertEntry (
                    const Entry &entry,
                    ui32 index);
                /// \brief
                /// Remove the entry at the given index.
                /// \param[in] index Index of entry to remove.
                void RemoveEntry (ui32 index);
                /// \brief
                /// Return true if the node is empty.
                /// \return true == node is empty.
                inline bool IsEmpty () const {
                    return count == 0;
                }
                /// \brief
                /// Return true if the node is full.
                /// \return true == node is full.
                inline bool IsFull () const {
                    return count == btree.header.entriesPerNode;
                }
                /// \brief
                /// Return true if less than half the nodes entries are occupied.
                /// \return true if less than half the nodes entries are occupied.
                inline bool IsPoor () const {
                    return count < btree.header.entriesPerNode / 2;
                }
                /// \brief
                /// Return true if more than half the nodes entries are occupied.
                /// \return true if more than half the nodes entries are occupied.
                inline bool IsPlentiful () const {
                    return count > btree.header.entriesPerNode / 2;
                }

                /// \brief
                /// Dump the nodes entries to stdout. Used to debug the implementation.
                void Dump ();
            } *root;
            /// \break
            /// Synchronization lock.
            SpinLock spinLock;

        public:
            /// \brief
            /// Default number of entries per node.
            /// NOTE: This is a tunable parameter that should be used
            /// during system integration to provide the best performance
            /// for your needs. Once the heap is created though, this
            /// value is set in stone and the only way to change it is
            /// to delete the file and try again.
            static const std::size_t DEFAULT_ENTRIES_PER_NODE = 256;

            /// \brief
            /// ctor.
            /// \param[in] fileAllocator_ \see{FileAllocator} to which this btree belongs.
            /// \param[in] offset_ Offset of the \see{Header} block on disk.
            /// \param[in] entriesPerNode If we're creating the heap, contains entries per
            /// \see{Node}. If we're reading an existing heap, this value will come from the
            /// \see{Header}.
            /// \param[in] nodesPerPage \see{Node}s are allocated using a \see{BlockAllocator}.
            /// This value sets the number of nodes that will fit on it's page. It's a subtle
            /// tunning parameter that might result in slight performance gains (depending on
            /// your situation). For the most part leaving it alone is the most sensible thing
            /// to do.
            /// \param[in] allocator This is the \see{Allocator} used to allocate pages
            /// for the \see{BlockAllocator}. As with the previous parameter, the same
            /// advice aplies.
            BTree (
                FileAllocator::SharedPtr fileAllocator_,
                ui64 offset_,
                std::size_t entriesPerNode = DEFAULT_ENTRIES_PER_NODE,
                std::size_t nodesPerPage = BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            /// \brief
            /// dtor.
            ~BTree ();

            /// \brief
            /// Delete the btree from the heap.
            /// \param[in] fileAllocator Heap where the btree resides.
            /// \param[in] offset \see{Header} offset.
            static void Delete (
                FileAllocator &fileAllocator,
                ui64 offset);

            /// \brief
            /// Find the given key in the btree.
            /// \param[in] key Key to find.
            /// \param[out] value If found the given key's value will be returned in value.
            /// \return true == found.
            bool Search (
                const Key &key,
                ui64 &value);
            /// \brief
            /// Add the given key to the btree.
            /// \param[in] key Key to add.
            /// \param[in] value Value associated with the given key.
            /// \return true == added. false == duplicate.
            bool Add (
                const Key &key,
                ui64 value);
            /// \brief
            /// Delete the given key from the btree.
            /// \param[in] key Key whose entry to delete.
            /// \return true == entry deleted. false == entry not found.
            bool Delete (const Key &key);

            /// \brief
            /// Flush the node cache (used in tight memory situations).
            void Flush ();

            /// \brief
            /// Use for debugging. Dump the btree nodes to stdout.
            void Dump ();

        private:
            /// \brief
            /// Write the \see{Header} to disk.
            void Save ();
            /// \brief
            /// Set root node.
            /// \param[in] node \see{Node} to set as new root.
            void SetRoot (Node *node);

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

#endif // !defined (__thekogans_util_BTree_h)
