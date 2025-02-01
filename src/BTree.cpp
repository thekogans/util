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
#include "thekogans/util/Path.h"
#include "thekogans/util/File.h"
#include "thekogans/util/BlockAllocator.h"
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
                const GUID &id_) :
                btree (btree_),
                id (id_),
                path (MakePath (btree.path, id.ToString ())),
                count (0),
                leftId (GUID::Empty),
                leftNode (nullptr) {
            Path nodePath (path);
            if (nodePath.Exists ()) {
                ReadOnlyFile file (HostEndian, nodePath.path);
                ui32 magic;
                file >> magic;
                if (magic == MAGIC32) {
                    // File is host endian.
                }
                else if (ByteSwap<GuestEndian, HostEndian> (magic) == MAGIC32) {
                    // File is guest endian.
                    file.endianness = GuestEndian;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt btree node file (%s)",
                        path.c_str ());
                }
                file >> count;
                if (count > 0) {
                    file >> leftId;
                    for (ui32 i = 0; i < count; ++i) {
                        file >> entries[i];
                    }
                }
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

        std::size_t BTree::Node::Size (ui32 entriesPerNode) {
            return sizeof (Node) + (entriesPerNode - 1) * sizeof (Entry);
        }

        BTree::Node *BTree::Node::Alloc (
                BTree &btree,
                const GUID &id) {
            return new (
                btree.allocator->Alloc (
                    Size (btree.header.entriesPerNode))) Node (btree, id);
        }

        void BTree::Node::Free (Node *node) {
            if (node != nullptr) {
                node->~Node ();
                node->btree.allocator->Free (
                    node, Size (node->btree.header.entriesPerNode));
            }
        }

        void BTree::Node::Delete (Node *node) {
            if (node->count == 0) {
                Path nodePath (node->path);
                if (nodePath.Exists ()) {
                    nodePath.Delete ();
                }
                Free (node);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Logic error: trying to delete a node with count > 0 (%s)",
                    node->path.c_str ());
            }
        }

        void BTree::Node::Save () {
            SimpleFile file (
                HostEndian,
                path,
                SimpleFile::ReadWrite | SimpleFile::Create | SimpleFile::Truncate);
            file << MAGIC32 << count;
            if (count > 0) {
                file << leftId;
                for (ui32 i = 0; i < count; ++i) {
                    file << entries[i];
                }
            }
        }

        BTree::Node *BTree::Node::GetChild (ui32 index) {
            if (index == 0) {
                if (leftNode == nullptr && leftId != GUID::Empty) {
                    leftNode = Alloc (btree, leftId);
                }
                return leftNode;
            }
            else {
                --index;
                if (entries[index].rightNode == nullptr &&
                        entries[index].rightId != GUID::Empty) {
                    entries[index].rightNode = Alloc (
                        btree, entries[index].rightId);
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

        BTree::BTree (
                const std::string &path_,
                ui32 entriesPerNode,
                Allocator::SharedPtr allocator_) :
                path (path_),
                header (entriesPerNode > 0 ? entriesPerNode : DEFAULT_ENTRIES_PER_NODE),
                allocator (
                    BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                        Node::Size (header.entriesPerNode),
                        BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                        allocator_ != nullptr ? allocator_ : DefaultAllocator::Instance ())),
                root (nullptr) {
            Path btreePath (MakePath (path, "btree"));
            if (btreePath.Exists ()) {
                ReadOnlyFile file (HostEndian, btreePath.path);
                ui32 magic;
                file >> magic;
                if (magic == MAGIC32) {
                    // File is host endian.
                }
                else if (ByteSwap<GuestEndian, HostEndian> (magic) == MAGIC32) {
                    // File is guest endian.
                    file.endianness = GuestEndian;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt btree header file (%s)",
                        btreePath.path.c_str ());
                }
                file >> header;
            }
            else {
                Save ();
            }
            root = Node::Alloc (*this, header.rootId);
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
                node->leftId = root->id;
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
            root = Node::Alloc (*this, header.rootId);
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
                        right->leftId = entry.rightId;
                        right->leftNode = entry.rightNode;
                        right->Save ();
                        entry.rightId = right->id;
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
                    while (leaf->leftId != GUID::Empty) {
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
            node->entries[index].rightId = right->leftId;
            node->entries[index].rightNode = right->leftNode;
            right->InsertEntry (node->entries[index], 0);
            ui32 lastIndex = left->count - 1;
            right->leftId = left->entries[lastIndex].rightId;
            right->leftNode = left->entries[lastIndex].rightNode;
            left->entries[lastIndex].rightId = right->id;
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
            node->entries[index].rightId = right->leftId;
            node->entries[index].rightNode = right->leftNode;
            right->leftId = right->entries[0].rightId;
            right->leftNode = right->entries[0].rightNode;
            right->entries[0].rightId = right->id;
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
            node->entries[index].rightId = right->leftId;
            node->entries[index].rightNode = right->leftNode;
            left->Concatenate (node->entries[index]);
            left->Concatenate (right);
            node->RemoveEntry (index);
            node->Save ();
            left->Save ();
            Node::Delete (right);
        }

        void BTree::Save () {
            SimpleFile file (
                HostEndian,
                MakePath (path, "btree"),
                SimpleFile::ReadWrite | SimpleFile::Create | SimpleFile::Truncate);
            file << MAGIC32 << header;
        }

        void BTree::SetRoot (Node *node) {
            root = node;
            header.rootId = root->id;
            Save ();
        }

        Serializer &operator << (
                Serializer &serializer,
                const BTree::Node::Entry &entry) {
            serializer << entry.key << entry.rightId;
            return serializer;
        }

        Serializer &operator >> (
                Serializer &serializer,
                BTree::Node::Entry &entry) {
            serializer >> entry.key >> entry.rightId;
            entry.rightNode = nullptr;
            return serializer;
        }

        Serializer &operator << (
                Serializer &serializer,
                const BTree::Header &header) {
            serializer << header.entriesPerNode << header.rootId;
            return serializer;
        }

        Serializer &operator >> (
                Serializer &serializer,
                BTree::Header &header) {
            serializer >> header.entriesPerNode >> header.rootId;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
