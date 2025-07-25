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

/// \struct FileAllocator::BTree FileAllocatorBTree.h thekogans/util/FileAllocatorBTree.h
///
/// \brief
/// BTree for managing \see{FileAllocator} free, random size block list.
/// This class is private to and is only included in \see{FileAllocator}.
/// This implementation is specifically tuned to act as \see{FileAllocator}
/// free list manager. It's logic is subtly different from the general
/// purpose \see{BTree} implementation. It has no need for values, just
/// properly structured keys. Its search is also different as it results
/// in near by entries being returned if they suit the need of an allocation.
/// It's broken out in to its own file because FileAllocator.h was getting
/// too big to maintain.
struct BTree : public BufferedFile::TransactionParticipant {
    /// \brief
    /// Declare \see{RefCounted} pointers.
    THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (BTree)

    /// \brief
    /// KeyType is structured on block {size, offset}.
    using KeyType = std::pair<ui64, PtrType>;
    /// \brief
    /// KeyType size on disk.
    static const std::size_t KEY_TYPE_SIZE = UI64_SIZE + PTR_TYPE_SIZE;

private:
    /// \brief
    /// Reference back to the FileAllocator for
    /// \see{Header} and \see{Node} blocks.
    FileAllocator &fileAllocator;
    /// \struct Fileallocator::BTree::Header FileallocatorBTree.h
    /// thekogans/util/FileallocatorBTree.h
    ///
    /// \brief
    /// Header contains global btree info.
    struct Header {
        /// \brief
        /// Entries per node.
        ui32 entriesPerNode;
        /// \brief
        /// Root node offset.
        PtrType rootOffset;

        /// \brief
        /// Size of header on disk.
        static const std::size_t SIZE =
            UI32_SIZE +     // magic
            UI32_SIZE +     // entriesPerNode
            PTR_TYPE_SIZE;  // rootOffset

        /// \brief
        /// ctor.
        /// \param[in] entriesPerNode_ Entries per node.
        Header (ui32 entriesPerNode_ = DEFAULT_BTREE_ENTRIES_PER_NODE) :
            entriesPerNode (entriesPerNode_),
            rootOffset (0) {}
    } header;
    /// \struct Fileallocator::BTree::Node FileallocatorBTree.h
    /// thekogans/util/FileallocatorBTree.h
    ///
    /// \brief
    /// BTree nodes store sorted keys and pointers to children nodes.
    struct Node : public BufferedFile::TransactionParticipant {
        /// \brief
        /// BTree to which this node belongs.
        BTree &btree;
        /// \brief
        /// Node block offset.
        PtrType offset;
        /// \brief
        /// Count of entries.
        ui32 count;
        /// \brief
        /// Left most child node offset.
        PtrType leftOffset;
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
            KeyType key;
            /// \brief
            /// Right child node offset.
            PtrType rightOffset;
            /// \brief
            /// Right child node.
            Node *rightNode;

            /// \brief
            /// ctor.
            /// \param[in] key_ Entry key.
            Entry (const KeyType &key_ = KeyType (0, 0)) :
                key (key_),
                rightOffset (0),
                rightNode (nullptr) {}
        };
        /// \brief
        /// Entry array. Allocated when the node is allocated.
        Entry *entries;

        /// \brief
        /// ctor.
        /// \param[in] btree_ BTree to which this node belongs.
        /// \param[in] offset_ Node offset.
        Node (
            BTree &btree_,
            PtrType offset_ = 0);
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
            PtrType offset = 0);

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
            const KeyType &key,
            ui32 &index) const;
        /// \brief
        /// Try to recursively insert the given entry.
        /// \param[in] entry \see{Entry} to insert.
        /// \return true == entry inserted. false == the entire sub-tree
        /// rooted at this node is full. Time to split the node.
        bool Insert (Entry &entry);
        /// \brief
        /// Try to recursively delete the given key.
        /// \param[in] key \see{KeyType} whose entry we want to delete.
        /// \return true == entry was deleted. false == key not found.
        bool Remove (const KeyType &key);
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

    protected:
        // BufferedFile::TransactionParticipant
        /// \brief
        /// Allocate space for the node.
        virtual void Alloc () override;
        /// \brief
        virtual void Free () override;
        /// \brief
        /// Flush changes to file.
        virtual void Flush () override;
        /// \brief
        /// Reload from file.
        virtual void Reload () override;
        /// \brief
        virtual void Reset () override;

    private:
        /// \brief
        /// Common logic used by ctor and Reload.
        void Load ();

        virtual void Harakiri () override {
            this->~Node ();
            btree.nodeAllocator->Free (this, Size (btree.header.entriesPerNode));
        }
    } *rootNode;
    /// \brief
    /// An instance of \see{BlockAllocator} to allocate \see{Node}s.
    BlockAllocator::SharedPtr nodeAllocator;

public:
    /// \brief
    /// ctor.
    /// \param[in] fileAllocator_ \see{FileAllocator} to which this btree belongs.
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
        FileAllocator &fileAllocator_,
        std::size_t entriesPerNode,
        std::size_t nodesPerPage,
        Allocator::SharedPtr allocator);
    /// \brief
    /// dtor.
    ~BTree ();

    /// \brief
    /// Find the given key in the btree.
    /// \param[in] key KeyType to find.
    /// \return If found the given key will be returned.
    /// If not found, return the nearest larger key.
    KeyType Find (const KeyType &key);
    /// \brief
    /// Add the given key to the btree.
    /// \param[in] key KeyType to add.
    /// NOTE: Duplicate keys are ignored.
    void Insert (const KeyType &key);
    /// \brief
    /// Delete the given key from the btree.
    /// \param[in] key KeyType whose entry to delete.
    /// \return true == entry deleted. false == entry not found.
    bool Remove (const KeyType &key);

    /// \brief
    /// Use for debugging. Dump the btree nodes to stdout.
    inline void Dump () {
        rootNode->Dump ();
    }

protected:
    // BufferedFile::TransactionParticipant
    /// \brief
    /// Allocate space for the \see{Header}.
    virtual void Alloc () override;
    /// \brief
    /// .
    virtual void Free () override;
    /// \brief
    /// Flush changes to file.
    virtual void Flush () override;
    /// \brief
    /// Reload from file.
    virtual void Reload () override;
    /// \brief
    /// .
    virtual void Reset () override;

    void Load ();

    /// \brief
    /// Needs access to private members.
    friend struct FileAllocator;

    /// \brief
    /// Needs access to private members.
    friend bool operator == (
        const KeyType &key1,
        const KeyType &key2);
    /// \brief
    /// Needs access to private members.
    friend bool operator != (
        const KeyType &key1,
        const KeyType &key2);
    /// \brief
    /// Needs access to private members.
    friend bool operator < (
        const KeyType &key1,
        const KeyType &key2);

    /// \brief
    /// Needs access to private members.
    friend Serializer &operator << (
        Serializer &serializer,
        const KeyType &key);
    /// \brief
    /// Needs access to private members.
    friend Serializer &operator >> (
        Serializer &serializer,
        KeyType &key);

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
