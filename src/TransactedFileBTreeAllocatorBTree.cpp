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
#include "thekogans/util/TransactedFileBTreeAllocatorBTree.h"

namespace thekogans {
    namespace util {

        inline bool operator == (
                const TransactedFileBTreeAllocatorBTree::Key &key1,
                const TransactedFileBTreeAllocatorBTree::Key &key2) {
            return key1.size == key2.size && key1.offset == key2.offset;
        }

        inline bool operator != (
                const TransactedFileBTreeAllocatorBTree::Key &key1,
                const TransactedFileBTreeAllocatorBTree::Key &key2) {
            return key1.size != key2.size || key1.offset != key2.offset;
        }

        inline bool operator < (
                const TransactedFileBTreeAllocatorBTree::Key &key1,
                const TransactedFileBTreeAllocatorBTree::Key &key2) {
            return key1.size < key2.size ||
                (key1.size == key2.size && key1.offset < key2.offset);
        }

        inline bool operator > (
                const TransactedFileBTreeAllocatorBTree::Key &key1,
                const TransactedFileBTreeAllocatorBTree::Key &key2) {
            return key1.size > key2.size ||
                (key1.size == key2.size && key1.offset > key2.offset);
        }

        inline Serializer &operator << (
               Serializer &serializer,
               const TransactedFileBTreeAllocatorBTree::Key &key) {
            serializer << key.size << key.offset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTreeAllocatorBTree::Key &key) {
            serializer >> key.size >> key.offset;
            return serializer;
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const TransactedFileBTreeAllocatorBTree::Node::Entry &entry) {
            serializer << entry.key << entry.rightOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTreeAllocatorBTree::Node::Entry &entry) {
            serializer >> entry.key >> entry.rightOffset;
            // Because of the way we allocate the BTree::Node the
            // Entry ctor is never called. In a way this extraction
            // operator is our ctor. Make sure all members are
            // properly initialized.
            entry.rightNode = nullptr;
            return serializer;
        }

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_OVERRIDE (TransactedFileBTreeAllocatorBTree::Node, 1, 0)
        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_STATIC_INIT_EX (TransactedFileBTreeAllocatorBTree::Node)

        TransactedFileBTreeAllocatorBTree::Node::Node (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset,
                TransactedFileBTreeAllocatorBTree *btree_) :
                TransactedFile::Object (file, offset),
                btree (btree_),
                entries ((Entry *)(this + 1)) {
            SecureZeroMemory (entries, btree->header.entriesPerNode * sizeof (Entry));
            Reload ();
        }

        TransactedFileBTreeAllocatorBTree::Node::~Node () {
            Reset ();
        }

        std::size_t TransactedFileBTreeAllocatorBTree::Node::FileSize (std::size_t entriesPerNode) {
                                            // key         rightOffset
            constexpr std::size_t ENTRY_SIZE = Key::SIZE + TransactedFile::AddressTypeSize;
                // magic       count       leftOffset                        entries
            return UI32_SIZE + UI32_SIZE + TransactedFile::AddressTypeSize + entriesPerNode * ENTRY_SIZE;
        }

        std::size_t TransactedFileBTreeAllocatorBTree::Node::Size (std::size_t entriesPerNode) {
            return sizeof (Node) + entriesPerNode * sizeof (Entry);
        }

        TransactedFileBTreeAllocatorBTree::Node *TransactedFileBTreeAllocatorBTree::Node::Alloc (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset,
                TransactedFileBTreeAllocatorBTree *btree) {
            Node *node = new (btree->nodeAllocator->Alloc ()) Node (file, offset, btree);
            node->AddRef ();
            return node;
        }

        TransactedFileBTreeAllocatorBTree::Node *TransactedFileBTreeAllocatorBTree::Node::GetChild (ui32 index) {
            if (index == 0) {
                if (leftNode == nullptr && leftOffset != 0) {
                    leftNode = Alloc (file, leftOffset, btree);
                }
                return leftNode;
            }
            else {
                --index;
                if (entries[index].rightNode == nullptr && entries[index].rightOffset != 0) {
                    entries[index].rightNode = Alloc (file, entries[index].rightOffset, btree);
                }
                return entries[index].rightNode;
            }
        }

        bool TransactedFileBTreeAllocatorBTree::Node::Find (
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

        bool TransactedFileBTreeAllocatorBTree::Node::Insert (Entry &entry) {
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

        bool TransactedFileBTreeAllocatorBTree::Node::Remove (const Key &key) {
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

        void TransactedFileBTreeAllocatorBTree::Node::RestoreBalance (ui32 index) {
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

        void TransactedFileBTreeAllocatorBTree::Node::RotateRight (
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

        void TransactedFileBTreeAllocatorBTree::Node::RotateLeft (
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

        void TransactedFileBTreeAllocatorBTree::Node::Merge (
            ui32 index,
            Node *left,
            Node *right) {
            assert (left->count + right->count < btree->header.entriesPerNode);
            entries[index].rightOffset = right->leftOffset;
            entries[index].rightNode = right->leftNode;
            left->Concatenate (entries[index]);
            left->Concatenate (right);
            RemoveEntry (index);
            Object::FreeObject (file, right->GetOffset ());
            right->Release ();
        }

        void TransactedFileBTreeAllocatorBTree::Node::Split (Node *node) {
            ui32 splitIndex = btree->header.entriesPerNode / 2;
            node->count = count - splitIndex;
            std::memcpy (node->entries, entries + splitIndex, node->count * sizeof (Entry));
            count = splitIndex;
            SetDirty ();
            node->SetDirty ();
        }

        void TransactedFileBTreeAllocatorBTree::Node::Concatenate (Node *node) {
            std::memcpy (entries + count, node->entries, node->count * sizeof (Entry));
            count += node->count;
            node->count = 0;
            SetDirty ();
            node->SetDirty ();
        }

        void TransactedFileBTreeAllocatorBTree::Node::InsertEntry (
            const Entry &entry,
            ui32 index) {
            std::memmove (entries + index + 1, entries + index, (count++ - index) * sizeof (Entry));
            entries[index] = entry;
            SetDirty ();
        }

        void TransactedFileBTreeAllocatorBTree::Node::RemoveEntry (ui32 index) {
            std::memcpy (entries + index, entries + index + 1, (--count - index) * sizeof (Entry));
            SetDirty ();
        }

        void TransactedFileBTreeAllocatorBTree::Node::Reset () {
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

        void TransactedFileBTreeAllocatorBTree::Node::Harakiri () {
            this->~Node ();
            btree->nodeAllocator->Free (this);
        }

        void TransactedFileBTreeAllocatorBTree::Node::Read (Serializer &serializer) {
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
                    "Corrupt TransactedFileBTreeAllocatorBTree::Node: @" THEKOGANS_UTIL_UI64_FORMAT,
                    GetOffset ());
            }
        }

        void TransactedFileBTreeAllocatorBTree::Node::Write (Serializer &serializer) {
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

        void TransactedFileBTreeAllocatorBTree::Node::FreeObject (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset) {
            if (file != nullptr && offset != 0) {
                TransactedFile::BlockRange range (file, offset);
                ui32 magic;
                range >> magic;
                if (magic == MAGIC) {
                    SerializableHeader header;
                    range >> header;
                    if (header.type == TYPE) {
                        ui32 magic;
                        range >> magic;
                        if (magic == MAGIC32) {
                            ui32 count;
                            range >> count;
                            if (count > 0) {
                                TransactedFile::AddressType leftOffset;
                                range >> leftOffset;
                                Node::FreeObject (file, leftOffset);
                                for (ui32 i = 0; i < count; ++i) {
                                    Entry entry;
                                    range >> entry;
                                    Node::FreeObject (file, entry.rightOffset);
                                }
                            }
                            Object::FreeObject (file, offset);
                        }
                        else {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "Corrupt BTree::Node @" THEKOGANS_UTIL_UI64_FORMAT,
                                offset);
                        }
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Header type: %s != TransactedFileBTreeAllocatorBTree::Node type: %s @" THEKOGANS_UTIL_UI64_FORMAT,
                            header.type.c_str (),
                            TYPE,
                            offset);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt TransactedFile::Object @" THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
            }
        }

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_OVERRIDE (TransactedFileBTreeAllocatorBTree, 1, 0)
        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_STATIC_INIT_EX (TransactedFileBTreeAllocatorBTree)

        TransactedFileBTreeAllocatorBTree::TransactedFileBTreeAllocatorBTree (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset,
                std::size_t entriesPerNode,
                std::size_t nodesPerPage) :
                TransactedFile::Object (file, offset),
                header ((ui32)entriesPerNode),
                nodeAllocator (
                    new SlabAllocatorImpl (
                        Node::Size (entriesPerNode),
                        nodesPerPage,
                        nodesPerPage / 4)),
                root (Node::Alloc (file, header.rootOffset, this)) {
            Reload ();
        }

        TransactedFileBTreeAllocatorBTree::~TransactedFileBTreeAllocatorBTree () {
            root->Release ();
        }

        TransactedFileBTreeAllocatorBTree::Key TransactedFileBTreeAllocatorBTree::Find (const Key &key) {
            Key result{UI64_MAX, 0};
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

        void TransactedFileBTreeAllocatorBTree::Insert (const Key &key) {
            Node::Entry entry (key);
            if (!root->Insert (entry)) {
                // The path to the leaf node is full.
                // Create a new root node and make the entry
                // its first.
                Node *node = Node::Alloc (file, 0, this);
                node->leftOffset = root->GetOffset ();
                node->leftNode = root;
                node->InsertEntry (entry, 0);
                root = node;
            }
            if (header.rootOffset == 0 || root->GetOffset () != header.rootOffset) {
                SetDirty ();
            }
        }

        bool TransactedFileBTreeAllocatorBTree::Remove (const Key &key) {
            bool removed = root->Remove (key);
            if (removed) {
                if (root->IsEmpty () && root->GetChild (0) != nullptr) {
                    Node *node = root;
                    root = root->GetChild (0);
                    Object::FreeObject (file, node->GetOffset ());
                    node->Release ();
                }
                if (header.rootOffset == 0 || root->GetOffset () != header.rootOffset) {
                    SetDirty ();
                }
            }
            return removed;
        }

        void TransactedFileBTreeAllocatorBTree::Read (Serializer &serializer) {
            ui32 magic;
            serializer >> magic;
            if (magic == MAGIC32) {
                serializer >> header;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFileBTreeAllocatorBTree: @" THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
            root->Release ();
            nodeAllocator.Reset (
                new SlabAllocatorImpl (
                    Node::Size (header.entriesPerNode),
                    nodeAllocator->GetSlotsPerPage (),
                    nodeAllocator->GetSlotsPerPage () / 4));
            root = Node::Alloc (file, header.rootOffset, this);
        }

        void TransactedFileBTreeAllocatorBTree::Write (Serializer &serializer) {
            header.rootOffset = root->GetOffset ();
            serializer << MAGIC32 << header;
        }

        void TransactedFileBTreeAllocatorBTree::FreeObject (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset) {
            if (file != nullptr && offset != 0) {
                TransactedFile::BlockRange range (file, offset);
                ui32 magic;
                range >> magic;
                if (magic == MAGIC) {
                    SerializableHeader header;
                    range >> header;
                    if (header.type == TYPE) {
                        ui32 magic;
                        range >> magic;
                        if (magic == MAGIC32) {
                            Header header;
                            range >> header;
                            Node::FreeObject (file, header.rootOffset);
                            Object::FreeObject (file, offset);
                        }
                        else {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "Corrupt BTree header @" THEKOGANS_UTIL_UI64_FORMAT,
                                offset);
                        }
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Invalid BTree header type: %s", header.type.c_str ());
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt TransactedFile::Object @" THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
            }
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const TransactedFileBTreeAllocatorBTree::Header &header) {
            serializer << header.entriesPerNode << header.rootOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTreeAllocatorBTree::Header &header) {
            serializer >> header.entriesPerNode >> header.rootOffset;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
