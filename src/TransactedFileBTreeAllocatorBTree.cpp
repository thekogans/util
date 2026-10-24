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

#include <cstring>
#include "thekogans/util/Exception.h"
#include "thekogans/util/TransactedFileBTreeAllocator.h"

namespace thekogans {
    namespace util {

        inline Serializer &operator << (
               Serializer &serializer,
               const TransactedFileBTreeAllocator::BTree::Key &key) {
            serializer << key.size << key.offset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTreeAllocator::BTree::Key &key) {
            serializer >> key.size >> key.offset;
            return serializer;
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const TransactedFileBTreeAllocator::BTree::Node::Entry &entry) {
            serializer << entry.key << entry.rightOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTreeAllocator::BTree::Node::Entry &entry) {
            serializer >> entry.key >> entry.rightOffset;
            // Because of the way we allocate the BTree::Node the
            // Entry ctor is never called. In a way this extraction
            // operator is our ctor. Make sure all members are
            // properly initialized.
            entry.rightNode = nullptr;
            return serializer;
        }

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (
            thekogans::util::TransactedFileBTreeAllocator::BTree::Node,
            1,
            0,
            TransactedFile::Object::TYPE)

        TransactedFileBTreeAllocator::BTree::Node::Node (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset,
                BTree *btree_) :
                TransactedFile::Object (file, offset),
                btree (btree_),
                count (0),
                leftOffset (0),
                leftNode (nullptr),
                entries ((Entry *)(this + 1)) {
            Reload ();
        }

        TransactedFileBTreeAllocator::BTree::Node::~Node () {
            Reset ();
        }

        std::size_t TransactedFileBTreeAllocator::BTree::Node::FileSize (std::size_t entriesPerNode) {
                                        // key         rightOffset
            const std::size_t ENTRY_SIZE = Key::SIZE + TransactedFile::AddressTypeSize;
                // magic       count       leftOffset                        entries
            return UI32_SIZE + UI32_SIZE + TransactedFile::AddressTypeSize + entriesPerNode * ENTRY_SIZE;
        }

        std::size_t TransactedFileBTreeAllocator::BTree::Node::Size (std::size_t entriesPerNode) {
            return sizeof (Node) + entriesPerNode * sizeof (Entry);
        }

        TransactedFileBTreeAllocator::BTree::Node *TransactedFileBTreeAllocator::BTree::Node::Alloc (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset,
                BTree *btree) {
            Node *node = new (
                btree->nodeAllocator->Alloc (Size (btree->header.entriesPerNode))) Node (file, offset, btree);
            node->AddRef ();
            return node;
        }

        void TransactedFileBTreeAllocator::BTree::Node::FreeSubtree (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset) {
            if (offset != 0) {
                TransactedFile::BlockRange range (file, offset);
                ui32 magic;
                range >> magic;
                if (magic == MAGIC32) {
                    ui32 count;
                    range >> count;
                    if (count > 0) {
                        TransactedFile::AddressType leftOffset;
                        range >> leftOffset;
                        FreeSubtree (file, leftOffset);
                        for (ui32 i = 0; i < count; ++i) {
                            TransactedFile::AddressType rightOffset;
                            range >> rightOffset;
                            FreeSubtree (file, rightOffset);
                        }
                    }
                    file->GetAllocator ()->Free (offset);
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt BTree::Node @" THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
            }
        }

        TransactedFileBTreeAllocator::BTree::Node *TransactedFileBTreeAllocator::BTree::Node::GetChild (ui32 index) {
            if (index == 0) {
                if (leftNode == nullptr && leftOffset != 0) {
                    leftNode = Alloc (file, leftOffset, btree);
                    leftNode->Track (this, &leftOffset);
                }
                return leftNode;
            }
            else {
                --index;
                if (entries[index].rightNode == nullptr && entries[index].rightOffset != 0) {
                    entries[index].rightNode = Alloc (file, entries[index].rightOffset, btree);
                    entries[index].rightNode->Track (this, &entries[index].rightOffset);
                }
                return entries[index].rightNode;
            }
        }

        bool TransactedFileBTreeAllocator::BTree::Node::Find (
                const Key &key,
                ui32 &index) const {
            index = 0;
            ui32 last = count;
            while (index < last) {
                ui32 middle = (index + last) / 2;
                if (key == entries[middle].key) {
                    index = middle;
                    return true;
                }
                if (key < entries[middle].key) {
                    last = middle;
                }
                else {
                    index = middle + 1;
                }
            }
            return false;
        }

        bool TransactedFileBTreeAllocator::BTree::Node::Insert (Entry &entry) {
            ui32 index;
            // This search collapses all duplicate nodes in to one.
            // That means that you can insert the same node all you
            // want, but it will only be inserted the first time with
            // all subsequent ones being a noop.
            if (!Find (entry.key, index)) {
                Node *child = GetChild (index);
                if (child == nullptr || !child->Insert (entry)) {
                    if (!IsFull ()) {
                        InsertEntry (entry, index);
                    }
                    else {
                        // Node is full. Split it and insert the entry
                        // in to the proper daughter node. Return the
                        // entry at the split point to be added up the
                        // parent chain.
                        Node *right = Alloc (file, 0, btree);
                        Split (right);
                        ui32 splitIndex = btree->header.entriesPerNode / 2;
                        if (index != splitIndex) {
                            if (index < splitIndex) {
                                InsertEntry (entry, index);
                            }
                            else {
                                right->InsertEntry (entry, index - splitIndex);
                            }
                            entry = right->entries[0];
                            right->RemoveEntry (0);
                        }
                        right->leftOffset = entry.rightOffset;
                        right->leftNode = entry.rightNode;
                        entry.rightOffset = right->offset;
                        entry.rightNode = right;
                        return false;
                    }
                }
            }
            return true;
        }

        bool TransactedFileBTreeAllocator::BTree::Node::Remove (const Key &key) {
            ui32 index;
            bool found = Find (key, index);
            Node *child = GetChild (found ? index + 1 : index);
            if (found) {
                if (child != nullptr) {
                    Node *leaf = child;
                    while (leaf->leftOffset != 0) {
                        leaf = leaf->GetChild (0);
                    }
                    entries[index].key = leaf->entries[0].key;
                    child->Remove (leaf->entries[0].key);
                    SetDirty ();
                    if (child->IsPoor ()) {
                        RestoreBalance (index);
                    }
                }
                else {
                    RemoveEntry (index);
                }
                return true;
            }
            else if (child != nullptr && child->Remove (key)) {
                if (child->IsPoor ()) {
                    RestoreBalance (index);
                }
                return true;
            }
            return false;
        }

        void TransactedFileBTreeAllocator::BTree::Node::RestoreBalance (ui32 index) {
            if (index == count) {
                Node *left = GetChild (index - 1);
                Node *right = GetChild (index);
                if (left != nullptr && right != nullptr) {
                    if (left->IsPlentiful ()) {
                        RotateRight (index - 1, left, right);
                    }
                    else {
                        Merge (index - 1, left, right);
                    }
                }
            }
            else {
                Node *left = GetChild (index);
                Node *right = GetChild (index + 1);
                if (left != nullptr && right != nullptr) {
                    if (left->IsPlentiful ()) {
                        RotateRight (index, left, right);
                    }
                    else if (right->IsPlentiful ()) {
                        RotateLeft (index, left, right);
                    }
                    else {
                        Merge (index, left, right);
                    }
                }
            }
        }

        void TransactedFileBTreeAllocator::BTree::Node::RotateRight (
                ui32 index,
                Node *left,
                Node *right) {
            entries[index].rightOffset = right->leftOffset;
            entries[index].rightNode = right->leftNode;
            right->InsertEntry (entries[index], 0);
            ui32 lastIndex = left->count - 1;
            right->leftOffset = left->entries[lastIndex].rightOffset;
            right->leftNode = left->entries[lastIndex].rightNode;
            left->entries[lastIndex].rightOffset = right->offset;
            left->entries[lastIndex].rightNode = right;
            entries[index] = left->entries[lastIndex];
            left->RemoveEntry (lastIndex);
            SetDirty ();
        }

        void TransactedFileBTreeAllocator::BTree::Node::RotateLeft (
                ui32 index,
                Node *left,
                Node *right) {
            entries[index].rightOffset = right->leftOffset;
            entries[index].rightNode = right->leftNode;
            right->leftOffset = right->entries[0].rightOffset;
            right->leftNode = right->entries[0].rightNode;
            right->entries[0].rightOffset = right->offset;
            right->entries[0].rightNode = right;
            left->Concatenate (entries[index]);
            entries[index] = right->entries[0];
            right->RemoveEntry (0);
            SetDirty ();
        }

        void TransactedFileBTreeAllocator::BTree::Node::Merge (
                ui32 index,
                Node *left,
                Node *right) {
            assert (left->count + right->count < btree->header.entriesPerNode);
            entries[index].rightOffset = right->leftOffset;
            entries[index].rightNode = right->leftNode;
            left->Concatenate (entries[index]);
            left->Concatenate (right);
            RemoveEntry (index);
            right->FreeBlock ();
            right->Release ();
        }

        void TransactedFileBTreeAllocator::BTree::Node::Split (Node *node) {
            ui32 splitIndex = btree->header.entriesPerNode / 2;
            node->count = count - splitIndex;
            std::memcpy (
                node->entries,
                entries + splitIndex,
                node->count * sizeof (Entry));
            count = splitIndex;
            SetDirty ();
            node->SetDirty ();
        }

        void TransactedFileBTreeAllocator::BTree::Node::Concatenate (Node *node) {
            std::memcpy (
                entries + count,
                node->entries,
                node->count * sizeof (Entry));
            count += node->count;
            node->count = 0;
            SetDirty ();
            node->SetDirty ();
        }

        void TransactedFileBTreeAllocator::BTree::Node::InsertEntry (
                const Entry &entry,
                ui32 index) {
            std::memmove (
                entries + index + 1,
                entries + index,
                (count++ - index) * sizeof (Entry));
            entries[index] = entry;
            SetDirty ();
        }

        void TransactedFileBTreeAllocator::BTree::Node::RemoveEntry (ui32 index) {
            std::memcpy (
                entries + index,
                entries + index + 1,
                (--count - index) * sizeof (Entry));
            SetDirty ();
        }

        void TransactedFileBTreeAllocator::BTree::Node::Reset () {
            if (count > 0) {
                if (leftNode != nullptr) {
                    leftNode->Untrack (this);
                    leftNode->Release ();
                    leftNode = nullptr;
                }
                for (ui32 i = 0; i < count; ++i) {
                    if (entries[i].rightNode != nullptr) {
                        entries[i].rightNode->Untrack (this);
                        entries[i].rightNode->Release ();
                        entries[i].rightNode = nullptr;
                    }
                }
                count = 0;
            }
        }

        void TransactedFileBTreeAllocator::BTree::Node::Read (Serializer &serializer) {
            Reset ();
            ui32 magic;
            serializer >> magic;
            if (magic == MAGIC32) {
                serializer >> count;
                if (count > 0) {
                    serializer >> leftOffset;
                    for (ui32 i = 0; i < count; ++i) {
                        serializer >> entries[i];
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFileBTreeAllocator::BTree::Node: @" THEKOGANS_UTIL_UI64_FORMAT,
                    GetOffset ());
            }
        }

        void TransactedFileBTreeAllocator::BTree::Node::Write (Serializer &serializer) {
            serializer << MAGIC32 << count;
            if (count > 0) {
                if (leftNode != nullptr) {
                    leftOffset = leftNode->offset;
                }
                serializer << leftOffset;
                for (ui32 i = 0; i < count; ++i) {
                    if (entries[i].rightNode != nullptr) {
                        entries[i].rightOffset = entries[i].rightNode->offset;
                    }
                    serializer << entries[i];
                }
            }
        }

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (
            thekogans::util::TransactedFileBTreeAllocator::BTree,
            1,
            0,
            TransactedFile::Object::TYPE)

        TransactedFileBTreeAllocator::BTree::BTree (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset,
                std::size_t entriesPerNode,
                std::size_t nodesPerPage,
                util::Allocator::SharedPtr allocator) :
                TransactedFile::Object (file, offset),
                header ((ui32)entriesPerNode),
                nodeAllocator (
                    new BlockAllocator (
                        Node::Size (entriesPerNode),
                        nodesPerPage,
                        allocator)),
                root (Node::Alloc (file, header.rootOffset, this)) {
            root->Track (this, &header.rootOffset);
            Reload ();
        }

        TransactedFileBTreeAllocator::BTree::~BTree () {
            root->Untrack (this);
            root->Release ();
        }

        TransactedFileBTreeAllocator::BTree::Key TransactedFileBTreeAllocator::BTree::Find (const Key &key) {
            Key result (UI64_MAX, 0);
            Node *node = root;
            while (node != nullptr) {
                ui32 index;
                if (node->Find (key, index)) {
                    return key;
                }
                if (index < node->count &&
                        node->entries[index].key > key && node->entries[index].key < result) {
                    result = node->entries[index].key;
                }
                node = node->GetChild (index);
            }
            return result;
        }

        void TransactedFileBTreeAllocator::BTree::Insert (const Key &key) {
            Node::Entry entry (key);
            if (!root->Insert (entry)) {
                // The path to the leaf node is full.
                // Create a new root node and make the entry
                // its first.
                Node *node = Node::Alloc (file, 0, this);
                root->Untrack (this);
                node->leftOffset = root->GetOffset ();
                node->leftNode = root;
                node->leftNode->Track (node, &node->leftOffset);
                node->InsertEntry (entry, 0);
                root = node;
                root->Track (this, &header.rootOffset);
            }
        }

        bool TransactedFileBTreeAllocator::BTree::Remove (const Key &key) {
            bool removed = root->Remove (key);
            if (removed) {
                if (root->IsEmpty () && root->GetChild (0) != nullptr) {
                    Node *node = root;
                    node->Untrack (this);
                    root = root->GetChild (0);
                    root->Track (this, &header.rootOffset);
                    node->FreeBlock ();
                    node->Release ();
                }
            }
            return removed;
        }

        void TransactedFileBTreeAllocator::BTree::FreeBlock () {
            Node::FreeSubtree (file, header.rootOffset);
            header.rootOffset = 0;
            Object::FreeBlock ();
            root->Untrack (this);
            root->Release ();
            root = Node::Alloc (file, header.rootOffset, this);
            root->Track (this, &header.rootOffset);
        }

        void TransactedFileBTreeAllocator::BTree::Read (Serializer &serializer) {
            ui32 magic;
            serializer >> magic;
            if (magic == MAGIC32) {
                serializer >> header;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFileBTreeAllocator::BTree: @" THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
            root->Untrack (this);
            root->Release ();
            nodeAllocator.Reset (
                new BlockAllocator (
                    Node::Size (header.entriesPerNode),
                    nodeAllocator->GetBlocksPerPage (),
                    nodeAllocator->GetAllocator ()));
            root = Node::Alloc (file, header.rootOffset, this);
            root->Track (this, &header.rootOffset);
        }

        void TransactedFileBTreeAllocator::BTree::Write (Serializer &serializer) {
            header.rootOffset = root->GetOffset ();
            serializer << MAGIC32 << header;
        }

        inline bool operator == (
                const TransactedFileBTreeAllocator::BTree::Key &key1,
                const TransactedFileBTreeAllocator::BTree::Key &key2) {
            return key1.size == key2.size && key1.offset == key2.offset;
        }

        inline bool operator != (
                const TransactedFileBTreeAllocator::BTree::Key &key1,
                const TransactedFileBTreeAllocator::BTree::Key &key2) {
            return key1.size != key2.size || key1.offset != key2.offset;
        }

        inline bool operator < (
                const TransactedFileBTreeAllocator::BTree::Key &key1,
                const TransactedFileBTreeAllocator::BTree::Key &key2) {
            return key1.size < key2.size ||
                (key1.size == key2.size && key1.offset < key2.offset);
        }

        inline bool operator > (
                const TransactedFileBTreeAllocator::BTree::Key &key1,
                const TransactedFileBTreeAllocator::BTree::Key &key2) {
            return key1.size > key2.size ||
                (key1.size == key2.size && key1.offset > key2.offset);
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const TransactedFileBTreeAllocator::BTree::Header &header) {
            serializer << header.entriesPerNode << header.rootOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTreeAllocator::BTree::Header &header) {
            serializer >> header.entriesPerNode >> header.rootOffset;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
