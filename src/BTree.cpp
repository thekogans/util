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
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/BTree.h"

namespace thekogans {
    namespace util {

        BTree::Node::Node (
                BTree &btree_,
                ui64 offset_) :
                btree (btree_),
                offset (offset_),
                count (0),
                leftOffset (0),
                leftNode (0),
                dirty (false) {
            if (offset != 0) {
                FileAllocator::LockedFilePtr file (*btree.fileAllocator);
                FileAllocator::BlockBuffer buffer (*btree.fileAllocator, offset);
                buffer.Read ();
                ui32 magic;
                buffer >> magic;
                if (magic == MAGIC32) {
                    buffer >> count;
                    if (count > 0) {
                        buffer >> leftOffset;
                        for (ui32 i = 0; i < count; ++i) {
                            buffer >> entries[i];
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
                offset = btree.fileAllocator->Alloc (
                    FileSize (btree.header.entriesPerNode));
                Save ();
            }
        }

        BTree::Node::~Node () {
            if (dirty) {
                FileAllocator::LockedFilePtr file (*btree.fileAllocator);
                FileAllocator::BlockBuffer buffer (*btree.fileAllocator, offset);
                buffer << MAGIC32 << count;
                if (count > 0) {
                    buffer << leftOffset;
                    Free (leftNode);
                    for (ui32 i = 0; i < count; ++i) {
                        buffer << entries[i];
                        Free (entries[i].rightNode);
                    }
                }
                buffer.Write ();
            }
            else if (count > 0) {
                Free (leftNode);
                for (ui32 i = 0; i < count; ++i) {
                    Free (entries[i].rightNode);
                }
            }
        }

        std::size_t BTree::Node::FileSize (std::size_t entriesPerNode) {
            const std::size_t ENTRY_SIZE = KEY_SIZE + UI64_SIZE + UI64_SIZE;
            return UI32_SIZE + UI32_SIZE + UI64_SIZE + entriesPerNode * ENTRY_SIZE;
        }

        std::size_t BTree::Node::Size (std::size_t entriesPerNode) {
            return sizeof (Node) + (entriesPerNode - 1) * sizeof (Entry);
        }

        BTree::Node *BTree::Node::Alloc (
                BTree &btree,
                ui64 offset) {
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
                node->btree.fileAllocator->Free (node->offset);
                // We've just deleted it's block, writting to it now would be a bad idea.
                node->dirty = false;
                Free (node);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Logic error: trying to delete a node with count > 0 "
                    THEKOGANS_UTIL_UI64_FORMAT,
                    node->offset);
            }
        }

        BTree::Node *BTree::Node::GetChild (ui32 index) {
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

        BTree::Node::InsertResult BTree::Node::Insert (Entry &entry) {
            ui32 index;
            if (Search (entry.key, index)) {
                return Duplicate;
            }
            Node *child = GetChild (index);
            if (child != nullptr) {
                InsertResult result = child->Insert (entry);
                if (result == Inserted || result == Duplicate) {
                    return result;
                }
            }
            if (!IsFull ()) {
                InsertEntry (entry, index);
                Save ();
                return Inserted;
            }
            else {
                // Node is full. Split it and insert the entry
                // in to the proper daughter node. Return the
                // entry at the split point to be added up the
                // parent chain.
                Node *right = Alloc (btree);
                Split (right);
                ui32 splitIndex = btree.header.entriesPerNode / 2;
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
                Save ();
                right->leftOffset = entry.rightOffset;
                right->leftNode = entry.rightNode;
                right->Save ();
                entry.rightOffset = right->offset;
                entry.rightNode = right;
                return Overflow;
            }
        }

        bool BTree::Node::Remove (const Key &key) {
            ui32 index;
            bool found = Search (key, index);
            Node *child = GetChild (found ? index + 1 : index);
            if (found) {
                if (child != nullptr) {
                    Node *leaf = child;
                    while (leaf->leftOffset != 0) {
                        leaf = leaf->GetChild (0);
                    }
                    entries[index].key = leaf->entries[0].key;
                    entries[index].value = leaf->entries[0].value;
                    child->Remove (leaf->entries[0].key);
                    if (child->IsPoor ()) {
                        RestoreBalance (index);
                    }
                }
                else {
                    RemoveEntry (index);
                }
                Save ();
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

        void BTree::Node::RestoreBalance (ui32 index) {
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

        void BTree::Node::RotateRight (
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
            Save ();
            left->Save ();
            right->Save ();
        }

        void BTree::Node::RotateLeft (
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
            Save ();
            left->Save ();
            right->Save ();
        }

        void BTree::Node::Merge (
                ui32 index,
                Node *left,
                Node *right) {
            assert (left->count + right->count < btree.header.entriesPerNode);
            entries[index].rightOffset = right->leftOffset;
            entries[index].rightNode = right->leftNode;
            left->Concatenate (entries[index]);
            left->Concatenate (right);
            RemoveEntry (index);
            Save ();
            left->Save ();
            Delete (right);
        }

        void BTree::Node::Split (Node *node) {
            ui32 splitIndex = btree.header.entriesPerNode / 2;
            node->count = count - splitIndex;
            std::memcpy (
                node->entries,
                entries + splitIndex,
                node->count * sizeof (Entry));
            count = splitIndex;
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

        void BTree::Node::Dump () {
            if (count > 0) {
                std::cout << offset << ": " << leftOffset;
                for (ui32 i = 0; i < count; ++i) {
                    std::cout << " ; [" << entries[i].key.ToString () << ", " <<
                        entries[i].value << "] ; " << entries[i].rightOffset;
                }
                std::cout << "\n";
                for (ui32 i = 0; i < count; ++i) {
                    Node *child = GetChild (i);
                    if (child != nullptr) {
                        child->Dump ();
                    }
                }
            }
        }

        BTree::BTree (
                FileAllocator::SharedPtr fileAllocator_,
                ui64 offset_,
                std::size_t entriesPerNode,
                std::size_t nodesPerPage,
                Allocator::SharedPtr allocator) :
                fileAllocator (fileAllocator_),
                offset (offset_),
                header ((ui32)entriesPerNode),
                root (nullptr) {
            if (offset != 0) {
                FileAllocator::LockedFilePtr file (*fileAllocator);
                FileAllocator::BlockBuffer buffer (*fileAllocator, offset, Header::SIZE);
                buffer.Read ();
                ui32 magic;
                buffer >> magic;
                if (magic == MAGIC32) {
                    buffer >> header;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt BTree: " THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
            }
            else {
                offset = fileAllocator->Alloc (Header::SIZE);
                Save ();
            }
            nodeAllocator =
                BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                    Node::Size (header.entriesPerNode),
                    nodesPerPage,
                    allocator);
            root = Node::Alloc (*this, header.rootOffset);
            if (header.rootOffset != root->offset) {
                header.rootOffset = root->offset;
                Save ();
            }
        }

        BTree::~BTree () {
            Node::Free (root);
        }

        bool BTree::Search (
                const Key &key,
                ui64 &value) {
            LockGuard<SpinLock> guard (spinLock);
            ui32 index;
            for (Node *node = root; node != nullptr; node = node->GetChild (index)) {
                if (node->Search (key, index)) {
                    value = node->entries[index].value;
                    return true;
                }
            }
            return false;
        }

        bool BTree::Add (
                const Key &key,
                ui64 value) {
            LockGuard<SpinLock> guard (spinLock);
            Node::Entry entry (key, value);
            Node::InsertResult result = root->Insert (entry);
            if (result == Node::Overflow) {
                // The path to the leaf node is full.
                // Create a new root node and make the entry
                // its first.
                Node *node = Node::Alloc (*this);
                node->leftOffset = root->offset;
                node->leftNode = root;
                node->InsertEntry (entry, 0);
                node->Save ();
                SetRoot (node);
                result = Node::Inserted;
            }
            return result == Node::Inserted;
        }

        bool BTree::Delete (const Key &key) {
            LockGuard<SpinLock> guard (spinLock);
            bool removed = root->Remove (key);
            if (removed && root->IsEmpty () && root->GetChild (0) != nullptr) {
                Node *node = root;
                SetRoot (root->GetChild (0));
                Node::Delete (node);
            }
            return removed;
        }

        void BTree::Flush () {
            LockGuard<SpinLock> guard (spinLock);
            Node::Free (root);
            root = Node::Alloc (*this, header.rootOffset);
        }

        void BTree::Dump () {
            LockGuard<SpinLock> guard (spinLock);
            if (root != nullptr) {
                root->Dump ();
            }
        }

        void BTree::Save () {
            FileAllocator::LockedFilePtr file (*fileAllocator);
            FileAllocator::BlockBuffer buffer (*fileAllocator, offset, Header::SIZE);
            buffer << MAGIC32 << header;
            buffer.Write ();
        }

        void BTree::SetRoot (Node *node) {
            root = node;
            header.rootOffset = root->offset;
            Save ();
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const BTree::Node::Entry &entry) {
            serializer << entry.key << entry.value << entry.rightOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                BTree::Node::Entry &entry) {
            serializer >> entry.key >> entry.value >> entry.rightOffset;
            // Because of the way we allocate the BTree::Node the
            // Entry cror is never called. In a way this extraction
            // operator is our ctor. Make sure all members are
            // properly initialized.
            entry.rightNode = nullptr;
            return serializer;
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const BTree::Header &header) {
            serializer << header.entriesPerNode << header.rootOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                BTree::Header &header) {
            serializer >> header.entriesPerNode >> header.rootOffset;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
