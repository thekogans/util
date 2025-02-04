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
#include "thekogans/util/FileAllocator.h"

namespace thekogans {
    namespace util {

        FileAllocator::BTree::Node::Node (
                BTree &btree_,
                ui64 offset_) :
                btree (btree_),
                offset (offset_),
                count (0),
                leftOffset (0),
                leftNode (0) {
            if (offset != 0) {
                BlockBuffer::SharedPtr buffer =
                    btree.fileAllocator.CreateBlockBuffer (offset, 0, true);
                ui32 magic;
                *buffer >> magic;
                if (magic == MAGIC32) {
                    *buffer >> count;
                    if (count > 0) {
                        *buffer >> leftOffset;
                        for (ui32 i = 0; i < count; ++i) {
                            *buffer >> entries[i];
                        }
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt BTree::Node: " THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
            }
            else {
                offset = btree.fileAllocator.AllocFixedBlock ();
                Save ();
            }
        }

        FileAllocator::BTree::Node::~Node () {
            if (count > 0) {
                Free (leftNode);
                for (ui32 i = 0; i < count; ++i) {
                    Free (entries[i].rightNode);
                }
            }
        }

        std::size_t FileAllocator::BTree::Node::FileSize (std::size_t entriesPerNode) {
            const std::size_t ENTRY_SIZE = KEY_SIZE + UI64_SIZE;
            return UI32_SIZE + UI32_SIZE + UI64_SIZE + entriesPerNode * ENTRY_SIZE;
        }

        std::size_t FileAllocator::BTree::Node::Size (std::size_t entriesPerNode) {
            return sizeof (Node) + (entriesPerNode - 1) * sizeof (Entry);
        }

        FileAllocator::BTree::Node *FileAllocator::BTree::Node::Alloc (
                BTree &btree,
                ui64 offset) {
            return new (
                btree.nodeAllocator->Alloc (
                    Size (btree.header.entriesPerNode))) Node (btree, offset);
        }

        void FileAllocator::BTree::Node::Free (Node *node) {
            if (node != nullptr) {
                node->~Node ();
                node->btree.nodeAllocator->Free (
                    node, Size (node->btree.header.entriesPerNode));
            }
        }

        void FileAllocator::BTree::Node::Delete (Node *node) {
            if (node->count == 0) {
                node->btree.fileAllocator.FreeFixedBlock (node->offset);
                Free (node);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Logic error: trying to delete a node with count > 0 "
                    THEKOGANS_UTIL_UI64_FORMAT,
                    node->offset);
            }
        }

        void FileAllocator::BTree::Node::Save () {
            BlockBuffer::SharedPtr buffer =
                btree.fileAllocator.CreateBlockBuffer (offset);
            *buffer << MAGIC32 << count;
            if (count > 0) {
                *buffer << leftOffset;
                for (ui32 i = 0; i < count; ++i) {
                    *buffer << entries[i];
                }
            }
            buffer->Write ();
        }

        FileAllocator::BTree::Node *FileAllocator::BTree::Node::GetChild (ui32 index) {
            if (index == 0) {
                if (leftNode == 0 && leftOffset != 0) {
                    leftNode = Alloc (btree, leftOffset);
                }
                return leftNode;
            }
            else {
                --index;
                if (entries[index].rightNode == 0 &&
                        entries[index].rightOffset != 0) {
                    entries[index].rightNode = Alloc (
                        btree, entries[index].rightOffset);
                }
                return entries[index].rightNode;
            }
        }

        bool FileAllocator::BTree::Node::Search (
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

        void FileAllocator::BTree::Node::Split (
                Node *node,
                ui32 index) {
            node->count = count - index;
            std::memcpy (
                node->entries,
                entries + index,
                node->count * sizeof (Entry));
            count = index;
        }

        void FileAllocator::BTree::Node::Concatenate (Node *node) {
            std::memcpy (
                entries + count,
                node->entries,
                node->count * sizeof (Entry));
            count += node->count;
            node->count = 0;
        }

        void FileAllocator::BTree::Node::InsertEntry (
                const Entry &entry,
                ui32 index) {
            std::memmove (
                entries + index + 1,
                entries + index,
                (count++ - index) * sizeof (Entry));
            entries[index] = entry;
        }

        void FileAllocator::BTree::Node::RemoveEntry (ui32 index) {
            std::memcpy (
                entries + index,
                entries + index + 1,
                (--count - index) * sizeof (Entry));
        }

        FileAllocator::BTree::BTree (
                FileAllocator &fileAllocator_,
                ui64 offset_,
                std::size_t entriesPerNode,
                std::size_t nodesPerPage,
                Allocator::SharedPtr allocator) :
                fileAllocator (fileAllocator_),
                offset (offset_),
                header ((ui32)entriesPerNode),
                nodeAllocator (
                    BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                        Node::Size (entriesPerNode),
                        nodesPerPage,
                        allocator)),
                root (nullptr) {
            if (offset != 0) {
                BlockBuffer::SharedPtr buffer =
                    fileAllocator.CreateBlockBuffer (offset, Header::SIZE, true);
                ui32 magic;
                *buffer >> magic;
                if (magic == MAGIC32) {
                    *buffer >> header;
                    if (header.entriesPerNode != entriesPerNode) {
                        nodeAllocator =
                            BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                                Node::Size (header.entriesPerNode),
                                nodesPerPage,
                                allocator);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt BTree: " THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
            }
            else {
                offset = fileAllocator.AllocFixedBlock ();
                Save ();
            }
            root = Node::Alloc (*this, header.rootOffset);
        }

        FileAllocator::BTree::~BTree () {
            Node::Free (root);
        }

        FileAllocator::BTree::Key FileAllocator::BTree::Search (const Key &key) {
            Key result (UI64_MAX, 0);
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

        void FileAllocator::BTree::Add (const Key &key) {
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

        bool FileAllocator::BTree::Delete (const Key &key) {
            bool removed = Remove (key, root);
            if (removed && root->IsEmpty () && root->GetChild (0) != nullptr) {
                Node *node = root;
                SetRoot (root->GetChild (0));
                Node::Delete (node);
            }
            return removed;
        }

        void FileAllocator::BTree::Flush () {
            Node::Free (root);
            root = Node::Alloc (*this, header.rootOffset);
        }

        bool FileAllocator::BTree::Insert (
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

        bool FileAllocator::BTree::Remove (
                const Key &key,
                Node *node) {
            ui32 index;
            bool found = node->Search (key, index);
            Node *child = node->GetChild (found ? index + 1 : index);
            if (found) {
                if (child != nullptr) {
                    Node *leaf = child;
                    while (leaf->leftOffset != 0) {
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

        void FileAllocator::BTree::RestoreBalance (
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

        void FileAllocator::BTree::RotateRight (
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

        void FileAllocator::BTree::RotateLeft (
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

        void FileAllocator::BTree::Merge (
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

        void FileAllocator::BTree::Save () {
            BlockBuffer::SharedPtr buffer =
                fileAllocator.CreateBlockBuffer (offset, Header::SIZE);
            *buffer << MAGIC32 << header;
            buffer->Write ();
        }

        void FileAllocator::BTree::SetRoot (Node *node) {
            root = node;
            header.rootOffset = root->offset;
            Save ();
        }

        inline bool operator == (
                const FileAllocator::BTree::Key &key1,
                const FileAllocator::BTree::Key &key2) {
            return key1.first == key2.first && key1.second == key2.second;
        }
        inline bool operator != (
                const FileAllocator::BTree::Key &key1,
                const FileAllocator::BTree::Key &key2) {
            return key1.first != key2.first || key1.second != key2.second;
        }

        inline bool operator < (
                const FileAllocator::BTree::Key &key1,
                const FileAllocator::BTree::Key &key2) {
            return key1.first < key2.first ||
                (key1.first == key2.first && key1.second < key2.second);
        }

        inline Serializer &operator << (
               Serializer &serializer,
               const FileAllocator::BTree::Key &key) {
            serializer << key.first << key.second;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                FileAllocator::BTree::Key &key) {
            serializer >> key.first >> key.second;
            return serializer;
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const FileAllocator::BTree::Node::Entry &entry) {
            serializer << entry.key << entry.rightOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                FileAllocator::BTree::Node::Entry &entry) {
            serializer >> entry.key >> entry.rightOffset;
            entry.rightNode = nullptr;
            return serializer;
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const FileAllocator::BTree::Header &header) {
            serializer << header.entriesPerNode << header.rootOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                FileAllocator::BTree::Header &header) {
            serializer >> header.entriesPerNode >> header.rootOffset;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
