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
                PtrType offset_) :
                BufferedFile::TransactionParticipant (btree_.fileAllocator.GetFile ()),
                btree (btree_),
                offset (offset_),
                count (0),
                leftOffset (0),
                leftNode (nullptr),
                entries ((Entry *)(this + 1)) {
            if (offset != 0) {
                Load ();
            }
        }

        FileAllocator::BTree::Node::~Node () {
            Reset ();
        }

        std::size_t FileAllocator::BTree::Node::FileSize (std::size_t entriesPerNode) {
            const std::size_t ENTRY_SIZE = KEY_TYPE_SIZE + PTR_TYPE_SIZE;
            return UI32_SIZE + UI32_SIZE + PTR_TYPE_SIZE + entriesPerNode * ENTRY_SIZE;
        }

        std::size_t FileAllocator::BTree::Node::Size (std::size_t entriesPerNode) {
            return sizeof (Node) + entriesPerNode * sizeof (Entry);
        }

        FileAllocator::BTree::Node *FileAllocator::BTree::Node::Alloc (
                BTree &btree,
                PtrType offset) {
            Node *node = new (
                btree.nodeAllocator->Alloc (
                    Size (btree.header.entriesPerNode))) Node (btree, offset);
            node->AddRef ();
            return node;
        }

        FileAllocator::BTree::Node *FileAllocator::BTree::Node::GetChild (ui32 index) {
            if (index == 0) {
                if (leftNode == nullptr && leftOffset != 0) {
                    leftNode = Alloc (btree, leftOffset);
                }
                return leftNode;
            }
            else {
                --index;
                if (entries[index].rightNode == nullptr &&
                        entries[index].rightOffset != 0) {
                    entries[index].rightNode = Alloc (
                        btree, entries[index].rightOffset);
                }
                return entries[index].rightNode;
            }
        }

        bool FileAllocator::BTree::Node::Find (
                const KeyType &key,
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

        bool FileAllocator::BTree::Node::Insert (Entry &entry) {
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

        bool FileAllocator::BTree::Node::Remove (const KeyType &key) {
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
                    SetDirty (true);
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

        void FileAllocator::BTree::Node::RestoreBalance (ui32 index) {
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

        void FileAllocator::BTree::Node::RotateRight (
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
            SetDirty (true);
        }

        void FileAllocator::BTree::Node::RotateLeft (
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
            SetDirty (true);
        }

        void FileAllocator::BTree::Node::Merge (
                ui32 index,
                Node *left,
                Node *right) {
            assert (left->count + right->count < btree.header.entriesPerNode);
            entries[index].rightOffset = right->leftOffset;
            entries[index].rightNode = right->leftNode;
            left->Concatenate (entries[index]);
            left->Concatenate (right);
            RemoveEntry (index);
            right->Delete ();
        }

        void FileAllocator::BTree::Node::Split (Node *node) {
            ui32 splitIndex = btree.header.entriesPerNode / 2;
            node->count = count - splitIndex;
            std::memcpy (
                node->entries,
                entries + splitIndex,
                node->count * sizeof (Entry));
            count = splitIndex;
            SetDirty (true);
            node->SetDirty (true);
        }

        void FileAllocator::BTree::Node::Concatenate (Node *node) {
            std::memcpy (
                entries + count,
                node->entries,
                node->count * sizeof (Entry));
            count += node->count;
            node->count = 0;
            SetDirty (true);
            node->SetDirty (true);
        }

        void FileAllocator::BTree::Node::InsertEntry (
                const Entry &entry,
                ui32 index) {
            std::memmove (
                entries + index + 1,
                entries + index,
                (count++ - index) * sizeof (Entry));
            entries[index] = entry;
            SetDirty (true);
        }

        void FileAllocator::BTree::Node::RemoveEntry (ui32 index) {
            std::memcpy (
                entries + index,
                entries + index + 1,
                (--count - index) * sizeof (Entry));
            SetDirty (true);
        }

        void FileAllocator::BTree::Node::Dump () {
            if (count > 0) {
                std::cout << offset << ": " << leftOffset;
                for (ui32 i = 0; i < count; ++i) {
                    std::cout << " ; [" << entries[i].key.first << ", " <<
                        entries[i].key.second << "] ; " << entries[i].rightOffset;
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

        void FileAllocator::BTree::Node::Alloc () {
            if (offset == 0) {
                offset = btree.fileAllocator.AllocBTreeNode (
                    FileSize (btree.header.entriesPerNode));
            }
        }

        void FileAllocator::BTree::Node::Free () {
            if (IsEmpty ()) {
                btree.fileAllocator.FreeBTreeNode (offset);
                Release ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Logic error: trying to delete a node with count > 0 @"
                    THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        }

        void FileAllocator::BTree::Node::Flush () {
            BlockBuffer buffer (btree.fileAllocator, offset);
            buffer << MAGIC32 << count;
            if (count > 0) {
                if (leftNode != nullptr) {
                    leftOffset = leftNode->offset;
                }
                buffer << leftOffset;
                for (ui32 i = 0; i < count; ++i) {
                    if (entries[i].rightNode != nullptr) {
                        entries[i].rightOffset = entries[i].rightNode->offset;
                    }
                    buffer << entries[i];
                }
            }
            if (btree.fileAllocator.IsSecure ()) {
                buffer.AdvanceWriteOffset (
                    SecureZeroMemory (
                        buffer.GetWritePtr (),
                        buffer.GetDataAvailableForWriting ()));
            }
            buffer.BlockWrite ();
        }

        void FileAllocator::BTree::Node::Reload () {
            Reset ();
            if (offset != 0) {
                Load ();
            }
        }

        void FileAllocator::BTree::Node::Reset () {
            if (count > 0) {
                if (leftNode != nullptr) {
                    leftNode->Release ();
                    leftNode = nullptr;
                }
                for (ui32 i = 0; i < count; ++i) {
                    if (entries[i].rightNode != nullptr) {
                        entries[i].rightNode->Release ();
                        entries[i].rightNode = nullptr;
                    }
                }
                count = 0;
            }
        }

        void FileAllocator::BTree::Node::Load () {
            BlockBuffer buffer (btree.fileAllocator, offset);
            buffer.BlockRead ();
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
                    "Corrupt FileAllocator::BTree::Node: @" THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        }

        FileAllocator::BTree::BTree (
                FileAllocator &fileAllocator_,
                std::size_t entriesPerNode,
                std::size_t nodesPerPage,
                Allocator::SharedPtr allocator) :
                BufferedFile::TransactionParticipant (fileAllocator_.GetFile ()),
                fileAllocator (fileAllocator_),
                header ((ui32)entriesPerNode),
                nodeAllocator (
                    new BlockAllocator (
                        Node::Size (entriesPerNode),
                        nodesPerPage,
                        allocator)),
                rootNode (Node::Alloc (*this, header.rootOffset)) {
            if (fileAllocator.header.btreeOffset != 0) {
                Load ();
            }
        }

        FileAllocator::BTree::~BTree () {
            rootNode->Release ();
        }

        FileAllocator::BTree::KeyType FileAllocator::BTree::Find (const KeyType &key) {
            KeyType result (UI64_MAX, 0);
            Node *node = rootNode;
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

        void FileAllocator::BTree::Insert (const KeyType &key) {
            Node::Entry entry (key);
            if (!rootNode->Insert (entry)) {
                // The path to the leaf node is full.
                // Create a new root node and make the entry
                // its first.
                Node *node = Node::Alloc (*this);
                node->leftOffset = rootNode->offset;
                node->leftNode = rootNode;
                node->InsertEntry (entry, 0);
                rootNode = node;
            }
            if (!IsDirty () &&
                    (header.rootOffset == 0 ||
                        rootNode->offset != header.rootOffset)) {
                SetDirty (true);
            }
        }

        bool FileAllocator::BTree::Remove (const KeyType &key) {
            bool removed = rootNode->Remove (key);
            if (removed) {
                if (rootNode->IsEmpty () && rootNode->GetChild (0) != nullptr) {
                    Node *node = rootNode;
                    rootNode = rootNode->GetChild (0);
                    node->Delete ();
                }
                if (!IsDirty () &&
                        (header.rootOffset == 0 ||
                            rootNode->offset != header.rootOffset)) {
                    SetDirty (true);
                }
            }
            return removed;
        }

        void FileAllocator::BTree::Alloc () {
            if (fileAllocator.header.btreeOffset == 0) {
                fileAllocator.header.btreeOffset =
                    fileAllocator.AllocBTreeNode (Header::SIZE);
                fileAllocator.SetDirty (true);
            }
        }

        void FileAllocator::BTree::Free () {
        }

        void FileAllocator::BTree::Flush () {
            header.rootOffset = rootNode->offset;
            BlockBuffer buffer (fileAllocator, fileAllocator.header.btreeOffset);
            buffer << MAGIC32 << header;
            buffer.BlockWrite ();
        }

        void FileAllocator::BTree::Reload () {
            if (fileAllocator.header.btreeOffset != 0) {
                Load ();
            }
            else {
                Reset ();
            }
        }

        void FileAllocator::BTree::Reset () {
            header.rootOffset = 0;
            rootNode->Release ();
            rootNode = Node::Alloc (*this, header.rootOffset);
        }

        void FileAllocator::BTree::Load () {
            BlockBuffer buffer (fileAllocator, fileAllocator.header.btreeOffset);
            buffer.BlockRead ();
            ui32 magic;
            buffer >> magic;
            if (magic == MAGIC32) {
                buffer >> header;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt FileAllocator::BTree: @" THEKOGANS_UTIL_UI64_FORMAT,
                    fileAllocator.header.btreeOffset);
            }
            rootNode->Release ();
            nodeAllocator.Reset (
                new BlockAllocator (
                    Node::Size (header.entriesPerNode),
                    nodeAllocator->GetBlocksPerPage (),
                    nodeAllocator->GetAllocator ()));
            rootNode = Node::Alloc (*this, header.rootOffset);
        }

        inline bool operator == (
                const FileAllocator::BTree::KeyType &key1,
                const FileAllocator::BTree::KeyType &key2) {
            return key1.first == key2.first && key1.second == key2.second;
        }

        inline bool operator != (
                const FileAllocator::BTree::KeyType &key1,
                const FileAllocator::BTree::KeyType &key2) {
            return key1.first != key2.first || key1.second != key2.second;
        }

        inline bool operator < (
                const FileAllocator::BTree::KeyType &key1,
                const FileAllocator::BTree::KeyType &key2) {
            return key1.first < key2.first ||
                (key1.first == key2.first && key1.second < key2.second);
        }

        inline Serializer &operator << (
               Serializer &serializer,
               const FileAllocator::BTree::KeyType &key) {
            serializer << key.first << key.second;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                FileAllocator::BTree::KeyType &key) {
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
            // Because of the way we allocate the BTree::Node the
            // Entry cror is never called. In a way this extraction
            // operator is our ctor. Make sure all members are
            // properly initialized.
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
