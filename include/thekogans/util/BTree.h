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

#include <string>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Serializable.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/BlockAllocator.h"
#include "thekogans/util/FileAllocatorObject.h"
#include "thekogans/util/FileAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct BTree BTree.h thekogans/util/BTree.h
        ///
        /// \brief
        /// A BTree is a \see{FileAllocator} container. It's attributes are that
        /// all searches, insertions and deletions take O(N) where N is the
        /// height of the tree. These are BTree's bigest strengths. One of it's
        /// biggest weaknesses is the fact that iterators don't survive modifications
        /// (Insert/Delete). This is why I provide a forward iterator only.
        /// Use it to step through a range of nodes collecting their data. See an example
        /// provided with \see{BTree::Iterator}. BTree uses the full power of
        /// \see{DynamicCreatable} and \see{Serializable} for it's key and value.
        /// That means that key and values can be practically any random size object
        /// (as long as it derives from BTree::Key and implements the interface).

        struct _LIB_THEKOGANS_UTIL_DECL BTree : public FileAllocatorObject {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (BTree)

            /// \struct BTree::Key BTree.h thekogans/util/BTree.h
            ///
            /// \brief
            /// Key adds order to the \see{Serializable}.
            struct _LIB_THEKOGANS_UTIL_DECL Key : public Serializable {
                /// \brief
                /// Key is a \see{util::DynamicCreatable} abstract base.
                THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_ABSTRACT_BASE (Key)

            #if defined (THEKOGANS_UTIL_TYPE_Static)
                /// \brief
                /// Because Key uses dynamic initialization, when using
                /// it in static builds call this method to have the Key
                /// explicitly include all internal key types. Without
                /// calling this api, the only keys that will be available
                /// to your application are the ones you explicitly link to.
                static void StaticInit ();
            #endif // defined (THEKOGANS_UTIL_TYPE_Static)

                /// \brief
                /// Used to find keys with matching prefixs.
                /// NOTE: This represents the prefix.
                /// \param[in] key Key whose prefix to compare against.
                /// \return -1 == this is < key, 0 == this == key, 1 == this is greater than key.
                virtual i32 PrefixCompare (const Key &key) const = 0;
                /// \brief
                /// Used to order keys.
                /// \param[in] key Key to compare against.
                /// \return -1 == this is < key, 0 == this == key, 1 == this is greater than key.
                virtual i32 Compare (const Key &key) const = 0;
                /// \brief
                /// This method is only used in Dump for debugging purposes.
                /// \return String representation of the key.
                virtual std::string ToString () const = 0;

                // Serializable
                /// \brief
                /// Read the Serializable from an XML DOM.
                /// \param[in] header \see{Serializable::Header}.
                /// \param[in] node XML DOM representation of a Serializable.
                virtual void ReadXML (
                        const Header & /*header*/,
                        const pugi::xml_node &node) override {
                    // FIXME: implement?
                    assert (0);
                }
                /// \brief
                /// Write the Serializable to the XML DOM.
                /// \param[out] node Parent node.
                virtual void WriteXML (pugi::xml_node &node) const override {
                    // FIXME: implement?
                    assert (0);
                }

                /// \brief
                /// Read the Serializable from an JSON DOM.
                /// \param[in] node JSON DOM representation of a Serializable.
                virtual void ReadJSON (
                        const Header & /*header*/,
                        const JSON::Object &object) override {
                    // FIXME: implement?
                    assert (0);
                }
                /// \brief
                /// Write the Serializable to the JSON DOM.
                /// \param[out] node Parent node.
                virtual void WriteJSON (JSON::Object &object) const override {
                    // FIXME: implement?
                    assert (0);
                }
            };

            /// \struct Value BTree.h thekogans/util/BTree.h
            ///
            /// \brief
            struct _LIB_THEKOGANS_UTIL_DECL Value : public Serializable {
                /// \brief
                /// Value is a \see{util::DynamicCreatable} abstract base.
                THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_ABSTRACT_BASE (Value)

            #if defined (THEKOGANS_UTIL_TYPE_Static)
                /// \brief
                /// Because Value uses dynamic initialization, when using
                /// it in static builds call this method to have the Value
                /// explicitly include all internal value types. Without
                /// calling this api, the only values that will be available
                /// to your application are the ones you explicitly link to.
                static void StaticInit ();
            #endif // defined (THEKOGANS_UTIL_TYPE_Static)

                /// \brief
                /// This method is only used in Dump for debugging purposes.
                /// \return String representation of the value.
                virtual std::string ToString () const = 0;

                // Serializable
                /// \brief
                /// Read the Serializable from an XML DOM.
                /// \param[in] header \see{Serializable::Header}.
                /// \param[in] node XML DOM representation of a Serializable.
                virtual void ReadXML (
                        const Header & /*header*/,
                        const pugi::xml_node &node) override {
                    // FIXME: implement?
                    assert (0);
                }
                /// \brief
                /// Write the Serializable to the XML DOM.
                /// \param[out] node Parent node.
                virtual void WriteXML (pugi::xml_node &node) const override {
                    // FIXME: implement?
                    assert (0);
                }

                /// \brief
                /// Read the Serializable from an JSON DOM.
                /// \param[in] node JSON DOM representation of a Serializable.
                virtual void ReadJSON (
                        const Header & /*header*/,
                        const JSON::Object &object) override {
                    // FIXME: implement?
                    assert (0);
                }
                /// \brief
                /// Write the Serializable to the JSON DOM.
                /// \param[out] node Parent node.
                virtual void WriteJSON (JSON::Object &object) const override {
                    // FIXME: implement?
                    assert (0);
                }
            };

        protected:
            /// \brief
            /// Forward declaration of node needed by \see{Iterator}.
            struct Node;

        public:
            /// \struct BTree::Iterator BTree.h thekogans/util/BTree.h
            ///
            /// \brief
            /// Iterator implements a forward cursor over a range of btree entries.
            /// Call \see{FindFirst} bellow with a reference to an iterator and then
            /// use it to move forward through the range of entries. The range can either
            /// be based on a prefix or the entire tree. Since Iterator is unidirectional
            /// (forward only), there's no backing up (the code complexity is just not
            /// worth the added benefit). You get one shot through the range. This
            /// design also means that you cannot know, a priori, how many entries
            /// are in the range. You need to step through it to count them up.
            /// WARNING: \see{FindFirst} will return a live iterator pointing in to the
            /// actual data in the btree (not a copy). The nature of BTrees is such that
            /// almost any modification to its structure invalidates iterators currently
            /// in existance. This means that you CAN'T (or at least shouldn't) write
            /// code like this:
            ///
            /// Deleting a range of nodes:
            ///
            /// Ex:
            ///
            /// \code{.cpp}
            /// // WARNING: This is wrong! DON'T do this. It can lead
            /// // to very hard to track down crashes as the posibility
            /// // of a crash is completely dependent on the state of
            /// // the BTree.
            /// Iterator it (some prefix);
            /// for (btree.FindFirst (it); !it.IsFinished (); it.Next ()) {
            ///     btree.Delete (*it.GetKey ());
            /// }
            /// \endcode
            ///
            /// Instead you must write it like this:
            ///
            /// Ex:
            ///
            /// \code{.cpp}
            /// // This is the right way.
            /// std::vector<Key::SharedPtr> keys;
            /// Iterator it (some prefix);
            /// for (btree.FindFirst (it); !it.IsFinished (); it.Next ()) {
            ///     keys.push_back (it.GetKey ());
            /// }
            /// for (std::size_t i = 0, count = keys.size (); i < count; ++i) {
            ///     btree.Delete (*keys[i]);
            /// }
            /// \endcode
            struct _LIB_THEKOGANS_UTIL_DECL Iterator {
            protected:
                /// \brief
                /// Prefix to iterate over (nullptr == entire tree).
                Key::SharedPtr prefix;
                /// \brief
                /// Alias for std::pair<Node *, ui32>.
                using NodeIndex = std::pair<Node *, ui32>;
                /// \brief
                /// Stack of parents allowing us to navigate the tree.
                /// The nodes store no parent pointers. They would be
                /// a nightmare to maintain.
                std::vector<NodeIndex> parents;
                /// \brief
                /// Current node we're iterating over.
                NodeIndex node;
                /// \brief
                /// Flag indicating the iterator is done and will not
                /// return true from Next again. It is only set in \see{BTree::FindFirst}
                /// and \see{Next} below and only after it made sure that it properly
                /// initialized the iterator.
                bool finished;

            public:
                /// \brief
                /// ctor.
                /// \param[in] prefix_ Prefix to iterate over (nullptr == entire tree).
                Iterator (Key::SharedPtr prefix_ = nullptr) :
                    prefix (prefix_),
                    node (NodeIndex (nullptr, 0)),
                    finished (true) {}

                /// \brief
                /// Return true if the iterator is finished.
                /// \return true == the iterator is finished.
                inline bool IsFinished () const {
                    return finished;
                }

                /// \brief
                /// Clear the internal state and reset the iterator.
                inline void Clear () {
                    // Leave the prefix in case they want to reuse the iterator.
                    parents.clear ();
                    node = NodeIndex (nullptr, 0);
                    finished = true;
                }

                /// \brief
                /// Step to the next entry in the range.
                /// \return true == Iterator is now pointing at the next entry.
                /// Use GetKey and GetValue to examine it's contents. false ==
                /// the iterator is finished through the range.
                bool Next ();

                /// \brief
                /// If we're not finished, return the key associated with the current entry.
                /// \return Key associated with the current entry.
                Key::SharedPtr GetKey () const;
                /// \brief
                /// If we're not finished, return the value associated with the current entry.
                /// \return Value associated with the current entry.
                Value::SharedPtr GetValue () const;
                /// \brief
                /// If we're not finished, set the value associated with the current entry.
                /// \param[in] value New \see{Value} to associate with the current entry.
                void SetValue (Value::SharedPtr value);

                /// \brief
                /// BTree is the only one trusted to access sensitive protected data.
                friend struct BTree;
            };

        protected:
            /// \brief
            /// \see{FileAllocator} where we allocate
            /// \see{Header} and \see{Node} blocks from.
            FileAllocator &fileAllocator;
            /// \brief
            /// Offset of the \see{Header} block.
            FileAllocator::PtrType offset;
            /// \struct BTree::Header BTree.h thekogans/util/BTree.h
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
                /// NOTE: Its type is ui32 because 1. we want something
                /// fixed size and 2. if you need more than 4G enries in
                /// one node, you don't need a tree. You need something else.
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
            /// \brief
            /// Key and Value types are cast in stone (in the ctor).
            /// Since we're going to be creating a lot of keys and values
            /// (every \see{Node::Entry} of every \see{Node}), we can save
            /// a ton on \see{DynamicCreatable::CreateType} by caching their
            /// factories and calling them directly when needed.
            DynamicCreatable::FactoryType keyFactory;
            /// \brief
            /// See comment for keyFactory.
            DynamicCreatable::FactoryType valueFactory;
            /// \struct BTree::Node BTree.h thekogans/util/BTree.h
            ///
            /// \brief
            /// BTree nodes store sorted key/value pairs and pointers to children nodes.
            struct Node {
                /// \brief
                /// BTree to which this node belongs.
                BTree &btree;
                /// \brief
                /// Node block offset.
                FileAllocator::PtrType offset;
                /// \brief
                /// Count of entries.
                ui32 count;
                /// \brief
                /// Left most child node block offset.
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
                /// \struct BTree::Node::Entry BTree.h thekogans/util/BTree.h
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
                    /// Right child node block offset.
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
                /// \param[in] btree_ BTree to which this node belongs.
                /// \param[in] offset_ Node offset.
                Node (
                    BTree &btree_,
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
                /// \param[in] btree BTree to which this node belongs.
                /// \param[in] offset Node offset.
                static Node *Alloc (
                    BTree &btree,
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
                /// Flush changes to file.
                void Flush ();
                /// \brief
                /// Return the left child of an entry at the given index.
                /// NOTE: If you need the very last (rightNode) child, call
                /// GetChild (node->count). If you find yourself with an entry
                /// index and you need its right child, call GetChild (index + 1).
                /// \param[in] index Index of entry whose left child to retrieve
                /// (0 == leftNode, !0 == entries[index-1].rightNode).
                /// \return Left child node at the given index. nullptr if no child
                /// at that index exists.
                Node *GetChild (ui32 index);
                /// \brief
                /// Just like Find but begins at whatever the index was set to at call
                /// time. You should never need to call this method directly. The driver
                /// which starts the whole process is FindFirstPrefix and it will do the
                /// right thing. But if you ever find yourself callling this method directly
                /// don't forget to initialize index prior to call.
                /// \param[in] prefix Prefix to find.
                /// \param[in,out] index On entry contains the last index of entry
                /// to search. On successful return will contain the index of the matching
                /// entry.
                /// \return true == found the prefix.
                bool PrefixFind (
                    const Key &prefix,
                    ui32 &index) const;
                /// \brief
                /// Used by \see{BTree::FindFirst} to locate the start of the prefix.
                /// Due to the nature of binary search, \see{PrefixFind} can return
                /// true with a prefix found in the middle of the range. This method
                /// keeps reducing the search space (using \see{PrefixFind}) to locate
                /// the start of the range.
                /// \param[in] prefix Prefix to find.
                /// \param[out] index On true return, contains the index of the first
                /// entry in the node that matches the given prefix.
                /// \return true == found the prefix and it's the first occurence.
                bool FindFirstPrefix (
                    const Key &prefix,
                    ui32 &index) const;
                /// \brief
                /// Search for a given key.
                /// \param[in] key \see{Key} to search for.
                /// \param[out] index If found will contain the index of the key.
                /// If not found will contain the index of the closest larger key.
                /// \return true == found the key.
                bool Find (
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
                /// \param[out] it Iterator to update with the inserted entry.
                /// \return true == entry inserted. false == the entire sub-tree
                /// rooted at this node is full. Time to split the node.
                InsertResult Insert (
                    Entry &entry,
                    Iterator &it);
                /// \brief
                /// Try to recursively delete the given key.
                /// \param[in] key \see{Key} whose entry we want to delete.
                /// \return true == entry was deleted. false == key not found.
                bool Remove (const Key &key);
                /// \brief
                /// The algorithm checks the left and right children @index for
                /// conformity and performs necessary adjustments to maintain the
                /// BTree structure. If one of the children is poor an entry is
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
            BlockAllocator::SharedPtr nodeAllocator;
            /// \brief
            /// We accumulate all changes and update the header in \see{Flush}.
            bool dirty;

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
            /// \param[in] fileAllocator_ BTree heap (see \see{FileAllocator}).
            /// \param[in] offset_ Heap offset of the \see{Header} block.
            /// \param[in] keyType \see{DynamicCreatable} key type.
            /// \param[in] valueType \see{DynamicCreatable} value type. If empty,
            /// will store any type derived from \see{Value}.
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
            /// PRO TIP: If you're interested in creating 'secure' BTrees, pass
            /// \see{thekogans::util::SecureAllocator}::Instance () for allocator.
            /// Keep in mind, secure pages are a scarce resource and should NOT be
            /// used like main application memory. But for small enough trees
            /// containing sensitive data like keys or other personal info, they
            /// might be just the ticket. You will probably need to call;
            /// \see{thekogans::util::SecureAllocator::ReservePages}.
            BTree (
                FileAllocator &fileAllocator_,
                FileAllocator::PtrType offset_,
                const std::string &keyType = std::string (),
                const std::string &valueType = std::string (),
                std::size_t entriesPerNode = DEFAULT_ENTRIES_PER_NODE,
                std::size_t nodesPerPage = BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            /// \brief
            /// This is a FileAllocatorObject ctor. It serves to seamlessly
            /// integrate creation of new FileAllocator objects in to already
            /// running \see{BufferedFile::Transaction}.
            BTree (
                FileAllocator &fileAllocator_,
                BufferedFile::Transaction::SharedPtr transaction,
                const std::string &keyType = std::string (),
                const std::string &valueType = std::string (),
                std::size_t entriesPerNode = DEFAULT_ENTRIES_PER_NODE,
                std::size_t nodesPerPage = BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            /// \brief
            /// dtor.
            virtual ~BTree ();

            // FileAllocatorObject
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
            virtual FileAllocator::PtrType GetOffset () const override {
                return offset;
            }

            /// \brief
            /// Search for the given key in the btree.
            /// \param[in] key \see{Key} to search for.
            /// \param[out] it If found the iterator will point to the entry.
            /// \return true == found.
            bool Find (
                const Key &key,
                Iterator &it);
            /// \brief
            /// Insert the given key in to the btree.
            /// \param[in] key KeyType to insert.
            /// \param[in] value Value associated with the given key.
            /// \param[out] it If inserted, will point at the node entry.
            /// \return true == added. false == duplicate. If false, return
            /// the current entry in it.
            bool Insert (
                Key::SharedPtr key,
                Value::SharedPtr value,
                Iterator &it);
            /// \brief
            /// Delete the given key from the btree.
            /// \param[in] key KeyType whose entry to delete.
            /// \return true == entry deleted. false == entry not found.
            bool Delete (const Key &key);

            /// \brief
            /// Reset the iterator to point to the first occurence of it.prefix.
            /// If the prefix is a nullptr, point to the smallest entry (smallest
            /// as returned by \see{Key::Compare}, which could actually be the largest
            /// if the tree is in descending  order).
            /// IMPORTANT: It's practically imposible to deduce that an iterator
            /// has been invalidated by Add/Delete. To maintain the btree structure
            /// the rotations and merges may delete some nodes while moving others.
            /// It is therefore important that any iterator be created, used quickly,
            /// and discarded.
            /// \param[in,out] it Iterator to reset.
            /// \return true == The iterator pointing to the first occurance of it.prefix
            /// (or smallest element if nullptr). false == the iterator is empty.
            bool FindFirst (Iterator &it);

            /// \brief
            /// Use for debugging. Dump the btree nodes to stdout.
            void Dump ();

        protected:
            // BufferedFileTransactionParticipant
            /// \brief
            /// Flush the node cache (used in tight memory situations).
            virtual void Flush () override;

            /// \brief
            /// Reload from file.
            virtual void Reload () override;

        private:
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

        /// \brief
        /// Implement BTree::Key extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EXTRACTION_OPERATORS (BTree::Key)

        /// \brief
        /// Implement BTree::Value extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EXTRACTION_OPERATORS (BTree::Value)

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_BTree_h)
