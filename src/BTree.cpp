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
#include "thekogans/util/BTree.h"

namespace thekogans {
    namespace util {

        namespace {
            inline Serializer &operator << (
                    Serializer &serializer,
                    const BTree::Key &key) {
                serializer << key.first << key.second;
                return serializer;
            }

            inline Serializer &operator >> (
                    Serializer &serializer,
                    BTree::Key &key) {
                serializer >> key.first >> key.second;
                return serializer;
            }
        }

        BTree::Node::Node (
                BTree &btree_,
                FileAllocator::PtrType offset_) :
                btree (btree_),
                offset (offset_),
                count (0),
                leftOffset (nullptr),
                leftNode (nullptr) {
            if (offset != nullptr) {
                FileAllocator::Block::SharedPtr block =
                    btree.fileNodeAllocator->CreateBlock (
                        offset, FileSize (btree.header.entriesPerNode), true);
                *block >> count;
                if (count > 0) {
                    *block >> leftOffset;
                    for (ui32 i = 0; i < count; ++i) {
                        *block >> entries[i];
                    }
                }
            }
            else {
                offset = btree.fileNodeAllocator->Alloc (
                    FileSize (btree.header.entriesPerNode));
                Save ();
            }
        }

        BTree::Node::~Node () {
            if (count > 0) {
                Free (leftNode);
                for (ui32 i = 0; i < count; ++i) {
                    Free (entries[i].rightNode);
                }
            }
        }

        std::size_t BTree::Node::FileSize (ui32 entriesPerNode) {
            const std::size_t ENTRY_SIZE = UI64_SIZE + UI64_SIZE + FileAllocator::PtrTypeSize;
            return UI32_SIZE + FileAllocator::PtrTypeSize + entriesPerNode * ENTRY_SIZE;
        }

        std::size_t BTree::Node::Size (ui32 entriesPerNode) {
            return sizeof (Node) + (entriesPerNode - 1) * sizeof (Entry);
        }

        BTree::Node *BTree::Node::Alloc (
                BTree &btree,
                FileAllocator::PtrType offset) {
            return new (
                btree.nodeAllocator->Alloc (
                    Size (btree.header.entriesPerNode))) Node (btree, offset);
        }

        void BTree::Node::Free (Node *node) {
            if (node != nullptr) {
                node->~Node ();
                node->btree.nodeAllocator->Free (
                    node, Size (node->btree.header.entriesPerNode));
            }
        }

        void BTree::Node::Delete (Node *node) {
            if (node->count == 0) {
                node->btree.fileNodeAllocator->Free (
                    node->offset,
                    FileSize (node->btree.header.entriesPerNode));
                Free (node);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Logic error: trying to delete a node with count > 0 "
                    THEKOGANS_UTIL_UI64_FORMAT,
                    node->offset);
            }
        }

        void BTree::Node::Save () {
            FileAllocator::Block::SharedPtr block =
                btree.fileNodeAllocator->CreateBlock (
                    offset, FileSize (btree.header.entriesPerNode));
            *block << count;
            if (count > 0) {
                *block << leftOffset;
                for (ui32 i = 0; i < count; ++i) {
                    *block << entries[i];
                }
            }
            btree.fileNodeAllocator->WriteBlock (block);
        }

        BTree::Node *BTree::Node::GetChild (ui32 index) {
            if (index == 0) {
                if (leftNode == nullptr && leftOffset != nullptr) {
                    leftNode = Alloc (btree, leftOffset);
                }
                return leftNode;
            }
            else {
                --index;
                if (entries[index].rightNode == nullptr &&
                        entries[index].rightOffset != nullptr) {
                    entries[index].rightNode = Alloc (
                        btree, entries[index].rightOffset);
                }
                return entries[index].rightNode;
            }
        }

        bool BTree::Node::Search (
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

        void BTree::Node::Split (
                Node *node,
                ui32 index) {
            node->count = count - index;
            std::memcpy (
                node->entries,
                entries + index,
                node->count * sizeof (Entry));
            count = index;
        }

        void BTree::Node::Concatenate (Node *node) {
            std::memcpy (
                entries + count,
                node->entries,
                node->count * sizeof (Entry));
            count += node->count;
            node->count = 0;
        }

        void BTree::Node::InsertEntry (
                const Entry &entry,
                ui32 index) {
            std::memmove (
                entries + index + 1,
                entries + index,
                (count++ - index) * sizeof (Entry));
            entries[index] = entry;
        }

        void BTree::Node::RemoveEntry (ui32 index) {
            std::memcpy (
                entries + index,
                entries + index + 1,
                (--count - index) * sizeof (Entry));
        }

        /*
                    FileAllocator::Pool::Instance ()->GetFileAllocator (
                        path,
                        Node::FileSize (entriesPerNode),
                        nodesPerPage,
                        allocator)),
         */

        BTree::BTree (
                FileAllocator::SharedPtr fileNodeAllocator_,
                FileAllocator::PtrType offset_,
                ui32 entriesPerNode,
                std::size_t nodesPerPage,
                Allocator::SharedPtr allocator) :
                fileNodeAllocator (fileNodeAllocator_),
                offset (offset_),
                header (entriesPerNode),
                nodeAllocator (
                    BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                        Node::Size (entriesPerNode),
                        nodesPerPage,
                        allocator)),
                root (nullptr) {
            if (offset != nullptr) {
                FileAllocator::Block::SharedPtr block =
                    fileNodeAllocator->CreateBlock (offset, Header::SIZE, true);
                *block >> header;
                if (header.entriesPerNode != entriesPerNode) {
                    nodeAllocator =
                        BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                            Node::Size (header.entriesPerNode),
                            nodesPerPage,
                            allocator);
                }
            }
            else {
                offset = fileNodeAllocator->Alloc (Header::SIZE);
                Save ();
            }
            root = Node::Alloc (*this, header.rootOffset);
        }

        BTree::~BTree () {
            Node::Free (root);
        }

        BTree::Key BTree::Search (const Key &key) {
            Key result (NIDX64, NIDX64);
            Node *node = root;
            while (node != nullptr) {
                ui32 index;
                if (node->Search (key, index)) {
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

        void BTree::Add (const Key &key) {
            Node::Entry entry (key);
            if (!Insert (entry, root)) {
                // The path to the leaf node is full.
                // Create a new root node and make the entry
                // its first.
                Node *node = Node::Alloc (*this);
                node->leftOffset = root->offset;
                node->leftNode = root;
                node->InsertEntry (entry, 0);
                node->Save ();
                SetRoot (node);
            }
        }

        bool BTree::Delete (const Key &key) {
            bool removed = Remove (key, root);
            if (removed && root->IsEmpty () && root->GetChild (0) != nullptr) {
                Node *node = root;
                SetRoot (root->GetChild (0));
                Node::Delete (node);
            }
            return removed;
        }

        void BTree::Flush () {
            Node::Free (root);
            root = Node::Alloc (*this, header.rootOffset);
        }

        bool BTree::Insert (
                Node::Entry &entry,
                Node *node) {
            if (node != nullptr) {
                ui32 index;
                // This search collapses all duplicate nodes in to one.
                // That means that you can insert the same node all you
                // want, but it will only be inserted the first time with
                // all subsequent ones being a noop.
                if (!node->Search (entry.key, index) &&
                        !Insert (entry, node->GetChild (index))) {
                    if (!node->IsFull ()) {
                        node->InsertEntry (entry, index);
                        node->Save ();
                    }
                    else {
                        // Node is full. Split it and insert the entry
                        // in to the proper daughter node. Return the
                        // entry at the split point to be added up the
                        // parent chain.
                        Node *right = Node::Alloc (*this);
                        ui32 splitIndex = header.entriesPerNode / 2;
                        node->Split (right, splitIndex);
                        if (index != splitIndex) {
                            if (index < splitIndex) {
                                node->InsertEntry (entry, index);
                            }
                            else {
                                right->InsertEntry (entry, index - splitIndex);
                            }
                            entry = right->entries[0];
                            right->RemoveEntry (0);
                        }
                        node->Save ();
                        right->leftOffset = entry.rightOffset;
                        right->leftNode = entry.rightNode;
                        right->Save ();
                        entry.rightOffset = right->offset;
                        entry.rightNode = right;
                        return false;
                    }
                }
                return true;
            }
            return false;
        }

        bool BTree::Remove (
                const Key &key,
                Node *node) {
            ui32 index;
            bool found = node->Search (key, index);
            Node *child = node->GetChild (found ? index + 1 : index);
            if (found) {
                if (child != nullptr) {
                    Node *leaf = child;
                    while (leaf->leftOffset != nullptr) {
                        leaf = leaf->GetChild (0);
                    }
                    node->entries[index].key = leaf->entries[0].key;
                    Remove (leaf->entries[0].key, child);
                    if (child->IsPoor ()) {
                        RestoreBalance (node, index);
                    }
                }
                else {
                    node->RemoveEntry (index);
                }
                node->Save ();
                return true;
            }
            else if (child != nullptr && Remove (key, child)) {
                if (child->IsPoor ()) {
                    RestoreBalance (node, index);
                }
                return true;
            }
            return false;
        }

        void BTree::RestoreBalance (
                Node *node,
                ui32 index) {
            if (index == node->count) {
                Node *left = node->GetChild (index - 1);
                Node *right = node->GetChild (index);
                if (left != nullptr && right != nullptr) {
                    if (left->IsPlentiful ()) {
                        RotateRight (node, index - 1, left, right);
                    }
                    else {
                        Merge (node, index - 1, left, right);
                    }
                }
            }
            else {
                Node *left = node->GetChild (index);
                Node *right = node->GetChild (index + 1);
                if (left != nullptr && right != nullptr) {
                    if (left->IsPlentiful ()) {
                        RotateRight (node, index, left, right);
                    }
                    else if (right->IsPlentiful ()) {
                        RotateLeft (node, index, left, right);
                    }
                    else {
                        Merge (node, index, left, right);
                    }
                }
            }
        }

        void BTree::RotateRight (
                Node *node,
                ui32 index,
                Node *left,
                Node *right) {
            node->entries[index].rightOffset = right->leftOffset;
            node->entries[index].rightNode = right->leftNode;
            right->InsertEntry (node->entries[index], 0);
            ui32 lastIndex = left->count - 1;
            right->leftOffset = left->entries[lastIndex].rightOffset;
            right->leftNode = left->entries[lastIndex].rightNode;
            left->entries[lastIndex].rightOffset = right->offset;
            left->entries[lastIndex].rightNode = right;
            node->entries[index] = left->entries[lastIndex];
            left->RemoveEntry (lastIndex);
            node->Save ();
            left->Save ();
            right->Save ();
        }

        void BTree::RotateLeft (
                Node *node,
                ui32 index,
                Node *left,
                Node *right) {
            node->entries[index].rightOffset = right->leftOffset;
            node->entries[index].rightNode = right->leftNode;
            right->leftOffset = right->entries[0].rightOffset;
            right->leftNode = right->entries[0].rightNode;
            right->entries[0].rightOffset = right->offset;
            right->entries[0].rightNode = right;
            left->Concatenate (node->entries[index]);
            node->entries[index] = right->entries[0];
            right->RemoveEntry (0);
            node->Save ();
            left->Save ();
            right->Save ();
        }

        void BTree::Merge (
                Node *node,
                ui32 index,
                Node *left,
                Node *right) {
            assert (left->count + right->count < header.entriesPerNode);
            node->entries[index].rightOffset = right->leftOffset;
            node->entries[index].rightNode = right->leftNode;
            left->Concatenate (node->entries[index]);
            left->Concatenate (right);
            node->RemoveEntry (index);
            node->Save ();
            left->Save ();
            Node::Delete (right);
        }

        void BTree::Save () {
            FileAllocator::Block::SharedPtr block =
                fileNodeAllocator->CreateBlock (offset, Header::SIZE);
            *block << header;
            fileNodeAllocator->WriteBlock (block);
        }

        void BTree::SetRoot (Node *node) {
            root = node;
            header.rootOffset = root->offset;
            Save ();
        }

        Serializer &operator << (
                Serializer &serializer,
                const BTree::Node::Entry &entry) {
            serializer << entry.key << entry.rightOffset;
            return serializer;
        }

        Serializer &operator >> (
                Serializer &serializer,
                BTree::Node::Entry &entry) {
            serializer >> entry.key >> entry.rightOffset;
            entry.rightNode = nullptr;
            return serializer;
        }

        Serializer &operator << (
                Serializer &serializer,
                const BTree::Header &header) {
            serializer << header.entriesPerNode << header.rootOffset;
            return serializer;
        }

        Serializer &operator >> (
                Serializer &serializer,
                BTree::Header &header) {
            serializer >> header.entriesPerNode >> header.rootOffset;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
