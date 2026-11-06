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

/// \struct TransactedFileBTreeAllocator::BTree TransactedFileBTreeAllocatorBTree.h
/// thekogans/util/TransactedFileBTreeAllocatorBTree.h
///
/// \brief
/// BTree for managing \see{TransactedFileBTreeAllocator} free, random size block list.
/// This class is private to and is only included in \see{TransactedFileBTreeAllocator}.
/// This implementation is specifically tuned to act as \see{TransactedFileBTreeAllocator}
/// free list manager. It's logic is subtly different from the general purpose
/// \see{TransactedFileBTree} implementation. It has no need for values, just
/// properly structured keys. Its search is also different as it results in near
/// by entries being returned if they suit the need of an allocation. It's broken
/// out in to its own file because TransactedFileBTreeAllocator.h was getting too
/// big to maintain.
struct BTree : public TransactedFile::Object {
    /// \brief
    /// Object is a \see{Serializable}.
    THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT (BTree)

    /// \struct TransactedFileBTreeAllocator::BTree::Key TransactedFileBTreeAllocator.h
    /// thekogans/util/TransactedFileBTreeAllocator.h
    ///
    /// \brief
    /// Key is structured on block {size, offset}.
    struct Key {
        /// \brief
        /// Block size.
        TransactedFile::SizeType size{0};
        /// \brief
        /// Block offset.
        TransactedFile::AddressType offset{0};

        /// \brief
        /// Key size on disk.
        static constexpr std::size_t SIZE =
            TransactedFile::SizeTypeSize +
            TransactedFile::AddressTypeSize;
    };

private:
    /// \struct TransactedFileBTreeAllocator::BTree::Header TransactedFileBTreeAllocatorBTree.h
    /// thekogans/util/TransactedFileBTreeAllocatorBTree.h
    ///
    /// \brief
    /// Header contains global btree info.
    struct Header {
        /// \brief
        /// Entries per node.
        ui32 entriesPerNode;
        /// \brief
        /// Root node offset.
        TransactedFile::AddressType rootOffset;

        /// \brief
        /// Size of header on disk.
        static const std::size_t SIZE =
            UI32_SIZE +     // magic
            UI32_SIZE +     // entriesPerNode
            TransactedFile::AddressTypeSize;  // rootOffset

        /// \brief
        /// ctor.
        /// \param[in] entriesPerNode_ Entries per node.
        Header (ui32 entriesPerNode_ = DEFAULT_BTREE_ENTRIES_PER_NODE) :
            entriesPerNode (entriesPerNode_),
            rootOffset (0) {}
    } header;
    /// \struct TransactedFileBTreeAllocator::BTree::Node TransactedFileBTreeAllocatorBTree.h
    /// thekogans/util/TransactedFileBTreeAllocatorBTree.h
    ///
    /// \brief
    /// BTree nodes store sorted keys and pointers to children nodes.
    struct Node : public TransactedFile::Object {
        /// \brief
        /// Object is a \see{Serializable}.
        THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT (Node)

        /// \brief
        ui32 maxEntries;
        /// \brief
        /// Count of entries.
        ui32 count;
        /// \brief
        /// Left most child node offset.
        TransactedFile::AddressType leftOffset;
        /// \brief
        /// Left most child node.
        Node *leftNode;
        tk_mem_t mem;
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
            TransactedFile::AddressType rightOffset;
            /// \brief
            /// Right child node.
            Node *rightNode;

            /// \brief
            /// ctor.
            /// \param[in] key_ Entry key.
            Entry (const Key &key_ = {0, 0}) :
                key (key_),
                rightOffset (0),
                rightNode (nullptr) {}
        };
        /// \brief
        /// Entry array. Allocated when the node is allocated.
        Entry *entries;

        /// \brief
        /// ctor.
        /// \param[in] file \see{TransactedFile} where the Node resides.
        /// \param[in] offset Node offset.
        /// \param[in] btree_.
        Node (
            TransactedFile::SharedPtr file = nullptr,
            TransactedFile::AddressType offset = 0,
            ui32 maxEntries_ = DEFAULT_BTREE_ENTRIES_PER_NODE,
            tk_mem_t mem_ = {nullptr, 0});
        /// \brief
        /// dtor.
        virtual ~Node ();

        /// \brief
        /// Given the number of entries, return the node file size in bytes.
        /// \param[in] maxEntries Entries per node.
        /// \return Size of node on disk.
        static std::size_t FileSize (std::size_t maxEntries);
        /// \brief
        /// Given the number of entries, return the node size in bytes.
        /// \param[in] maxEntries Entries per node.
        /// \return Size of node in memory.
        static std::size_t Size (std::size_t maxEntries);
        /// \brief
        /// Allocate a node.
        /// \param[in] file \see{TransactedFile} where the Node resides.
        /// \param[in] offset Node offset.
        /// \param[in] btree
        /// \return New node.
        static Node *Alloc (
            TransactedFile::SharedPtr file,
            TransactedFile::AddressType offset,
            ui32 maxEntries);
        /// \brief
        /// Free the subtree rooted at node.
        /// \param[in] file \see{TransactedFile} where the Node resides.
        /// \param[in] offset Node offset.
        static void FreeObject (
            TransactedFile::SharedPtr file,
            TransactedFile::AddressType offset);

        /// \brief
        /// Return the child at the given index.
        /// \param[in] index Index of child to retrieve
        /// (0 == left, !0 == entries[index - 1].right).
        /// \return Child node at the given index. nullptr if no child at that index exists.
        Node *GetChild (ui32 index);
        /// \brief
        /// Search for a given key.
        /// \param[in] key \see{Key} to search for.
        /// \param[out] index If found will contain the index of the key.
        /// If not found will contain the index of the closest larger key.
        /// \return true == found the key.
        bool Find (
            const Key &key,
            ui32 &index) const;
        /// \brief
        /// Try to recursively insert the given entry.
        /// \param[in] entry \see{Entry} to insert.
        /// \return true == entry inserted. false == the entire sub-tree
        /// rooted at this node is full. Time to split the node.
        bool Insert (Entry &entry);
        /// \brief
        /// Try to recursively delete the given key.
        /// \param[in] key \see{Key} whose entry we want to delete.
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
        /// Split the node in the middle (splitIndex = maxEntries / 2).
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
            return count == maxEntries;
        }
        /// \brief
        /// Return true if less than half the nodes entries are occupied.
        /// \return true if less than half the nodes entries are occupied.
        inline bool IsPoor () const {
            return count < maxEntries / 2;
        }
        /// \brief
        /// Return true if more than half the nodes entries are occupied.
        /// \return true if more than half the nodes entries are occupied.
        inline bool IsPlentiful () const {
            return count > maxEntries / 2;
        }

    protected:
        /// \brief
        /// Clear the cache.
        void Reset ();

        // RefCounted
        /// \brief
        /// Undo what \see{Alloc} did.
        virtual void Harakiri () override;

    protected:
        // Serializable
        /// \brief
        /// Return the node size.
        /// \return btree.allocator.btreeNodeFileSize.
        virtual std::size_t Size () const noexcept override {
            return FileSize (maxEntries);
        }
        /// \brief
        /// Read the node.
        /// \param[in] serializer \see{Serializer} to read the node from.
        virtual void Read (Serializer &serializer) override;
        /// \brief
        /// Write the node.
        /// \param[in] serializer \see{Serializer} to write the node to.
        virtual void Write (Serializer &serializer) override;

        /// \brief
        /// Node is neither copy or move constructable, nor assignable.
        THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (Node)
    } *root;

public:
    /// \brief
    /// ctor.
    /// \param[in] file \see{TransactedFile} to which this btree belongs.
    /// \param[in] offset \see{Header} offset.
    /// \param[in] entriesPerNode If we're creating the heap, contains entries per
    /// \see{Node}. If we're reading an existing heap, this value will come from the
    /// \see{Header}.
    BTree (
        TransactedFile::SharedPtr file = nullptr,
        TransactedFile::AddressType offset = 0,
        std::size_t entriesPerNode = DEFAULT_BTREE_ENTRIES_PER_NODE);
    /// \brief
    /// dtor.
    virtual ~BTree ();

    /// \brief
    /// Find the given key in the btree.
    /// \param[in] key Key to find.
    /// \return If found the given key will be returned.
    /// If not found, return the nearest larger key.
    Key Find (const Key &key);
    /// \brief
    /// Add the given key to the btree.
    /// \param[in] key Key to add.
    /// NOTE: Duplicate keys are ignored.
    void Insert (const Key &key);
    /// \brief
    /// Delete the given key from the btree.
    /// \param[in] key Key whose entry to delete.
    /// \return true == entry deleted. false == entry not found.
    bool Remove (const Key &key);

protected:
    // Serializable
    /// \brief
    /// Our size is \see{Header::SIZE}.
    /// \return \see{Header::SIZE}.
    virtual std::size_t Size () const noexcept override {
        return Header::SIZE;
    }
    /// \brief
    /// Read \see{Header::SIZE}.
    /// \param[in] serializer \see{Serializer} to read the header from.
    virtual void Read (Serializer &serializer) override;
    /// \brief
    /// Write \see{Header::SIZE}.
    /// \param[in] serializer \see{Serializer} to write the header to.
    virtual void Write (Serializer &serializer) override;

    // TransactedFile::Object
    virtual void Init (
            TransactedFile::SharedPtr file,
            TransactedFile::AddressType offset) noexcept override {
        TransactedFile::Object::Init (file, offset);
        root->Init (file, header.rootOffset);
    }

    /// \brief
    /// Free \see{Header} block using \see{TransactedFileBTreeAllocator::FreeBTreeNode}.
    /// Also free the \see{Node} hierarchy @root.
    static void FreeObject (
        TransactedFile::SharedPtr file,
        TransactedFile::AddressType offset);

    /// \brief
    /// Needs access to private members.
    friend struct TransactedFileBTreeAllocator;

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
    friend bool operator > (
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
    /// BTree is neither copy or move constructable, nor assignable.
    THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (BTree)
};
