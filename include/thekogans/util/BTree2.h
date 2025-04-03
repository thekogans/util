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

#if !defined (__thekogans_util_BTree2_h)
#define __thekogans_util_BTree2_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/BlockAllocator.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

        /// \struct BTree2 BTree2.h thekogans/util/BTree2.h
        ///
        /// \brief
        /// A BTree2 is a \see{FileAllocator} container. It's attributes are that
        /// all searches, additions and deletions take O(N) where N is the
        /// height of the tree. These are BTree2's bigest strengths. One of it's
        /// biggest weaknesses is the fact that iterators don't survive modifications
        /// (Inserions/Deletions). This is why I don't provide an iterator api.
        /// BTree2 uses the full power of \see{DynamicCreatable} and \see{Serializable}
        /// for it's key and values. That means that key and values can be practically
        /// any random size object (as long as it derives from BTree2::Key/Value and
        /// implements the interface).

        struct BTree2 : public RefCounted {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (BTree2)

            struct Key : public Serializable {
                /// \brief
                /// Declare \see{DynamicCreatable} boilerplate.
                THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE (Key)

                /// \brief
                /// Used to find keys with matching prefixs.
                /// \param[in] prefix Key representing the prefix to compare against.
                /// \return -1 == this is < key, 0 == this == key, 1 == this is greater than key.
                virtual i32 PrefixCompare (const Key &prefix) const = 0;
                /// \brief
                /// Used to order keys.
                /// \param[in] key Key to compare against.
                /// \return -1 == this is < key, 0 == this == key, 1 == this is greater than key.
                virtual i32 Compare (const Key &key) const = 0;
                /// \brief
                /// This method is only used in Dump for debugging purposes.
                /// \return String representation of the key.
                virtual std::string ToString () const = 0;
            };

            struct Value : public Serializable {
                /// \brief
                /// Declare \see{DynamicCreatable} boilerplate.
                THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE (Value)

                /// \brief
                /// This method is only used in Dump for debugging purposes.
                /// \return String representation of the value.
                virtual std::string ToString () const = 0;
            };

        private:
            /// \brief
            /// \see{FileAllocator} where we allocate
            /// \see{Header} and \see{Node} blocks from.
            FileAllocator::SharedPtr fileAllocator;
            /// \brief
            /// Offset of the \see{Header} block.
            FileAllocator::PtrType offset;
            /// \struct BTree2::Header BTree2.h thekogans/util/BTree2.h
            ///
            /// \brief
            /// Header contains global btree info.
            struct Header {
                /// \brief
                /// Key type.
                std::string keyType;
                /// \brief
                /// Value type.
                std::string valueType;
                /// \brief
                /// Entries per node.
                ui32 entriesPerNode;
                /// \brief
                /// Root node offset.
                FileAllocator::PtrType rootOffset;

                /// \brief
                /// ctor.
                /// \param[in] keyType_ \see{DynamicCreatable} type.
                /// \param[in] valueType_ \see{DynamicCreatable} type.
                /// \param[in] entriesPerNode_ Entries per node.
                Header (
                    const std::string &keyType_ = std::string (),
                    const std::string &valueType_ = std::string (),
                    ui32 entriesPerNode_ = DEFAULT_ENTRIES_PER_NODE) :
                    keyType (keyType_),
                    valueType (valueType_),
                    entriesPerNode (entriesPerNode_),
                    rootOffset (0) {}

                /// \brief
                /// Return the serialized size of the header.
                inline std::size_t Size () const {
                    return
                        UI32_SIZE + // magic
                        Serializer::Size (keyType) +
                        Serializer::Size (valueType) +
                        Serializer::Size (entriesPerNode) +
                        Serializer::Size (rootOffset);
                }
            } header;
            /// \struct BTree2::Node BTree2.h thekogans/util/BTree2.h
            ///
            /// \brief
            /// BTree2 nodes store sorted key/value pairs and pointers to children nodes.
            struct Node {
                /// \brief
                /// BTree2 to which this node belongs.
                BTree2 &btree;
                /// \brief
                /// Node block offset.
                FileAllocator::PtrType offset;
                /// \brief
                /// Count of entries.
                ui32 count;
                /// \brief
                /// Left most child node offset.
                FileAllocator::PtrType leftOffset;
                /// \brief
                /// Left most child node.
                Node *leftNode;
                /// \brief
                /// Key/value array offset.
                FileAllocator::PtrType keyValueOffset;
                /// \brief
                /// We accumulate all changes and update the file block in the dtor.
                bool dirty;
                /// \struct BTree2::Node::Entry BTree2.h thekogans/util/BTree2.h
                ///
                /// \brief
                /// Node entries contain keys, values and right (grater then) children.
                struct Entry {
                    /// \brief
                    /// Entry key.
                    Key *key;
                    /// \brief
                    /// Entry value.
                    Value *value;
                    /// \brief
                    /// Right child node offset.
                    FileAllocator::PtrType rightOffset;
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
                    /// \param[in] value_ Entry value.
                    Entry (
                        Key *key_ = nullptr,
                        Value *value_ = nullptr) :
                        key (key_),
                        value (value_),
                        rightOffset (0),
                        rightNode (nullptr) {}
                };
                /// \brief
                /// Entry array. The rest of the entries are
                /// allocated when the node is allocated.
                Entry entries[1];

                /// \brief
                /// ctor.
                /// \param[in] btree_ BTree2 to which this node belongs.
                /// \param[in] offset_ Node offset.
                Node (
                    BTree2 &btree_,
                    FileAllocator::PtrType offset_ = 0);
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
                /// \param[in] btree BTree2 to which this node belongs.
                /// \param[in] offset Node offset.
                static Node *Alloc (
                    BTree2 &btree,
                    FileAllocator::PtrType offset = 0);
                /// \brief
                /// Free the given node.
                /// \param[in] node Node to free.
                static void Free (Node *node);
                /// \brief
                /// Delete and free the given empty node.
                /// If the node is not empty, throw exception.
                /// \param[in] node Empty node to delete.
                static void Delete (Node *node);
                /// \brief
                /// Delete the node and it's sub-tree.
                /// \param[in] fileAllocator Btree heap.
                /// \param[in] offset Node offset.
                static void Delete (
                    FileAllocator &fileAllocator,
                    FileAllocator::PtrType offset);

                /// \brief
                /// Nodes delay writting themselves to disk until they are destroyed.
                /// This way we amortize the cost of disk writes across multiple node
                /// updates. Call BTree2::Flush to flush the cache in tight memory
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
                bool PrefixSearch (
                    const Key &prefix,
                    ui32 &index) const;
                bool FindFirstPrefix (
                    const Key &prefix,
                    ui32 &index) const;
                /// \brief
                /// Search for a given key.
                /// \param[in] key \see{Key} to search for.
                /// \param[out] index If found will contain the index of the key.
                /// If not found will contain the index of the closest larger key.
                /// \return true == found the key.
                bool Search (
                    const Key &key,
                    ui32 &index) const;
                /// \enum
                /// Insert return codes.
                enum InsertResult {
                    /// \brief
                    /// Entry was inserted.
                    Inserted,
                    /// \brief
                    /// Entry is a duplicate.
                    Duplicate,
                    /// \brief
                    /// Node is full.
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
                /// \return true == entry was deleted. false == key not found.
                bool Remove (const Key &key);
                /// \brief
                /// The algorithm checks the left and right children @index for
                /// conformity and performs necessary adjustments to maintain the
                /// BTree2 structure. If one of the children is poor an entry is
                /// rotated in to it from the other child. If they are both poor,
                /// the algorithm merges the two children in to one.
                /// \param[in] index Index of entry whos left and right child to check.
                void RestoreBalance (ui32 index);
                /// \brief
                /// Called by RestoreBalance to rotate an \see{Entry} from left
                /// child to right child.
                /// \param[in] index Index of \see{Entry}.
                /// \param[in] left Left child.
                /// \param[in] right Right child.
                void RotateRight (
                    ui32 index,
                    Node *left,
                    Node *right);
                /// \brief
                /// Called by RestoreBalance to rotate an \see{Entry} from right
                /// child to left child.
                /// \param[in] index Index of \see{Entry}.
                /// \param[in] left Left child.
                /// \param[in] right Right child.
                void RotateLeft (
                    ui32 index,
                    Node *left,
                    Node *right);
                /// \brief
                /// Called by RestoreBalance to merge two poor nodes in to one.
                /// \param[in] index Index of the \see{Entry} whos left
                /// and right children to merge.
                /// \param[in] left \see{Entry} left child.
                /// \param[in] right \see{Entry} right child.
                /// NOTE: The algorithm merges the entries from right
                /// together with entry @index in to the left child.
                /// Right is deleted after.
                void Merge (
                    ui32 index,
                    Node *left,
                    Node *right);
                /// \brief
                /// Split the full node in the middle
                /// (splitIndex = btree.header.entriesPerNode / 2).
                /// \param[out] node Node that will receive entries >= splitIndex.
                void Split (Node *node);
                /// \brief
                /// Add the given node's entries to this one. The empty node
                /// is deleted after.
                /// \param[in] node Node whose entries are to be added.
                void Concatenate (Node *node);
                /// \brief
                /// Add the given \see{Entry} to the end of the list.
                /// \param[in] entry \see{Entry} to add.
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
            /// \brief
            /// An instance of \see{BlockAllocator} to allocate \see{Node}s.
            Allocator::SharedPtr nodeAllocator;
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
            /// \param[in] fileAllocator_ BTree2 heap (see \see{FileAllocator}).
            /// \param[in] offset_ Heap offset of the \see{Header} block.
            /// \param[in] keyType \see{DynamicCreatable} key type.
            /// \param[in] valueType \see{DynamicCreatable} value type.
            /// \param[in] entriesPerNode If we're creating the btree, contains entries per
            /// \see{Node}. If we're reading an existing btree, this value will come from the
            /// \see{Header}.
            /// \param[in] nodesPerPage \see{Node}s are allocated using a \see{BlockAllocator}.
            /// This value sets the number of nodes that will fit on it's page. It's a subtle
            /// tunning parameter that might result in slight performance gains (depending on
            /// your situation). For the most part leaving it alone is the most sensible thing
            /// to do.
            /// \param[in] allocator This is the \see{Allocator} used to allocate pages
            /// for the \see{BlockAllocator}. As with the previous parameter, the same
            /// advice aplies.
            BTree2 (
                FileAllocator::SharedPtr fileAllocator_,
                FileAllocator::PtrType offset_,
                const std::string &keyType = std::string (),
                const std::string &valueType = std::string (),
                std::size_t entriesPerNode = DEFAULT_ENTRIES_PER_NODE,
                std::size_t nodesPerPage = BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            /// \brief
            /// dtor.
            ~BTree2 ();

            /// \brief
            /// Delete the btree from the heap.
            /// \param[in] fileAllocator Heap where the btree resides.
            /// \param[in] offset \see{Header} offset.
            static void Delete (
                FileAllocator &fileAllocator,
                FileAllocator::PtrType offset);

            /// \brief
            /// Return the offset of the btree \see{Header} block.
            /// \return Offset of the btree \see{Header} block.
            inline FileAllocator::PtrType GetOffset () const {
                return offset;
            }

            /// \brief
            /// Find the given key in the btree.
            /// \param[in] key \see{Key} to find.
            /// \param[out] value If found the given key's value will be returned in value.
            /// \return true == found.
            bool Search (
                const Key &key,
                Value::SharedPtr &value);
            /// \brief
            /// Add the given key to the btree.
            /// \param[in] key KeyType to add.
            /// \param[in] value Value associated with the given key.
            /// \return true == added. false == duplicate.
            bool Add (
                Key::SharedPtr key,
                Value::SharedPtr value);
            /// \brief
            /// Delete the given key from the btree.
            /// \param[in] key KeyType whose entry to delete.
            /// \return true == entry deleted. false == entry not found.
            bool Delete (const Key &key);

            /// \struct BTree2::Iterator BTree2.h thekogans/util/BTree2.h
            ///
            /// \brief
            /// Iterator implements a forward iterator over a range of btree nodes.
            /// Call \see{FindFirst} bellow with a reference to an iterator and then
            /// use it to move forward through the range of nodes. The range can either
            /// be based on a prefix or the entire tree.
            struct Iterator {
            protected:
                /// \brief
                /// Prefix to iterate over (nullptr == entire tree).
                Key::SharedPtr prefix;
                /// \brief
                /// Alias for std::pair<Node *, ui32>.
                using NodeInfo = std::pair<Node *, ui32>;
                /// \brief
                /// Stack of parents allowing us to navigate the tree.
                /// The nodes store no parent pointers. They would be
                /// a nightmare to maintain.
                std::vector<NodeInfo> parents;
                /// \brief
                /// Current node we're iterating over.
                NodeInfo node;
                /// \brief
                /// Flag indicating the iterator is done and will not
                /// return true from Next again. It is only set in \see{FindFirst}
                /// below and only after it made sure that it properly initialized
                /// the iterator.
                bool finished;

            public:
                Iterator (Key::SharedPtr prefix_ = nullptr) :
                    prefix (prefix_),
                    node (NodeInfo (nullptr, 0)),
                    finished (true) {}

                bool Next ();

                inline void Clear () {
                    parents.clear ();
                    node = Iterator::NodeInfo (nullptr, 0);
                    finished = true;
                }

                inline Key::SharedPtr GetKey () const {
                    return !finished && node.first != nullptr ?
                        node.first->entries[node.second].key : nullptr;
                }
                inline Value::SharedPtr GetValue () const {
                    return !finished && node.first != nullptr ?
                        node.first->entries[node.second].value : nullptr;
                }

                friend struct BTree2;
            };

            bool FindFirst (Iterator &it);

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
            /// BTree2 is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BTree2)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_BTree2_h)
