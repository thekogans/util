#include <cstring>
#include "thekogans/util/Exception.h"
#include "thekogans/util/BTree.h"

namespace thekogans {
    namespace util {

        namespace {
            inline bool operator == (
                    const BTree::Key &key1,
                    const BTree::Key &key2) {
                return key1.first == key2.first && key1.second == key2.second;
            }

            inline bool operator < (
                    const BTree::Key &key1,
                    const BTree::Key &key2) {
                return key1.first < key2.first ||
                    (key1.first == key2.first && key1.second < key2.second);
            }

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

            const std::size_t BTREE_SIZE = UI32_SIZE + UI32_SIZE + UI64_SIZE;

            inline std::size_t BTREE_NODE_SIZE (ui32 entriesPerNode) {
                // key + rightOffset
                const ui32 ENTRY_SIZE = UI64_SIZE + UI64_SIZE + UI64_SIZE;
                // count + leftOffset + entries
                return UI32_SIZE + UI64_SIZE + entriesPerNode * ENTRY_SIZE;
            }
        }

        BTree::Node::Node (
                BTree &btree_,
                ui64 offset_) :
                btree (btree_),
                offset (offset_),
                count (0),
                leftOffset (NIDX64),
                leftNode (nullptr),
                dirty (false) {
            if (offset != NIDX64) {
                btree.file.Seek (offset, SEEK_SET);
                btree.file >> count >> leftOffset;
                for (ui32 i = 0; i < count; ++i) {
                    btree.file >> entries[i];
                }
            }
            else {
                offset = btree.file.Alloc (
                    BTREE_NODE_SIZE (btree.header.entriesPerNode));
                btree.file.Seek (offset, SEEK_SET);
                btree.file << count << leftOffset;
            }
        }

        BTree::Node::~Node () {
            if (dirty) {
                btree.file.Seek (offset, SEEK_SET);
                btree.file << count << leftOffset;
                for (ui32 i = 0; i < count; ++i) {
                    btree.file << entries[i];
                }
            }
            if (count > 0) {
                Free (btree, leftNode);
                for (ui32 i = 0; i < count; ++i) {
                    Free (btree, entries[i].rightNode);
                }
            }
        }

        std::size_t BTree::Node::Size (ui32 entriesPerNode) {
            return sizeof (Node) + (entriesPerNode - 1) * sizeof (Entry);
        }

        BTree::Node *BTree::Node::Alloc (
                BTree &btree,
                ui64 offset) {
            return new (
                DefaultAllocator::Instance ()->Alloc (
                    Size (btree.header.entriesPerNode))) Node (btree, offset);
        }

        void BTree::Node::Free (
                BTree &btree,
                Node *node) {
            if (node != nullptr) {
                node->~Node ();
                DefaultAllocator::Instance ()->Free (
                    node, Size (btree.header.entriesPerNode));
            }
        }

        BTree::Node *BTree::Node::GetChild (ui32 index) {
            if (index == 0) {
                if (leftNode == nullptr && leftOffset != NIDX64) {
                    leftNode = Alloc (btree, leftOffset);
                }
                return leftNode;
            }
            if (entries[index - 1].rightNode == nullptr &&
                    entries[index - 1].rightOffset != NIDX64) {
                entries[index - 1].rightNode = Alloc (
                    btree, entries[index - 1].rightOffset);
            }
            return entries[index - 1].rightNode;
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

#if 0
        1, 2, 4, 6, 8, 10, 12
        0, 0, 1, 1, 2, 2, 3

        +---+ --+----+
        | 2 | 6 | 10 |
        +---+---+----+
#endif

        void BTree::Node::Split (
                Node *node,
                ui32 index) {
            node->count = count - index;
            std::memcpy (
                node->entries,
                entries + index,
                node->count * sizeof (Entry));
            node->dirty = true;
            count = index;
            dirty = true;
        }

        void BTree::Node::Concatenate (Node *node) {
            std::memcpy (
                entries + count,
                node->entries,
                node->count * sizeof (Entry));
            count += node->count;
            dirty = true;
            btree.DeleteNode (node);
        }

        void BTree::Node::InsertEntry (
                const Entry &entry,
                ui32 index) {
            std::memmove (
                entries + index + 1,
                entries + index,
                (count++ - index) * sizeof (Entry));
            entries[index] = entry;
            dirty = true;
        }

        void BTree::Node::RemoveEntry (ui32 index) {
            std::memcpy (
                entries + index,
                entries + index + 1,
                (--count - index) * sizeof (Entry));
            dirty = true;
        }

        BTree::BTree (
                File &file_,
                ui64 offset_,
                ui32 entriesPerNode) :
                file (file_),
                offset (offset_),
                header (entriesPerNode),
                root (nullptr),
                dirty (false) {
            if (offset != NIDX64) {
                file.Seek (offset, SEEK_SET);
                file >> header;
            }
            else {
                offset = file.Alloc (BTREE_SIZE);
                file.Seek (offset, SEEK_SET);
                file << header;
            }
            SetRoot (Node::Alloc (*this, header.rootOffset));
        }

        BTree::~BTree () {
            if (dirty) {
                file.Seek (offset, SEEK_SET);
                file << header;
            }
            Node::Free (*this, root);
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
                // The path to the leaf node is all full.
                // Create a new root node and make the entry
                // its first.
                Node *node = Node::Alloc (*this);
                node->leftOffset = root->offset;
                node->leftNode = root;
                node->InsertEntry (entry, 0);
                SetRoot (node);
            }
        }

        void BTree::Delete (const Key &key) {
            if (Remove (key, root) && root->IsEmpty () && root->GetChild (0) != nullptr) {
				Node *node = root;
                SetRoot (root->GetChild (0));
                DeleteNode (node);
            }
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
                        right->leftOffset = entry.rightOffset;
                        right->leftNode = entry.rightNode;
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
                    while (leaf->leftOffset != NIDX64) {
                        leaf = leaf->GetChild (0);
                    }
                    node->entries[index].key = leaf->entries[0].key;
                    node->dirty = true;
                    Remove (leaf->entries[0].key, child);
                    if (child->IsPoor ()) {
                        RestoreBalance (node, index);
                    }
                }
                else {
                    node->RemoveEntry (index);
                }
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
            node->dirty = true;
            right->InsertEntry (node->entries[index], 0);
            ui32 lastIndex = left->count - 1;
            right->leftOffset = left->entries[lastIndex].rightOffset;
            right->leftNode = left->entries[lastIndex].rightNode;
            left->entries[lastIndex].rightOffset = right->offset;
            left->entries[lastIndex].rightNode = right;
            node->entries[index] = left->entries[lastIndex];
            left->RemoveEntry (lastIndex);
        }

        void BTree::RotateLeft (
                Node *node,
                ui32 index,
                Node *left,
                Node *right) {
            node->entries[index].rightOffset = right->leftOffset;
            node->entries[index].rightNode = right->leftNode;
            node->dirty = true;
            right->leftOffset = right->entries[0].rightOffset;
            right->leftNode = right->entries[0].rightNode;
            right->entries[0].rightOffset = right->offset;
            right->entries[0].rightNode = right;
            left->Concatenate (node->entries[index]);
            node->entries[index] = right->entries[0];
            right->RemoveEntry (0);
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
        }

        void BTree::SetRoot (Node *node) {
            root = node;
            if (header.rootOffset != root->offset) {
                header.rootOffset = root->offset;
                dirty = true;
            }
        }

        void BTree::DeleteNode (Node *node) {
            file.Free (node->offset);
            node->count = 0;
            node->dirty = false;
            Node::Free (*this, node);
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
            return serializer;
        }

        Serializer &operator << (
                Serializer &serializer,
                const BTree::Header &header) {
            serializer <<
                header.magic <<
                header.entriesPerNode <<
                header.rootOffset;
            return serializer;
        }

        Serializer &operator >> (
                Serializer &serializer,
                BTree::Header &header) {
            serializer >>
                header.magic >>
                header.entriesPerNode >>
                header.rootOffset;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
