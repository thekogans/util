#include <cstring>
#include "thekogans/util/Exception.h"
#include "thekogans/util/Path.h"
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
        }

        BTree::Node::Node (
                BTree &btree_,
                ui64 number_) :
                btree (btree_),
                number (number_),
                count (0),
                leftNumber (NIDX64),
                entries (btree.header.entriesPerNode) {
            Path nodePath (GetPath ());
            if (nodePath.Exists ()) {
                ReadOnlyFile file (HostEndian, nodePath.path);
                ui32 magic;
                file >> magic;
                if (magic == MAGIC32) {
                    // File is host endian.
                }
                else if (ByteSwap<GuestEndian, HostEndian> (magic) == MAGIC32) {
                    file.endianness = GuestEndian;
                }
                else {
                    // FIXME: throw something.
                    assert (0);
                }
                file >> count >> leftNumber;
                for (ui32 i = 0; i < count; ++i) {
                    file >> entries[i];
                }
            }
        }

        void BTree::Node::Save () {
            SimpleFile file (
                HostEndian,
                GetPath (),
                SimpleFile::ReadWrite | SimpleFile::Create | SimpleFile::Truncate);
            file << MAGIC32 << count << leftNumber;
            for (ui32 i = 0; i < count; ++i) {
                file << entries[i];
            }
        }

        void BTree::Node::Delete () {
            Path nodePath (GetPath ());
            if (nodePath.Exists ()) {
                nodePath.Delete ();
            }
        }

        std::string BTree::Node::GetPath () const {
            std::string name (16, ' ');
            HexEncodeBuffer (&number, 8, name.data ());
            return MakePath (btree.path, name);
        }

        BTree::Node::SharedPtr BTree::Node::GetChild (ui32 index) {
            if (index == 0) {
                if (leftNode == nullptr && leftNumber != NIDX64) {
                    leftNode.Reset (new Node (btree, leftNumber));
                }
                return leftNode;
            }
            if (entries[index - 1].rightNode == nullptr &&
                    entries[index - 1].rightNumber != NIDX64) {
                entries[index - 1].rightNode.Reset (
                    new Node (btree, entries[index - 1].rightNumber));
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
                SharedPtr node,
                ui32 index) {
            for (ui32 i = index; i < count; ++i) {
                node->entries[node->count++] = entries[i];
            }
            count = index;
        }

        void BTree::Node::Concatenate (SharedPtr node) {
            for (ui32 i = 0; i < node->count; ++i) {
                entries[count++] = node->entries[i];
            }
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
            for (ui32 i = index; i < count; ++i) {
                entries[i] = entries[i + 1];
            }
            --count;
        }

        BTree::BTree (
                const std::string &path_,
                ui32 entriesPerNode) :
                path (path_),
                header (entriesPerNode) {
            Path btreePath (MakePath (path, "btree"));
            if (btreePath.Exists ()) {
                ReadOnlyFile file (HostEndian, btreePath.path);
                ui32 magic;
                file >> magic;
                if (magic == MAGIC32) {
                    // File is host endian.
                }
                else if (ByteSwap<GuestEndian, HostEndian> (magic) == MAGIC32) {
                    file.endianness = GuestEndian;
                }
                else {
                    // FIXME: throw something.
                    assert (0);
                }
                file >> header;
            }
            SetRoot (new Node (*this, header.rootNumber));
        }

        BTree::Key BTree::Search (const Key &key) {
            Key result (NIDX64, NIDX64);
            Node::SharedPtr node = root;
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
                Node::SharedPtr node (new Node (*this, header.nodeCounter++));
                Save ();
                node->leftNumber = root->number;
                node->leftNode = root;
                node->InsertEntry (entry, 0);
                node->Save ();
                SetRoot (node);
            }
        }

        void BTree::Delete (const Key &key) {
            if (Remove (key, root) && root->IsEmpty () && root->GetChild (0) != nullptr) {
                Node::SharedPtr node = root;
                SetRoot (root->GetChild (0));
                node->Delete ();
            }
        }

        bool BTree::Insert (
                Node::Entry &entry,
                Node::SharedPtr node) {
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
                        Node::SharedPtr right (new Node (*this, header.nodeCounter++));
                        Save ();
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
                        right->leftNumber = entry.rightNumber;
                        right->leftNode = entry.rightNode;
                        right->Save ();
                        entry.rightNumber = right->number;
                        entry.rightNode = right;
                        return false;
                    }
                    node->Save ();
                }
                return true;
            }
            return false;
        }

        bool BTree::Remove (
                const Key &key,
                Node::SharedPtr node) {
            ui32 index;
            bool found = node->Search (key, index);
            Node::SharedPtr child = node->GetChild (found ? index + 1 : index);
            if (found) {
                if (child != nullptr) {
                    Node::SharedPtr leaf = child;
                    while (leaf->leftNumber != NIDX64) {
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
                Node::SharedPtr node,
                ui32 index) {
            if (index == node->count) {
                Node::SharedPtr left = node->GetChild (index - 1);
                Node::SharedPtr right = node->GetChild (index);
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
                Node::SharedPtr left = node->GetChild (index);
                Node::SharedPtr right = node->GetChild (index + 1);
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
                Node::SharedPtr node,
                ui32 index,
                Node::SharedPtr left,
                Node::SharedPtr right) {
            node->entries[index].rightNumber = right->leftNumber;
            node->entries[index].rightNode = right->leftNode;
            right->InsertEntry (node->entries[index], 0);
            ui32 lastIndex = left->count - 1;
            right->leftNumber = left->entries[lastIndex].rightNumber;
            right->leftNode = left->entries[lastIndex].rightNode;
            left->entries[lastIndex].rightNumber = right->number;
            left->entries[lastIndex].rightNode = right;
            node->entries[index] = left->entries[lastIndex];
            left->RemoveEntry (lastIndex);
            node->Save ();
            left->Save ();
            right->Save ();
        }

        void BTree::RotateLeft (
                Node::SharedPtr node,
                ui32 index,
                Node::SharedPtr left,
                Node::SharedPtr right) {
            node->entries[index].rightNumber = right->leftNumber;
            node->entries[index].rightNode = right->leftNode;
            right->leftNumber = right->entries[0].rightNumber;
            right->leftNode = right->entries[0].rightNode;
            right->entries[0].rightNumber = right->number;
            right->entries[0].rightNode = right;
            left->Concatenate (node->entries[index]);
            node->entries[index] = right->entries[0];
            right->RemoveEntry (0);
            node->Save ();
            left->Save ();
            right->Save ();
        }

        void BTree::Merge (
                Node::SharedPtr node,
                ui32 index,
                Node::SharedPtr left,
                Node::SharedPtr right) {
            assert (left->count + right->count < header.entriesPerNode);
            node->entries[index].rightNumber = right->leftNumber;
            node->entries[index].rightNode = right->leftNode;
            left->Concatenate (node->entries[index]);
            left->Concatenate (right);
            right->Delete ();
            node->RemoveEntry (index);
            node->Save ();
            left->Save ();
        }

        void BTree::Save () {
            SimpleFile file (
                HostEndian,
                MakePath (path, "btree"),
                SimpleFile::ReadWrite | SimpleFile::Create | SimpleFile::Truncate);
            file << MAGIC32 << header;
        }

        void BTree::SetRoot (Node::SharedPtr node) {
            root = node;
            if (header.rootNumber != root->number) {
                header.rootNumber = root->number;
                Save ();
            }
        }

        Serializer &operator << (
                Serializer &serializer,
                const BTree::Node::Entry &entry) {
            serializer << entry.key << entry.rightNumber;
            return serializer;
        }

        Serializer &operator >> (
                Serializer &serializer,
                BTree::Node::Entry &entry) {
            serializer >> entry.key >> entry.rightNumber;
            return serializer;
        }

        Serializer &operator << (
                Serializer &serializer,
                const BTree::Header &header) {
            serializer <<
                header.entriesPerNode <<
                header.nodeCounter <<
                header.rootNumber;
            return serializer;
        }

        Serializer &operator >> (
                Serializer &serializer,
                BTree::Header &header) {
            serializer >>
                header.entriesPerNode >>
                header.nodeCounter >>
                header.rootNumber;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
